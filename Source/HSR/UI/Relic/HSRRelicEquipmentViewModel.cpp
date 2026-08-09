#include "HSRRelicEquipmentViewModel.h"

#include "../../Data/Definitions/HSREquipmentEnhancementCatalog.h"
#include "../../Data/Definitions/HSRItemEquipmentMappingCatalog.h"
#include "../../Equipment/HSREquipmentSubsystem.h"
#include "../../Inventory/HSRInventorySubsystem.h"

void UHSRRelicEquipmentViewModel::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

void UHSRRelicEquipmentViewModel::Initialize(UHSREquipmentSubsystem* InEquipment,
	UHSRInventorySubsystem* InInventory, UHSRItemEquipmentMappingCatalog* InMappingCatalog,
	UHSREquipmentEnhancementCatalog* InEnhancementCatalog, const FGuid& InCharacterId)
{
	Shutdown();
	Equipment = InEquipment;
	Inventory = InInventory;
	MappingCatalog = InMappingCatalog;
	EnhancementCatalog = InEnhancementCatalog;
	CharacterId = InCharacterId;
	Stage = EHSRRelicEquipmentStage::SlotSelection;
	SelectedSlot = EHSRRelicSlot::Head;
	SelectedCandidateId.Invalidate();
	if (Equipment.IsValid())
	{
		EquipmentHandle = Equipment->OnLoadoutChanged().AddUObject(this, &ThisClass::HandleEquipmentChanged);
	}
	if (Inventory.IsValid())
	{
		InventoryHandle = Inventory->OnInventoryChanged().AddUObject(this, &ThisClass::HandleInventoryChanged);
	}
	Rebuild();
}

void UHSRRelicEquipmentViewModel::Shutdown()
{
	if (Equipment.IsValid() && EquipmentHandle.IsValid()) Equipment->OnLoadoutChanged().Remove(EquipmentHandle);
	if (Inventory.IsValid() && InventoryHandle.IsValid()) Inventory->OnInventoryChanged().Remove(InventoryHandle);
	EquipmentHandle.Reset();
	InventoryHandle.Reset();
	Equipment.Reset();
	Inventory.Reset();
	MappingCatalog.Reset();
	EnhancementCatalog.Reset();
	CharacterId.Invalidate();
	Stage = EHSRRelicEquipmentStage::SlotSelection;
	SelectedSlot = EHSRRelicSlot::Head;
	SelectedCandidateId.Invalidate();
	Snapshot = FHSRRelicEquipmentSnapshot();
	bHasSnapshot = false;
}

EHSRRelicEquipmentResult UHSRRelicEquipmentViewModel::SelectSlot(const EHSRRelicSlot InSlot)
{
	if (!IsInitialized())
	{
		PublishFailure(EHSRRelicEquipmentResult::NotInitialized);
		return EHSRRelicEquipmentResult::NotInitialized;
	}
	if (!IsValidRelicSlot(InSlot))
	{
		PublishFailure(EHSRRelicEquipmentResult::InvalidSlot);
		return EHSRRelicEquipmentResult::InvalidSlot;
	}
	SelectedSlot = InSlot;
	SelectedCandidateId.Invalidate();
	Stage = EHSRRelicEquipmentStage::CandidateSelection;
	Rebuild();
	return Snapshot.bIsValid ? EHSRRelicEquipmentResult::Success : Snapshot.FailureReason;
}

EHSRRelicEquipmentResult UHSRRelicEquipmentViewModel::SelectCandidate(const FGuid& InInstanceId)
{
	if (!IsInitialized())
	{
		PublishFailure(EHSRRelicEquipmentResult::NotInitialized);
		return EHSRRelicEquipmentResult::NotInitialized;
	}
	const bool bFound = Snapshot.Candidates.ContainsByPredicate([&InInstanceId](const FHSRRelicCandidateRow& Row)
	{
		return Row.InstanceId == InInstanceId;
	});
	if (!bFound)
	{
		PublishFailure(EHSRRelicEquipmentResult::CandidateUnavailable);
		return EHSRRelicEquipmentResult::CandidateUnavailable;
	}
	SelectedCandidateId = InInstanceId;
	Stage = EHSRRelicEquipmentStage::Comparison;
	Rebuild();
	return Snapshot.bIsValid ? EHSRRelicEquipmentResult::Success : Snapshot.FailureReason;
}

