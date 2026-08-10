#include "HSRInventoryViewModel.h"

#include "../../Data/Definitions/HSREquipmentEnhancementCatalog.h"
#include "../../Data/Definitions/HSRInventoryCatalog.h"
#include "../../Data/Definitions/HSRItemEquipmentMappingCatalog.h"
#include "../../Equipment/HSREquipmentSubsystem.h"
#include "../../Inventory/HSRInventorySubsystem.h"

namespace
{
bool HasFilterMatch(const FHSRInventoryEntryRow& Row, const FString& Filter)
{
	if (Filter.IsEmpty()) return true;
	return Row.DisplayName.ToString().Contains(Filter, ESearchCase::IgnoreCase)
		|| Row.ItemId.ToString().Contains(Filter, ESearchCase::IgnoreCase);
}

int32 CompareDisplayNames(const FHSRInventoryEntryRow& A, const FHSRInventoryEntryRow& B)
{
	const FString Left = A.DisplayName.ToString();
	const FString Right = B.DisplayName.ToString();
	return FCString::Stricmp(*Left, *Right);
}

bool IsBeforeStable(const FHSRInventoryEntryRow& A, const FHSRInventoryEntryRow& B,
	EHSRInventorySortMode SortMode)
{
	if (SortMode == EHSRInventorySortMode::QuantityDescending && A.Quantity != B.Quantity)
	{
		return A.Quantity > B.Quantity;
	}
	if (SortMode == EHSRInventorySortMode::DisplayNameAscending)
	{
		const int32 NameComparison = CompareDisplayNames(A, B);
		if (NameComparison != 0) return NameComparison < 0;
	}
	if (A.SortOrder != B.SortOrder) return A.SortOrder < B.SortOrder;
	if (SortMode != EHSRInventorySortMode::DisplayNameAscending)
	{
		const int32 NameComparison = CompareDisplayNames(A, B);
		if (NameComparison != 0) return NameComparison < 0;
	}
	if (A.ItemId != B.ItemId) return A.ItemId.LexicalLess(B.ItemId);
	return A.Key.InstanceId < B.Key.InstanceId;
}

bool IsSlotOccupied(const FHSREquipmentLoadout& Loadout, const EHSREquipmentKind Kind,
	const int32 Slot)
{
	return Kind == EHSREquipmentKind::Equipment
		? Loadout.Equipment.Contains(static_cast<EHSREquipmentSlot>(Slot))
		: Loadout.Relics.Contains(static_cast<EHSRRelicSlot>(Slot));
}
}

void UHSRInventoryViewModel::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

void UHSRInventoryViewModel::Initialize(UHSRInventorySubsystem* InInventory,
	UHSRInventoryCatalog* InCatalog)
{
	Shutdown();
	Inventory = InInventory;
	Catalog = InCatalog;
	Category = EHSRInventoryCategory::Other;
	FilterText.Reset();
	SortMode = EHSRInventorySortMode::CatalogOrder;

	if (Inventory.IsValid())
	{
		InventoryHandle = Inventory->OnInventoryChanged().AddUObject(
			this, &ThisClass::HandleInventoryChanged);
	}
	Rebuild();
}

void UHSRInventoryViewModel::SetCommandContext(UHSREquipmentSubsystem* InEquipment,
	UHSRItemEquipmentMappingCatalog* InMappingCatalog,
	UHSREquipmentEnhancementCatalog* InEnhancementCatalog, const FGuid& InCharacterId)
{
	if (Equipment.IsValid() && EquipmentHandle.IsValid())
	{
		Equipment->OnLoadoutChanged().Remove(EquipmentHandle);
	}
	EquipmentHandle.Reset();
	Equipment = InEquipment;
	MappingCatalog = InMappingCatalog;
	EnhancementCatalog = InEnhancementCatalog;
	CharacterId = InCharacterId;
	if (Equipment.IsValid() && CharacterId.IsValid())
	{
		EquipmentHandle = Equipment->OnLoadoutChanged().AddUObject(
			this, &ThisClass::HandleEquipmentChanged);
	}
	Rebuild();
}

