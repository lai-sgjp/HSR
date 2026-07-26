#if WITH_DEV_AUTOMATION_TESTS

#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

namespace HSR::Inventory::Tests
{
	static UHSRInventorySubsystem* MakeSubsystem()
	{
		UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
		return NewObject<UHSRInventorySubsystem>(GameInstance);
	}

	static UHSRItemDefinition* MakeDefinition(FName ItemId, EHSRItemStorageKind Kind, int32 MaxStack)
	{
		UHSRItemDefinition* Definition = NewObject<UHSRItemDefinition>();
		Definition->ItemId = ItemId;
		Definition->StorageKind = Kind;
		Definition->MaxStack = MaxStack;
		return Definition;
	}

	static FHSRItemInstance MakeInstance(FName DefinitionId, uint32 Seed)
	{
		FHSRItemInstance Instance;
		Instance.DefinitionId = DefinitionId;
		Instance.InstanceId = FGuid(Seed, Seed + 1, Seed + 2, Seed + 3);
		return Instance;
	}

	static void RegisterStandardDefinitions(FAutomationTestBase& Test, UHSRInventorySubsystem& Inventory)
	{
		Test.TestEqual(TEXT("Register material"), Inventory.RegisterDefinition(*MakeDefinition(TEXT("Item.Material.A"), EHSRItemStorageKind::Stackable, 10)), EHSRInventoryOperationResult::Success);
		Test.TestEqual(TEXT("Register unique"), Inventory.RegisterDefinition(*MakeDefinition(TEXT("Item.Unique.A"), EHSRItemStorageKind::Unique, 1)), EHSRInventoryOperationResult::Success);
		Test.TestEqual(TEXT("Register second unique"), Inventory.RegisterDefinition(*MakeDefinition(TEXT("Item.Unique.B"), EHSRItemStorageKind::Unique, 1)), EHSRInventoryOperationResult::Success);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRInventoryTransactionsTest, "HSR.Inventory.Transactions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRInventoryTransactionsTest::RunTest(const FString& Parameters)
{
	using namespace HSR::Inventory::Tests;
	UHSRInventorySubsystem* Inventory = MakeSubsystem();
	RegisterStandardDefinitions(*this, *Inventory);
	TestTrue(TEXT("Set capacity"), Inventory->SetCapacityForAutomation(2));
	int32 BroadcastCount = 0;
	Inventory->OnInventoryChanged().AddLambda([&BroadcastCount](int64) { ++BroadcastCount; });

	TestEqual(TEXT("Add stack"), Inventory->AddStack(TEXT("Item.Material.A"), 4), EHSRInventoryOperationResult::Success);
	TestEqual(TEXT("Grow stack reuses slot"), Inventory->AddStack(TEXT("Item.Material.A"), 6), EHSRInventoryOperationResult::Success);
	const FHSRItemInstance Unique = MakeInstance(TEXT("Item.Unique.A"), 10);
	TestEqual(TEXT("Add unique"), Inventory->AddUnique(Unique), EHSRInventoryOperationResult::Success);
	TestEqual(TEXT("Identical unique add is no-op"), Inventory->AddUnique(Unique), EHSRInventoryOperationResult::NoOp);
	FHSRItemInstance Collision = Unique;
	Collision.DefinitionId = TEXT("Item.Unique.B");
	TestEqual(TEXT("Conflicting unique id rejected"), Inventory->AddUnique(Collision), EHSRInventoryOperationResult::DuplicateInstanceId);

	FHSRInventorySnapshot Snapshot;
	Inventory->GetSnapshot(Snapshot);
	TestEqual(TEXT("Two used slots"), Snapshot.UsedSlots, 2);
	TestEqual(TEXT("Three revisions"), Snapshot.Revision, int64(3));
	TestEqual(TEXT("Three broadcasts"), BroadcastCount, 3);
	TestEqual(TEXT("Stack quantity"), Snapshot.Stacks[0].Quantity, 10);

	TestEqual(TEXT("Remove partial stack"), Inventory->RemoveStack(TEXT("Item.Material.A"), 3), EHSRInventoryOperationResult::Success);
	TestEqual(TEXT("Remove unique"), Inventory->RemoveUnique(Unique.InstanceId), EHSRInventoryOperationResult::Success);
	TestEqual(TEXT("Remove missing unique"), Inventory->RemoveUnique(Unique.InstanceId), EHSRInventoryOperationResult::InstanceNotFound);
	TestEqual(TEXT("Remove remaining stack"), Inventory->RemoveStack(TEXT("Item.Material.A"), 7), EHSRInventoryOperationResult::Success);
	Inventory->GetSnapshot(Snapshot);
	TestEqual(TEXT("Inventory empty"), Snapshot.UsedSlots, 0);
	TestEqual(TEXT("Successful mutations only broadcast"), BroadcastCount, 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRInventoryValidationTest, "HSR.Inventory.Validation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRInventoryValidationTest::RunTest(const FString& Parameters)
{
	using namespace HSR::Inventory::Tests;
	UHSRInventorySubsystem* Inventory = MakeSubsystem();
	TestEqual(TEXT("Empty definition id"), Inventory->RegisterDefinition(*MakeDefinition(NAME_None, EHSRItemStorageKind::Stackable, 10)), EHSRInventoryOperationResult::InvalidDefinitionId);
	TestEqual(TEXT("Invalid stack cap"), Inventory->RegisterDefinition(*MakeDefinition(TEXT("BadStack"), EHSRItemStorageKind::Stackable, 0)), EHSRInventoryOperationResult::InvalidDefinition);
	TestEqual(TEXT("Invalid unique cap"), Inventory->RegisterDefinition(*MakeDefinition(TEXT("BadUnique"), EHSRItemStorageKind::Unique, 2)), EHSRInventoryOperationResult::InvalidDefinition);
	RegisterStandardDefinitions(*this, *Inventory);
	TestEqual(TEXT("Register overflow stack"), Inventory->RegisterDefinition(*MakeDefinition(TEXT("Item.Material.Max"), EHSRItemStorageKind::Stackable, MAX_int32)), EHSRInventoryOperationResult::Success);
	TestEqual(TEXT("Identical definition registration is no-op"), Inventory->RegisterDefinition(*MakeDefinition(TEXT("Item.Material.A"), EHSRItemStorageKind::Stackable, 10)), EHSRInventoryOperationResult::NoOp);
	TestEqual(TEXT("Conflicting definition rejected"), Inventory->RegisterDefinition(*MakeDefinition(TEXT("Item.Material.A"), EHSRItemStorageKind::Stackable, 9)), EHSRInventoryOperationResult::DuplicateDefinitionId);
	TestTrue(TEXT("One slot capacity"), Inventory->SetCapacityForAutomation(1));
	int32 BroadcastCount = 0;
	Inventory->OnInventoryChanged().AddLambda([&BroadcastCount](int64) { ++BroadcastCount; });

	TestEqual(TEXT("Empty add item id"), Inventory->AddStack(NAME_None, 1), EHSRInventoryOperationResult::InvalidDefinitionId);
	TestEqual(TEXT("Empty remove item id"), Inventory->RemoveStack(NAME_None, 1), EHSRInventoryOperationResult::InvalidDefinitionId);
	TestEqual(TEXT("Zero add"), Inventory->AddStack(TEXT("Item.Material.A"), 0), EHSRInventoryOperationResult::InvalidQuantity);
	TestEqual(TEXT("Zero remove"), Inventory->RemoveStack(TEXT("Item.Material.A"), 0), EHSRInventoryOperationResult::InvalidQuantity);
	TestEqual(TEXT("Negative add"), Inventory->AddStack(TEXT("Item.Material.A"), -1), EHSRInventoryOperationResult::InvalidQuantity);
	TestEqual(TEXT("Negative remove"), Inventory->RemoveStack(TEXT("Item.Material.A"), -1), EHSRInventoryOperationResult::InvalidQuantity);
	TestEqual(TEXT("Unknown stack"), Inventory->AddStack(TEXT("Missing"), 1), EHSRInventoryOperationResult::UnknownDefinition);
	TestEqual(TEXT("Unique through stack API"), Inventory->AddStack(TEXT("Item.Unique.A"), 1), EHSRInventoryOperationResult::StorageKindMismatch);
	TestEqual(TEXT("Fill stack"), Inventory->AddStack(TEXT("Item.Material.A"), 1), EHSRInventoryOperationResult::Success);
	TestEqual(TEXT("Stack cap"), Inventory->AddStack(TEXT("Item.Material.A"), 10), EHSRInventoryOperationResult::StackLimitExceeded);
	TestEqual(TEXT("Remove stack to free slot"), Inventory->RemoveStack(TEXT("Item.Material.A"), 1), EHSRInventoryOperationResult::Success);
	TestEqual(TEXT("Prime overflow stack"), Inventory->AddStack(TEXT("Item.Material.Max"), 1), EHSRInventoryOperationResult::Success);
	TestEqual(TEXT("Integer overflow rejected"), Inventory->AddStack(TEXT("Item.Material.Max"), MAX_int32), EHSRInventoryOperationResult::QuantityOverflow);
	TestEqual(TEXT("Capacity rejects unique"), Inventory->AddUnique(MakeInstance(TEXT("Item.Unique.A"), 20)), EHSRInventoryOperationResult::CapacityExceeded);
	TestEqual(TEXT("Insufficient remove"), Inventory->RemoveStack(TEXT("Item.Material.Max"), 2), EHSRInventoryOperationResult::InsufficientQuantity);

	FHSRItemInstance Invalid = MakeInstance(TEXT("Item.Unique.A"), 30);
	Invalid.InstanceId.Invalidate();
	TestEqual(TEXT("Invalid instance id"), Inventory->AddUnique(Invalid), EHSRInventoryOperationResult::InvalidInstanceId);
	Invalid = MakeInstance(NAME_None, 31);
	TestEqual(TEXT("Empty instance definition"), Inventory->AddUnique(Invalid), EHSRInventoryOperationResult::InvalidDefinitionId);
	TestEqual(TEXT("Stack through unique API"), Inventory->AddUnique(MakeInstance(TEXT("Item.Material.A"), 32)), EHSRInventoryOperationResult::StorageKindMismatch);

	FHSRInventorySnapshot Snapshot;
	Inventory->GetSnapshot(Snapshot);
	TestEqual(TEXT("Failures preserve revision"), Snapshot.Revision, int64(3));
	TestEqual(TEXT("Failures preserve quantity"), Snapshot.Stacks[0].Quantity, 1);
	TestEqual(TEXT("Failures never broadcast"), BroadcastCount, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRInventoryOrderingTest, "HSR.Inventory.Ordering", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRInventoryOrderingTest::RunTest(const FString& Parameters)
{
	using namespace HSR::Inventory::Tests;
	UHSRInventorySubsystem* Inventory = MakeSubsystem();
	TestEqual(TEXT("Register stack B"), Inventory->RegisterDefinition(*MakeDefinition(TEXT("Item.Stack.B"), EHSRItemStorageKind::Stackable, 10)), EHSRInventoryOperationResult::Success);
	TestEqual(TEXT("Register stack A"), Inventory->RegisterDefinition(*MakeDefinition(TEXT("Item.Stack.A"), EHSRItemStorageKind::Stackable, 10)), EHSRInventoryOperationResult::Success);
	TestEqual(TEXT("Register unique B"), Inventory->RegisterDefinition(*MakeDefinition(TEXT("Item.Unique.B"), EHSRItemStorageKind::Unique, 1)), EHSRInventoryOperationResult::Success);
	TestEqual(TEXT("Register unique A"), Inventory->RegisterDefinition(*MakeDefinition(TEXT("Item.Unique.A"), EHSRItemStorageKind::Unique, 1)), EHSRInventoryOperationResult::Success);
	Inventory->AddStack(TEXT("Item.Stack.B"), 1);
	Inventory->AddStack(TEXT("Item.Stack.A"), 1);
	Inventory->AddUnique(MakeInstance(TEXT("Item.Unique.B"), 50));
	Inventory->AddUnique(MakeInstance(TEXT("Item.Unique.A"), 60));
	Inventory->AddUnique(MakeInstance(TEXT("Item.Unique.A"), 55));

	FHSRInventorySnapshot Snapshot;
	Inventory->GetSnapshot(Snapshot);
	TestEqual(TEXT("Stacks sorted"), Snapshot.Stacks[0].ItemId, FName(TEXT("Item.Stack.A")));
	TestEqual(TEXT("Unique definitions sorted"), Snapshot.UniqueItems[0].DefinitionId, FName(TEXT("Item.Unique.A")));
	TestTrue(TEXT("Unique ids sorted within definition"), Snapshot.UniqueItems[0].InstanceId < Snapshot.UniqueItems[1].InstanceId);
	return true;
}

#endif