EHSRRelicEquipmentResult UHSRRelicEquipmentViewModel::OpenEnhancement()
{
	if (!IsInitialized())
	{
		PublishFailure(EHSRRelicEquipmentResult::NotInitialized);
		return EHSRRelicEquipmentResult::NotInitialized;
	}
	Stage = EHSRRelicEquipmentStage::Enhancement;
	Rebuild();
	if (Snapshot.EnhancementOptions.IsEmpty())
	{
		PublishFailure(EHSRRelicEquipmentResult::NoEnhancementOption);
		return EHSRRelicEquipmentResult::NoEnhancementOption;
	}
	return Snapshot.bIsValid ? EHSRRelicEquipmentResult::Success : Snapshot.FailureReason;
}

EHSRRelicEquipmentResult UHSRRelicEquipmentViewModel::CommitSelectedMovement()
{
	if (!IsInitialized())
	{
		PublishFailure(EHSRRelicEquipmentResult::NotInitialized);
		return EHSRRelicEquipmentResult::NotInitialized;
	}
	if (Stage != EHSRRelicEquipmentStage::Comparison || !SelectedCandidateId.IsValid())
	{
		PublishFailure(EHSRRelicEquipmentResult::ComparisonUnavailable);
		return EHSRRelicEquipmentResult::ComparisonUnavailable;
	}
	if (!MappingCatalog.IsValid())
	{
		PublishFailure(EHSRRelicEquipmentResult::CatalogUnavailable);
		return EHSRRelicEquipmentResult::CatalogUnavailable;
	}
	const FHSRRelicCandidateRow* Candidate = Snapshot.Candidates.FindByPredicate(
		[this](const FHSRRelicCandidateRow& Row) { return Row.InstanceId == SelectedCandidateId; });
	if (Candidate == nullptr)
	{
		PublishFailure(EHSRRelicEquipmentResult::CandidateUnavailable);
		return EHSRRelicEquipmentResult::CandidateUnavailable;
	}
	FHSREquipmentMovementRequest Request;
	Request.OperationId = FGuid::NewGuid();
	Request.CharacterId = CharacterId;
	Request.InstanceId = Candidate->InstanceId;
	Request.Intent = Snapshot.CurrentInstanceId.IsValid()
		? EHSREquipmentMovementIntent::Replace : EHSREquipmentMovementIntent::Equip;
	Request.Kind = EHSREquipmentKind::Relic;
	Request.Slot = static_cast<int32>(SelectedSlot);
	Request.ExpectedInventoryRevision = Snapshot.InventoryRevision;
	Request.ExpectedEquipmentRevision = Snapshot.EquipmentRevision;
	const FHSREquipmentMovementResult Result = Equipment->ExecuteMovement(Request, *Inventory, *MappingCatalog);
	const EHSRRelicEquipmentResult MappedResult = MapMovementResult(Result.Code);
	if (MappedResult != EHSRRelicEquipmentResult::Success)
	{
		PublishFailure(MappedResult);
		return MappedResult;
	}
	Stage = EHSRRelicEquipmentStage::CandidateSelection;
	SelectedCandidateId.Invalidate();
	Rebuild();
	return EHSRRelicEquipmentResult::Success;
}

