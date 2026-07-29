#include "HSREquipmentSubsystem.h"

#include "../Data/Definitions/HSREquipmentDefinition.h"
#include "../Data/Definitions/HSRRelicDefinition.h"
#include "../Data/Definitions/HSRItemEquipmentMappingCatalog.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Save/HSRSaveTypes.h"

#include <cmath>

void UHSREquipmentSubsystem::ExportSaveData(TArray<FHSREquipmentSaveDto>& Out) const
{
	Out.Reset();
	for (const auto& L : Loadouts)
	{
		for (const auto& P : L.Value.Equipment)
		{
			const FHSREquipmentInstance* I=InstanceRegistry.Find(P.Value); if(!I)continue; FHSREquipmentSaveDto D; D.CharacterId=L.Key; D.DefinitionId=I->DefinitionId; D.InstanceId=I->InstanceId; D.Kind=0; D.Slot=(int32)P.Key; D.EnhancementLevel=I->EnhancementLevel; D.Modifiers=I->Modifiers; D.AuthorityRevision=L.Value.Revision; Out.Add(D);
		}
		for (const auto& P : L.Value.Relics)
		{
			const FHSREquipmentInstance* I=InstanceRegistry.Find(P.Value); if(!I)continue; FHSREquipmentSaveDto D; D.CharacterId=L.Key; D.DefinitionId=I->DefinitionId; D.InstanceId=I->InstanceId; D.Kind=1; D.Slot=(int32)P.Key; D.EnhancementLevel=I->EnhancementLevel; D.Modifiers=I->Modifiers; D.AuthorityRevision=L.Value.Revision; if(const FDefinitionRule* Rule=Definitions.Find(D.DefinitionId))D.SetId=Rule->SetId; Out.Add(D);
		}
	}
	Out.Sort([](const FHSREquipmentSaveDto& A,const FHSREquipmentSaveDto& B){if(A.CharacterId!=B.CharacterId)return A.CharacterId<B.CharacterId;if(A.Kind!=B.Kind)return A.Kind<B.Kind;if(A.Slot!=B.Slot)return A.Slot<B.Slot;return A.InstanceId<B.InstanceId;});
}

