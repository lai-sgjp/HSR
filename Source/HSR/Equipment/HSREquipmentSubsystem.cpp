#include "HSREquipmentSubsystem.h"

#include "../Data/Definitions/HSREquipmentDefinition.h"
#include "../Data/Definitions/HSRRelicDefinition.h"
#include "../Save/HSRSaveTypes.h"

#include <cmath>

void UHSREquipmentSubsystem::ExportSaveData(TArray<FHSREquipmentSaveDto>& Out) const
{
	Out.Reset();
	for (const auto& L : Loadouts)
	{
		for (const auto& P : L.Value.Loadout.Equipment)
		{
			FHSREquipmentSaveDto D; D.CharacterId=L.Key; D.DefinitionId=P.Value.DefinitionId; D.InstanceId=P.Value.InstanceId; D.Kind=0; D.Slot=(int32)P.Key; D.EnhancementLevel=P.Value.EnhancementLevel; D.Modifiers=P.Value.Modifiers; D.AuthorityRevision=L.Value.Revision; Out.Add(D);
		}
		for (const auto& P : L.Value.Loadout.Relics)
		{
			FHSREquipmentSaveDto D; D.CharacterId=L.Key; D.DefinitionId=P.Value.DefinitionId; D.InstanceId=P.Value.InstanceId; D.Kind=1; D.Slot=(int32)P.Key; D.EnhancementLevel=P.Value.EnhancementLevel; D.Modifiers=P.Value.Modifiers; D.AuthorityRevision=L.Value.Revision; if(const FDefinitionRule* Rule=Definitions.Find(D.DefinitionId))D.SetId=Rule->SetId; Out.Add(D);
		}
	}
	Out.Sort([](const FHSREquipmentSaveDto& A,const FHSREquipmentSaveDto& B){if(A.CharacterId!=B.CharacterId)return A.CharacterId<B.CharacterId;if(A.Kind!=B.Kind)return A.Kind<B.Kind;if(A.Slot!=B.Slot)return A.Slot<B.Slot;return A.InstanceId<B.InstanceId;});
}

bool UHSREquipmentSubsystem::PrepareRestore(const TArray<FHSREquipmentSaveDto>& In, FHSREquipmentRestoreMap& Out) const
{
	Out.Reset(); TSet<FGuid> Seen;
	for (const FHSREquipmentSaveDto& D : In)
	{
		if (!D.CharacterId.IsValid() || !D.InstanceId.IsValid() || Seen.Contains(D.InstanceId) || !IsSlotValid((EHSREquipmentKind)D.Kind,D.Slot)) return false;
		Seen.Add(D.InstanceId); FHSREquipmentInstance I; I.InstanceId=D.InstanceId; I.EnhancementLevel=D.EnhancementLevel; I.Modifiers=D.Modifiers; I.Kind=(EHSREquipmentKind)D.Kind;
		const FDefinitionRule* Rule=Definitions.Find(D.DefinitionId); if(!Rule || Rule->Kind!=I.Kind || D.EnhancementLevel<0 || D.EnhancementLevel>Rule->EnhancementCap || D.AuthorityRevision<0 || !IsValidModifiers(I.Modifiers) || (I.Kind==EHSREquipmentKind::Relic && D.SetId!=Rule->SetId)) return false;
		I.DefinitionId=D.DefinitionId; const bool bExistingCharacter=Out.Contains(D.CharacterId); FHSREquipmentRestoreState& State=Out.FindOrAdd(D.CharacterId); if(bExistingCharacter && State.Revision!=D.AuthorityRevision)return false; State.Revision=D.AuthorityRevision; FHSREquipmentLoadout& L=State.Loadout; if (I.Kind==EHSREquipmentKind::Equipment) { if(L.Equipment.Contains((EHSREquipmentSlot)D.Slot)) return false; L.Equipment.Add((EHSREquipmentSlot)D.Slot,I);} else {if(L.Relics.Contains((EHSRRelicSlot)D.Slot)) return false; L.Relics.Add((EHSRRelicSlot)D.Slot,I); ++State.RelicSetCounts.FindOrAdd(D.SetId);}
	}
	return true;
}

void UHSREquipmentSubsystem::CommitRestore(const FHSREquipmentRestoreMap& Candidate)
{
	Loadouts.Reset(); InstanceOwners.Reset(); for(const auto& P:Candidate){FLoadoutState& S=Loadouts.Add(P.Key); S.Loadout=P.Value.Loadout; S.Revision=P.Value.Revision; for(const auto& E:S.Loadout.Equipment) InstanceOwners.Add(E.Value.InstanceId,P.Key); for(const auto& R:S.Loadout.Relics) InstanceOwners.Add(R.Value.InstanceId,P.Key);} }
