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

	FHSREquipmentMovementRequest UnequipRequest;
	UnequipRequest.OperationId = FGuid::NewGuid();
	UnequipRequest.CharacterId = CharacterId;
	UnequipRequest.InstanceId = InstanceId;
	UnequipRequest.Intent = EHSREquipmentMovementIntent::Unequip;
	UnequipRequest.Kind = EHSREquipmentKind::Equipment;
	UnequipRequest.Slot = static_cast<int32>(EHSREquipmentSlot::Weapon);
	UnequipRequest.ExpectedInventoryRevision = Result.NewInventoryRevision;
	UnequipRequest.ExpectedEquipmentRevision = Result.NewEquipmentRevision;
	const FHSREquipmentMovementResult UnequipResult = Equipment->ExecuteMovement(UnequipRequest, *Inventory, *Catalog);
	TestEqual(TEXT("Unequip aggregate committed"), UnequipResult.Code, EHSREquipmentMovementResultCode::Success);
	TestTrue(TEXT("Unequip marks commit"), UnequipResult.bCommitted);
	Inventory->GetSnapshot(InventoryAfter);
	TestEqual(TEXT("Unequip returns one bag membership"), InventoryAfter.UniqueItems.Num(), 1);
	if (InventoryAfter.UniqueItems.Num() == 1)
	{
		TestEqual(TEXT("Unequip returns same InstanceId"), InventoryAfter.UniqueItems[0].InstanceId, InstanceId);
		TestEqual(TEXT("Unequip returns mapped ItemId"), InventoryAfter.UniqueItems[0].DefinitionId, ItemDefinition->ItemId);
	}
	Equipment->GetLoadout(CharacterId, Loadout, EquipmentRevision);
	TestEqual(TEXT("Unequip clears weapon placement"), Loadout.Equipment.Num(), 0);
	TestEqual(TEXT("Unequip inventory revision advances once"), InventoryAfter.Revision, Result.NewInventoryRevision + 1);
	TestEqual(TEXT("Unequip equipment revision advances once"), EquipmentRevision, Result.NewEquipmentRevision + 1);
	TestTrue(TEXT("Unequip retains Registry payload"), Equipment->FindRegisteredInstance(InstanceId, ResolvedRegistry));
	TestEqual(TEXT("Unequip retains enhancement"), ResolvedRegistry.EnhancementLevel, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSREquipmentMovementReplaceTest,
	"HSR.Equipment.Movement.Transaction.ReplaceNetCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSREquipmentMovementReplaceTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UHSRInventorySubsystem* Inventory = NewObject<UHSRInventorySubsystem>(GameInstance);
	UHSREquipmentSubsystem* Equipment = NewObject<UHSREquipmentSubsystem>(GameInstance);
	UHSRItemEquipmentMappingCatalog* Catalog = NewObject<UHSRItemEquipmentMappingCatalog>();
	TestTrue(TEXT("Capacity one configured"), Inventory->SetCapacityForAutomation(1));

	auto RegisterPair = [this, Inventory, Equipment, Catalog](FName ItemId, FName EquipmentId)
	{
		UHSRItemDefinition* Item = NewObject<UHSRItemDefinition>();
		Item->ItemId = ItemId; Item->StorageKind = EHSRItemStorageKind::Unique; Item->MaxStack = 1;
		TestEqual(TEXT("Inventory definition registered"), Inventory->RegisterDefinition(*Item), EHSRInventoryOperationResult::Success);
		UHSREquipmentDefinition* Definition = NewObject<UHSREquipmentDefinition>();
		Definition->DefinitionId = EquipmentId; Definition->Slot = EHSREquipmentSlot::Weapon; Definition->EnhancementCap = 10;
		TestEqual(TEXT("Equipment definition registered"), Equipment->RegisterDefinition(*Definition), EHSREquipmentOperationResult::Success);
		FHSRItemEquipmentMappingEntry Mapping;
		Mapping.ItemId = ItemId; Mapping.EquipmentDefinitionId = EquipmentId; Mapping.Kind = EHSREquipmentKind::Equipment; Mapping.Slot = 0;
		TestTrue(TEXT("Mapping registered"), Catalog->AddMapping(Mapping));
	};
	RegisterPair(TEXT("Item.Weapon.Old"), TEXT("Equipment.Weapon.Old"));
	RegisterPair(TEXT("Item.Weapon.New"), TEXT("Equipment.Weapon.New"));

	const FGuid CharacterId = FGuid::NewGuid();
	const FGuid OldId = FGuid::NewGuid();
	const FGuid NewId = FGuid::NewGuid();
	FHSREquipmentInstance OldPayload; OldPayload.InstanceId = OldId; OldPayload.DefinitionId = TEXT("Equipment.Weapon.Old"); OldPayload.EnhancementLevel = 4;
	FHSREquipmentInstance NewPayload; NewPayload.InstanceId = NewId; NewPayload.DefinitionId = TEXT("Equipment.Weapon.New"); NewPayload.EnhancementLevel = 7;
	TestEqual(TEXT("Old Registry payload"), Equipment->RegisterInstance(OldPayload), EHSREquipmentOperationResult::Success);
	TestEqual(TEXT("New Registry payload"), Equipment->RegisterInstance(NewPayload), EHSREquipmentOperationResult::Success);
	FHSRItemInstance OldMembership{OldId, TEXT("Item.Weapon.Old")};
	TestEqual(TEXT("Old membership seeded"), Inventory->AddUnique(OldMembership), EHSRInventoryOperationResult::Success);
	FHSRInventorySnapshot Snapshot; Inventory->GetSnapshot(Snapshot);
	FHSREquipmentMovementRequest EquipRequest;
	EquipRequest.OperationId=FGuid::NewGuid();EquipRequest.CharacterId=CharacterId;EquipRequest.InstanceId=OldId;EquipRequest.Intent=EHSREquipmentMovementIntent::Equip;EquipRequest.Kind=EHSREquipmentKind::Equipment;EquipRequest.Slot=0;EquipRequest.ExpectedInventoryRevision=Snapshot.Revision;EquipRequest.ExpectedEquipmentRevision=0;
	const FHSREquipmentMovementResult EquipResult=Equipment->ExecuteMovement(EquipRequest,*Inventory,*Catalog);
	TestEqual(TEXT("Old equipped"),EquipResult.Code,EHSREquipmentMovementResultCode::Success);
	FHSRItemInstance NewMembership{NewId, TEXT("Item.Weapon.New")};
	TestEqual(TEXT("New membership fills capacity"), Inventory->AddUnique(NewMembership), EHSRInventoryOperationResult::Success);
	Inventory->GetSnapshot(Snapshot);
	FHSREquipmentMovementRequest ReplaceRequest;
	ReplaceRequest.OperationId=FGuid::NewGuid();ReplaceRequest.CharacterId=CharacterId;ReplaceRequest.InstanceId=NewId;ReplaceRequest.Intent=EHSREquipmentMovementIntent::Replace;ReplaceRequest.Kind=EHSREquipmentKind::Equipment;ReplaceRequest.Slot=0;ReplaceRequest.ExpectedInventoryRevision=Snapshot.Revision;ReplaceRequest.ExpectedEquipmentRevision=EquipResult.NewEquipmentRevision;

	UHSRItemEquipmentMappingCatalog* ConflictingCatalog = NewObject<UHSRItemEquipmentMappingCatalog>();
	FHSRItemEquipmentMappingEntry IncomingMapping;
	IncomingMapping.ItemId = TEXT("Item.Weapon.New"); IncomingMapping.EquipmentDefinitionId = TEXT("Equipment.Weapon.New");
	IncomingMapping.Kind = EHSREquipmentKind::Equipment; IncomingMapping.Slot = 0;
	TestTrue(TEXT("Incoming mapping registered in conflicting catalog"), ConflictingCatalog->AddMapping(IncomingMapping));
	FHSRItemEquipmentMappingEntry DisplacedMapping;
	DisplacedMapping.ItemId = TEXT("Item.Weapon.Old"); DisplacedMapping.EquipmentDefinitionId = TEXT("Equipment.Weapon.Old");
	DisplacedMapping.Kind = EHSREquipmentKind::Equipment; DisplacedMapping.Slot = static_cast<int32>(EHSREquipmentSlot::Head);
	TestTrue(TEXT("Conflicting displaced mapping registered"), ConflictingCatalog->AddMapping(DisplacedMapping));
	int32 InventoryEvents = 0;
	int32 EquipmentEvents = 0;
	Inventory->OnInventoryChanged().AddLambda([&InventoryEvents](int64) { ++InventoryEvents; });
	Equipment->OnLoadoutChanged().AddLambda([&EquipmentEvents](const FGuid&, int32) { ++EquipmentEvents; });
	const FHSREquipmentMovementResult RejectedReplace = Equipment->ExecuteMovement(ReplaceRequest, *Inventory, *ConflictingCatalog);
	TestEqual(TEXT("Replace rejects incompatible displaced mapping"), RejectedReplace.Code, EHSREquipmentMovementResultCode::MappingRejected);
	TestFalse(TEXT("Rejected replace does not commit"), RejectedReplace.bCommitted);
	FHSRInventorySnapshot RejectedSnapshot; Inventory->GetSnapshot(RejectedSnapshot);
	TestEqual(TEXT("Rejected replace keeps Inventory revision"), RejectedSnapshot.Revision, Snapshot.Revision);
	TestEqual(TEXT("Rejected replace keeps Inventory membership"), RejectedSnapshot.UniqueItems.Num(), Snapshot.UniqueItems.Num());
	FHSREquipmentLoadout RejectedLoadout; int32 RejectedEquipmentRevision = 0;
	TestTrue(TEXT("Rejected replace keeps loadout resolvable"), Equipment->GetLoadout(CharacterId, RejectedLoadout, RejectedEquipmentRevision));
	TestEqual(TEXT("Rejected replace keeps Equipment revision"), RejectedEquipmentRevision, EquipResult.NewEquipmentRevision);
	TestEqual(TEXT("Rejected replace keeps old placement"), RejectedLoadout.Equipment.FindChecked(EHSREquipmentSlot::Weapon).InstanceId, OldId);
	TestEqual(TEXT("Rejected replace publishes no Inventory delegate"), InventoryEvents, 0);
	TestEqual(TEXT("Rejected replace publishes no Equipment delegate"), EquipmentEvents, 0);

	const FHSREquipmentMovementResult ReplaceResult=Equipment->ExecuteMovement(ReplaceRequest,*Inventory,*Catalog);
	TestEqual(TEXT("Replace commits at net capacity"),ReplaceResult.Code,EHSREquipmentMovementResultCode::Success);
	TestTrue(TEXT("Replace committed"),ReplaceResult.bCommitted);
	TestEqual(TEXT("Displaced InstanceId returned"),ReplaceResult.DisplacedInstanceId,OldId);
	Inventory->GetSnapshot(Snapshot);
	TestEqual(TEXT("Bag remains at one slot"),Snapshot.UniqueItems.Num(),1);
	if(Snapshot.UniqueItems.Num()==1){TestEqual(TEXT("Old membership returned"),Snapshot.UniqueItems[0].InstanceId,OldId);TestEqual(TEXT("Old ItemId returned"),Snapshot.UniqueItems[0].DefinitionId,FName(TEXT("Item.Weapon.Old")));}
	FHSREquipmentLoadout Loadout;int32 Revision=0;TestTrue(TEXT("Loadout resolves after replace"),Equipment->GetLoadout(CharacterId,Loadout,Revision));
	TestEqual(TEXT("New instance placed"),Loadout.Equipment.FindChecked(EHSREquipmentSlot::Weapon).InstanceId,NewId);
	TestEqual(TEXT("Inventory revision advances once"),Snapshot.Revision,ReplaceRequest.ExpectedInventoryRevision+1);
	TestEqual(TEXT("Equipment revision advances once"),Revision,ReplaceRequest.ExpectedEquipmentRevision+1);
	FHSREquipmentInstance Resolved;TestTrue(TEXT("Old Registry retained"),Equipment->FindRegisteredInstance(OldId,Resolved));TestEqual(TEXT("Old enhancement retained"),Resolved.EnhancementLevel,4);
	TestTrue(TEXT("New Registry retained"),Equipment->FindRegisteredInstance(NewId,Resolved));TestEqual(TEXT("New enhancement retained"),Resolved.EnhancementLevel,7);
	return true;
}

#endif
