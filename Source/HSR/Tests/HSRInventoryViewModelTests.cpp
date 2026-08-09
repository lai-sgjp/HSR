#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "../Data/Definitions/HSREquipmentEnhancementCatalog.h"
#include "../Data/Definitions/HSREquipmentDefinition.h"
#include "../Data/Definitions/HSRInventoryCatalog.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Data/Definitions/HSRItemEquipmentMappingCatalog.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Equipment/HSREquipmentTypes.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../UI/Inventory/HSRInventoryTypes.h"
#include "../UI/Inventory/HSRInventoryViewModel.h"
#include "../UI/Inventory/HSRInventoryModuleWidget.h"
#include "../UI/HSRUIManagerSubsystem.h"
#include "../UI/Frontend/HSRFrontendRouteTypes.h"

namespace HSR::P17::InventoryTests
{
	static UHSRUIManagerSubsystem* MakeUIManager()
	{
		ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
		UHSRUIManagerSubsystem* Manager = NewObject<UHSRUIManagerSubsystem>(LocalPlayer);
		Manager->InitializeForAutomation();
		return Manager;
	}

	static UHSRInventorySubsystem* MakeInventory(UGameInstance* GameInstance)
	{
		return NewObject<UHSRInventorySubsystem>(GameInstance);
	}

	static UHSRItemDefinition* RegisterItem(UHSRInventorySubsystem* Inventory,
		UObject* Outer, const FName ItemId, const EHSRItemStorageKind StorageKind,
		const int32 MaxStack)
	{
		UHSRItemDefinition* Definition = NewObject<UHSRItemDefinition>(Outer);
		Definition->ItemId = ItemId;
		Definition->StorageKind = StorageKind;
		Definition->MaxStack = MaxStack;
		return Inventory->RegisterDefinition(*Definition) == EHSRInventoryOperationResult::Success
			? Definition : nullptr;
	}

	static void AddCatalogEntry(UHSRInventoryCatalog* Catalog, const FName ItemId,
		const EHSRInventoryCategory Category, const TCHAR* DisplayName, const int32 SortOrder)
	{
		FHSRInventoryCatalogEntry& Entry = Catalog->Entries.AddDefaulted_GetRef();
		Entry.ItemId = ItemId;
		Entry.Category = Category;
		Entry.DisplayName = FText::FromString(DisplayName);
		Entry.SortOrder = SortOrder;
	}

	static bool SameKey(const FHSRInventoryEntryKey& A, const FHSRInventoryEntryKey& B)
	{
		return A.ItemId == B.ItemId && A.InstanceId == B.InstanceId;
	}

	static bool SameRow(const FHSRInventoryEntryRow& A, const FHSRInventoryEntryRow& B)
	{
		return SameKey(A.Key, B.Key)
			&& A.ItemId == B.ItemId
			&& A.DefinitionId == B.DefinitionId
			&& A.Category == B.Category
			&& A.DisplayName.ToString() == B.DisplayName.ToString()
			&& A.Quantity == B.Quantity
			&& A.MaxStack == B.MaxStack
			&& A.SortOrder == B.SortOrder
			&& A.bIsUnique == B.bIsUnique
			&& A.UniqueInstance.InstanceId == B.UniqueInstance.InstanceId
			&& A.UniqueInstance.DefinitionId == B.UniqueInstance.DefinitionId;
	}