void UHSREquipmentSubsystem::NotifyRestored(const TSet<FGuid>& Changed){for(const FGuid& Id:Changed){int32 Rev=Loadouts.FindRef(Id).Revision; LoadoutChanged.Broadcast(Id,Rev);}}

EHSREquipmentOperationResult UHSREquipmentSubsystem::RegisterDefinition(const UHSREquipmentDefinition& Definition)
{
	if (Definition.DefinitionId.IsNone())
	{
		return EHSREquipmentOperationResult::InvalidDefinitionId;
	}
	if (Definition.EnhancementCap < 0)
	{
		return EHSREquipmentOperationResult::InvalidEnhancementLevel;
	}
	if (Definitions.Contains(Definition.DefinitionId))
	{
		return EHSREquipmentOperationResult::DuplicateDefinitionId;
	}

	FDefinitionRule Rule;
	Rule.Kind = EHSREquipmentKind::Equipment;
	Rule.Slot = static_cast<int32>(Definition.Slot);
	Rule.EnhancementCap = Definition.EnhancementCap;
	Definitions.Add(Definition.DefinitionId, Rule);
	return EHSREquipmentOperationResult::Success;
}

EHSREquipmentOperationResult UHSREquipmentSubsystem::RegisterDefinition(const UHSRRelicDefinition& Definition)
{
	if (Definition.DefinitionId.IsNone())
	{
		return EHSREquipmentOperationResult::InvalidDefinitionId;
	}
	if (Definition.EnhancementCap < 0)
	{
		return EHSREquipmentOperationResult::InvalidEnhancementLevel;
	}
	if (Definitions.Contains(Definition.DefinitionId))
	{
		return EHSREquipmentOperationResult::DuplicateDefinitionId;
	}

	FDefinitionRule Rule;
	Rule.Kind = EHSREquipmentKind::Relic;
	Rule.Slot = static_cast<int32>(Definition.Slot);
	Rule.EnhancementCap = Definition.EnhancementCap;
	Rule.SetId = Definition.SetId;
	Definitions.Add(Definition.DefinitionId, Rule);
	return EHSREquipmentOperationResult::Success;
}

EHSREquipmentOperationResult UHSREquipmentSubsystem::Equip(const FGuid& CharacterId, const FHSREquipmentInstance& Instance)
{
	if (!CharacterId.IsValid()) return EHSREquipmentOperationResult::InvalidCharacterId;
	if (!Instance.InstanceId.IsValid()) return EHSREquipmentOperationResult::InvalidInstanceId;
	if (!IsValidInstance(Instance)) return EHSREquipmentOperationResult::InvalidModifier;
	const FDefinitionRule* Rule = FindDefinition(Instance);
	if (Rule == nullptr) return EHSREquipmentOperationResult::UnknownDefinition;
	if (Rule->Kind != Instance.Kind || !IsSlotValid(Rule->Kind, Rule->Slot)) return EHSREquipmentOperationResult::InvalidSlot;
	if (Instance.EnhancementLevel < 0 || Instance.EnhancementLevel > Rule->EnhancementCap) return EHSREquipmentOperationResult::InvalidEnhancementLevel;
	if (InstanceOwners.Contains(Instance.InstanceId)) return EHSREquipmentOperationResult::InstanceAlreadyEquipped;

	FHSREquipmentLoadout Candidate = Loadouts.FindRef(CharacterId).Loadout;
	if (IsSlotOccupied(Candidate, Rule->Kind, Rule->Slot)) return EHSREquipmentOperationResult::SlotOccupied;
	if (Rule->Kind == EHSREquipmentKind::Equipment) Candidate.Equipment.Add(static_cast<EHSREquipmentSlot>(Rule->Slot), Instance);
	else Candidate.Relics.Add(static_cast<EHSRRelicSlot>(Rule->Slot), Instance);
	CommitLoadout(CharacterId, Candidate);
	return EHSREquipmentOperationResult::Success;
}

EHSREquipmentOperationResult UHSREquipmentSubsystem::Replace(const FGuid& CharacterId, const FHSREquipmentInstance& Instance)
{
	if (!CharacterId.IsValid()) return EHSREquipmentOperationResult::InvalidCharacterId;
	if (!Instance.InstanceId.IsValid()) return EHSREquipmentOperationResult::InvalidInstanceId;
	if (!IsValidInstance(Instance)) return EHSREquipmentOperationResult::InvalidModifier;
	const FDefinitionRule* Rule = FindDefinition(Instance);
	if (Rule == nullptr) return EHSREquipmentOperationResult::UnknownDefinition;
	if (Rule->Kind != Instance.Kind || !IsSlotValid(Rule->Kind, Rule->Slot)) return EHSREquipmentOperationResult::InvalidSlot;
	if (Instance.EnhancementLevel < 0 || Instance.EnhancementLevel > Rule->EnhancementCap) return EHSREquipmentOperationResult::InvalidEnhancementLevel;
	if (InstanceOwners.Contains(Instance.InstanceId)) return EHSREquipmentOperationResult::InstanceAlreadyEquipped;

	const FLoadoutState* Existing = Loadouts.Find(CharacterId);
	if (Existing == nullptr || !IsSlotOccupied(Existing->Loadout, Rule->Kind, Rule->Slot)) return EHSREquipmentOperationResult::TargetNotFound;
	FHSREquipmentLoadout Candidate = Existing->Loadout;
	if (Rule->Kind == EHSREquipmentKind::Equipment) Candidate.Equipment.Add(static_cast<EHSREquipmentSlot>(Rule->Slot), Instance);
	else Candidate.Relics.Add(static_cast<EHSRRelicSlot>(Rule->Slot), Instance);
	CommitLoadout(CharacterId, Candidate);
	return EHSREquipmentOperationResult::Success;
}