void UHSRInventoryViewModel::Shutdown()
{
	if (Inventory.IsValid() && InventoryHandle.IsValid())
	{
		Inventory->OnInventoryChanged().Remove(InventoryHandle);
	}
	InventoryHandle.Reset();
	if (Equipment.IsValid() && EquipmentHandle.IsValid())
	{
		Equipment->OnLoadoutChanged().Remove(EquipmentHandle);
	}
	EquipmentHandle.Reset();
	Inventory.Reset();
	Catalog.Reset();
	Equipment.Reset();
	MappingCatalog.Reset();
	EnhancementCatalog.Reset();
	CharacterId.Invalidate();
	Category = EHSRInventoryCategory::Other;
	FilterText.Reset();
	SortMode = EHSRInventorySortMode::CatalogOrder;
	Snapshot = FHSRInventoryModuleSnapshot();
	bHasSnapshot = false;
}

bool UHSRInventoryViewModel::GetSnapshot(FHSRInventoryModuleSnapshot& OutSnapshot) const
{
	if (!bHasSnapshot) return false;
	OutSnapshot = Snapshot;
	return true;
}

EHSRInventoryViewModelResult UHSRInventoryViewModel::SelectCategory(
	const EHSRInventoryCategory InCategory)
{
	if (!IsInitialized())
	{
		PublishFailure(EHSRInventoryViewModelResult::NotInitialized);
		return EHSRInventoryViewModelResult::NotInitialized;
	}
	if (!IsValidCategory(InCategory))
	{
		PublishFailure(EHSRInventoryViewModelResult::InvalidCategory);
		return EHSRInventoryViewModelResult::InvalidCategory;
	}
	return ApplyPresentationState(InCategory, FilterText, SortMode, Snapshot.SelectedKey);
}

EHSRInventoryViewModelResult UHSRInventoryViewModel::SetFilterText(const FString& InFilterText)
{
	if (!IsInitialized())
	{
		PublishFailure(EHSRInventoryViewModelResult::NotInitialized);
		return EHSRInventoryViewModelResult::NotInitialized;
	}
	FString Normalized = InFilterText;
	Normalized.TrimStartAndEndInline();
	return ApplyPresentationState(Category, Normalized, SortMode, Snapshot.SelectedKey);
}

EHSRInventoryViewModelResult UHSRInventoryViewModel::SetSortMode(
	const EHSRInventorySortMode InSortMode)
{
	if (!IsInitialized())
	{
		PublishFailure(EHSRInventoryViewModelResult::NotInitialized);
		return EHSRInventoryViewModelResult::NotInitialized;
	}
	if (!IsValidSortMode(InSortMode))
	{
		PublishFailure(EHSRInventoryViewModelResult::InvalidSortMode);
		return EHSRInventoryViewModelResult::InvalidSortMode;
	}
	return ApplyPresentationState(Category, FilterText, InSortMode, Snapshot.SelectedKey);
}

EHSRInventoryViewModelResult UHSRInventoryViewModel::SelectEntry(
	const FHSRInventoryEntryKey& InKey)
{
	if (!IsInitialized())
	{
		PublishFailure(EHSRInventoryViewModelResult::NotInitialized);
		return EHSRInventoryViewModelResult::NotInitialized;
	}
	if (!bHasSnapshot || !Snapshot.Entries.ContainsByPredicate(
		[&InKey](const FHSRInventoryEntryRow& Row) { return Row.Key == InKey; }))
	{
		PublishFailure(EHSRInventoryViewModelResult::EntryUnavailable);
		return EHSRInventoryViewModelResult::EntryUnavailable;
	}
	return ApplyPresentationState(Category, FilterText, SortMode, InKey);
}