EHSRRelicEquipmentResult UHSRRelicEquipmentViewModel::CommitEnhancement(const int32 TargetLevel)
{
	if (!IsInitialized())
	{
		PublishFailure(EHSRRelicEquipmentResult::NotInitialized);
		return EHSRRelicEquipmentResult::NotInitialized;
	}
	const FHSRRelicEnhancementOption* Option = Snapshot.EnhancementOptions.FindByPredicate(
		[TargetLevel](const FHSRRelicEnhancementOption& Row) { return Row.TargetLevel == TargetLevel; });
	if (Option == nullptr)
	{
		PublishFailure(EHSRRelicEquipmentResult::InvalidTargetLevel);
		return EHSRRelicEquipmentResult::InvalidTargetLevel;
	}
	if (!Option->bAffordable || !Option->bAvailable || !Snapshot.CurrentInstanceId.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("HSR.Relic CommitEnhancement PreflightRejected Target=%d Affordable=%d Available=%d CurInst=%d ")
			TEXT("Material=%s Cost=%d Held=%d CurLevel=%d"),
			TargetLevel, Option->bAffordable ? 1 : 0, Option->bAvailable ? 1 : 0,
			Snapshot.CurrentInstanceId.IsValid() ? 1 : 0, *Option->MaterialItemId.ToString(),
			Option->MaterialCost, GetHeldMaterialQuantity(Option->MaterialItemId),
			Snapshot.CurrentEnhancementLevel);
		// A shortfall is the one preflight failure the player can actually act on, so say so
		// instead of collapsing it into a generic rejection.
		const EHSRRelicEquipmentResult Reason = !Option->bAffordable
			? EHSRRelicEquipmentResult::InsufficientMaterial
			: EHSRRelicEquipmentResult::AuthorityRejected;
		PublishFailure(Reason);
		return Reason;
	}
	if (!EnhancementCatalog.IsValid())
	{
		PublishFailure(EHSRRelicEquipmentResult::CatalogUnavailable);
		return EHSRRelicEquipmentResult::CatalogUnavailable;
	}
	FHSREquipmentEnhancementRequest Request;
	Request.OperationId = FGuid::NewGuid();
	Request.CharacterId = CharacterId;
	Request.InstanceId = Snapshot.CurrentInstanceId;
	Request.Kind = EHSREquipmentKind::Relic;
	Request.ExpectedInventoryRevision = Snapshot.InventoryRevision;
	Request.ExpectedEquipmentRevision = Snapshot.EquipmentRevision;
	Request.ExpectedEnhancementLevel = Snapshot.CurrentEnhancementLevel;
	Request.TargetLevel = TargetLevel;
	const FHSREquipmentEnhancementResult Result = Equipment->ExecuteEnhancement(
		Request, *Inventory, *EnhancementCatalog);
	const EHSRRelicEquipmentResult MappedResult = MapEnhancementResult(Result.Code);
	if (MappedResult != EHSRRelicEquipmentResult::Success)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("HSR.Relic CommitEnhancement SubsystemRejected Target=%d SubsystemCode=%d Mapped=%d ")
			TEXT("InvRev=%lld EquipRev=%d ExpectedLevel=%d"),
			TargetLevel, static_cast<int32>(Result.Code), static_cast<int32>(MappedResult),
			Request.ExpectedInventoryRevision, Request.ExpectedEquipmentRevision,
			Request.ExpectedEnhancementLevel);
		PublishFailure(MappedResult);
		return MappedResult;
	}
	Stage = EHSRRelicEquipmentStage::Enhancement;
	Rebuild();
	return EHSRRelicEquipmentResult::Success;
}

EHSRRelicEquipmentResult UHSRRelicEquipmentViewModel::Back()
{
	if (!bHasSnapshot) return EHSRRelicEquipmentResult::NotInitialized;
	switch (Stage)
	{
	case EHSRRelicEquipmentStage::Enhancement:
		Stage = EHSRRelicEquipmentStage::Comparison;
		Rebuild();
		return EHSRRelicEquipmentResult::Success;
	case EHSRRelicEquipmentStage::Comparison:
		Stage = EHSRRelicEquipmentStage::CandidateSelection;
		Rebuild();
		return EHSRRelicEquipmentResult::Success;
	case EHSRRelicEquipmentStage::CandidateSelection:
		SelectedCandidateId.Invalidate();
		Stage = EHSRRelicEquipmentStage::SlotSelection;
		Rebuild();
		return EHSRRelicEquipmentResult::Success;
	case EHSRRelicEquipmentStage::SlotSelection:
		return EHSRRelicEquipmentResult::AtRoot;
	}
	return EHSRRelicEquipmentResult::AtRoot;
}