	static bool SameSnapshot(const FHSRInventoryModuleSnapshot& A,
		const FHSRInventoryModuleSnapshot& B)
	{
		if (A.Category != B.Category || A.FilterText != B.FilterText || A.SortMode != B.SortMode
			|| !SameKey(A.SelectedKey, B.SelectedKey) || A.InventoryRevision != B.InventoryRevision
			|| A.EquipmentRevision != B.EquipmentRevision
			|| A.Entries.Num() != B.Entries.Num() || A.Detail.bHasSelection != B.Detail.bHasSelection
			|| A.Actions.Num() != B.Actions.Num()
			|| A.EnhancementOptions.Num() != B.EnhancementOptions.Num()
			|| A.bIsValid != B.bIsValid
			|| A.FailureReason != B.FailureReason)
		{
			return false;
		}

		for (int32 Index = 0; Index < A.Entries.Num(); ++Index)
		{
			if (!SameRow(A.Entries[Index], B.Entries[Index])) return false;
		}

		if (!SameRow(A.Detail.Entry, B.Detail.Entry)) return false;
		for (int32 Index = 0; Index < A.Actions.Num(); ++Index)
		{
			const FHSRInventoryActionState& Left = A.Actions[Index];
			const FHSRInventoryActionState& Right = B.Actions[Index];
			if (Left.Action != Right.Action || Left.bIsAvailable != Right.bIsAvailable
				|| Left.UnavailableReason != Right.UnavailableReason)
			{
				return false;
			}
		}
		for (int32 Index = 0; Index < A.EnhancementOptions.Num(); ++Index)
		{
			const FHSRInventoryEnhancementOption& Left = A.EnhancementOptions[Index];
			const FHSRInventoryEnhancementOption& Right = B.EnhancementOptions[Index];
			if (Left.TargetLevel != Right.TargetLevel || Left.MaterialItemId != Right.MaterialItemId
				|| Left.MaterialCost != Right.MaterialCost || Left.bAffordable != Right.bAffordable
				|| Left.bAvailable != Right.bAvailable)
			{
				return false;
			}
		}

		return true;
	}