EHSRInventoryViewModelResult UHSRInventoryViewModel::SubmitAction(
	const EHSRInventoryAction Action, const int32 TargetLevel)
{
	if (!IsInitialized())
	{
		PublishFailure(EHSRInventoryViewModelResult::NotInitialized);
		return EHSRInventoryViewModelResult::NotInitialized;
	}
	if (!bHasSnapshot || !Snapshot.Detail.bHasSelection)
	{
		PublishFailure(EHSRInventoryViewModelResult::EntryUnavailable);
		return EHSRInventoryViewModelResult::EntryUnavailable;
	}
	switch (Action)
	{
	case EHSRInventoryAction::Use:
	case EHSRInventoryAction::Disassemble:
		UE_LOG(LogTemp, Verbose,
			TEXT("HSR.Inventory action=%d unavailable: no supporting Authority"),
			static_cast<int32>(Action));
		return EHSRInventoryViewModelResult::AuthorityUnavailable;
	case EHSRInventoryAction::Equip:
		return SubmitEquip();
	case EHSRInventoryAction::Enhance:
		return SubmitEnhancement(TargetLevel);
	default:
		PublishFailure(EHSRInventoryViewModelResult::AuthorityUnavailable);
		return EHSRInventoryViewModelResult::AuthorityUnavailable;
	}
}

EHSRInventoryViewModelResult UHSRInventoryViewModel::SubmitEquip()
{
	if (!Equipment.IsValid() || !MappingCatalog.IsValid() || !CharacterId.IsValid())
	{
		PublishFailure(EHSRInventoryViewModelResult::AuthorityUnavailable);
		return EHSRInventoryViewModelResult::AuthorityUnavailable;
	}
	const FHSRInventoryEntryRow& Row = Snapshot.Detail.Entry;
	if (!Row.bIsUnique || !Row.Key.InstanceId.IsValid())
	{
		PublishFailure(EHSRInventoryViewModelResult::EntryUnavailable);
		return EHSRInventoryViewModelResult::EntryUnavailable;
	}

	FHSRItemEquipmentMappingEntry Mapping;
	if (!MappingCatalog->Resolve(Row.ItemId, Mapping))
	{
		PublishFailure(EHSRInventoryViewModelResult::CatalogUnavailable);
		return EHSRInventoryViewModelResult::CatalogUnavailable;
	}

	FHSREquipmentLoadout Loadout;
	int32 EquipmentRevision = 0;
	const bool bHasLoadout = Equipment->GetLoadout(CharacterId, Loadout, EquipmentRevision);
	const EHSREquipmentMovementIntent Intent = bHasLoadout
		&& IsSlotOccupied(Loadout, Mapping.Kind, Mapping.Slot)
		? EHSREquipmentMovementIntent::Replace : EHSREquipmentMovementIntent::Equip;

	FHSREquipmentMovementRequest Request;
	Request.OperationId = FGuid::NewGuid();
	Request.CharacterId = CharacterId;
	Request.InstanceId = Row.Key.InstanceId;
	Request.Intent = Intent;
	Request.Kind = Mapping.Kind;
	Request.Slot = Mapping.Slot;
	Request.ExpectedInventoryRevision = Snapshot.InventoryRevision;
	Request.ExpectedEquipmentRevision = EquipmentRevision;
	const FHSREquipmentMovementResult AuthorityResult = Equipment->ExecuteMovement(
		Request, *Inventory, *MappingCatalog);
	const EHSRInventoryViewModelResult Result = MapMovementResult(AuthorityResult.Code);
	if (Result != EHSRInventoryViewModelResult::Success)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("HSR.Inventory Equip rejected op=%s authorityCode=%d mapped=%d invRev=%lld equipRev=%d"),
			*Request.OperationId.ToString(), static_cast<int32>(AuthorityResult.Code),
			static_cast<int32>(Result), Request.ExpectedInventoryRevision,
			Request.ExpectedEquipmentRevision);
		PublishFailure(Result);
	}
	return Result;
}