EHSREquipmentOperationResult UHSREquipmentSubsystem::Unequip(const FGuid& CharacterId, EHSREquipmentKind Kind, int32 Slot, const FGuid& ExpectedInstanceId)
{
	if (!CharacterId.IsValid()) return EHSREquipmentOperationResult::InvalidCharacterId;
	if (!ExpectedInstanceId.IsValid()) return EHSREquipmentOperationResult::InvalidInstanceId;
	if (!IsSlotValid(Kind, Slot)) return EHSREquipmentOperationResult::InvalidSlot;
	const FLoadoutState* Existing = Loadouts.Find(CharacterId);
	if (Existing == nullptr) return EHSREquipmentOperationResult::TargetNotFound;
	const FHSREquipmentInstance* Current = FindInstance(Existing->Loadout, Kind, Slot);
	if (Current == nullptr) return EHSREquipmentOperationResult::TargetNotFound;
	if (Current->InstanceId != ExpectedInstanceId) return EHSREquipmentOperationResult::InstanceMismatch;

	FHSREquipmentLoadout Candidate = Existing->Loadout;
	if (Kind == EHSREquipmentKind::Equipment) Candidate.Equipment.Remove(static_cast<EHSREquipmentSlot>(Slot));
	else Candidate.Relics.Remove(static_cast<EHSRRelicSlot>(Slot));
	CommitLoadout(CharacterId, Candidate);
	return EHSREquipmentOperationResult::Success;
}

EHSREquipmentOperationResult UHSREquipmentSubsystem::SetEnhancementLevel(const FGuid& CharacterId, const FGuid& InstanceId, int32 NewLevel)
{
	if (!CharacterId.IsValid()) return EHSREquipmentOperationResult::InvalidCharacterId;
	if (!InstanceId.IsValid()) return EHSREquipmentOperationResult::InvalidInstanceId;
	const FGuid* Owner = InstanceOwners.Find(InstanceId);
	if (Owner == nullptr) return EHSREquipmentOperationResult::TargetNotFound;
	if (*Owner != CharacterId) return EHSREquipmentOperationResult::InstanceMismatch;
	FLoadoutState* Existing = Loadouts.Find(CharacterId);
	check(Existing != nullptr);

	FHSREquipmentLoadout Candidate = Existing->Loadout;
	FHSREquipmentInstance* Instance = nullptr;
	for (TPair<EHSREquipmentSlot, FHSREquipmentInstance>& Pair : Candidate.Equipment)
	{
		if (Pair.Value.InstanceId == InstanceId) { Instance = &Pair.Value; break; }
	}
	if (Instance == nullptr)
	{
		for (TPair<EHSRRelicSlot, FHSREquipmentInstance>& Pair : Candidate.Relics)
		{
			if (Pair.Value.InstanceId == InstanceId) { Instance = &Pair.Value; break; }
		}
	}
	if (Instance == nullptr) return EHSREquipmentOperationResult::TargetNotFound;
	const FDefinitionRule* Rule = FindDefinition(*Instance);
	if (Rule == nullptr || NewLevel < 0 || NewLevel > Rule->EnhancementCap) return EHSREquipmentOperationResult::InvalidEnhancementLevel;
	if (Instance->EnhancementLevel == NewLevel) return EHSREquipmentOperationResult::NoOp;
	Instance->EnhancementLevel = NewLevel;
	CommitLoadout(CharacterId, Candidate);
	return EHSREquipmentOperationResult::Success;
}

bool UHSREquipmentSubsystem::GetLoadout(const FGuid& CharacterId, FHSREquipmentLoadout& OutLoadout, int32& OutRevision) const
{
	const FLoadoutState* State = Loadouts.Find(CharacterId);
	if (State == nullptr) return false;
	OutLoadout = State->Loadout;
	OutRevision = State->Revision;
	return true;
}

