#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "../Data/Definitions/HSREquipmentEnhancementCatalog.h"
#include "../Data/Definitions/HSREquipmentDefinition.h"
#include "../Data/Definitions/HSRRelicDefinition.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Inventory/HSRInventorySubsystem.h"

namespace HSRRelicEnhancementTests
{
	struct FFixture
	{
		UGameInstance* GameInstance = nullptr;
		UHSRInventorySubsystem* Inventory = nullptr;
		UHSREquipmentSubsystem* Equipment = nullptr;
		UHSREquipmentEnhancementCatalog* Catalog = nullptr;
		FGuid CharacterId;
		FGuid InstanceId;
		FName MaterialId = TEXT("Item.Material.Test");
		FName RelicDefinitionId = TEXT("Relic.Head.Test");
	};

	static FFixture MakeFixture(FAutomationTestBase& Test)
	{
		FFixture Fixture;
		Fixture.GameInstance = NewObject<UGameInstance>(GetTransientPackage());
		Fixture.Inventory = NewObject<UHSRInventorySubsystem>(Fixture.GameInstance);
		Fixture.Equipment = NewObject<UHSREquipmentSubsystem>(Fixture.GameInstance);
		Fixture.Catalog = NewObject<UHSREquipmentEnhancementCatalog>(Fixture.GameInstance);
		Fixture.CharacterId = FGuid::NewGuid();
		Fixture.InstanceId = FGuid::NewGuid();

		UHSRItemDefinition* Material = NewObject<UHSRItemDefinition>(Fixture.GameInstance);
		Material->ItemId = Fixture.MaterialId;
		Material->StorageKind = EHSRItemStorageKind::Stackable;
		Material->MaxStack = 20;
		Test.TestEqual(TEXT("Material definition registered"),
			Fixture.Inventory->RegisterDefinition(*Material), EHSRInventoryOperationResult::Success);
		Test.TestEqual(TEXT("Material stack seeded"), Fixture.Inventory->AddStack(Fixture.MaterialId, 5),
			EHSRInventoryOperationResult::Success);

		UHSRRelicDefinition* Relic = NewObject<UHSRRelicDefinition>(Fixture.GameInstance);
		Relic->DefinitionId = Fixture.RelicDefinitionId;
		Relic->SetId = TEXT("Set.Test");
		Relic->Slot = EHSRRelicSlot::Head;
		Relic->EnhancementCap = 3;
		Test.TestEqual(TEXT("Relic definition registered"), Fixture.Equipment->RegisterDefinition(*Relic),
			EHSREquipmentOperationResult::Success);

		FHSREquipmentEnhancementRule LevelOne;
		LevelOne.DefinitionId = Fixture.RelicDefinitionId;
		LevelOne.Kind = EHSREquipmentKind::Relic;
		LevelOne.TargetLevel = 1;
		LevelOne.MaterialItemId = Fixture.MaterialId;
		LevelOne.MaterialCost = 2;
		LevelOne.TargetModifiers.Add({EHSREquipmentStat::Attack, 8.0f});
		Test.TestTrue(TEXT("Level one rule registered"), Fixture.Catalog->AddRule(LevelOne));

		FHSREquipmentEnhancementRule LevelTwo = LevelOne;
		LevelTwo.TargetLevel = 2;
		LevelTwo.MaterialCost = 10;
		LevelTwo.TargetModifiers[0].Value = 16.0f;
		Test.TestTrue(TEXT("Level two rule registered"), Fixture.Catalog->AddRule(LevelTwo));

		FHSREquipmentInstance Instance;
		Instance.InstanceId = Fixture.InstanceId;
		Instance.DefinitionId = Fixture.RelicDefinitionId;
		Instance.Kind = EHSREquipmentKind::Relic;
		Instance.EnhancementLevel = 0;
		Instance.Modifiers.Add({EHSREquipmentStat::Attack, 4.0f});
		Test.TestEqual(TEXT("Relic equipped"), Fixture.Equipment->Equip(Fixture.CharacterId, Instance),
			EHSREquipmentOperationResult::Success);
		return Fixture;
	}