EHSRInventoryViewModelResult UHSRInventoryViewModel::SubmitEnhancement(const int32 TargetLevel)
{
	if (!Equipment.IsValid() || !EnhancementCatalog.IsValid() || !CharacterId.IsValid())
	{
		PublishFailure(EHSRInventoryViewModelResult::AuthorityUnavailable);
		return EHSRInventoryViewModelResult::AuthorityUnavailable;
	}
	if (TargetLevel < 0)
	{
		PublishFailure(EHSRInventoryViewModelResult::InvalidTargetLevel);
		return EHSRInventoryViewModelResult::InvalidTargetLevel;
	}
	const FHSRInventoryEntryRow& Row = Snapshot.Detail.Entry;
	if (!Row.bIsUnique || !Row.Key.InstanceId.IsValid())
	{
		PublishFailure(EHSRInventoryViewModelResult::EntryUnavailable);
		return EHSRInventoryViewModelResult::EntryUnavailable;
	}

	FHSREquipmentInstance CurrentInstance;
	if (!Equipment->FindRegisteredInstance(Row.Key.InstanceId, CurrentInstance))
	{
		PublishFailure(EHSRInventoryViewModelResult::AuthorityRejected);
		return EHSRInventoryViewModelResult::AuthorityRejected;
	}
	FGuid OwnerCharacterId;
	if (!Equipment->FindInstanceOwner(Row.Key.InstanceId, OwnerCharacterId)
		|| OwnerCharacterId != CharacterId)
	{
		PublishFailure(EHSRInventoryViewModelResult::AuthorityRejected);
		return EHSRInventoryViewModelResult::AuthorityRejected;
	}
	FHSREquipmentLoadout Loadout;
	int32 EquipmentRevision = 0;
	if (!Equipment->GetLoadout(CharacterId, Loadout, EquipmentRevision))
	{
		PublishFailure(EHSRInventoryViewModelResult::AuthorityRejected);
		return EHSRInventoryViewModelResult::AuthorityRejected;
	}

	FHSREquipmentEnhancementRule Rule;
	if (!EnhancementCatalog->ResolveRule(CurrentInstance.DefinitionId, CurrentInstance.Kind,
		TargetLevel, Rule))
	{
		PublishFailure(EHSRInventoryViewModelResult::NoEnhancementOption);
		return EHSRInventoryViewModelResult::NoEnhancementOption;
	}

	FHSREquipmentEnhancementRequest Request;
	Request.OperationId = FGuid::NewGuid();
	Request.CharacterId = CharacterId;
	Request.InstanceId = Row.Key.InstanceId;
	Request.Kind = CurrentInstance.Kind;
	Request.ExpectedInventoryRevision = Snapshot.InventoryRevision;
	Request.ExpectedEquipmentRevision = EquipmentRevision;
	Request.ExpectedEnhancementLevel = CurrentInstance.EnhancementLevel;
	Request.TargetLevel = TargetLevel;
	const FHSREquipmentEnhancementResult AuthorityResult = Equipment->ExecuteEnhancement(
		Request, *Inventory, *EnhancementCatalog);
	const EHSRInventoryViewModelResult Result = MapEnhancementResult(AuthorityResult.Code);
	if (Result != EHSRInventoryViewModelResult::Success)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("HSR.Inventory Enhance rejected op=%s authorityCode=%d mapped=%d invRev=%lld equipRev=%d target=%d"),
			*Request.OperationId.ToString(), static_cast<int32>(AuthorityResult.Code),
			static_cast<int32>(Result), Request.ExpectedInventoryRevision,
			Request.ExpectedEquipmentRevision, TargetLevel);
		PublishFailure(Result);
	}
	return Result;
}

bool UHSRInventoryViewModel::GetEntry(const int32 Index, FHSRInventoryEntryRow& OutEntry) const
{
	if (!bHasSnapshot || !Snapshot.Entries.IsValidIndex(Index)) return false;
	OutEntry = Snapshot.Entries[Index];
	return true;
}

bool UHSRInventoryViewModel::GetActionState(const int32 Index,
	FHSRInventoryActionState& OutAction) const
{
	if (!bHasSnapshot || !Snapshot.Actions.IsValidIndex(Index)) return false;
	OutAction = Snapshot.Actions[Index];
	return true;
}

void UHSRInventoryViewModel::HandleInventoryChanged(const int64)
{
	Rebuild();
}