void UHSRRelicEquipmentViewModel::Rebuild()
{
	Snapshot = FHSRRelicEquipmentSnapshot();
	Snapshot.CharacterId = CharacterId;
	Snapshot.Stage = Stage;
	Snapshot.SelectedSlot = SelectedSlot;
	Snapshot.SelectedCandidateId = SelectedCandidateId;
	if (!IsInitialized())
	{
		PublishFailure(EHSRRelicEquipmentResult::NotInitialized);
		return;
	}
	FHSREquipmentLoadout Loadout;
	BuildSlotRows(Loadout);
	FHSRInventorySnapshot InventorySnapshot;
	Inventory->GetSnapshot(InventorySnapshot);
	Snapshot.InventoryRevision = InventorySnapshot.Revision;
	if (!MappingCatalog.IsValid())
	{
		PublishFailure(EHSRRelicEquipmentResult::CatalogUnavailable);
		return;
	}
	BuildCandidateRows(Loadout);
	if (Stage == EHSRRelicEquipmentStage::Comparison
		|| (Stage == EHSRRelicEquipmentStage::Enhancement && SelectedCandidateId.IsValid()))
	{
		if (!BuildComparison())
		{
			Stage = EHSRRelicEquipmentStage::CandidateSelection;
			Snapshot.Stage = Stage;
			Snapshot.SelectedCandidateId = SelectedCandidateId;
			PublishFailure(EHSRRelicEquipmentResult::ComparisonUnavailable);
			return;
		}
	}
	if (Stage == EHSRRelicEquipmentStage::Enhancement && !BuildEnhancementOptions())
	{
		// Match the comparison failure above: never publish Enhancement stage with no options,
		// or a view can render an option list that does not exist.
		Stage = EHSRRelicEquipmentStage::CandidateSelection;
		Snapshot.Stage = Stage;
		Snapshot.EnhancementOptions.Reset();
		PublishFailure(EHSRRelicEquipmentResult::NoEnhancementOption);
		return;
	}
	Snapshot.bIsValid = true;
	Snapshot.FailureReason = EHSRRelicEquipmentResult::Success;
	bHasSnapshot = true;
	Broadcast();
}

void UHSRRelicEquipmentViewModel::Broadcast()
{
	Changed.Broadcast(Snapshot);
	OnSnapshotChanged.Broadcast(Snapshot);
}

void UHSRRelicEquipmentViewModel::PublishFailure(const EHSRRelicEquipmentResult Result)
{
	Snapshot.CharacterId = CharacterId;
	Snapshot.Stage = Stage;
	Snapshot.SelectedSlot = SelectedSlot;
	Snapshot.SelectedCandidateId = SelectedCandidateId;
	Snapshot.bIsValid = false;
	Snapshot.FailureReason = Result;
	bHasSnapshot = true;
	Broadcast();
}

bool UHSRRelicEquipmentViewModel::IsInitialized() const
{
	return Equipment.IsValid() && Inventory.IsValid() && CharacterId.IsValid();
}

bool UHSRRelicEquipmentViewModel::IsValidRelicSlot(const EHSRRelicSlot InSlot) const
{
	return static_cast<uint8>(InSlot) <= static_cast<uint8>(EHSRRelicSlot::LinkRope);
}

bool UHSRRelicEquipmentViewModel::BuildSlotRows(FHSREquipmentLoadout& OutLoadout)
{
	int32 Revision = 0;
	const bool bHasLoadout = Equipment->GetLoadout(CharacterId, OutLoadout, Revision);
	Snapshot.EquipmentRevision = bHasLoadout ? Revision : 0;
	Snapshot.Slots.Reset();
	Snapshot.CurrentInstanceId.Invalidate();
	Snapshot.CurrentEnhancementLevel = 0;
	for (uint8 SlotIndex = 0; SlotIndex <= static_cast<uint8>(EHSRRelicSlot::LinkRope); ++SlotIndex)
	{
		const EHSRRelicSlot Slot = static_cast<EHSRRelicSlot>(SlotIndex);
		FHSRRelicSlotRow Row;
		Row.Slot = Slot;
		Row.bIsSelected = Slot == SelectedSlot;
		if (bHasLoadout)
		{
			if (const FHSREquipmentInstance* Instance = OutLoadout.Relics.Find(Slot))
			{
				Row.bHasEquipped = true;
				Row.EquippedInstanceId = Instance->InstanceId;
				Row.EquippedInstance = *Instance;
				if (Slot == SelectedSlot)
				{
					Snapshot.CurrentInstanceId = Instance->InstanceId;
					Snapshot.CurrentEnhancementLevel = Instance->EnhancementLevel;
				}
			}
		}
		Snapshot.Slots.Add(MoveTemp(Row));
	}
	return true;
}