void UHSREquipmentSubsystem::GetRelicSetSnapshots(const FGuid& CharacterId, TArray<FHSRRelicSetSnapshot>& Out) const
{
	Out.Reset();
	const FLoadoutState* State=Loadouts.Find(CharacterId);
	if(!State)return;
	TMap<FName,int32> Counts;
	for(const auto& Pair:State->Loadout.Relics)if(const FDefinitionRule* Rule=Definitions.Find(Pair.Value.DefinitionId))if(!Rule->SetId.IsNone())++Counts.FindOrAdd(Rule->SetId);
	for(const auto& Pair:Counts){FHSRRelicSetSnapshot Row;Row.SetId=Pair.Key;Row.EquippedCount=Pair.Value;Row.bActive=Row.EquippedCount>=Row.Threshold;Row.SetSourceId=Row.bActive?Row.SetId:NAME_None;Out.Add(Row);}
	Out.Sort([](const FHSRRelicSetSnapshot& A,const FHSRRelicSetSnapshot& B){return A.SetId.LexicalLess(B.SetId);});
}

bool UHSREquipmentSubsystem::IsValidInstance(const FHSREquipmentInstance& Instance) const
{
	return !Instance.DefinitionId.IsNone() && IsValidModifiers(Instance.Modifiers);
}

bool UHSREquipmentSubsystem::IsValidModifiers(const TArray<FHSREquipmentModifier>& Modifiers) const
{
	double Totals[4] = { 0.0, 0.0, 0.0, 0.0 };
	for (const FHSREquipmentModifier& Modifier : Modifiers)
	{
		const int32 StatIndex = static_cast<int32>(Modifier.Stat);
		if (StatIndex < 0 || StatIndex >= UE_ARRAY_COUNT(Totals) || !FMath::IsFinite(Modifier.Value) || Modifier.Value < 0.0f) return false;
		Totals[StatIndex] += static_cast<double>(Modifier.Value);
		if (!std::isfinite(Totals[StatIndex]) || Totals[StatIndex] > static_cast<double>(TNumericLimits<float>::Max())) return false;
	}
	return true;
}

const UHSREquipmentSubsystem::FDefinitionRule* UHSREquipmentSubsystem::FindDefinition(const FHSREquipmentInstance& Instance) const
{
	return Definitions.Find(Instance.DefinitionId);
}

bool UHSREquipmentSubsystem::IsSlotValid(EHSREquipmentKind Kind, int32 Slot) const
{
	return Slot >= 0 && Slot < (Kind == EHSREquipmentKind::Equipment ? static_cast<int32>(EHSREquipmentSlot::Feet) + 1 : static_cast<int32>(EHSRRelicSlot::LinkRope) + 1);
}

bool UHSREquipmentSubsystem::IsSlotOccupied(const FHSREquipmentLoadout& Loadout, EHSREquipmentKind Kind, int32 Slot) const
{
	return FindInstance(Loadout, Kind, Slot) != nullptr;
}

FHSREquipmentInstance* UHSREquipmentSubsystem::FindInstance(FHSREquipmentLoadout& Loadout, EHSREquipmentKind Kind, int32 Slot)
{
	return Kind == EHSREquipmentKind::Equipment ? Loadout.Equipment.Find(static_cast<EHSREquipmentSlot>(Slot)) : Loadout.Relics.Find(static_cast<EHSRRelicSlot>(Slot));
}

const FHSREquipmentInstance* UHSREquipmentSubsystem::FindInstance(const FHSREquipmentLoadout& Loadout, EHSREquipmentKind Kind, int32 Slot) const
{
	return Kind == EHSREquipmentKind::Equipment ? Loadout.Equipment.Find(static_cast<EHSREquipmentSlot>(Slot)) : Loadout.Relics.Find(static_cast<EHSRRelicSlot>(Slot));
}

void UHSREquipmentSubsystem::CommitLoadout(const FGuid& CharacterId, const FHSREquipmentLoadout& Candidate)
{
	FLoadoutState& State = Loadouts.FindOrAdd(CharacterId);
	for (const TPair<EHSREquipmentSlot, FHSREquipmentInstance>& Pair : State.Loadout.Equipment) InstanceOwners.Remove(Pair.Value.InstanceId);
	for (const TPair<EHSRRelicSlot, FHSREquipmentInstance>& Pair : State.Loadout.Relics) InstanceOwners.Remove(Pair.Value.InstanceId);
	State.Loadout = Candidate;
	for (const TPair<EHSREquipmentSlot, FHSREquipmentInstance>& Pair : State.Loadout.Equipment) InstanceOwners.Add(Pair.Value.InstanceId, CharacterId);
	for (const TPair<EHSRRelicSlot, FHSREquipmentInstance>& Pair : State.Loadout.Relics) InstanceOwners.Add(Pair.Value.InstanceId, CharacterId);
	++State.Revision;
	LoadoutChanged.Broadcast(CharacterId, State.Revision);
}