void UHSRInventoryViewModel::HandleEquipmentChanged(const FGuid& ChangedCharacterId,
	const int32)
{
	if (ChangedCharacterId == CharacterId)
	{
		Rebuild();
	}
}

void UHSRInventoryViewModel::Rebuild()
{
	FHSRInventoryModuleSnapshot Candidate;
	const EHSRInventoryViewModelResult Result = BuildSnapshot(
		Candidate, Category, FilterText, SortMode, bHasSnapshot ? Snapshot.SelectedKey : FHSRInventoryEntryKey());
	if (Result != EHSRInventoryViewModelResult::Success)
	{
		if (!bHasSnapshot)
		{
			Snapshot = Candidate;
			Snapshot.Category = Category;
			Snapshot.FilterText = FilterText;
			Snapshot.SortMode = SortMode;
			Snapshot.bIsValid = false;
			Snapshot.FailureReason = Result;
			bHasSnapshot = true;
			Changed.Broadcast(Snapshot);
			OnSnapshotChanged.Broadcast(Snapshot);
		}
		else
		{
			PublishFailure(Result);
		}
		return;
	}

	Snapshot = MoveTemp(Candidate);
	bHasSnapshot = true;
	Changed.Broadcast(Snapshot);
	OnSnapshotChanged.Broadcast(Snapshot);
}

void UHSRInventoryViewModel::PublishFailure(const EHSRInventoryViewModelResult Result) const
{
	UE_LOG(LogTemp, Warning, TEXT("HSR.Inventory ViewModel rejected result=%d; committed snapshot retained"),
		static_cast<int32>(Result));
}

EHSRInventoryViewModelResult UHSRInventoryViewModel::ApplyPresentationState(
	const EHSRInventoryCategory InCategory, const FString& InFilterText,
	const EHSRInventorySortMode InSortMode, const FHSRInventoryEntryKey& InSelectedKey)
{
	FHSRInventoryModuleSnapshot Candidate;
	const EHSRInventoryViewModelResult Result = BuildSnapshot(
		Candidate, InCategory, InFilterText, InSortMode, InSelectedKey);
	if (Result != EHSRInventoryViewModelResult::Success)
	{
		PublishFailure(Result);
		return Result;
	}

	Category = InCategory;
	FilterText = InFilterText;
	SortMode = InSortMode;
	Snapshot = MoveTemp(Candidate);
	bHasSnapshot = true;
	Changed.Broadcast(Snapshot);
	OnSnapshotChanged.Broadcast(Snapshot);
	return EHSRInventoryViewModelResult::Success;
}

