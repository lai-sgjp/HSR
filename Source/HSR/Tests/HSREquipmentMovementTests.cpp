#if WITH_DEV_AUTOMATION_TESTS

#include "../Data/Definitions/HSRItemEquipmentMappingCatalog.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Data/Definitions/HSREquipmentDefinition.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSRItemEquipmentMappingContractTest,
	"HSR.Equipment.Movement.MappingContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRItemEquipmentMappingContractTest::RunTest(const FString& Parameters)
{
	UHSRItemEquipmentMappingCatalog* Catalog = NewObject<UHSRItemEquipmentMappingCatalog>();
	FHSRItemEquipmentMappingEntry Entry;
	Entry.ItemId = TEXT("Item.Equipment.Test");
	Entry.EquipmentDefinitionId = TEXT("Equipment.Test");
	Entry.Kind = EHSREquipmentKind::Equipment;
	Entry.Slot = static_cast<int32>(EHSREquipmentSlot::Weapon);
	TestTrue(TEXT("Valid mapping accepted"), Catalog->AddMapping(Entry));
	TestFalse(TEXT("Duplicate ItemId rejected"), Catalog->AddMapping(Entry));
	TestTrue(TEXT("Mapping resolves by explicit ItemId"), Catalog->Resolve(TEXT("Item.Equipment.Test"), Entry));
	TestEqual(TEXT("Resolved target is explicit"), Entry.EquipmentDefinitionId, FName(TEXT("Equipment.Test")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSREquipmentMovementBagToEquipTest,
	"HSR.Equipment.Movement.Transaction.BagToEquip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSREquipmentMovementBagToEquipTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UHSRInventorySubsystem* Inventory = NewObject<UHSRInventorySubsystem>(GameInstance);
	UHSREquipmentSubsystem* Equipment = NewObject<UHSREquipmentSubsystem>(GameInstance);
	UHSRItemEquipmentMappingCatalog* Catalog = NewObject<UHSRItemEquipmentMappingCatalog>();

	UHSRItemDefinition* ItemDefinition = NewObject<UHSRItemDefinition>();
	ItemDefinition->ItemId = TEXT("Item.Equipment.Test");
	ItemDefinition->StorageKind = EHSRItemStorageKind::Unique;
	ItemDefinition->MaxStack = 1;
	TestEqual(TEXT("Inventory definition registered"), Inventory->RegisterDefinition(*ItemDefinition), EHSRInventoryOperationResult::Success);

	UHSREquipmentDefinition* EquipmentDefinition = NewObject<UHSREquipmentDefinition>();
	EquipmentDefinition->DefinitionId = TEXT("Equipment.Test");
	EquipmentDefinition->Slot = EHSREquipmentSlot::Weapon;
	EquipmentDefinition->EnhancementCap = 10;
	TestEqual(TEXT("Equipment definition registered"), Equipment->RegisterDefinition(*EquipmentDefinition), EHSREquipmentOperationResult::Success);

	FHSRItemEquipmentMappingEntry Mapping;
	Mapping.ItemId = ItemDefinition->ItemId;
	Mapping.EquipmentDefinitionId = EquipmentDefinition->DefinitionId;
	Mapping.Kind = EHSREquipmentKind::Equipment;
	Mapping.Slot = static_cast<int32>(EHSREquipmentSlot::Weapon);
	TestTrue(TEXT("Mapping registered"), Catalog->AddMapping(Mapping));

	const FGuid CharacterId = FGuid::NewGuid();
	const FGuid InstanceId = FGuid::NewGuid();
	FHSRItemInstance InventoryInstance;
	InventoryInstance.InstanceId = InstanceId;
	InventoryInstance.DefinitionId = ItemDefinition->ItemId;
	TestEqual(TEXT("Bag membership seeded"), Inventory->AddUnique(InventoryInstance), EHSRInventoryOperationResult::Success);

	FHSREquipmentInstance RegistryInstance;
	RegistryInstance.InstanceId = InstanceId;
	RegistryInstance.DefinitionId = EquipmentDefinition->DefinitionId;
	RegistryInstance.Kind = EHSREquipmentKind::Equipment;
	RegistryInstance.EnhancementLevel = 3;
	TestEqual(TEXT("Registry payload seeded"), Equipment->RegisterInstance(RegistryInstance), EHSREquipmentOperationResult::Success);

	FHSRInventorySnapshot InventoryBefore;
	Inventory->GetSnapshot(InventoryBefore);
	FHSREquipmentMovementRequest Request;
	Request.OperationId = FGuid::NewGuid();
	Request.CharacterId = CharacterId;
	Request.InstanceId = InstanceId;
	Request.Intent = EHSREquipmentMovementIntent::Equip;
	Request.Kind = EHSREquipmentKind::Equipment;
	Request.Slot = static_cast<int32>(EHSREquipmentSlot::Weapon);
	Request.ExpectedInventoryRevision = InventoryBefore.Revision;
	Request.ExpectedEquipmentRevision = 0;

	const FHSREquipmentMovementResult Result = Equipment->ExecuteMovement(Request, *Inventory, *Catalog);
	TestEqual(TEXT("Aggregate committed"), Result.Code, EHSREquipmentMovementResultCode::Success);
	TestTrue(TEXT("Result marks commit"), Result.bCommitted);
	TestEqual(TEXT("Inventory revision advanced once"), Result.NewInventoryRevision, InventoryBefore.Revision + 1);
	TestEqual(TEXT("Equipment revision advanced once"), Result.NewEquipmentRevision, 1);

	FHSRInventorySnapshot InventoryAfter;
	Inventory->GetSnapshot(InventoryAfter);
	TestEqual(TEXT("Bag membership removed"), InventoryAfter.UniqueItems.Num(), 0);
	FHSREquipmentLoadout Loadout;
	int32 EquipmentRevision = 0;
	TestTrue(TEXT("Loadout resolves"), Equipment->GetLoadout(CharacterId, Loadout, EquipmentRevision));
	TestTrue(TEXT("Instance placed"), Loadout.Equipment.Contains(EHSREquipmentSlot::Weapon));
	FHSREquipmentInstance ResolvedRegistry;
	TestTrue(TEXT("Registry payload retained"), Equipment->FindRegisteredInstance(InstanceId, ResolvedRegistry));
	TestEqual(TEXT("Enhancement retained"), ResolvedRegistry.EnhancementLevel, 3);

	int32 InventoryEvents = 0;
	int32 EquipmentEvents = 0;
	Inventory->OnInventoryChanged().AddLambda([&InventoryEvents](int64) { ++InventoryEvents; });
	Equipment->OnLoadoutChanged().AddLambda([&EquipmentEvents](const FGuid&, int32) { ++EquipmentEvents; });
	const FHSREquipmentMovementResult Replay = Equipment->ExecuteMovement(Request, *Inventory, *Catalog);
	TestEqual(TEXT("Replay returns cached success"), Replay.Code, EHSREquipmentMovementResultCode::Success);
	TestTrue(TEXT("Replay is identified"), Replay.bReplay);
	TestFalse(TEXT("Replay does not commit again"), Replay.bCommitted);
	Inventory->GetSnapshot(InventoryAfter);
	Equipment->GetLoadout(CharacterId, Loadout, EquipmentRevision);
	TestEqual(TEXT("Replay inventory revision stable"), InventoryAfter.Revision, Result.NewInventoryRevision);
	TestEqual(TEXT("Replay equipment revision stable"), EquipmentRevision, Result.NewEquipmentRevision);
	TestEqual(TEXT("Replay inventory publication count"), InventoryEvents, 0);
	TestEqual(TEXT("Replay equipment publication count"), EquipmentEvents, 0);

	FHSREquipmentMovementRequest ConflictingRequest = Request;
	ConflictingRequest.Slot = static_cast<int32>(EHSREquipmentSlot::Head);
	const FHSREquipmentMovementResult Conflict = Equipment->ExecuteMovement(ConflictingRequest, *Inventory, *Catalog);
	TestEqual(TEXT("Changed request under same OperationId rejected"), Conflict.Code, EHSREquipmentMovementResultCode::OperationIdConflict);
	TestFalse(TEXT("Conflict does not commit"), Conflict.bCommitted);
	return true;
}

#endif
