#if WITH_DEV_AUTOMATION_TESTS

#include "../Data/Definitions/HSRItemEquipmentMappingCatalog.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Data/Definitions/HSREquipmentDefinition.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Save/HSRSaveTypes.h"
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
	FHSRItemEquipmentMappingEntry ConflictingEntry = Entry;
	ConflictingEntry.EquipmentDefinitionId = TEXT("Equipment.Conflicting");
	Catalog->Mappings.Add(ConflictingEntry);
	TestFalse(TEXT("Authored duplicate ItemId is rejected at resolve"), Catalog->Resolve(Entry.ItemId, ConflictingEntry));
	TestFalse(TEXT("Authored duplicate Equipment definition is rejected at reverse resolve"),
		Catalog->ResolveEquipmentDefinition(Entry.EquipmentDefinitionId, ConflictingEntry));
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

	InventoryEvents = 0;
	EquipmentEvents = 0;
	TArray<FName> PublicationOrder;
	bool bCandidateResolved = false;
	bool bPendingCaptureObservedOldAuthority = false;
	Inventory->OnInventoryChanged().AddLambda([&PublicationOrder](int64) { PublicationOrder.Add(TEXT("Inventory")); });
	Equipment->OnLoadoutChanged().AddLambda([&PublicationOrder](const FGuid&, int32) { PublicationOrder.Add(TEXT("Equipment")); });
	Equipment->SetMovementProjection(
		UHSREquipmentSubsystem::FMovementProjectionPreflight::CreateLambda(
			[Inventory, Equipment, &bCandidateResolved, &bPendingCaptureObservedOldAuthority]
			(const FHSREquipmentMovementRequest&, const FHSREquipmentLoadout& CandidateLoadout)
			{
				bCandidateResolved = CandidateLoadout.Equipment.Contains(EHSREquipmentSlot::Weapon);
				FHSRInventorySnapshot PendingInventory;
				Inventory->GetSnapshot(PendingInventory);
				TArray<FHSREquipmentRegistryDto> PendingRegistry;
				TArray<FHSREquipmentPlacementDto> PendingPlacements;
				Equipment->ExportSaveData(PendingRegistry, PendingPlacements);
				bPendingCaptureObservedOldAuthority = PendingInventory.UniqueItems.Num() == 1
					&& PendingRegistry.Num() == 1 && PendingPlacements.IsEmpty();
				return false;
			}),
		UHSREquipmentSubsystem::FMovementProjectionCommit::CreateLambda(
			[&PublicationOrder](const FHSREquipmentMovementRequest&, const FHSREquipmentLoadout&) { PublicationOrder.Add(TEXT("Projection")); }));
	FHSREquipmentMovementRequest ProjectionRequest = Request;
	ProjectionRequest.OperationId = FGuid::NewGuid();
	ProjectionRequest.ExpectedInventoryRevision = UnequipResult.NewInventoryRevision;
	ProjectionRequest.ExpectedEquipmentRevision = UnequipResult.NewEquipmentRevision;
	const FHSREquipmentMovementResult ProjectionRejected = Equipment->ExecuteMovement(ProjectionRequest, *Inventory, *Catalog);
	TestEqual(TEXT("Projection preflight rejection is typed"), ProjectionRejected.Code, EHSREquipmentMovementResultCode::ProjectionRejected);
	TestFalse(TEXT("Projection rejection does not commit"), ProjectionRejected.bCommitted);
	Inventory->GetSnapshot(InventoryAfter);
	Equipment->GetLoadout(CharacterId, Loadout, EquipmentRevision);
	TestEqual(TEXT("Projection rejection keeps Inventory revision"), InventoryAfter.Revision, UnequipResult.NewInventoryRevision);
	TestEqual(TEXT("Projection rejection keeps Equipment revision"), EquipmentRevision, UnequipResult.NewEquipmentRevision);
	TestEqual(TEXT("Projection rejection keeps bag membership"), InventoryAfter.UniqueItems.Num(), 1);
	TestEqual(TEXT("Projection rejection keeps placement empty"), Loadout.Equipment.Num(), 0);
	TestEqual(TEXT("Projection rejection publishes no Inventory delegate"), InventoryEvents, 0);
	TestEqual(TEXT("Projection rejection publishes no Equipment delegate"), EquipmentEvents, 0);
	TestEqual(TEXT("Projection rejection publishes no callback"), PublicationOrder.Num(), 0);
	TestTrue(TEXT("Projection preflight receives resolved candidate"), bCandidateResolved);
	TestTrue(TEXT("Save capture during pending candidate sees old authority"), bPendingCaptureObservedOldAuthority);

	Equipment->SetMovementProjection(
		UHSREquipmentSubsystem::FMovementProjectionPreflight::CreateLambda(
			[](const FHSREquipmentMovementRequest&, const FHSREquipmentLoadout&) { return true; }),
		UHSREquipmentSubsystem::FMovementProjectionCommit::CreateLambda(
			[&PublicationOrder](const FHSREquipmentMovementRequest&, const FHSREquipmentLoadout&) { PublicationOrder.Add(TEXT("Projection")); }));
	const FHSREquipmentMovementResult ProjectionCommitted = Equipment->ExecuteMovement(ProjectionRequest, *Inventory, *Catalog);
	TestEqual(TEXT("Projection-ready retry commits"), ProjectionCommitted.Code, EHSREquipmentMovementResultCode::Success);
	TestTrue(TEXT("Projection-ready retry marks commit"), ProjectionCommitted.bCommitted);
	TestEqual(TEXT("Projection-ready retry publishes Inventory once"), InventoryEvents, 1);
	TestEqual(TEXT("Projection-ready retry publishes Equipment once"), EquipmentEvents, 1);
	TestEqual(TEXT("Projection-ready publication count"), PublicationOrder.Num(), 3);
	if (PublicationOrder.Num() == 3)
	{
		TestEqual(TEXT("Inventory publishes first"), PublicationOrder[0], FName(TEXT("Inventory")));
		TestEqual(TEXT("Equipment publishes second"), PublicationOrder[1], FName(TEXT("Equipment")));
		TestEqual(TEXT("Projection publishes last"), PublicationOrder[2], FName(TEXT("Projection")));
	}
	TArray<FHSREquipmentRegistryDto> SavedRegistry;
	TArray<FHSREquipmentPlacementDto> SavedPlacements;
	Equipment->ExportSaveData(SavedRegistry, SavedPlacements);
	FHSREquipmentRegistryRestoreState RestoreCandidate;
	TestTrue(TEXT("Committed movement state prepares for restore"),
		Equipment->PrepareRestore(SavedRegistry, SavedPlacements, RestoreCandidate));
	Equipment->CommitRestore(RestoreCandidate);
	const FHSREquipmentMovementResult PostRestoreRequest = Equipment->ExecuteMovement(ProjectionRequest, *Inventory, *Catalog);
	TestEqual(TEXT("Restore clears transient ledger and revalidates authority"),
		PostRestoreRequest.Code, EHSREquipmentMovementResultCode::InventoryRevisionConflict);
	TestFalse(TEXT("Post-restore request is not historical replay"), PostRestoreRequest.bReplay);
	TestFalse(TEXT("Post-restore rejected request does not commit"), PostRestoreRequest.bCommitted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSREquipmentMovementDomainFailureMatrixTest,
	"HSR.Equipment.Movement.FailureMatrix.DomainPreflight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSREquipmentMovementDomainFailureMatrixTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UHSRInventorySubsystem* Inventory = NewObject<UHSRInventorySubsystem>(GameInstance);
	UHSREquipmentSubsystem* Equipment = NewObject<UHSREquipmentSubsystem>(GameInstance);
	UHSRItemEquipmentMappingCatalog* Catalog = NewObject<UHSRItemEquipmentMappingCatalog>();

	auto RegisterPair = [this, Inventory, Equipment, Catalog](const FName ItemId, const FName EquipmentId)
	{
		UHSRItemDefinition* Item = NewObject<UHSRItemDefinition>();
		Item->ItemId = ItemId; Item->StorageKind = EHSRItemStorageKind::Unique; Item->MaxStack = 1;
		TestEqual(TEXT("Failure fixture Inventory definition"), Inventory->RegisterDefinition(*Item), EHSRInventoryOperationResult::Success);
		UHSREquipmentDefinition* Definition = NewObject<UHSREquipmentDefinition>();
		Definition->DefinitionId = EquipmentId; Definition->Slot = EHSREquipmentSlot::Weapon; Definition->EnhancementCap = 10;
		TestEqual(TEXT("Failure fixture Equipment definition"), Equipment->RegisterDefinition(*Definition), EHSREquipmentOperationResult::Success);
		FHSRItemEquipmentMappingEntry Mapping;
		Mapping.ItemId = ItemId; Mapping.EquipmentDefinitionId = EquipmentId;
		Mapping.Kind = EHSREquipmentKind::Equipment; Mapping.Slot = static_cast<int32>(EHSREquipmentSlot::Weapon);
		TestTrue(TEXT("Failure fixture mapping"), Catalog->AddMapping(Mapping));
	};
	RegisterPair(TEXT("Item.Weapon.Primary"), TEXT("Equipment.Weapon.Primary"));
	RegisterPair(TEXT("Item.Weapon.Other"), TEXT("Equipment.Weapon.Other"));

	const FGuid CharacterId = FGuid::NewGuid();
	const FGuid EquippedId = FGuid::NewGuid();
	FHSREquipmentInstance EquippedPayload;
	EquippedPayload.InstanceId = EquippedId; EquippedPayload.DefinitionId = TEXT("Equipment.Weapon.Primary");
	EquippedPayload.Kind = EHSREquipmentKind::Equipment; EquippedPayload.EnhancementLevel = 6;
	TestEqual(TEXT("Failure fixture Registry payload"), Equipment->RegisterInstance(EquippedPayload), EHSREquipmentOperationResult::Success);
	TestEqual(TEXT("Failure fixture bag membership"), Inventory->AddUnique({EquippedId, TEXT("Item.Weapon.Primary")}), EHSRInventoryOperationResult::Success);
	FHSRInventorySnapshot Snapshot;
	Inventory->GetSnapshot(Snapshot);
	FHSREquipmentMovementRequest EquipRequest;
	EquipRequest.OperationId = FGuid::NewGuid(); EquipRequest.CharacterId = CharacterId; EquipRequest.InstanceId = EquippedId;
	EquipRequest.Intent = EHSREquipmentMovementIntent::Equip; EquipRequest.Kind = EHSREquipmentKind::Equipment;
	EquipRequest.Slot = static_cast<int32>(EHSREquipmentSlot::Weapon);
	EquipRequest.ExpectedInventoryRevision = Snapshot.Revision; EquipRequest.ExpectedEquipmentRevision = 0;
	const FHSREquipmentMovementResult EquipResult = Equipment->ExecuteMovement(EquipRequest, *Inventory, *Catalog);
	TestEqual(TEXT("Failure fixture equipped"), EquipResult.Code, EHSREquipmentMovementResultCode::Success);

	const FGuid CollisionId = FGuid::NewGuid();
	FHSREquipmentInstance CollisionPayload;
	CollisionPayload.InstanceId = CollisionId; CollisionPayload.DefinitionId = TEXT("Equipment.Weapon.Other");
	CollisionPayload.Kind = EHSREquipmentKind::Equipment; CollisionPayload.EnhancementLevel = 2;
	TestEqual(TEXT("Collision Registry payload"), Equipment->RegisterInstance(CollisionPayload), EHSREquipmentOperationResult::Success);
	TestEqual(TEXT("Collision membership intentionally disagrees with Registry"),
		Inventory->AddUnique({CollisionId, TEXT("Item.Weapon.Primary")}), EHSRInventoryOperationResult::Success);

	int32 InventoryEvents = 0;
	int32 EquipmentEvents = 0;
	Inventory->OnInventoryChanged().AddLambda([&InventoryEvents](int64) { ++InventoryEvents; });
	Equipment->OnLoadoutChanged().AddLambda([&EquipmentEvents](const FGuid&, int32) { ++EquipmentEvents; });

	auto MakeUnequip = [CharacterId, EquippedId](const int64 InventoryRevision, const int32 EquipmentRevision)
	{
		FHSREquipmentMovementRequest Request;
		Request.OperationId = FGuid::NewGuid(); Request.CharacterId = CharacterId; Request.InstanceId = EquippedId;
		Request.Intent = EHSREquipmentMovementIntent::Unequip; Request.Kind = EHSREquipmentKind::Equipment;
		Request.Slot = static_cast<int32>(EHSREquipmentSlot::Weapon);
		Request.ExpectedInventoryRevision = InventoryRevision; Request.ExpectedEquipmentRevision = EquipmentRevision;
		return Request;
	};
	auto VerifyUnchanged = [this, Inventory, Equipment, CharacterId, EquippedId, &InventoryEvents, &EquipmentEvents]
		(const TCHAR* Label, const int64 InventoryRevision, const int32 EquipmentRevision, const int32 MembershipCount)
	{
		FHSRInventorySnapshot CurrentInventory; Inventory->GetSnapshot(CurrentInventory);
		TestEqual(*FString::Printf(TEXT("%s Inventory revision stable"), Label), CurrentInventory.Revision, InventoryRevision);
		TestEqual(*FString::Printf(TEXT("%s membership stable"), Label), CurrentInventory.UniqueItems.Num(), MembershipCount);
		FHSREquipmentLoadout CurrentLoadout; int32 CurrentEquipmentRevision = 0;
		TestTrue(*FString::Printf(TEXT("%s loadout resolves"), Label), Equipment->GetLoadout(CharacterId, CurrentLoadout, CurrentEquipmentRevision));
		TestEqual(*FString::Printf(TEXT("%s Equipment revision stable"), Label), CurrentEquipmentRevision, EquipmentRevision);
		TestEqual(*FString::Printf(TEXT("%s placement stable"), Label),
			CurrentLoadout.Equipment.FindChecked(EHSREquipmentSlot::Weapon).InstanceId, EquippedId);
		FHSREquipmentInstance CurrentPayload;
		TestTrue(*FString::Printf(TEXT("%s Registry payload retained"), Label), Equipment->FindRegisteredInstance(EquippedId, CurrentPayload));
		TestEqual(*FString::Printf(TEXT("%s enhancement stable"), Label), CurrentPayload.EnhancementLevel, 6);
		TestEqual(*FString::Printf(TEXT("%s Inventory delegates zero"), Label), InventoryEvents, 0);
		TestEqual(*FString::Printf(TEXT("%s Equipment delegates zero"), Label), EquipmentEvents, 0);
	};

	Inventory->GetSnapshot(Snapshot);
	const int64 StableInventoryRevision = Snapshot.Revision;
	const int32 StableEquipmentRevision = EquipResult.NewEquipmentRevision;
	const int32 StableMembershipCount = Snapshot.UniqueItems.Num();

	FHSREquipmentMovementRequest Request = MakeUnequip(StableInventoryRevision - 1, StableEquipmentRevision);
	TestEqual(TEXT("Stale Inventory revision rejected"), Equipment->ExecuteMovement(Request, *Inventory, *Catalog).Code,
		EHSREquipmentMovementResultCode::InventoryRevisionConflict);
	VerifyUnchanged(TEXT("StaleInventory"), StableInventoryRevision, StableEquipmentRevision, StableMembershipCount);

	Request = MakeUnequip(StableInventoryRevision, StableEquipmentRevision - 1);
	TestEqual(TEXT("Stale Equipment revision rejected"), Equipment->ExecuteMovement(Request, *Inventory, *Catalog).Code,
		EHSREquipmentMovementResultCode::EquipmentRevisionConflict);
	VerifyUnchanged(TEXT("StaleEquipment"), StableInventoryRevision, StableEquipmentRevision, StableMembershipCount);

	Request = MakeUnequip(StableInventoryRevision - 1, StableEquipmentRevision - 1);
	TestEqual(TEXT("Mismatched revision pair rejected before candidates"), Equipment->ExecuteMovement(Request, *Inventory, *Catalog).Code,
		EHSREquipmentMovementResultCode::InventoryRevisionConflict);
	VerifyUnchanged(TEXT("RevisionPair"), StableInventoryRevision, StableEquipmentRevision, StableMembershipCount);

	Request = MakeUnequip(StableInventoryRevision, 0);
	Request.CharacterId = FGuid::NewGuid();
	TestEqual(TEXT("Foreign Character owner rejected"), Equipment->ExecuteMovement(Request, *Inventory, *Catalog).Code,
		EHSREquipmentMovementResultCode::EquipmentRejected);
	VerifyUnchanged(TEXT("ForeignOwner"), StableInventoryRevision, StableEquipmentRevision, StableMembershipCount);

	Request = MakeUnequip(StableInventoryRevision, StableEquipmentRevision);
	Request.Slot = static_cast<int32>(EHSREquipmentSlot::Head);
	TestEqual(TEXT("Wrong slot rejected"), Equipment->ExecuteMovement(Request, *Inventory, *Catalog).Code,
		EHSREquipmentMovementResultCode::MappingRejected);
	VerifyUnchanged(TEXT("WrongSlot"), StableInventoryRevision, StableEquipmentRevision, StableMembershipCount);

	Request = MakeUnequip(StableInventoryRevision, StableEquipmentRevision);
	Request.InstanceId = CollisionId;
	TestEqual(TEXT("Wrong expected placement rejected"), Equipment->ExecuteMovement(Request, *Inventory, *Catalog).Code,
		EHSREquipmentMovementResultCode::EquipmentRejected);
	VerifyUnchanged(TEXT("WrongPlacement"), StableInventoryRevision, StableEquipmentRevision, StableMembershipCount);

	Request = MakeUnequip(StableInventoryRevision, StableEquipmentRevision);
	Request.InstanceId = FGuid::NewGuid();
	TestEqual(TEXT("Missing Registry instance rejected"), Equipment->ExecuteMovement(Request, *Inventory, *Catalog).Code,
		EHSREquipmentMovementResultCode::EquipmentRejected);
	VerifyUnchanged(TEXT("MissingRegistry"), StableInventoryRevision, StableEquipmentRevision, StableMembershipCount);

	Request = MakeUnequip(StableInventoryRevision, StableEquipmentRevision);
	Request.Intent = static_cast<EHSREquipmentMovementIntent>(255);
	TestEqual(TEXT("Invalid movement intent rejected"), Equipment->ExecuteMovement(Request, *Inventory, *Catalog).Code,
		EHSREquipmentMovementResultCode::InvalidRequest);
	VerifyUnchanged(TEXT("InvalidIntent"), StableInventoryRevision, StableEquipmentRevision, StableMembershipCount);

	UHSRItemEquipmentMappingCatalog* EmptyCatalog = NewObject<UHSRItemEquipmentMappingCatalog>();
	Request = MakeUnequip(StableInventoryRevision, StableEquipmentRevision);
	TestEqual(TEXT("Missing mapping rejected"), Equipment->ExecuteMovement(Request, *Inventory, *EmptyCatalog).Code,
		EHSREquipmentMovementResultCode::MappingRejected);
	VerifyUnchanged(TEXT("MissingMapping"), StableInventoryRevision, StableEquipmentRevision, StableMembershipCount);

	Request = EquipRequest;
	Request.OperationId = FGuid::NewGuid(); Request.InstanceId = CollisionId;
	Request.ExpectedInventoryRevision = StableInventoryRevision; Request.ExpectedEquipmentRevision = StableEquipmentRevision;
	TestEqual(TEXT("Inventory membership and Registry payload collision rejected"), Equipment->ExecuteMovement(Request, *Inventory, *Catalog).Code,
		EHSREquipmentMovementResultCode::MappingRejected);
	VerifyUnchanged(TEXT("PayloadCollision"), StableInventoryRevision, StableEquipmentRevision, StableMembershipCount);

	TestTrue(TEXT("Capacity constrained to current membership"), Inventory->SetCapacityForAutomation(StableMembershipCount));
	Request = MakeUnequip(StableInventoryRevision, StableEquipmentRevision);
	const FGuid RetryOperationId = Request.OperationId;
	TestEqual(TEXT("Unequip capacity rejection"), Equipment->ExecuteMovement(Request, *Inventory, *Catalog).Code,
		EHSREquipmentMovementResultCode::InventoryRejected);
	VerifyUnchanged(TEXT("Capacity"), StableInventoryRevision, StableEquipmentRevision, StableMembershipCount);

	TestEqual(TEXT("Collision membership removed for retry"), Inventory->RemoveUnique(CollisionId), EHSRInventoryOperationResult::Success);
	InventoryEvents = 0; EquipmentEvents = 0;
	Inventory->GetSnapshot(Snapshot);
	Request = MakeUnequip(Snapshot.Revision, StableEquipmentRevision);
	Request.OperationId = RetryOperationId;
	const FHSREquipmentMovementResult Retry = Equipment->ExecuteMovement(Request, *Inventory, *Catalog);
	TestEqual(TEXT("Rejected OperationId remains reusable"), Retry.Code, EHSREquipmentMovementResultCode::Success);
	TestTrue(TEXT("Corrected retry commits"), Retry.bCommitted);
	TestEqual(TEXT("Corrected retry publishes Inventory once"), InventoryEvents, 1);
	TestEqual(TEXT("Corrected retry publishes Equipment once"), EquipmentEvents, 1);
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
	FHSRInventorySaveData SavedInventory;
	Inventory->ExportSaveData(SavedInventory);
	FHSRInventoryRestoreState InventoryRestore;
	TestTrue(TEXT("Replace membership prepares for round-trip"), Inventory->PrepareRestore(SavedInventory, InventoryRestore));
	Inventory->CommitRestore(MoveTemp(InventoryRestore), false);
	Inventory->GetSnapshot(Snapshot);
	TestEqual(TEXT("Round-trip keeps displaced membership count"), Snapshot.UniqueItems.Num(), 1);
	if (Snapshot.UniqueItems.Num() == 1)
	{
		TestEqual(TEXT("Round-trip keeps displaced InstanceId"), Snapshot.UniqueItems[0].InstanceId, OldId);
		TestEqual(TEXT("Round-trip keeps displaced ItemId"), Snapshot.UniqueItems[0].DefinitionId, FName(TEXT("Item.Weapon.Old")));
	}
	TArray<FHSREquipmentRegistryDto> SavedRegistry;
	TArray<FHSREquipmentPlacementDto> SavedPlacements;
	Equipment->ExportSaveData(SavedRegistry, SavedPlacements);
	FHSREquipmentRegistryRestoreState EquipmentRestore;
	TestTrue(TEXT("Schema-7 Registry/Placement prepares for round-trip"),
		Equipment->PrepareRestore(SavedRegistry, SavedPlacements, EquipmentRestore));
	Equipment->CommitRestore(EquipmentRestore);
	TestTrue(TEXT("Round-trip keeps old Registry payload"), Equipment->FindRegisteredInstance(OldId, Resolved));
	TestEqual(TEXT("Round-trip keeps old enhancement"), Resolved.EnhancementLevel, 4);
	TestTrue(TEXT("Round-trip keeps new Registry payload"), Equipment->FindRegisteredInstance(NewId, Resolved));
	TestEqual(TEXT("Round-trip keeps new enhancement"), Resolved.EnhancementLevel, 7);
	TestTrue(TEXT("Round-trip keeps loadout resolvable"), Equipment->GetLoadout(CharacterId, Loadout, Revision));
	TestEqual(TEXT("Round-trip keeps new placement"), Loadout.Equipment.FindChecked(EHSREquipmentSlot::Weapon).InstanceId, NewId);
	return true;
}

#endif