EHSRInventoryViewModelResult UHSRInventoryViewModel::BuildSnapshot(
	FHSRInventoryModuleSnapshot& OutSnapshot, const EHSRInventoryCategory InCategory,
	const FString& InFilterText, const EHSRInventorySortMode InSortMode,
	const FHSRInventoryEntryKey& InSelectedKey) const
{
	OutSnapshot = FHSRInventoryModuleSnapshot();
	OutSnapshot.Category = InCategory;
	OutSnapshot.FilterText = InFilterText;
	OutSnapshot.SortMode = InSortMode;
	OutSnapshot.SelectedKey = InSelectedKey;

	if (!Inventory.IsValid()) return EHSRInventoryViewModelResult::NotInitialized;
	if (!Catalog.IsValid()) return EHSRInventoryViewModelResult::CatalogUnavailable;
	FString CatalogError;
	if (!Catalog->Validate(&CatalogError))
	{
		UE_LOG(LogTemp, Warning, TEXT("HSR.Inventory invalid catalog: %s"), *CatalogError);
		return EHSRInventoryViewModelResult::InvalidCatalog;
	}
	if (!IsValidCategory(InCategory)) return EHSRInventoryViewModelResult::InvalidCategory;
	if (!IsValidSortMode(InSortMode)) return EHSRInventoryViewModelResult::InvalidSortMode;

	FHSRInventorySnapshot InventorySnapshot;
	Inventory->GetSnapshot(InventorySnapshot);
	OutSnapshot.InventoryRevision = InventorySnapshot.Revision;
	if (Equipment.IsValid() && CharacterId.IsValid())
	{
		FHSREquipmentLoadout Loadout;
		int32 EquipmentRevision = 0;
		if (Equipment->GetLoadout(CharacterId, Loadout, EquipmentRevision))
		{
			OutSnapshot.EquipmentRevision = EquipmentRevision;
		}
	}
	FString NormalizedFilter = InFilterText;
	NormalizedFilter.TrimStartAndEndInline();

	auto AddRow = [&OutSnapshot, this, InCategory, &NormalizedFilter](
		const FName ItemId, const FName DefinitionId, const int32 Quantity,
		const int32 MaxStack, const bool bIsUnique, const FHSRItemInstance* UniqueInstance)
	{
		FHSRInventoryCatalogEntry CatalogEntry;
		if (!Catalog->FindEntry(ItemId, CatalogEntry)) return false;
		if (CatalogEntry.Category != InCategory) return true;

		FHSRInventoryEntryRow Row;
		Row.ItemId = ItemId;
		Row.DefinitionId = DefinitionId;
		Row.Category = CatalogEntry.Category;
		Row.DisplayName = CatalogEntry.DisplayName;
		Row.Quantity = Quantity;
		Row.MaxStack = MaxStack;
		Row.bIsUnique = bIsUnique;
		Row.SortOrder = CatalogEntry.SortOrder;
		Row.Key.ItemId = ItemId;
		Row.Key.InstanceId = bIsUnique && UniqueInstance ? UniqueInstance->InstanceId : FGuid();
		if (UniqueInstance) Row.UniqueInstance = *UniqueInstance;
		if (HasFilterMatch(Row, NormalizedFilter)) OutSnapshot.Entries.Add(MoveTemp(Row));
		return true;
	};

	for (const FHSRItemStackSnapshot& Stack : InventorySnapshot.Stacks)
	{
		EHSRItemStorageKind StorageKind = EHSRItemStorageKind::Unique;
		int32 MaxStack = 0;
		if (!Inventory->GetDefinitionInfo(Stack.ItemId, StorageKind, MaxStack)
			|| StorageKind != EHSRItemStorageKind::Stackable
			|| !AddRow(Stack.ItemId, Stack.ItemId, Stack.Quantity, MaxStack, false, nullptr))
		{
			return EHSRInventoryViewModelResult::EntryUnavailable;
		}
	}
	for (const FHSRItemInstance& Instance : InventorySnapshot.UniqueItems)
	{
		EHSRItemStorageKind StorageKind = EHSRItemStorageKind::Stackable;
		int32 MaxStack = 0;
		if (!Inventory->GetDefinitionInfo(Instance.DefinitionId, StorageKind, MaxStack)
			|| StorageKind != EHSRItemStorageKind::Unique
			|| !Instance.InstanceId.IsValid()
			|| !AddRow(Instance.DefinitionId, Instance.DefinitionId, 1, 1, true, &Instance))
		{
			return EHSRInventoryViewModelResult::EntryUnavailable;
		}
	}

	OutSnapshot.Entries.Sort([InSortMode](const FHSRInventoryEntryRow& A, const FHSRInventoryEntryRow& B)
	{
		return IsBeforeStable(A, B, InSortMode);
	});

	const FHSRInventoryEntryRow* Selected = OutSnapshot.Entries.FindByPredicate(
		[&InSelectedKey](const FHSRInventoryEntryRow& Row) { return Row.Key == InSelectedKey; });
	if (Selected)
	{
		OutSnapshot.SelectedKey = Selected->Key;
		OutSnapshot.Detail.bHasSelection = true;
		OutSnapshot.Detail.Entry = *Selected;
	}
	else
	{
		OutSnapshot.SelectedKey = FHSRInventoryEntryKey();
	}
	BuildActionStates(OutSnapshot, InventorySnapshot);
	OutSnapshot.bIsValid = true;
	OutSnapshot.FailureReason = EHSRInventoryViewModelResult::Success;
	return EHSRInventoryViewModelResult::Success;
}

