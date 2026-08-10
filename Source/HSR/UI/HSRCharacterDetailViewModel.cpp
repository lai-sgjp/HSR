#include "HSRCharacterDetailViewModel.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Save/HSRSaveSubsystem.h"
#include "../Party/HSRPartySubsystem.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Equipment/HSREquipmentStatAggregator.h"
#include "../Equipment/HSREquipmentTypes.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "Curves/CurveFloat.h"

void UHSRCharacterDetailViewModel::Initialize(UHSRCharacterProfileSubsystem* P, UHSRSaveSubsystem* S,
	UHSRPartySubsystem* T, UHSREquipmentSubsystem* E)
{
	Uninitialize();
	Profiles = P;
	Save = S;
	Party = T;
	Equipment = E;
	if (P) ProfileHandle = P->OnProfileChanged().AddUObject(this, &UHSRCharacterDetailViewModel::HandleProfile);
	if (S) RestoreHandle = S->OnRestoreCommitted().AddUObject(this, &UHSRCharacterDetailViewModel::HandleRestore);
	if (T) PartyHandle = T->OnPartyChanged().AddUObject(this, &UHSRCharacterDetailViewModel::HandleParty);
	if (E) EquipmentHandle = E->OnLoadoutChanged().AddUObject(this, &UHSRCharacterDetailViewModel::HandleEquipmentLoadout);
}

void UHSRCharacterDetailViewModel::Uninitialize()
{
	if (Profiles.IsValid() && ProfileHandle.IsValid()) Profiles->OnProfileChanged().Remove(ProfileHandle);
	if (Save.IsValid() && RestoreHandle.IsValid()) Save->OnRestoreCommitted().Remove(RestoreHandle);
	if (Party.IsValid() && PartyHandle.IsValid()) Party->OnPartyChanged().Remove(PartyHandle);
	if (Equipment.IsValid() && EquipmentHandle.IsValid()) Equipment->OnLoadoutChanged().Remove(EquipmentHandle);
	ProfileHandle.Reset();
	RestoreHandle.Reset();
	PartyHandle.Reset();
	EquipmentHandle.Reset();
	Profiles.Reset();
	Save.Reset();
	Party.Reset();
	Equipment.Reset();
}

EHSRCharacterDetailResult UHSRCharacterDetailViewModel::BuildSnapshot(FName Id, FHSRCharacterDetailSnapshot& Out) const
{
	if (!Profiles.IsValid()) return EHSRCharacterDetailResult::NotInitialized;
	if (Id.IsNone()) return EHSRCharacterDetailResult::InvalidCharacterId;
	FHSRCharacterProfileSnapshot P;
	if (!Profiles->GetProfileSnapshot(Id, P)) return EHSRCharacterDetailResult::ProfileNotFound;
	const UHSRCharacterDefinition* D = nullptr;
	if (!Profiles->GetDefinition(Id, D) || !D) return EHSRCharacterDetailResult::DefinitionNotFound;
	FHSRCharacterProgressionContext C;
	if (!Profiles->GetProgressionContext(Id, C)) return EHSRCharacterDetailResult::InvalidSnapshot;
	const UCurveFloat* Curve = D->CumulativeExperienceCurve.LoadSynchronous();
	if (!Curve) return EHSRCharacterDetailResult::InvalidSnapshot;

	FHSRCharacterDetailSnapshot N;
	N.CharacterId = Id;
	N.DisplayName = D->DisplayName;
	N.Level = P.RuntimeState.Level;
	N.MaxLevel = D->MaxLevel;
	N.Experience = P.RuntimeState.Experience;
	N.ExperienceForCurrentLevel = N.Level <= 1 ? 0 : FMath::RoundToInt(Curve->GetFloatValue(N.Level));
	N.bAtMaxLevel = N.Level >= N.MaxLevel;
	N.ExperienceForNextLevel = N.bAtMaxLevel ? N.ExperienceForCurrentLevel : FMath::RoundToInt(Curve->GetFloatValue(N.Level + 1));
	N.Ascension = P.RuntimeState.Ascension;
	N.RuntimeRevision = P.RuntimeRevision;
	N.BaseStats.MaxHealth = D->BaseMaxHealth;
	N.BaseStats.Attack = D->BaseAttack;
	N.BaseStats.Defense = D->BaseDefense;
	N.BaseStats.Speed = D->BaseSpeed;
	N.DerivedStats = C.DerivedStats;
	N.ProgressionBonuses = C.ProgressionBonuses;

	// Aggregate equipped weapon/relic stat bonuses on top of the derived (base + progression) values,
	// so the character detail screen reflects the loadout the same way battle does.
	if (Equipment.IsValid())
	{
		FHSREquipmentLoadout Loadout;
		int32 EquipmentRevision = 0;
		if (Equipment->GetLoadout(HSRCharacterGuidFromProfileName(Id), Loadout, EquipmentRevision))
		{
			FHSREquipmentAggregate Aggregate;
			if (UHSREquipmentStatAggregator::Aggregate(Loadout, EquipmentRevision, Aggregate))
			{
				N.DerivedStats.MaxHealth = N.DerivedStats.MaxHealth + Aggregate.MaxHealth;
				N.DerivedStats.Attack = N.DerivedStats.Attack + Aggregate.Attack;
				N.DerivedStats.Defense = N.DerivedStats.Defense + Aggregate.Defense;
				N.DerivedStats.Speed = N.DerivedStats.Speed + Aggregate.Speed;
			}
		}
	}

	N.PortraitPath = D->Portrait.ToSoftObjectPath();
	N.bHasPortrait = !N.PortraitPath.IsNull();
	N.bIsValid = true;
	N.FailureReason = EHSRCharacterDetailResult::Success;
	for (const auto& K : D->SkillMaxLevels)
	{
		FHSRCharacterDetailSkill Skill;
		Skill.SkillId = K.Key;
		Skill.Level = P.RuntimeState.SkillLevels.FindRef(K.Key);
		Skill.MaxLevel = K.Value;
		N.Skills.Add(Skill);
	}
	N.Skills.Sort([](const auto& A, const auto& B) { return A.SkillId.LexicalLess(B.SkillId); });
	Out = MoveTemp(N);
	return EHSRCharacterDetailResult::Success;
}

