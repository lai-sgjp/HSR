#include "HSRPreBattleCandidateViewModel.h"

#include "../Party/HSRPartySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"

void UHSRPreBattleCandidateViewModel::Initialize(UHSRPartySubsystem* InParty,
	UHSRCharacterProfileSubsystem* InProfiles, const FHSREncounterRequest& InTemplate)
{
	Party = InParty;
	Profiles = InProfiles;
	Template = InTemplate;
	CandidateCharacterIds.Reset();
	BuffIds.Reset();
	FHSRPartySnapshot PartySnapshot;
	if (Party.IsValid() && Party->GetSnapshot(PartySnapshot))
	{
		for (const FHSRPartySlot& Slot : PartySnapshot.Slots) CandidateCharacterIds.Add(Slot.CharacterId);
	}
	RebuildSnapshot();
}

bool UHSRPreBattleCandidateViewModel::IsKnownProfile(FName CharacterId) const
{
	FHSRCharacterProfileSnapshot Profile;
	return Profiles.IsValid() && Profiles->GetProfileSnapshot(CharacterId, Profile);
}

bool UHSRPreBattleCandidateViewModel::ContainsCandidate(FName CharacterId, int32 IgnoreSlot) const
{
	for (int32 Index = 0; Index < CandidateCharacterIds.Num(); ++Index)
	{
		if (Index != IgnoreSlot && CandidateCharacterIds[Index] == CharacterId && !CharacterId.IsNone()) return true;
	}
	return false;
}

EHSRPreBattleCandidateResult UHSRPreBattleCandidateViewModel::SetCandidateSlot(int32 SlotIndex, FName CharacterId)
{
	if (!CandidateCharacterIds.IsValidIndex(SlotIndex)) return EHSRPreBattleCandidateResult::InvalidSlot;
	if (!IsKnownProfile(CharacterId)) return EHSRPreBattleCandidateResult::ProfileNotFound;
	if (ContainsCandidate(CharacterId, SlotIndex)) return EHSRPreBattleCandidateResult::DuplicateCharacter;
	CandidateCharacterIds[SlotIndex] = CharacterId;
	RebuildSnapshot();
	return EHSRPreBattleCandidateResult::Success;
}

EHSRPreBattleCandidateResult UHSRPreBattleCandidateViewModel::SetBuff(FName BuffId)
{
	if (BuffId.IsNone()) return EHSRPreBattleCandidateResult::InvalidCandidate;
	if (!BuffIds.Contains(BuffId)) BuffIds.Add(BuffId);
	RebuildSnapshot();
	return EHSRPreBattleCandidateResult::Success;
}

EHSRPreBattleCandidateResult UHSRPreBattleCandidateViewModel::ConfirmCandidate(FHSREncounterRequest& OutRequest)
{
	if (Template.EncounterId.IsNone() || Template.EnemyDefinitionId.IsNone() || Template.BattleMapPath.IsNone())
		return EHSRPreBattleCandidateResult::InvalidEncounter;
	if (CandidateCharacterIds.IsEmpty() || CandidateCharacterIds[0].IsNone())
		return EHSRPreBattleCandidateResult::EmptyLeader;
	TSet<FName> Seen;
	for (const FName CharacterId : CandidateCharacterIds)
	{
		if (!CharacterId.IsNone() && !IsKnownProfile(CharacterId)) return EHSRPreBattleCandidateResult::ProfileNotFound;
		if (!CharacterId.IsNone() && Seen.Contains(CharacterId)) return EHSRPreBattleCandidateResult::DuplicateCharacter;
		if (!CharacterId.IsNone()) Seen.Add(CharacterId);
	}
	OutRequest = Template;
	OutRequest.PlayerCharacterId = CandidateCharacterIds[0];
	// Densify: empty candidate slots are legal in the grid but meaningless as participants,
	// so the committed request carries only filled members, leader first.
	OutRequest.PlayerPartyIds.Reset();
	for (const FName CharacterId : CandidateCharacterIds)
	{
		if (!CharacterId.IsNone()) OutRequest.PlayerPartyIds.Add(CharacterId);
	}
	OutRequest.BuffIds = BuffIds;
	return EHSRPreBattleCandidateResult::Success;
}

EHSRPreBattleCandidateResult UHSRPreBattleCandidateViewModel::CancelCandidate()
{
	if (!Party.IsValid()) return EHSRPreBattleCandidateResult::InvalidCandidate;
	FHSRPartySnapshot PartySnapshot;
	if (!Party->GetSnapshot(PartySnapshot)) return EHSRPreBattleCandidateResult::InvalidCandidate;
	CandidateCharacterIds.Reset();
	for (const FHSRPartySlot& Slot : PartySnapshot.Slots) CandidateCharacterIds.Add(Slot.CharacterId);
	BuffIds.Reset();
	RebuildSnapshot();
	return EHSRPreBattleCandidateResult::Success;
}

void UHSRPreBattleCandidateViewModel::RebuildSnapshot()
{
	FHSRPartySnapshot PartySnapshot;
	Snapshot = FHSRPreBattleCandidateSnapshot();
	Snapshot.CandidateCharacterIds = CandidateCharacterIds;
	Snapshot.BuffIds = BuffIds;
	Snapshot.EncounterId = Template.EncounterId;
	if (Party.IsValid() && Party->GetSnapshot(PartySnapshot)) Snapshot.PartyRevision = PartySnapshot.Revision;
	Snapshot.bHasPendingChanges = !BuffIds.IsEmpty();
	if (PartySnapshot.Slots.Num() == CandidateCharacterIds.Num())
	{
		for (int32 Index = 0; Index < CandidateCharacterIds.Num(); ++Index)
			Snapshot.bHasPendingChanges |= PartySnapshot.Slots[Index].CharacterId != CandidateCharacterIds[Index];
	}
}