void UHSRInventoryViewModel::BuildActionStates(FHSRInventoryModuleSnapshot& InOutSnapshot,
	const FHSRInventorySnapshot& InventorySnapshot) const
{
	InOutSnapshot.Actions.Reset();
	BuildEnhancementOptions(InOutSnapshot, InventorySnapshot);
	const bool bHasSelection = InOutSnapshot.Detail.bHasSelection;
	const FHSRInventoryEntryRow& Row = InOutSnapshot.Detail.Entry;

	for (const EHSRInventoryAction Action : {EHSRInventoryAction::Use, EHSRInventoryAction::Equip,
		EHSRInventoryAction::Enhance, EHSRInventoryAction::Disassemble})
	{
		FHSRInventoryActionState& ActionState = InOutSnapshot.Actions.AddDefaulted_GetRef();
		ActionState.Action = Action;
		ActionState.bIsAvailable = false;
		ActionState.UnavailableReason = EHSRInventoryViewModelResult::AuthorityUnavailable;
		if (Action == EHSRInventoryAction::Use || Action == EHSRInventoryAction::Disassemble)
		{
			continue;
		}
		if (!bHasSelection)
		{
			ActionState.UnavailableReason = EHSRInventoryViewModelResult::EntryUnavailable;
			continue;
		}
		if (!Row.bIsUnique || !Row.Key.InstanceId.IsValid())
		{
			ActionState.UnavailableReason = EHSRInventoryViewModelResult::EntryUnavailable;
			continue;
		}
		if (!Equipment.IsValid() || !CharacterId.IsValid())
		{
			continue;
		}

		if (Action == EHSRInventoryAction::Equip)
		{
			if (!MappingCatalog.IsValid()) continue;
			FHSRItemEquipmentMappingEntry Mapping;
			if (!MappingCatalog->Resolve(Row.ItemId, Mapping))
			{
				ActionState.UnavailableReason = EHSRInventoryViewModelResult::CatalogUnavailable;
				continue;
			}
			// A dropped reward item may not have an equipment instance registered yet; ExecuteMovement
			// mints one on the fly.  Treat a resolvable mapping as equippable so the button is not
			// disabled for items that just entered the bag.
			ActionState.bIsAvailable = true;
			ActionState.UnavailableReason = EHSRInventoryViewModelResult::Success;
		}
		else if (Action == EHSRInventoryAction::Enhance)
		{
			if (!EnhancementCatalog.IsValid())
			{
				ActionState.UnavailableReason = EHSRInventoryViewModelResult::CatalogUnavailable;
				continue;
			}
			FHSREquipmentInstance RegisteredInstance;
			FGuid OwnerCharacterId;
			if (!Equipment->FindRegisteredInstance(Row.Key.InstanceId, RegisteredInstance)
				|| !Equipment->FindInstanceOwner(Row.Key.InstanceId, OwnerCharacterId)
				|| OwnerCharacterId != CharacterId)
			{
				ActionState.UnavailableReason = EHSRInventoryViewModelResult::AuthorityRejected;
				continue;
			}
			if (InOutSnapshot.EnhancementOptions.IsEmpty())
			{
				ActionState.UnavailableReason = EHSRInventoryViewModelResult::NoEnhancementOption;
				continue;
			}
			ActionState.bIsAvailable = true;
			ActionState.UnavailableReason = EHSRInventoryViewModelResult::Success;
		}
	}
}