void UHSRRelicEquipmentViewModel::BuildCandidateRows(const FHSREquipmentLoadout&)
{
	Snapshot.Candidates.Reset();
	if (!MappingCatalog.IsValid()) return;
	FHSRInventorySnapshot InventorySnapshot;
	Inventory->GetSnapshot(InventorySnapshot);
	for (const FHSRItemInstance& Item : InventorySnapshot.UniqueItems)
	{
		FHSRItemEquipmentMappingEntry Mapping;
		if (!MappingCatalog->Resolve(Item.DefinitionId, Mapping)
			|| Mapping.Kind != EHSREquipmentKind::Relic
			|| Mapping.Slot != static_cast<int32>(SelectedSlot))
		{
			continue;
		}
		FHSREquipmentInstance Instance;
		if (!Equipment->FindRegisteredInstance(Item.InstanceId, Instance)
			|| Instance.Kind != EHSREquipmentKind::Relic
			|| Instance.DefinitionId != Mapping.EquipmentDefinitionId)
		{
			continue;
		}
		FGuid Owner;
		if (Equipment->FindInstanceOwner(Item.InstanceId, Owner)) continue;
		FHSRRelicCandidateRow Row;
		Row.InstanceId = Item.InstanceId;
		Row.ItemId = Item.DefinitionId;
		Row.DefinitionId = Instance.DefinitionId;
		Row.Slot = SelectedSlot;
		Row.Instance = Instance;
		Row.bIsSelected = Item.InstanceId == SelectedCandidateId;
		Snapshot.Candidates.Add(MoveTemp(Row));
	}
	Snapshot.Candidates.Sort([](const FHSRRelicCandidateRow& A, const FHSRRelicCandidateRow& B)
	{
		return A.InstanceId < B.InstanceId;
	});
}

bool UHSRRelicEquipmentViewModel::BuildComparison()
{
	const FHSRRelicCandidateRow* Candidate = Snapshot.Candidates.FindByPredicate(
		[this](const FHSRRelicCandidateRow& Row) { return Row.InstanceId == SelectedCandidateId; });
	if (Candidate == nullptr) return false;
	FHSRRelicComparisonSnapshot Comparison;
	Comparison.CandidateInstanceId = Candidate->InstanceId;
	Comparison.CandidateInstance = Candidate->Instance;
	if (Snapshot.CurrentInstanceId.IsValid())
	{
		const FHSRRelicSlotRow* Current = Snapshot.Slots.FindByPredicate(
			[this](const FHSRRelicSlotRow& Row) { return Row.Slot == SelectedSlot; });
		if (Current != nullptr && Current->bHasEquipped)
		{
			Comparison.CurrentInstanceId = Current->EquippedInstanceId;
			Comparison.CurrentInstance = Current->EquippedInstance;
		}
	}
	for (uint8 StatIndex = 0; StatIndex <= static_cast<uint8>(EHSREquipmentStat::Speed); ++StatIndex)
	{
		FHSRRelicStatDeltaRow Delta;
		Delta.Stat = static_cast<EHSREquipmentStat>(StatIndex);
		Delta.CurrentValue = GetStatValue(Comparison.CurrentInstance, Delta.Stat);
		Delta.CandidateValue = GetStatValue(Comparison.CandidateInstance, Delta.Stat);
		Delta.Delta = Delta.CandidateValue - Delta.CurrentValue;
		Comparison.StatDeltas.Add(MoveTemp(Delta));
	}
	Comparison.bIsValid = true;
	Snapshot.Comparison = MoveTemp(Comparison);
	return true;
}

bool UHSRRelicEquipmentViewModel::BuildEnhancementOptions()
{
	Snapshot.EnhancementOptions.Reset();
	if (!EnhancementCatalog.IsValid() || !Snapshot.CurrentInstanceId.IsValid()) return false;
	const FHSRRelicSlotRow* Current = Snapshot.Slots.FindByPredicate(
		[this](const FHSRRelicSlotRow& Row) { return Row.Slot == SelectedSlot; });
	if (Current == nullptr || !Current->bHasEquipped) return false;
	TArray<FHSREquipmentEnhancementRule> Rules;
	EnhancementCatalog->GetRulesFor(Current->EquippedInstance.DefinitionId,
		Current->EquippedInstance.Kind, Current->EquippedInstance.EnhancementLevel, Rules);
	FHSRInventorySnapshot InventorySnapshot;
	Inventory->GetSnapshot(InventorySnapshot);
	for (const FHSREquipmentEnhancementRule& Rule : Rules)
	{
		FHSRRelicEnhancementOption Option;
		Option.TargetLevel = Rule.TargetLevel;
		Option.MaterialItemId = Rule.MaterialItemId;
		Option.MaterialCost = Rule.MaterialCost;
		Option.TargetModifiers = Rule.TargetModifiers;
		Option.bAffordable = InventorySnapshot.GetStackQuantity(Rule.MaterialItemId) >= Rule.MaterialCost;
		Option.bAvailable = Rule.TargetLevel > Current->EquippedInstance.EnhancementLevel
			&& Rule.MaterialCost > 0;
		Snapshot.EnhancementOptions.Add(MoveTemp(Option));
	}
	return !Snapshot.EnhancementOptions.IsEmpty();
}