void UHSREquipmentSubsystem::ExportSaveData(TArray<FHSREquipmentRegistryDto>& OutRegistry, TArray<FHSREquipmentPlacementDto>& OutPlacements) const
{
	OutRegistry.Reset(); OutPlacements.Reset();
	for (const auto& Pair : InstanceRegistry)
	{
		FHSREquipmentRegistryDto Row; Row.InstanceId=Pair.Key; Row.DefinitionId=Pair.Value.DefinitionId; Row.Kind=static_cast<int32>(Pair.Value.Kind); Row.EnhancementLevel=Pair.Value.EnhancementLevel; Row.Modifiers=Pair.Value.Modifiers; if(const FDefinitionRule* Rule=Definitions.Find(Row.DefinitionId))Row.SetId=Rule->SetId; OutRegistry.Add(MoveTemp(Row));
	}
	for (const auto& Owner : Loadouts)
	{
		for(const auto& Pair:Owner.Value.Equipment){FHSREquipmentPlacementDto Row;Row.InstanceId=Pair.Value;Row.CharacterId=Owner.Key;Row.Kind=0;Row.Slot=static_cast<int32>(Pair.Key);Row.AuthorityRevision=Owner.Value.Revision;OutPlacements.Add(Row);}
		for(const auto& Pair:Owner.Value.Relics){FHSREquipmentPlacementDto Row;Row.InstanceId=Pair.Value;Row.CharacterId=Owner.Key;Row.Kind=1;Row.Slot=static_cast<int32>(Pair.Key);Row.AuthorityRevision=Owner.Value.Revision;OutPlacements.Add(Row);}
	}
	OutRegistry.Sort([](const auto& A,const auto& B){return A.InstanceId<B.InstanceId;});
	OutPlacements.Sort([](const auto& A,const auto& B){if(A.CharacterId!=B.CharacterId)return A.CharacterId<B.CharacterId;if(A.Kind!=B.Kind)return A.Kind<B.Kind;if(A.Slot!=B.Slot)return A.Slot<B.Slot;return A.InstanceId<B.InstanceId;});
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

bool UHSREquipmentSubsystem::PrepareRestore(const TArray<FHSREquipmentRegistryDto>& Registry, const TArray<FHSREquipmentPlacementDto>& Placements, FHSREquipmentRegistryRestoreState& Out) const
{
	Out = FHSREquipmentRegistryRestoreState();
	for(const auto& D:Registry){FHSREquipmentInstance I;I.InstanceId=D.InstanceId;I.DefinitionId=D.DefinitionId;I.Kind=static_cast<EHSREquipmentKind>(D.Kind);I.EnhancementLevel=D.EnhancementLevel;I.Modifiers=D.Modifiers;const FDefinitionRule* Rule=Definitions.Find(I.DefinitionId);if(!I.InstanceId.IsValid()||Out.Registry.Contains(I.InstanceId)||!Rule||Rule->Kind!=I.Kind||D.EnhancementLevel<0||D.EnhancementLevel>Rule->EnhancementCap||!IsValidModifiers(I.Modifiers)||(I.Kind==EHSREquipmentKind::Relic&&D.SetId!=Rule->SetId))return false;Out.Registry.Add(I.InstanceId,MoveTemp(I));}
	TSet<FGuid> Seen;
	for(const auto& D:Placements){const FHSREquipmentInstance* I=Out.Registry.Find(D.InstanceId);if(!I||!D.CharacterId.IsValid()||Seen.Contains(D.InstanceId)||D.AuthorityRevision<0||D.Kind!=static_cast<int32>(I->Kind)||!IsSlotValid(I->Kind,D.Slot))return false;const FDefinitionRule* Rule=FindDefinition(*I);if(!Rule||Rule->Slot!=D.Slot)return false;Seen.Add(D.InstanceId);const bool bExisting=Out.Loadouts.Contains(D.CharacterId);FHSREquipmentRestoreState& S=Out.Loadouts.FindOrAdd(D.CharacterId);if(bExisting&&S.Revision!=D.AuthorityRevision)return false;S.Revision=D.AuthorityRevision;if(I->Kind==EHSREquipmentKind::Equipment){if(S.Loadout.Equipment.Contains(static_cast<EHSREquipmentSlot>(D.Slot)))return false;S.Loadout.Equipment.Add(static_cast<EHSREquipmentSlot>(D.Slot),*I);}else{if(S.Loadout.Relics.Contains(static_cast<EHSRRelicSlot>(D.Slot)))return false;S.Loadout.Relics.Add(static_cast<EHSRRelicSlot>(D.Slot),*I);++S.RelicSetCounts.FindOrAdd(Rule->SetId);}}
	return true;
}

void UHSREquipmentSubsystem::CommitRestore(const FHSREquipmentRestoreMap& Candidate)
{
	Loadouts.Reset(); InstanceOwners.Reset(); InstanceRegistry.Reset(); for(const auto& P:Candidate){FLoadoutState& S=Loadouts.Add(P.Key); S.Revision=P.Value.Revision; for(const auto& E:P.Value.Loadout.Equipment){InstanceRegistry.Add(E.Value.InstanceId,E.Value);S.Equipment.Add(E.Key,E.Value.InstanceId);InstanceOwners.Add(E.Value.InstanceId,P.Key);} for(const auto& R:P.Value.Loadout.Relics){InstanceRegistry.Add(R.Value.InstanceId,R.Value);S.Relics.Add(R.Key,R.Value.InstanceId);InstanceOwners.Add(R.Value.InstanceId,P.Key);}}
	MovementLedger.Reset(); MovementLedgerOrder.Reset();
}
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
void UHSREquipmentSubsystem::CommitRestore(const FHSREquipmentRegistryRestoreState& Candidate)
{
	InstanceRegistry=Candidate.Registry;Loadouts.Reset();InstanceOwners.Reset();for(const auto& P:Candidate.Loadouts){FLoadoutState& S=Loadouts.Add(P.Key);S.Revision=P.Value.Revision;for(const auto& E:P.Value.Loadout.Equipment){S.Equipment.Add(E.Key,E.Value.InstanceId);InstanceOwners.Add(E.Value.InstanceId,P.Key);}for(const auto& R:P.Value.Loadout.Relics){S.Relics.Add(R.Key,R.Value.InstanceId);InstanceOwners.Add(R.Value.InstanceId,P.Key);}}
	MovementLedger.Reset();MovementLedgerOrder.Reset();
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

EHSREquipmentOperationResult UHSREquipmentSubsystem::RegisterInstance(const FHSREquipmentInstance& Instance)
{
	if (!Instance.InstanceId.IsValid()) return EHSREquipmentOperationResult::InvalidInstanceId;
	if (!IsValidInstance(Instance)) return EHSREquipmentOperationResult::InvalidModifier;
	const FDefinitionRule* Rule = FindDefinition(Instance);
	if (!Rule) return EHSREquipmentOperationResult::UnknownDefinition;
	if (Rule->Kind != Instance.Kind || !IsSlotValid(Rule->Kind, Rule->Slot)) return EHSREquipmentOperationResult::InvalidSlot;
	if (Instance.EnhancementLevel < 0 || Instance.EnhancementLevel > Rule->EnhancementCap) return EHSREquipmentOperationResult::InvalidEnhancementLevel;
	if (const FHSREquipmentInstance* Existing = InstanceRegistry.Find(Instance.InstanceId))
	{
		return IsSamePayload(*Existing, Instance) ? EHSREquipmentOperationResult::NoOp : EHSREquipmentOperationResult::InstancePayloadConflict;
	}
	InstanceRegistry.Add(Instance.InstanceId, Instance);
	return EHSREquipmentOperationResult::Success;
}

bool UHSREquipmentSubsystem::FindRegisteredInstance(const FGuid& InstanceId, FHSREquipmentInstance& OutInstance) const
{
	const FHSREquipmentInstance* Instance = InstanceRegistry.Find(InstanceId);
	if (!Instance) return false;
	OutInstance = *Instance;
	return true;
}

EHSREquipmentOperationResult UHSREquipmentSubsystem::Equip(const FGuid& CharacterId, const FHSREquipmentInstance& Instance)
{
	const EHSREquipmentOperationResult Registration = RegisterInstance(Instance);
	if (Registration != EHSREquipmentOperationResult::Success && Registration != EHSREquipmentOperationResult::NoOp) return Registration;
	const EHSREquipmentOperationResult Result = EquipById(CharacterId, Instance.InstanceId);
	if (Registration == EHSREquipmentOperationResult::Success && Result != EHSREquipmentOperationResult::Success) InstanceRegistry.Remove(Instance.InstanceId);
	return Result;
}

EHSREquipmentOperationResult UHSREquipmentSubsystem::EquipById(const FGuid& CharacterId, const FGuid& InstanceId)
{
	if (!CharacterId.IsValid()) return EHSREquipmentOperationResult::InvalidCharacterId;
	if (!InstanceId.IsValid()) return EHSREquipmentOperationResult::InvalidInstanceId;
	const FHSREquipmentInstance* Instance = InstanceRegistry.Find(InstanceId);
	if (!Instance) return EHSREquipmentOperationResult::TargetNotFound;
	const FDefinitionRule* Rule = FindDefinition(*Instance);
	if (!Rule || Rule->Kind != Instance->Kind || !IsSlotValid(Rule->Kind, Rule->Slot)) return EHSREquipmentOperationResult::InvalidSlot;
	if (InstanceOwners.Contains(InstanceId)) return EHSREquipmentOperationResult::InstanceAlreadyEquipped;
	FLoadoutState Candidate = Loadouts.FindRef(CharacterId);
	if (IsSlotOccupied(Candidate, Rule->Kind, Rule->Slot)) return EHSREquipmentOperationResult::SlotOccupied;
	if (Rule->Kind == EHSREquipmentKind::Equipment) Candidate.Equipment.Add(static_cast<EHSREquipmentSlot>(Rule->Slot), InstanceId);
	else Candidate.Relics.Add(static_cast<EHSRRelicSlot>(Rule->Slot), InstanceId);
	CommitLoadout(CharacterId, MoveTemp(Candidate));
	return EHSREquipmentOperationResult::Success;
}

EHSREquipmentOperationResult UHSREquipmentSubsystem::Replace(const FGuid& CharacterId, const FHSREquipmentInstance& Instance)
{
	const EHSREquipmentOperationResult Registration = RegisterInstance(Instance);
	if (Registration != EHSREquipmentOperationResult::Success && Registration != EHSREquipmentOperationResult::NoOp) return Registration;
	const EHSREquipmentOperationResult Result = ReplaceById(CharacterId, Instance.InstanceId);
	if (Registration == EHSREquipmentOperationResult::Success && Result != EHSREquipmentOperationResult::Success) InstanceRegistry.Remove(Instance.InstanceId);
	return Result;
}

EHSREquipmentOperationResult UHSREquipmentSubsystem::ReplaceById(const FGuid& CharacterId, const FGuid& InstanceId)
{
	if (!CharacterId.IsValid()) return EHSREquipmentOperationResult::InvalidCharacterId;
	if (!InstanceId.IsValid()) return EHSREquipmentOperationResult::InvalidInstanceId;
	const FHSREquipmentInstance* Instance = InstanceRegistry.Find(InstanceId);
	if (!Instance) return EHSREquipmentOperationResult::TargetNotFound;
	const FDefinitionRule* Rule = FindDefinition(*Instance);
	if (!Rule || Rule->Kind != Instance->Kind || !IsSlotValid(Rule->Kind, Rule->Slot)) return EHSREquipmentOperationResult::InvalidSlot;
	if (InstanceOwners.Contains(InstanceId)) return EHSREquipmentOperationResult::InstanceAlreadyEquipped;
	const FLoadoutState* Existing = Loadouts.Find(CharacterId);
	if (!Existing || !IsSlotOccupied(*Existing, Rule->Kind, Rule->Slot)) return EHSREquipmentOperationResult::TargetNotFound;
	FLoadoutState Candidate = *Existing;
	if (Rule->Kind == EHSREquipmentKind::Equipment) Candidate.Equipment.Add(static_cast<EHSREquipmentSlot>(Rule->Slot), InstanceId);
	else Candidate.Relics.Add(static_cast<EHSRRelicSlot>(Rule->Slot), InstanceId);
	CommitLoadout(CharacterId, MoveTemp(Candidate));
	return EHSREquipmentOperationResult::Success;
}

FHSREquipmentMovementResult UHSREquipmentSubsystem::ExecuteMovement(const FHSREquipmentMovementRequest& Request,
	UHSRInventorySubsystem& Inventory, const UHSRItemEquipmentMappingCatalog& MappingCatalog)
{
	FHSREquipmentMovementResult Result;
	Result.OperationId = Request.OperationId;
	if (const FMovementLedgerEntry* ExistingOperation = MovementLedger.Find(Request.OperationId))
	{
		if (!IsSameMovementRequest(ExistingOperation->Request, Request))
		{
			Result.Code = EHSREquipmentMovementResultCode::OperationIdConflict;
			return Result;
		}
		Result = ExistingOperation->Result;
		Result.bCommitted = false;
		Result.bReplay = true;
		return Result;
	}
	FHSRInventorySnapshot InventorySnapshot;
	Inventory.GetSnapshot(InventorySnapshot);
	Result.OldInventoryRevision = InventorySnapshot.Revision;
	Result.NewInventoryRevision = InventorySnapshot.Revision;
	const FLoadoutState* ExistingLoadout = Loadouts.Find(Request.CharacterId);
	Result.OldEquipmentRevision = ExistingLoadout ? ExistingLoadout->Revision : 0;
	Result.NewEquipmentRevision = Result.OldEquipmentRevision;

	if (!Request.OperationId.IsValid() || !Request.CharacterId.IsValid() || !Request.InstanceId.IsValid())
	{
		return Result;
	}
	if (Request.ExpectedInventoryRevision != InventorySnapshot.Revision)
	{
		Result.Code = EHSREquipmentMovementResultCode::InventoryRevisionConflict;
		return Result;
	}
	if (Request.ExpectedEquipmentRevision != Result.OldEquipmentRevision)
	{
		Result.Code = EHSREquipmentMovementResultCode::EquipmentRevisionConflict;
		return Result;
	}

	FHSREquipmentInstance RegistryInstance;
	if (!FindRegisteredInstance(Request.InstanceId, RegistryInstance))
	{
		Result.Code = EHSREquipmentMovementResultCode::EquipmentRejected;
		return Result;
	}
	FHSRItemEquipmentMappingEntry Mapping;
	FGuid DisplacedInstanceId;
	FHSREquipmentInstance DisplacedRegistryInstance;
	FHSRItemEquipmentMappingEntry DisplacedMapping;
	const FHSRItemInstance* InventoryMembership = InventorySnapshot.UniqueItems.FindByPredicate([&Request](const FHSRItemInstance& Item)
	{
		return Item.InstanceId == Request.InstanceId;
	});
	const bool bResolvedMapping = Request.Intent != EHSREquipmentMovementIntent::Unequip
		? InventoryMembership && MappingCatalog.Resolve(InventoryMembership->DefinitionId, Mapping)
		: MappingCatalog.ResolveEquipmentDefinition(RegistryInstance.DefinitionId, Mapping);
	const bool bMappingValid = bResolvedMapping && MappingCatalog.Validate(
		Mapping.ItemId, EHSRItemStorageKind::Unique,
		[this, &Request, &RegistryInstance](const FName DefinitionId, const EHSREquipmentKind Kind, const int32 Slot)
		{
			const FDefinitionRule* Rule = Definitions.Find(DefinitionId);
			return Rule && DefinitionId == RegistryInstance.DefinitionId && Kind == RegistryInstance.Kind
				&& Kind == Request.Kind && Slot == Request.Slot && Rule->Kind == Kind && Rule->Slot == Slot;
		}, Mapping);
	if (!bMappingValid)
	{
		Result.Code = EHSREquipmentMovementResultCode::MappingRejected;
		return Result;
	}
	if (Request.Intent == EHSREquipmentMovementIntent::Replace)
	{
		if (!ExistingLoadout)
		{
			Result.Code = EHSREquipmentMovementResultCode::EquipmentRejected;
			return Result;
		}
		const FGuid* Current = FindPlacedInstance(*ExistingLoadout, Request.Kind, Request.Slot);
		if (!Current || !InstanceOwners.Contains(*Current) || InstanceOwners.FindRef(*Current) != Request.CharacterId
			|| !FindRegisteredInstance(*Current, DisplacedRegistryInstance))
		{
			Result.Code = EHSREquipmentMovementResultCode::EquipmentRejected;
			return Result;
		}
		if (!MappingCatalog.ResolveEquipmentDefinition(DisplacedRegistryInstance.DefinitionId, DisplacedMapping)
			|| !MappingCatalog.Validate(DisplacedMapping.ItemId, EHSRItemStorageKind::Unique,
				[this, &Request, &DisplacedRegistryInstance](const FName DefinitionId, const EHSREquipmentKind Kind, const int32 Slot)
				{
					const FDefinitionRule* Rule = Definitions.Find(DefinitionId);
					return Rule && DefinitionId == DisplacedRegistryInstance.DefinitionId
						&& Kind == DisplacedRegistryInstance.Kind && Kind == Request.Kind && Slot == Request.Slot
						&& Rule->Kind == Kind && Rule->Slot == Slot;
				}, DisplacedMapping))
		{
			Result.Code = EHSREquipmentMovementResultCode::MappingRejected;
			return Result;
		}
		DisplacedInstanceId = *Current;
	}
	else if (Request.Intent == EHSREquipmentMovementIntent::Unequip)
	{
		const FGuid* Owner = InstanceOwners.Find(Request.InstanceId);
		const FGuid* Placed = ExistingLoadout ? FindPlacedInstance(*ExistingLoadout, Request.Kind, Request.Slot) : nullptr;
		if (!Owner || *Owner != Request.CharacterId || !Placed || *Placed != Request.InstanceId)
		{
			Result.Code = EHSREquipmentMovementResultCode::EquipmentRejected;
			return Result;
		}
	}

	FHSRInventoryMovementCandidate InventoryCandidate;
	EHSRInventoryOperationResult InventoryResult = EHSRInventoryOperationResult::InvalidDefinition;
	if (Request.Intent == EHSREquipmentMovementIntent::Equip)
	{
		InventoryResult = Inventory.PrepareEquipmentRemovalCandidate(Request.InstanceId, Mapping.ItemId, Request.ExpectedInventoryRevision, InventoryCandidate);
	}
	else if (Request.Intent == EHSREquipmentMovementIntent::Unequip)
	{
		InventoryResult = Inventory.PrepareEquipmentAdditionCandidate(Request.InstanceId, Mapping.ItemId, Request.ExpectedInventoryRevision, InventoryCandidate);
	}
	else
	{
		InventoryResult = Inventory.PrepareEquipmentSwapCandidate(Request.InstanceId, Mapping.ItemId,
			DisplacedInstanceId, DisplacedMapping.ItemId, Request.ExpectedInventoryRevision, InventoryCandidate);
	}
	if (InventoryResult != EHSRInventoryOperationResult::Success)
	{
		Result.Code = EHSREquipmentMovementResultCode::InventoryRejected;
		return Result;
	}
	FLoadoutState EquipmentCandidate = ExistingLoadout ? *ExistingLoadout : FLoadoutState();
	if (Request.Intent == EHSREquipmentMovementIntent::Equip)
	{
		if (InstanceOwners.Contains(Request.InstanceId) || IsSlotOccupied(EquipmentCandidate, Request.Kind, Request.Slot))
		{
			Result.Code = EHSREquipmentMovementResultCode::EquipmentRejected;
			return Result;
		}
		if (Request.Kind == EHSREquipmentKind::Equipment) EquipmentCandidate.Equipment.Add(static_cast<EHSREquipmentSlot>(Request.Slot), Request.InstanceId);
		else EquipmentCandidate.Relics.Add(static_cast<EHSRRelicSlot>(Request.Slot), Request.InstanceId);
	}
	else if (Request.Intent == EHSREquipmentMovementIntent::Unequip)
	{
		const FGuid* Owner = InstanceOwners.Find(Request.InstanceId);
		const FGuid* Placed = FindPlacedInstance(EquipmentCandidate, Request.Kind, Request.Slot);
		if (!Owner || *Owner != Request.CharacterId || !Placed || *Placed != Request.InstanceId)
		{
			Result.Code = EHSREquipmentMovementResultCode::EquipmentRejected;
			return Result;
		}
		if (Request.Kind == EHSREquipmentKind::Equipment) EquipmentCandidate.Equipment.Remove(static_cast<EHSREquipmentSlot>(Request.Slot));
		else EquipmentCandidate.Relics.Remove(static_cast<EHSRRelicSlot>(Request.Slot));
	}
	else
	{
		if (InstanceOwners.Contains(Request.InstanceId))
		{
			Result.Code = EHSREquipmentMovementResultCode::EquipmentRejected;
			return Result;
		}
		if (Request.Kind == EHSREquipmentKind::Equipment) EquipmentCandidate.Equipment.Add(static_cast<EHSREquipmentSlot>(Request.Slot), Request.InstanceId);
		else EquipmentCandidate.Relics.Add(static_cast<EHSRRelicSlot>(Request.Slot), Request.InstanceId);
	}
	EquipmentCandidate.Revision = Result.OldEquipmentRevision + 1;
	FHSREquipmentLoadout ProjectionLoadout;
	if (!ResolveLoadout(EquipmentCandidate, ProjectionLoadout))
	{
		Result.Code = EHSREquipmentMovementResultCode::EquipmentRejected;
		return Result;
	}
	const bool bHasProjectionPreflight = MovementProjectionPreflight.IsBound();
	const bool bHasProjectionCommit = MovementProjectionCommit.IsBound();
	if (bHasProjectionPreflight != bHasProjectionCommit
		|| (bHasProjectionPreflight && !MovementProjectionPreflight.Execute(Request, ProjectionLoadout)))
	{
		Result.Code = EHSREquipmentMovementResultCode::ProjectionRejected;
		return Result;
	}

	const int64 NewInventoryRevision = InventoryCandidate.NextRevision;
	Inventory.InstallEquipmentMovementCandidateNoFail(MoveTemp(InventoryCandidate));
	FLoadoutState& InstalledLoadout = Loadouts.FindOrAdd(Request.CharacterId);
	InstalledLoadout = MoveTemp(EquipmentCandidate);
	if (Request.Intent == EHSREquipmentMovementIntent::Equip) InstanceOwners.Add(Request.InstanceId, Request.CharacterId);
	else if (Request.Intent == EHSREquipmentMovementIntent::Unequip) InstanceOwners.Remove(Request.InstanceId);
	else { InstanceOwners.Remove(DisplacedInstanceId); InstanceOwners.Add(Request.InstanceId, Request.CharacterId); Result.DisplacedInstanceId = DisplacedInstanceId; }
	Inventory.FinalizeEquipmentMovementRevisionNoFail(NewInventoryRevision);
	Result.NewInventoryRevision = NewInventoryRevision;
	Result.NewEquipmentRevision = InstalledLoadout.Revision;
	Inventory.PublishEquipmentMovementCommit(NewInventoryRevision);
	LoadoutChanged.Broadcast(Request.CharacterId, InstalledLoadout.Revision);
	if (bHasProjectionCommit)
	{
		MovementProjectionCommit.Execute(Request, ProjectionLoadout);
	}
	Result.Code = EHSREquipmentMovementResultCode::Success;
	Result.bCommitted = true;
	MovementLedger.Add(Request.OperationId, {Request, Result});
	MovementLedgerOrder.Add(Request.OperationId);
	constexpr int32 MaxMovementLedgerEntries = 128;
	if (MovementLedgerOrder.Num() > MaxMovementLedgerEntries)
	{
		MovementLedger.Remove(MovementLedgerOrder[0]);
		MovementLedgerOrder.RemoveAt(0);
	}
	return Result;
}

EHSREquipmentOperationResult UHSREquipmentSubsystem::Unequip(const FGuid& CharacterId, EHSREquipmentKind Kind, int32 Slot, const FGuid& ExpectedInstanceId)
{
	if (!CharacterId.IsValid()) return EHSREquipmentOperationResult::InvalidCharacterId;
	if (!ExpectedInstanceId.IsValid()) return EHSREquipmentOperationResult::InvalidInstanceId;
	if (!IsSlotValid(Kind, Slot)) return EHSREquipmentOperationResult::InvalidSlot;
	const FLoadoutState* Existing = Loadouts.Find(CharacterId);
	if (Existing == nullptr) return EHSREquipmentOperationResult::TargetNotFound;
	const FGuid* Current = FindPlacedInstance(*Existing, Kind, Slot);
	if (Current == nullptr) return EHSREquipmentOperationResult::TargetNotFound;
	if (*Current != ExpectedInstanceId) return EHSREquipmentOperationResult::InstanceMismatch;

	FLoadoutState Candidate = *Existing;
	if (Kind == EHSREquipmentKind::Equipment) Candidate.Equipment.Remove(static_cast<EHSREquipmentSlot>(Slot));
	else Candidate.Relics.Remove(static_cast<EHSRRelicSlot>(Slot));
	CommitLoadout(CharacterId, MoveTemp(Candidate));
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
	FHSREquipmentInstance* Instance = InstanceRegistry.Find(InstanceId);
	if (Instance == nullptr) return EHSREquipmentOperationResult::TargetNotFound;
	const FDefinitionRule* Rule = FindDefinition(*Instance);
	if (Rule == nullptr || NewLevel < 0 || NewLevel > Rule->EnhancementCap) return EHSREquipmentOperationResult::InvalidEnhancementLevel;
	if (Instance->EnhancementLevel == NewLevel) return EHSREquipmentOperationResult::NoOp;
	Instance->EnhancementLevel = NewLevel;
	FLoadoutState Candidate = *Existing;
	CommitLoadout(CharacterId, MoveTemp(Candidate));
	return EHSREquipmentOperationResult::Success;
}

bool UHSREquipmentSubsystem::GetLoadout(const FGuid& CharacterId, FHSREquipmentLoadout& OutLoadout, int32& OutRevision) const
{
	const FLoadoutState* State = Loadouts.Find(CharacterId);
	if (State == nullptr) return false;
	if (!ResolveLoadout(*State, OutLoadout)) return false;
	OutRevision = State->Revision;
	return true;
}

void UHSREquipmentSubsystem::GetRelicSetSnapshots(const FGuid& CharacterId, TArray<FHSRRelicSetSnapshot>& Out) const
{
	Out.Reset();
	const FLoadoutState* State=Loadouts.Find(CharacterId);
	if(!State)return;
	TMap<FName,int32> Counts;
	for(const auto& Pair:State->Relics)if(const FHSREquipmentInstance* Instance=InstanceRegistry.Find(Pair.Value))if(const FDefinitionRule* Rule=Definitions.Find(Instance->DefinitionId))if(!Rule->SetId.IsNone())++Counts.FindOrAdd(Rule->SetId);
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

bool UHSREquipmentSubsystem::IsSlotOccupied(const FLoadoutState& Loadout, EHSREquipmentKind Kind, int32 Slot) const
{
	return FindPlacedInstance(Loadout, Kind, Slot) != nullptr;
}

const FGuid* UHSREquipmentSubsystem::FindPlacedInstance(const FLoadoutState& Loadout, EHSREquipmentKind Kind, int32 Slot) const
{
	return Kind == EHSREquipmentKind::Equipment ? Loadout.Equipment.Find(static_cast<EHSREquipmentSlot>(Slot)) : Loadout.Relics.Find(static_cast<EHSRRelicSlot>(Slot));
}

bool UHSREquipmentSubsystem::ResolveLoadout(const FLoadoutState& State, FHSREquipmentLoadout& OutLoadout) const
{
	OutLoadout = FHSREquipmentLoadout();
	for (const auto& Pair : State.Equipment) { const FHSREquipmentInstance* Instance = InstanceRegistry.Find(Pair.Value); if (!Instance) return false; OutLoadout.Equipment.Add(Pair.Key, *Instance); }
	for (const auto& Pair : State.Relics) { const FHSREquipmentInstance* Instance = InstanceRegistry.Find(Pair.Value); if (!Instance) return false; OutLoadout.Relics.Add(Pair.Key, *Instance); }
	return true;
}

void UHSREquipmentSubsystem::CommitLoadout(const FGuid& CharacterId, FLoadoutState Candidate)
{
	FLoadoutState& State = Loadouts.FindOrAdd(CharacterId);
	for (const auto& Pair : State.Equipment) InstanceOwners.Remove(Pair.Value);
	for (const auto& Pair : State.Relics) InstanceOwners.Remove(Pair.Value);
	Candidate.Revision = State.Revision + 1;
	State = MoveTemp(Candidate);
	for (const auto& Pair : State.Equipment) InstanceOwners.Add(Pair.Value, CharacterId);
	for (const auto& Pair : State.Relics) InstanceOwners.Add(Pair.Value, CharacterId);
	LoadoutChanged.Broadcast(CharacterId, State.Revision);
}

bool UHSREquipmentSubsystem::IsSamePayload(const FHSREquipmentInstance& A, const FHSREquipmentInstance& B)
{
	if (A.InstanceId != B.InstanceId || A.DefinitionId != B.DefinitionId || A.Kind != B.Kind || A.EnhancementLevel != B.EnhancementLevel || A.Modifiers.Num() != B.Modifiers.Num()) return false;
	for (int32 Index = 0; Index < A.Modifiers.Num(); ++Index)
	{
		if (A.Modifiers[Index].Stat != B.Modifiers[Index].Stat || A.Modifiers[Index].Value != B.Modifiers[Index].Value) return false;
	}
	return true;
}

bool UHSREquipmentSubsystem::IsSameMovementRequest(const FHSREquipmentMovementRequest& A,
	const FHSREquipmentMovementRequest& B)
{
	return A.OperationId == B.OperationId && A.CharacterId == B.CharacterId && A.InstanceId == B.InstanceId
		&& A.Intent == B.Intent && A.Kind == B.Kind && A.Slot == B.Slot
		&& A.ExpectedInventoryRevision == B.ExpectedInventoryRevision
		&& A.ExpectedEquipmentRevision == B.ExpectedEquipmentRevision;
}