	static void AddEquipmentMapping(UHSRItemEquipmentMappingCatalog* Catalog,
		const FName ItemId, const FName EquipmentDefinitionId,
		const EHSREquipmentKind Kind, const int32 Slot)
	{
		FHSRItemEquipmentMappingEntry& Mapping = Catalog->Mappings.AddDefaulted_GetRef();
		Mapping.ItemId = ItemId;
		Mapping.EquipmentDefinitionId = EquipmentDefinitionId;
		Mapping.Kind = Kind;
		Mapping.Slot = Slot;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSRInventoryViewModelClassificationSortSelectionTest,
	"HSR.UI.Inventory.ViewModel.ClassificationSortSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRInventoryViewModelClassificationSortSelectionTest::RunTest(const FString&)
{
	using namespace HSR::P17::InventoryTests;
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UHSRInventorySubsystem* Inventory = MakeInventory(GameInstance);
	UHSRInventoryCatalog* Catalog = NewObject<UHSRInventoryCatalog>(GameInstance);

	static const FName AlphaRelicItem(TEXT("Item.Relic.Alpha"));
	static const FName BetaRelicItem(TEXT("Item.Relic.Beta"));
	static const FName MaterialItem(TEXT("Item.Material.Test"));
	RegisterItem(Inventory, GameInstance, AlphaRelicItem, EHSRItemStorageKind::Unique, 1);
	RegisterItem(Inventory, GameInstance, BetaRelicItem, EHSRItemStorageKind::Unique, 1);
	RegisterItem(Inventory, GameInstance, MaterialItem, EHSRItemStorageKind::Stackable, 99);
	AddCatalogEntry(Catalog, AlphaRelicItem, EHSRInventoryCategory::Relic, TEXT("Beta Relic"), 20);
	AddCatalogEntry(Catalog, BetaRelicItem, EHSRInventoryCategory::Relic, TEXT("Alpha Relic"), 10);
	AddCatalogEntry(Catalog, MaterialItem, EHSRInventoryCategory::Material, TEXT("Test Material"), 30);
	TestTrue(TEXT("Inventory catalog validates unique ItemIds"), Catalog->Validate());

	const FGuid AlphaInstanceId(1, 2, 3, 4);
	const FGuid BetaInstanceId(5, 6, 7, 8);
	TestEqual(TEXT("Alpha unique item is added"),
		Inventory->AddUnique({AlphaInstanceId, AlphaRelicItem}), EHSRInventoryOperationResult::Success);
	TestEqual(TEXT("Beta unique item is added"),
		Inventory->AddUnique({BetaInstanceId, BetaRelicItem}), EHSRInventoryOperationResult::Success);
	TestEqual(TEXT("Material stack is added"),
		Inventory->AddStack(MaterialItem, 2), EHSRInventoryOperationResult::Success);

	UHSRInventoryViewModel* ViewModel = NewObject<UHSRInventoryViewModel>(GameInstance);
	ViewModel->Initialize(Inventory, Catalog);
	TestEqual(TEXT("Relic category selection succeeds"),
		ViewModel->SelectCategory(EHSRInventoryCategory::Relic), EHSRInventoryViewModelResult::Success);
	TestEqual(TEXT("Display-name sort succeeds"),
		ViewModel->SetSortMode(EHSRInventorySortMode::DisplayNameAscending),
		EHSRInventoryViewModelResult::Success);

	FHSRInventoryModuleSnapshot Snapshot;
	TestTrue(TEXT("Initial categorized snapshot is available"), ViewModel->GetSnapshot(Snapshot));
	TestTrue(TEXT("Categorized snapshot is valid"), Snapshot.bIsValid);
	TestEqual(TEXT("Only relics are projected"), Snapshot.Entries.Num(), 2);
	TestEqual(TEXT("Stable first row uses display-name order"),
		Snapshot.Entries[0].DisplayName.ToString(), FString(TEXT("Alpha Relic")));
	TestEqual(TEXT("Stable second row uses display-name order"),
		Snapshot.Entries[1].DisplayName.ToString(), FString(TEXT("Beta Relic")));

	const FHSRInventoryEntryKey SelectedKey = Snapshot.Entries[1].Key;
	TestEqual(TEXT("Stable key selection succeeds"), ViewModel->SelectEntry(SelectedKey),
		EHSRInventoryViewModelResult::Success);
	TestTrue(TEXT("Selected detail is available"), ViewModel->GetSnapshot(Snapshot));
	TestTrue(TEXT("Selected detail keeps the stable key"), SameKey(Snapshot.SelectedKey, SelectedKey));
	TestTrue(TEXT("Selected detail is marked present"), Snapshot.Detail.bHasSelection);

	TestEqual(TEXT("Case-insensitive display-name filter succeeds"),
		ViewModel->SetFilterText(TEXT("beta relic")),
		EHSRInventoryViewModelResult::Success);
	TestTrue(TEXT("Filtered snapshot is available"), ViewModel->GetSnapshot(Snapshot));
	TestEqual(TEXT("Filter keeps only matching row"), Snapshot.Entries.Num(), 1);
	TestEqual(TEXT("Filter matches display name"),
		Snapshot.Entries[0].DisplayName.ToString(), FString(TEXT("Beta Relic")));
	TestTrue(TEXT("Filter preserves matching selection"), SameKey(Snapshot.SelectedKey, SelectedKey));
	TestEqual(TEXT("Clearing filter succeeds"), ViewModel->SetFilterText(TEXT("")),
		EHSRInventoryViewModelResult::Success);

	TestEqual(TEXT("Adding a new item succeeds"),
		Inventory->AddUnique({FGuid(9, 10, 11, 12), AlphaRelicItem}),
		EHSRInventoryOperationResult::Success);
	TestTrue(TEXT("Committed callback refreshes the snapshot"), ViewModel->GetSnapshot(Snapshot));
	TestTrue(TEXT("Selection survives a resort and refresh"), SameKey(Snapshot.SelectedKey, SelectedKey));
	TestTrue(TEXT("Selection remains in the projected rows"),
		Snapshot.Entries.ContainsByPredicate([&SelectedKey](const FHSRInventoryEntryRow& Row)
		{
			return SameKey(Row.Key, SelectedKey);
		}));

	ViewModel->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSRInventoryModuleWidgetReadOnlyLifecycleTest,
	"HSR.UI.Inventory.Widget.ReadOnlyLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRInventoryModuleWidgetReadOnlyLifecycleTest::RunTest(const FString&)
{
	using namespace HSR::P17::InventoryTests;
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UHSRInventorySubsystem* Inventory = MakeInventory(GameInstance);
	UHSRInventoryCatalog* Catalog = NewObject<UHSRInventoryCatalog>(GameInstance);
	static const FName MaterialItem(TEXT("Item.Material.Widget"));
	RegisterItem(Inventory, GameInstance, MaterialItem, EHSRItemStorageKind::Stackable, 20);
	AddCatalogEntry(Catalog, MaterialItem, EHSRInventoryCategory::Material, TEXT("Widget Material"), 1);
	TestTrue(TEXT("Widget catalog validates"), Catalog->Validate());
	TestEqual(TEXT("Widget material stack is added"), Inventory->AddStack(MaterialItem, 2),
		EHSRInventoryOperationResult::Success);

	UHSRInventoryViewModel* ViewModel = NewObject<UHSRInventoryViewModel>(GameInstance);
	ViewModel->Initialize(Inventory, Catalog);
	UHSRInventoryModuleWidget* Widget = NewObject<UHSRInventoryModuleWidget>(GameInstance);
	Widget->SetViewModel(ViewModel);
	Widget->AttachForAutomation();
	FHSRInventoryModuleSnapshot InitialSnapshot;
	TestTrue(TEXT("Widget receives the initial pure snapshot"), Widget->GetCurrentSnapshot(InitialSnapshot));
	TestEqual(TEXT("Widget binds once"), Widget->GetBindCountForAutomation(), 1);

	TestEqual(TEXT("Widget forwards category intent"),
		Widget->SelectCategory(EHSRInventoryCategory::Material), EHSRInventoryViewModelResult::Success);
	FHSRInventoryModuleSnapshot Snapshot;
	TestTrue(TEXT("Widget snapshot is readable"), Widget->GetCurrentSnapshot(Snapshot));
	TestEqual(TEXT("Widget exposes one material row"), Snapshot.Entries.Num(), 1);

	Widget->SetViewModel(nullptr);
	TestEqual(TEXT("Widget unbinds once"), Widget->GetUnbindCountForAutomation(), 1);
	const FHSRInventoryModuleSnapshot BeforeDetachedMutation = Snapshot;
	TestEqual(TEXT("Detached inventory mutation succeeds"), Inventory->AddStack(MaterialItem, 1),
		EHSRInventoryOperationResult::Success);
	TestTrue(TEXT("Detached widget keeps its last snapshot"), Widget->GetCurrentSnapshot(Snapshot));
	TestEqual(TEXT("Detached widget keeps old quantity"), Snapshot.Entries[0].Quantity,
		BeforeDetachedMutation.Entries[0].Quantity);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSRInventoryViewModelUnsupportedActionsPreserveSnapshotTest,
	"HSR.UI.Inventory.ViewModel.UnsupportedActionsPreserveSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRInventoryViewModelUnsupportedActionsPreserveSnapshotTest::RunTest(const FString&)
{
	using namespace HSR::P17::InventoryTests;
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UHSRInventorySubsystem* Inventory = MakeInventory(GameInstance);
	UHSRInventoryCatalog* Catalog = NewObject<UHSRInventoryCatalog>(GameInstance);
	static const FName ConsumableItem(TEXT("Item.Consumable.Test"));
	RegisterItem(Inventory, GameInstance, ConsumableItem, EHSRItemStorageKind::Stackable, 20);
	AddCatalogEntry(Catalog, ConsumableItem, EHSRInventoryCategory::Consumable, TEXT("Test Consumable"), 1);
	TestTrue(TEXT("Consumable catalog validates"), Catalog->Validate());
	TestEqual(TEXT("Consumable stack is added"), Inventory->AddStack(ConsumableItem, 3),
		EHSRInventoryOperationResult::Success);

	UHSRInventoryViewModel* ViewModel = NewObject<UHSRInventoryViewModel>(GameInstance);
	ViewModel->Initialize(Inventory, Catalog);
	TestEqual(TEXT("Consumable category selection succeeds"),
		ViewModel->SelectCategory(EHSRInventoryCategory::Consumable), EHSRInventoryViewModelResult::Success);
	FHSRInventoryModuleSnapshot Before;
	TestTrue(TEXT("Snapshot before unsupported command is available"), ViewModel->GetSnapshot(Before));
	TestTrue(TEXT("Snapshot before unsupported command is valid"), Before.bIsValid);
	TestEqual(TEXT("Consumable selection succeeds"), ViewModel->SelectEntry(Before.Entries[0].Key),
		EHSRInventoryViewModelResult::Success);
	TestTrue(TEXT("Selected snapshot is available"), ViewModel->GetSnapshot(Before));

	for (const EHSRInventoryAction Action : {EHSRInventoryAction::Use, EHSRInventoryAction::Disassemble,
		EHSRInventoryAction::Equip, EHSRInventoryAction::Enhance})
	{
		const EHSRInventoryViewModelResult Result = ViewModel->SubmitAction(Action);
		TestEqual(TEXT("Missing command authority is typed unavailable"), Result,
			EHSRInventoryViewModelResult::AuthorityUnavailable);
		FHSRInventoryModuleSnapshot After;
		TestTrue(TEXT("Failed command keeps a readable snapshot"), ViewModel->GetSnapshot(After));
		TestTrue(TEXT("Failed command preserves the complete snapshot"), SameSnapshot(Before, After));
	}

	ViewModel->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSRInventoryViewModelEquipThroughAuthorityTest,
	"HSR.UI.Inventory.ViewModel.Commands.EquipThroughAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRInventoryViewModelEquipThroughAuthorityTest::RunTest(const FString&)
{
	using namespace HSR::P17::InventoryTests;
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UHSRInventorySubsystem* Inventory = MakeInventory(GameInstance);
	UHSREquipmentSubsystem* Equipment = NewObject<UHSREquipmentSubsystem>(GameInstance);
	UHSRInventoryCatalog* Catalog = NewObject<UHSRInventoryCatalog>(GameInstance);
	UHSRItemEquipmentMappingCatalog* MappingCatalog =
		NewObject<UHSRItemEquipmentMappingCatalog>(GameInstance);

	const FName ItemId(TEXT("Item.Weapon.InventoryCommand"));
	const FName EquipmentDefinitionId(TEXT("Equipment.Weapon.InventoryCommand"));
	const FGuid CharacterId(13, 14, 15, 16);
	const FGuid InstanceId(17, 18, 19, 20);
	RegisterItem(Inventory, GameInstance, ItemId, EHSRItemStorageKind::Unique, 1);
	AddCatalogEntry(Catalog, ItemId, EHSRInventoryCategory::Weapon, TEXT("Command Weapon"), 1);
	AddEquipmentMapping(MappingCatalog, ItemId, EquipmentDefinitionId,
		EHSREquipmentKind::Equipment, static_cast<int32>(EHSREquipmentSlot::Weapon));

	UHSREquipmentDefinition* Definition = NewObject<UHSREquipmentDefinition>(GameInstance);
	Definition->DefinitionId = EquipmentDefinitionId;
	Definition->Slot = EHSREquipmentSlot::Weapon;
	Definition->EnhancementCap = 3;
	TestEqual(TEXT("Equipment definition registers"), Equipment->RegisterDefinition(*Definition),
		EHSREquipmentOperationResult::Success);
	FHSREquipmentInstance Instance;
	Instance.InstanceId = InstanceId;
	Instance.DefinitionId = EquipmentDefinitionId;
	Instance.Kind = EHSREquipmentKind::Equipment;
	TestEqual(TEXT("Equipment instance registers"), Equipment->RegisterInstance(Instance),
		EHSREquipmentOperationResult::Success);
	TestEqual(TEXT("Unique item enters bag"), Inventory->AddUnique({InstanceId, ItemId}),
		EHSRInventoryOperationResult::Success);

	UHSRInventoryViewModel* ViewModel = NewObject<UHSRInventoryViewModel>(GameInstance);
	ViewModel->Initialize(Inventory, Catalog);
	ViewModel->SetCommandContext(Equipment, MappingCatalog, nullptr, CharacterId);
	TestEqual(TEXT("Weapon category selection succeeds"),
		ViewModel->SelectCategory(EHSRInventoryCategory::Weapon),
		EHSRInventoryViewModelResult::Success);
	FHSRInventoryModuleSnapshot Before;
	TestTrue(TEXT("Equip snapshot is available"), ViewModel->GetSnapshot(Before));
	TestTrue(TEXT("Equip fixture projects a row"), Before.Entries.Num() == 1);
	if (Before.Entries.Num() != 1) return false;
	TestEqual(TEXT("Stable entry selection succeeds"), ViewModel->SelectEntry(Before.Entries[0].Key),
		EHSRInventoryViewModelResult::Success);

	TestEqual(TEXT("Equip delegates to Equipment Authority"),
		ViewModel->SubmitAction(EHSRInventoryAction::Equip),
		EHSRInventoryViewModelResult::Success);
	FHSRInventorySnapshot InventoryAfter;
	Inventory->GetSnapshot(InventoryAfter);
	TestEqual(TEXT("Authority movement removes the item once"), InventoryAfter.UniqueItems.Num(), 0);
	FHSREquipmentLoadout Loadout;
	int32 EquipmentRevision = 0;
	TestTrue(TEXT("Equipment loadout resolves after Equip"),
		Equipment->GetLoadout(CharacterId, Loadout, EquipmentRevision));
	TestTrue(TEXT("Equipment Authority owns the equipped slot"),
		Loadout.Equipment.Contains(EHSREquipmentSlot::Weapon));
	TestEqual(TEXT("Equipment revision advances once"), EquipmentRevision, 1);
	ViewModel->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSRInventoryViewModelEnhancementFailurePreservesSnapshotTest,
	"HSR.UI.Inventory.ViewModel.Commands.EnhancementFailurePreservesSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRInventoryViewModelEnhancementFailurePreservesSnapshotTest::RunTest(const FString&)
{
	using namespace HSR::P17::InventoryTests;
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UHSRInventorySubsystem* Inventory = MakeInventory(GameInstance);
	UHSREquipmentSubsystem* Equipment = NewObject<UHSREquipmentSubsystem>(GameInstance);
	UHSRInventoryCatalog* Catalog = NewObject<UHSRInventoryCatalog>(GameInstance);
	UHSRItemEquipmentMappingCatalog* MappingCatalog =
		NewObject<UHSRItemEquipmentMappingCatalog>(GameInstance);
	UHSREquipmentEnhancementCatalog* EnhancementCatalog =
		NewObject<UHSREquipmentEnhancementCatalog>(GameInstance);

	const FName ItemId(TEXT("Item.Weapon.EnhanceCommand"));
	const FName EquipmentDefinitionId(TEXT("Equipment.Weapon.EnhanceCommand"));
	const FName MaterialId(TEXT("Item.Material.EnhanceCommand"));
	const FGuid CharacterId(21, 22, 23, 24);
	const FGuid InstanceId(25, 26, 27, 28);
	RegisterItem(Inventory, GameInstance, ItemId, EHSRItemStorageKind::Unique, 1);
	RegisterItem(Inventory, GameInstance, MaterialId, EHSRItemStorageKind::Stackable, 20);
	AddCatalogEntry(Catalog, ItemId, EHSRInventoryCategory::Weapon, TEXT("Enhance Weapon"), 1);
	AddCatalogEntry(Catalog, MaterialId, EHSRInventoryCategory::Material, TEXT("Enhance Material"), 2);
	AddEquipmentMapping(MappingCatalog, ItemId, EquipmentDefinitionId,
		EHSREquipmentKind::Equipment, static_cast<int32>(EHSREquipmentSlot::Weapon));
	TestEqual(TEXT("Enhancement material enters bag"), Inventory->AddStack(MaterialId, 5),
		EHSRInventoryOperationResult::Success);

	UHSREquipmentDefinition* Definition = NewObject<UHSREquipmentDefinition>(GameInstance);
	Definition->DefinitionId = EquipmentDefinitionId;
	Definition->Slot = EHSREquipmentSlot::Weapon;
	Definition->EnhancementCap = 3;
	TestEqual(TEXT("Enhancement definition registers"), Equipment->RegisterDefinition(*Definition),
		EHSREquipmentOperationResult::Success);
	FHSREquipmentInstance Instance;
	Instance.InstanceId = InstanceId;
	Instance.DefinitionId = EquipmentDefinitionId;
	Instance.Kind = EHSREquipmentKind::Equipment;
	TestEqual(TEXT("Enhancement registry instance registers"), Equipment->RegisterInstance(Instance),
		EHSREquipmentOperationResult::Success);
	TestTrue(TEXT("Enhancement rule registers"), EnhancementCatalog->AddRule({
		EquipmentDefinitionId, EHSREquipmentKind::Equipment, 1, MaterialId, 2,
		{{EHSREquipmentStat::Attack, 8.0f}}}));
	TestEqual(TEXT("Enhancement item enters bag"), Inventory->AddUnique({InstanceId, ItemId}),
		EHSRInventoryOperationResult::Success);

	UHSRInventoryViewModel* ViewModel = NewObject<UHSRInventoryViewModel>(GameInstance);
	ViewModel->Initialize(Inventory, Catalog);
	ViewModel->SetCommandContext(Equipment, MappingCatalog, EnhancementCatalog, CharacterId);
	TestEqual(TEXT("Enhancement category selection succeeds"),
		ViewModel->SelectCategory(EHSRInventoryCategory::Weapon),
		EHSRInventoryViewModelResult::Success);
	FHSRInventoryModuleSnapshot Before;
	TestTrue(TEXT("Enhancement snapshot is available"), ViewModel->GetSnapshot(Before));
	TestTrue(TEXT("Enhancement fixture projects a row"), Before.Entries.Num() == 1);
	if (Before.Entries.Num() != 1) return false;
	TestEqual(TEXT("Enhancement entry selection succeeds"), ViewModel->SelectEntry(Before.Entries[0].Key),
		EHSRInventoryViewModelResult::Success);
	TestTrue(TEXT("Selected enhancement snapshot is available"), ViewModel->GetSnapshot(Before));
	TestEqual(TEXT("Unowned enhancement is rejected by Authority"),
		ViewModel->SubmitAction(EHSRInventoryAction::Enhance, 1),
		EHSRInventoryViewModelResult::AuthorityRejected);
	FHSRInventoryModuleSnapshot After;
	TestTrue(TEXT("Failed enhancement keeps snapshot available"), ViewModel->GetSnapshot(After));
	TestTrue(TEXT("Failed enhancement preserves complete snapshot"), SameSnapshot(Before, After));
	FHSRInventorySnapshot InventoryAfter;
	Inventory->GetSnapshot(InventoryAfter);
	TestEqual(TEXT("Failed enhancement keeps materials"), InventoryAfter.Stacks[0].Quantity, 5);
	FHSREquipmentInstance AfterInstance;
	TestTrue(TEXT("Failed enhancement keeps registry payload"),
		Equipment->FindRegisteredInstance(InstanceId, AfterInstance));
	TestEqual(TEXT("Failed enhancement keeps level"), AfterInstance.EnhancementLevel, 0);
	ViewModel->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSRInventoryViewModelUnsupportedCommandsPreserveSnapshotTest,
	"HSR.UI.Inventory.ViewModel.Commands.UnsupportedPreserveSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRInventoryViewModelUnsupportedCommandsPreserveSnapshotTest::RunTest(const FString&)
{
	using namespace HSR::P17::InventoryTests;
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UHSRInventorySubsystem* Inventory = MakeInventory(GameInstance);
	UHSRInventoryCatalog* Catalog = NewObject<UHSRInventoryCatalog>(GameInstance);
	const FName ConsumableItem(TEXT("Item.Consumable.Command"));
	RegisterItem(Inventory, GameInstance, ConsumableItem, EHSRItemStorageKind::Stackable, 20);
	AddCatalogEntry(Catalog, ConsumableItem, EHSRInventoryCategory::Consumable,
		TEXT("Command Consumable"), 1);
	TestEqual(TEXT("Command consumable enters bag"), Inventory->AddStack(ConsumableItem, 3),
		EHSRInventoryOperationResult::Success);

	UHSRInventoryViewModel* ViewModel = NewObject<UHSRInventoryViewModel>(GameInstance);
	ViewModel->Initialize(Inventory, Catalog);
	TestEqual(TEXT("Consumable category selection succeeds"),
		ViewModel->SelectCategory(EHSRInventoryCategory::Consumable),
		EHSRInventoryViewModelResult::Success);
	FHSRInventoryModuleSnapshot Before;
	TestTrue(TEXT("Unsupported command snapshot is available"), ViewModel->GetSnapshot(Before));
	TestEqual(TEXT("Consumable selection succeeds"), ViewModel->SelectEntry(Before.Entries[0].Key),
		EHSRInventoryViewModelResult::Success);
	TestTrue(TEXT("Selected unsupported command snapshot is available"), ViewModel->GetSnapshot(Before));

	for (const TPair<EHSRInventoryAction, int32>& Command : {
		TPair<EHSRInventoryAction, int32>(EHSRInventoryAction::Use, INDEX_NONE),
		TPair<EHSRInventoryAction, int32>(EHSRInventoryAction::Disassemble, INDEX_NONE),
		TPair<EHSRInventoryAction, int32>(EHSRInventoryAction::Enhance, 1)})
	{
		TestEqual(TEXT("Unsupported command returns typed unavailable"),
			ViewModel->SubmitAction(Command.Key, Command.Value),
			EHSRInventoryViewModelResult::AuthorityUnavailable);
		FHSRInventoryModuleSnapshot After;
		TestTrue(TEXT("Unsupported command keeps snapshot readable"), ViewModel->GetSnapshot(After));
		TestTrue(TEXT("Unsupported command preserves complete snapshot"), SameSnapshot(Before, After));
	}
	ViewModel->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSRInventoryFrontendDynamicRouteTest,
	"HSR.UI.Inventory.Frontend.DynamicRoute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRInventoryFrontendDynamicRouteTest::RunTest(const FString&)
{
	using namespace HSR::P17::InventoryTests;
	UHSRUIManagerSubsystem* Manager = MakeUIManager();
	Manager->ConfigureAutomationBackend(true, true, true, true, true);
	Manager->ConfigureAutomationInventoryModuleBackend(true, true, true);
	Manager->RegisterHostForAutomation();

	TestEqual(TEXT("P17 Inventory opens through the shared frontend route"),
		Manager->OpenFrontendModule(EHSRFrontendModule::Inventory), EHSRUIScreenResult::Success);
	TestTrue(TEXT("P17 Inventory reports an owned open module"), Manager->HasOpenInventoryScreen());
	TestEqual(TEXT("P17 Inventory owns one dynamic module content widget"),
		Manager->GetFrontendModuleContentCountForAutomation(), 1);
	TestEqual(TEXT("P17 Inventory dynamic content reports the Inventory route"),
		Manager->GetFrontendModuleContentModuleForAutomation(), EHSRFrontendModule::Inventory);

	TestEqual(TEXT("Back returns from P17 Inventory to the Pause Hub"),
		Manager->RequestBack(), EHSRUIScreenResult::Success);
	TestFalse(TEXT("Back releases P17 Inventory ownership"), Manager->HasOpenInventoryScreen());
	TestEqual(TEXT("Back clears dynamic Inventory content"),
		Manager->GetFrontendModuleContentCountForAutomation(), 0);
	TestEqual(TEXT("X closes the shared frontend shell"),
		Manager->CloseFrontendToRoot(), EHSRUIScreenResult::Success);
	TestFalse(TEXT("X releases the frontend shell pause"), Manager->IsPausedForAutomation());

	Manager->DeinitializeForAutomation();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSRInventoryFrontendDynamicTravelRestoreTest,
	"HSR.UI.Inventory.Frontend.DynamicTravelRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRInventoryFrontendDynamicTravelRestoreTest::RunTest(const FString&)
{
	using namespace HSR::P17::InventoryTests;
	UHSRUIManagerSubsystem* Manager = MakeUIManager();
	Manager->ConfigureAutomationBackend(true, true, true, true, true);
	Manager->ConfigureAutomationInventoryModuleBackend(true, true, true);
	Manager->RegisterHostForAutomation();
	TestEqual(TEXT("Travel fixture opens P17 Inventory"),
		Manager->OpenFrontendModule(EHSRFrontendModule::Inventory), EHSRUIScreenResult::Success);
	TestEqual(TEXT("Travel teardown captures the dynamic Inventory route"),
		Manager->TeardownHostIdentityForTravelForAutomation(1), EHSRUIScreenResult::Success);
	TestTrue(TEXT("Travel descriptor waits for arrival"),
		Manager->HasPendingTravelRestoreForAutomation());
	TestEqual(TEXT("A fresh host registers after travel"),
		Manager->RegisterHostIdentityForAutomation(2), EHSRUIScreenResult::Success);
	Manager->NotifyArrivalCommittedForAutomation(1, TEXT("Map.B"));
	TestFalse(TEXT("Arrival consumes the dynamic Inventory descriptor"),
		Manager->HasPendingTravelRestoreForAutomation());
	TestTrue(TEXT("Arrival restores the P17 Inventory ownership pair"),
		Manager->HasOpenInventoryScreen());
	TestEqual(TEXT("Arrival restores one dynamic Inventory content widget"),
		Manager->GetFrontendModuleContentCountForAutomation(), 1);
	TestEqual(TEXT("Duplicate arrival does not add another dynamic widget"),
		Manager->GetFrontendModuleContentCountForAutomation(), 1);

	Manager->DeinitializeForAutomation();
	return true;
}

#endif