int32 UHSRRelicEquipmentViewModel::GetHeldMaterialQuantity(const FName ItemId) const
{
	if (!Inventory.IsValid()) return -1;
	FHSRInventorySnapshot InventorySnapshot;
	Inventory->GetSnapshot(InventorySnapshot);
	return InventorySnapshot.GetStackQuantity(ItemId);
}

void UHSRRelicEquipmentViewModel::HandleEquipmentChanged(const FGuid& ChangedCharacterId, int32)
{
	if (ChangedCharacterId == CharacterId) Rebuild();
}

void UHSRRelicEquipmentViewModel::HandleInventoryChanged(int64)
{
	Rebuild();
}

float UHSRRelicEquipmentViewModel::GetStatValue(const FHSREquipmentInstance& Instance,
	const EHSREquipmentStat Stat)
{
	float Total = 0.0f;
	for (const FHSREquipmentModifier& Modifier : Instance.Modifiers)
	{
		if (Modifier.Stat == Stat) Total += Modifier.Value;
	}
	return Total;
}

EHSRRelicEquipmentResult UHSRRelicEquipmentViewModel::MapMovementResult(
	const EHSREquipmentMovementResultCode Code)
{
	if (Code == EHSREquipmentMovementResultCode::Success) return EHSRRelicEquipmentResult::Success;
	if (Code == EHSREquipmentMovementResultCode::InventoryRevisionConflict
		|| Code == EHSREquipmentMovementResultCode::EquipmentRevisionConflict)
	{
		return EHSRRelicEquipmentResult::StaleSnapshot;
	}
	if (Code == EHSREquipmentMovementResultCode::MappingRejected)
		return EHSRRelicEquipmentResult::CatalogUnavailable;
	if (Code == EHSREquipmentMovementResultCode::InvalidRequest)
		return EHSRRelicEquipmentResult::InvalidRequest;
	// InventoryRejected/EquipmentRejected/ProjectionRejected/OperationIdConflict all mean an
	// authority refused the move; the player cannot act on the distinction.
	return EHSRRelicEquipmentResult::AuthorityRejected;
}

EHSRRelicEquipmentResult UHSRRelicEquipmentViewModel::MapEnhancementResult(
	const EHSREquipmentEnhancementResultCode Code)
{
	if (Code == EHSREquipmentEnhancementResultCode::Success
		|| Code == EHSREquipmentEnhancementResultCode::NoOp)
	{
		return EHSRRelicEquipmentResult::Success;
	}
	if (Code == EHSREquipmentEnhancementResultCode::InventoryRevisionConflict
		|| Code == EHSREquipmentEnhancementResultCode::EquipmentRevisionConflict
		|| Code == EHSREquipmentEnhancementResultCode::EnhancementLevelConflict)
	{
		return EHSRRelicEquipmentResult::StaleSnapshot;
	}
	if (Code == EHSREquipmentEnhancementResultCode::CatalogRejected)
		return EHSRRelicEquipmentResult::CatalogUnavailable;
	// The only inventory gate in ExecuteEnhancement is PrepareEquipmentEnhancementCandidate,
	// which fails when the player cannot pay the material cost -- an actionable message.
	if (Code == EHSREquipmentEnhancementResultCode::InventoryRejected)
		return EHSRRelicEquipmentResult::InsufficientMaterial;
	if (Code == EHSREquipmentEnhancementResultCode::InvalidRequest)
		return EHSRRelicEquipmentResult::InvalidRequest;
	// EquipmentRejected/ProjectionRejected/OperationIdConflict: authority refused, not actionable.
	return EHSRRelicEquipmentResult::AuthorityRejected;
}