void UHSRCharacterDetailViewModel::BroadcastSnapshot()
{
	Changed.Broadcast(Snapshot);
	OnSnapshotChanged.Broadcast(Snapshot);
}

EHSRCharacterDetailResult UHSRCharacterDetailViewModel::SelectCharacter(FName Id)
{
	FHSRCharacterDetailSnapshot Candidate;
	const auto R = BuildSnapshot(Id, Candidate);
	if (R != EHSRCharacterDetailResult::Success) return R;
	SelectedId = Id;
	Snapshot = MoveTemp(Candidate);
	bHasSnapshot = true;
	BroadcastSnapshot();
	return R;
}

EHSRCharacterDetailResult UHSRCharacterDetailViewModel::SelectPartySlot0()
{
	if (!Party.IsValid()) return EHSRCharacterDetailResult::NotInitialized;
	FHSRPartySnapshot P;
	Party->GetSnapshot(P);
	if (P.Slots.IsEmpty() || P.Slots[0].IsEmpty()) return EHSRCharacterDetailResult::PartySlotEmpty;
	return SelectCharacter(P.Slots[0].CharacterId);
}

void UHSRCharacterDetailViewModel::RefreshSelected()
{
	if (SelectedId.IsNone()) return;
	FHSRCharacterDetailSnapshot N;
	if (BuildSnapshot(SelectedId, N) != EHSRCharacterDetailResult::Success) return;
	if (bHasSnapshot && N.RuntimeRevision == Snapshot.RuntimeRevision) return;
	Snapshot = MoveTemp(N);
	bHasSnapshot = true;
	BroadcastSnapshot();
}

void UHSRCharacterDetailViewModel::HandleProfile(FName Id, int64)
{
	if (Id == SelectedId) RefreshSelected();
}

void UHSRCharacterDetailViewModel::HandleRestore(const FHSRRestoreCommitInfo& Info)
{
	if (Info.ChangedCharacterIds.Contains(SelectedId)) RefreshSelected();
}

void UHSRCharacterDetailViewModel::HandleParty(int64)
{
	if (SelectedId.IsNone()) SelectPartySlot0();
}

void UHSRCharacterDetailViewModel::HandleEquipmentLoadout(const FGuid& CharacterId, int32)
{
	// A loadout change affects the selected character's displayed derived stats; refresh so the
	// detail panel and shell both pick up the new equipment aggregate immediately.
	if (CharacterId == HSRCharacterGuidFromProfileName(SelectedId)) RefreshSelected();
}