	static FHSREquipmentEnhancementRequest MakeRequest(const FFixture& Fixture, int32 TargetLevel,
		int64 InventoryRevision, int32 EquipmentRevision, int32 ExpectedLevel)
	{
		FHSREquipmentEnhancementRequest Request;
		Request.OperationId = FGuid::NewGuid();
		Request.CharacterId = Fixture.CharacterId;
		Request.InstanceId = Fixture.InstanceId;
		Request.Kind = EHSREquipmentKind::Relic;
		Request.TargetLevel = TargetLevel;
		Request.ExpectedInventoryRevision = InventoryRevision;
		Request.ExpectedEquipmentRevision = EquipmentRevision;
		Request.ExpectedEnhancementLevel = ExpectedLevel;
		return Request;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSREquipmentEnhancementExactlyOnceTest,
	"HSR.Equipment.Enhancement.ExactlyOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSREquipmentEnhancementExactlyOnceTest::RunTest(const FString&)
{
	using namespace HSRRelicEnhancementTests;
	FFixture Fixture = MakeFixture(*this);
	FHSRInventorySnapshot BeforeInventory;
	Fixture.Inventory->GetSnapshot(BeforeInventory);
	FHSREquipmentLoadout BeforeLoadout;
	int32 BeforeEquipmentRevision = 0;
	TestTrue(TEXT("Initial loadout resolves"),
		Fixture.Equipment->GetLoadout(Fixture.CharacterId, BeforeLoadout, BeforeEquipmentRevision));

	int32 InventoryEvents = 0;
	int32 EquipmentEvents = 0;
	Fixture.Inventory->OnInventoryChanged().AddLambda([&InventoryEvents](int64) { ++InventoryEvents; });
	Fixture.Equipment->OnLoadoutChanged().AddLambda([&EquipmentEvents](const FGuid&, int32) { ++EquipmentEvents; });
	const FHSREquipmentEnhancementRequest Request = MakeRequest(Fixture, 1, BeforeInventory.Revision,
		BeforeEquipmentRevision, 0);
	const FHSREquipmentEnhancementResult Result = Fixture.Equipment->ExecuteEnhancement(
		Request, *Fixture.Inventory, *Fixture.Catalog);
	TestEqual(TEXT("Enhancement commits"), Result.Code, EHSREquipmentEnhancementResultCode::Success);
	TestTrue(TEXT("Enhancement marks committed"), Result.bCommitted);
	TestFalse(TEXT("First enhancement is not replay"), Result.bReplay);
	TestEqual(TEXT("Inventory revision advances once"), Result.NewInventoryRevision,
		BeforeInventory.Revision + 1);
	TestEqual(TEXT("Equipment revision advances once"), Result.NewEquipmentRevision,
		BeforeEquipmentRevision + 1);
	TestEqual(TEXT("Inventory publishes once"), InventoryEvents, 1);
	TestEqual(TEXT("Equipment publishes once"), EquipmentEvents, 1);

	FHSRInventorySnapshot AfterInventory;
	Fixture.Inventory->GetSnapshot(AfterInventory);
	TestEqual(TEXT("Material cost is removed once"), AfterInventory.Stacks[0].Quantity, 3);
	FHSREquipmentInstance AfterInstance;
	TestTrue(TEXT("Enhanced registry instance resolves"),
		Fixture.Equipment->FindRegisteredInstance(Fixture.InstanceId, AfterInstance));
	TestEqual(TEXT("Registry level updates once"), AfterInstance.EnhancementLevel, 1);
	TestEqual(TEXT("Registry modifier snapshot replaces atomically"), AfterInstance.Modifiers.Num(), 1);
	if (AfterInstance.Modifiers.Num() == 1)
	{
		TestEqual(TEXT("Target modifier is authoritative"), AfterInstance.Modifiers[0].Value, 8.0f);
	}

	const FHSREquipmentEnhancementResult Replay = Fixture.Equipment->ExecuteEnhancement(
		Request, *Fixture.Inventory, *Fixture.Catalog);
	TestEqual(TEXT("Replay returns cached success"), Replay.Code, EHSREquipmentEnhancementResultCode::Success);
	TestTrue(TEXT("Replay is identified"), Replay.bReplay);
	TestFalse(TEXT("Replay does not commit"), Replay.bCommitted);
	TestEqual(TEXT("Replay does not publish Inventory"), InventoryEvents, 1);
	TestEqual(TEXT("Replay does not publish Equipment"), EquipmentEvents, 1);

	FHSREquipmentEnhancementRequest ConflictRequest = Request;
	ConflictRequest.TargetLevel = 2;
	const FHSREquipmentEnhancementResult Conflict = Fixture.Equipment->ExecuteEnhancement(
		ConflictRequest, *Fixture.Inventory, *Fixture.Catalog);
	TestEqual(TEXT("Changed request under same OperationId is rejected"), Conflict.Code,
		EHSREquipmentEnhancementResultCode::OperationIdConflict);
	TestFalse(TEXT("OperationId conflict does not commit"), Conflict.bCommitted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSREquipmentEnhancementFailureMatrixTest,
	"HSR.Equipment.Enhancement.FailureMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSREquipmentEnhancementFailureMatrixTest::RunTest(const FString&)
{
	using namespace HSRRelicEnhancementTests;
	FFixture Fixture = MakeFixture(*this);
	FHSRInventorySnapshot InventorySnapshot;
	Fixture.Inventory->GetSnapshot(InventorySnapshot);
	FHSREquipmentLoadout Loadout;
	int32 EquipmentRevision = 0;
	Fixture.Equipment->GetLoadout(Fixture.CharacterId, Loadout, EquipmentRevision);
	FHSREquipmentInstance BeforeInstance;
	Fixture.Equipment->FindRegisteredInstance(Fixture.InstanceId, BeforeInstance);

	const FHSREquipmentEnhancementRequest Stale = MakeRequest(Fixture, 1,
		InventorySnapshot.Revision - 1, EquipmentRevision, 0);
	const FHSREquipmentEnhancementResult StaleResult = Fixture.Equipment->ExecuteEnhancement(
		Stale, *Fixture.Inventory, *Fixture.Catalog);
	TestEqual(TEXT("Stale Inventory revision rejected"), StaleResult.Code,
		EHSREquipmentEnhancementResultCode::InventoryRevisionConflict);

	const FHSREquipmentEnhancementRequest WrongExpectedLevel = MakeRequest(Fixture, 1,
		InventorySnapshot.Revision, EquipmentRevision, 2);
	const FHSREquipmentEnhancementResult WrongLevelResult = Fixture.Equipment->ExecuteEnhancement(
		WrongExpectedLevel, *Fixture.Inventory, *Fixture.Catalog);
	TestEqual(TEXT("Stale expected enhancement level rejected"), WrongLevelResult.Code,
		EHSREquipmentEnhancementResultCode::EnhancementLevelConflict);

	Fixture.Equipment->SetEnhancementProjection(
		UHSREquipmentSubsystem::FEnhancementProjectionPreflight::CreateLambda(
			[](const FHSREquipmentEnhancementRequest&, const FHSREquipmentInstance&) { return false; }),
		UHSREquipmentSubsystem::FEnhancementProjectionCommit::CreateLambda(
			[](const FHSREquipmentEnhancementRequest&, const FHSREquipmentInstance&) {}));
	const FHSREquipmentEnhancementRequest ProjectionFailure = MakeRequest(Fixture, 1,
		InventorySnapshot.Revision, EquipmentRevision, 0);
	const FHSREquipmentEnhancementResult ProjectionResult = Fixture.Equipment->ExecuteEnhancement(
		ProjectionFailure, *Fixture.Inventory, *Fixture.Catalog);
	TestEqual(TEXT("Projection preflight failure is typed"), ProjectionResult.Code,
		EHSREquipmentEnhancementResultCode::ProjectionRejected);
	TestFalse(TEXT("Projection failure does not commit"), ProjectionResult.bCommitted);

	Fixture.Inventory->GetSnapshot(InventorySnapshot);
	FHSREquipmentInstance AfterFailure;
	Fixture.Equipment->FindRegisteredInstance(Fixture.InstanceId, AfterFailure);
	TestEqual(TEXT("Projection failure keeps Inventory revision"), InventorySnapshot.Revision,
		int64(1));
	TestEqual(TEXT("Projection failure keeps registry level"), AfterFailure.EnhancementLevel,
		BeforeInstance.EnhancementLevel);

	const FHSREquipmentEnhancementRequest Insufficient = MakeRequest(Fixture, 2,
		InventorySnapshot.Revision, EquipmentRevision, 0);
	Fixture.Equipment->SetEnhancementProjection(
		UHSREquipmentSubsystem::FEnhancementProjectionPreflight::CreateLambda(
			[](const FHSREquipmentEnhancementRequest&, const FHSREquipmentInstance&) { return true; }),
		UHSREquipmentSubsystem::FEnhancementProjectionCommit::CreateLambda(
			[](const FHSREquipmentEnhancementRequest&, const FHSREquipmentInstance&) {}));
	const FHSREquipmentEnhancementResult InsufficientResult = Fixture.Equipment->ExecuteEnhancement(
		Insufficient, *Fixture.Inventory, *Fixture.Catalog);
	TestEqual(TEXT("Insufficient material is rejected"), InsufficientResult.Code,
		EHSREquipmentEnhancementResultCode::InventoryRejected);
	TestFalse(TEXT("Insufficient material does not commit"), InsufficientResult.bCommitted);
	Fixture.Inventory->GetSnapshot(InventorySnapshot);
	TestEqual(TEXT("Insufficient material keeps Inventory revision"), InventorySnapshot.Revision,
		int64(1));
	Fixture.Equipment->FindRegisteredInstance(Fixture.InstanceId, AfterFailure);
	TestEqual(TEXT("Insufficient material keeps registry level"), AfterFailure.EnhancementLevel,
		BeforeInstance.EnhancementLevel);
	return true;
}

#endif