void UHSRInventoryViewModel::BuildEnhancementOptions(
	FHSRInventoryModuleSnapshot& InOutSnapshot,
	const FHSRInventorySnapshot& InventorySnapshot) const
{
	InOutSnapshot.EnhancementOptions.Reset();
	if (!InOutSnapshot.Detail.bHasSelection || !EnhancementCatalog.IsValid()
		|| !Equipment.IsValid() || !CharacterId.IsValid())
	{
		return;
	}
	const FHSRInventoryEntryRow& Row = InOutSnapshot.Detail.Entry;
	if (!Row.bIsUnique || !Row.Key.InstanceId.IsValid()) return;
	FHSREquipmentInstance CurrentInstance;
	FGuid OwnerCharacterId;
	if (!Equipment->FindRegisteredInstance(Row.Key.InstanceId, CurrentInstance)
		|| !Equipment->FindInstanceOwner(Row.Key.InstanceId, OwnerCharacterId)
		|| OwnerCharacterId != CharacterId)
	{
		return;
	}

	TArray<FHSREquipmentEnhancementRule> Rules;
	EnhancementCatalog->GetRulesFor(CurrentInstance.DefinitionId, CurrentInstance.Kind,
		CurrentInstance.EnhancementLevel, Rules);
	for (const FHSREquipmentEnhancementRule& Rule : Rules)
	{
		FHSRInventoryEnhancementOption& Option = InOutSnapshot.EnhancementOptions.AddDefaulted_GetRef();
		Option.TargetLevel = Rule.TargetLevel;
		Option.MaterialItemId = Rule.MaterialItemId;
		Option.MaterialCost = Rule.MaterialCost;
		Option.bAffordable = FindStackQuantity(InventorySnapshot, Rule.MaterialItemId) >= Rule.MaterialCost;
		Option.bAvailable = Option.bAffordable && Rule.TargetLevel > CurrentInstance.EnhancementLevel;
	}
}

EHSRInventoryViewModelResult UHSRInventoryViewModel::MapMovementResult(
	const EHSREquipmentMovementResultCode Code)
{
	if (Code == EHSREquipmentMovementResultCode::Success)
	{
		return EHSRInventoryViewModelResult::Success;
	}
	if (Code == EHSREquipmentMovementResultCode::InventoryRevisionConflict
		|| Code == EHSREquipmentMovementResultCode::EquipmentRevisionConflict)
	{
		return EHSRInventoryViewModelResult::StaleSnapshot;
	}
	if (Code == EHSREquipmentMovementResultCode::MappingRejected)
	{
		return EHSRInventoryViewModelResult::CatalogUnavailable;
	}
	return EHSRInventoryViewModelResult::AuthorityRejected;
}

EHSRInventoryViewModelResult UHSRInventoryViewModel::MapEnhancementResult(
	const EHSREquipmentEnhancementResultCode Code)
{
	if (Code == EHSREquipmentEnhancementResultCode::Success
		|| Code == EHSREquipmentEnhancementResultCode::NoOp)
	{
		return EHSRInventoryViewModelResult::Success;
	}
	if (Code == EHSREquipmentEnhancementResultCode::InventoryRevisionConflict
		|| Code == EHSREquipmentEnhancementResultCode::EquipmentRevisionConflict
		|| Code == EHSREquipmentEnhancementResultCode::EnhancementLevelConflict)
	{
		return EHSRInventoryViewModelResult::StaleSnapshot;
	}
	if (Code == EHSREquipmentEnhancementResultCode::CatalogRejected)
	{
		return EHSRInventoryViewModelResult::CatalogUnavailable;
	}
	return EHSRInventoryViewModelResult::AuthorityRejected;
}

int32 UHSRInventoryViewModel::FindStackQuantity(const FHSRInventorySnapshot& InventorySnapshot,
	const FName ItemId)
{
	for (const FHSRItemStackSnapshot& Stack : InventorySnapshot.Stacks)
	{
		if (Stack.ItemId == ItemId) return Stack.Quantity;
	}
	return 0;
}

bool UHSRInventoryViewModel::IsInitialized() const
{
	return Inventory.IsValid();
}

bool UHSRInventoryViewModel::IsValidCategory(const EHSRInventoryCategory InCategory)
{
	return InCategory >= EHSRInventoryCategory::Weapon && InCategory <= EHSRInventoryCategory::Other;
}

bool UHSRInventoryViewModel::IsValidSortMode(const EHSRInventorySortMode InSortMode)
{
	return InSortMode >= EHSRInventorySortMode::CatalogOrder
		&& InSortMode <= EHSRInventorySortMode::QuantityDescending;
}

bool UHSRInventoryViewModel::AreKeysEqual(const FHSRInventoryEntryKey& A,
	const FHSRInventoryEntryKey& B)
{
	return A == B;
}
