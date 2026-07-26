#if WITH_DEV_AUTOMATION_TESTS

#include "../Data/Definitions/HSRDropTableDefinition.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Data/Definitions/HSRRewardDefinition.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Reward/HSRRewardSubsystem.h"
#include "../Reward/HSRRewardResolver.h"
#include "Algo/Reverse.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

namespace HSR::Reward::Tests
{
	static FGuid Id(uint32 Seed) { return FGuid(Seed, Seed + 1, Seed + 2, Seed + 3); }

	struct FFixture
	{
		UHSRInventorySubsystem* Inventory = nullptr;
		UHSRRewardSubsystem* Reward = nullptr;
	};

	static FFixture MakeFixture(FAutomationTestBase& Test, int32 Capacity = 10)
	{
		UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
		FFixture Result;
		Result.Inventory = NewObject<UHSRInventorySubsystem>(GameInstance);
		Result.Reward = NewObject<UHSRRewardSubsystem>(GameInstance);
		Result.Reward->InitializeForAutomation(Result.Inventory);
		Result.Inventory->SetCapacityForAutomation(Capacity);

		UHSRItemDefinition* Stack = NewObject<UHSRItemDefinition>();
		Stack->ItemId = TEXT("Item.Material.LumenShard");
		Stack->StorageKind = EHSRItemStorageKind::Stackable;
		Stack->MaxStack = 99;
		Test.TestEqual(TEXT("Register stack item"), Result.Inventory->RegisterDefinition(*Stack), EHSRInventoryOperationResult::Success);
		UHSRItemDefinition* Unique = NewObject<UHSRItemDefinition>();
		Unique->ItemId = TEXT("Item.Unique.ArchiveToken");
		Unique->StorageKind = EHSRItemStorageKind::Unique;
		Unique->MaxStack = 1;
		Test.TestEqual(TEXT("Register unique item"), Result.Inventory->RegisterDefinition(*Unique), EHSRInventoryOperationResult::Success);
		return Result;
	}

	static UHSRDropTableDefinition* MakeDropTable()
	{
		UHSRDropTableDefinition* Drop = NewObject<UHSRDropTableDefinition>();
		Drop->DropTableId = TEXT("Drop.P13.Standard");
		Drop->Entries.Add({TEXT("Item.Material.LumenShard"), 1, 3, 3});
		Drop->Entries.Add({TEXT("Item.Unique.ArchiveToken"), 1, 1, 1});
		return Drop;
	}

	static UHSRRewardDefinition* MakeReward(bool bWithDrop = true)
	{
		UHSRRewardDefinition* Reward = NewObject<UHSRRewardDefinition>();
		Reward->RewardDefinitionId = TEXT("Reward.P13.Standard");
		Reward->FixedItems.Add({TEXT("Item.Material.LumenShard"), 2});
		if (bWithDrop)
		{
			Reward->DropTableId = TEXT("Drop.P13.Standard");
			Reward->DropRolls = 2;
		}
		return Reward;
	}

	static bool SameGrants(const TArray<FHSRInventoryGrant>& A, const TArray<FHSRInventoryGrant>& B)
	{
		if (A.Num() != B.Num()) return false;
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].ItemId != B[Index].ItemId || A[Index].Quantity != B[Index].Quantity || A[Index].InstanceIds != B[Index].InstanceIds) return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRRewardTransactionTest, "HSR.Reward.Transaction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRRewardTransactionTest::RunTest(const FString& Parameters)
{
	using namespace HSR::Reward::Tests;
	FFixture Fixture = MakeFixture(*this);
	TestEqual(TEXT("Register drop"), Fixture.Reward->RegisterDropTable(*MakeDropTable()), EHSRRewardOperationResult::Success);
	TestEqual(TEXT("Register reward"), Fixture.Reward->RegisterRewardDefinition(*MakeReward()), EHSRRewardOperationResult::Success);
	int32 RewardBroadcasts = 0;
	int32 InventoryBroadcasts = 0;
	bool bReceiptVisibleDuringInventoryBroadcast = false;
	Fixture.Reward->OnRewardCommitted().AddLambda([&RewardBroadcasts](const FHSRRewardReceipt&) { ++RewardBroadcasts; });

	FHSRRewardRequest Request;
	Request.ClaimId = Id(100);
	Request.RewardDefinitionId = TEXT("Reward.P13.Standard");
	Request.Seed = 12345;
	Fixture.Inventory->OnInventoryChanged().AddLambda([&](int64)
	{
		++InventoryBroadcasts;
		FHSRRewardReceipt VisibleReceipt;
		bReceiptVisibleDuringInventoryBroadcast = Fixture.Reward->GetReceipt(Request.ClaimId, VisibleReceipt);
	});
	FHSRRewardReceipt First;
	TestEqual(TEXT("First claim succeeds"), Fixture.Reward->SubmitReward(Request, First), EHSRRewardOperationResult::Success);
	FHSRInventorySnapshot AfterFirst;
	Fixture.Inventory->GetSnapshot(AfterFirst);
	TestEqual(TEXT("Single reward broadcast"), RewardBroadcasts, 1);
	TestEqual(TEXT("Single inventory broadcast"), InventoryBroadcasts, 1);
	TestTrue(TEXT("Claim ledger visible before inventory broadcast"), bReceiptVisibleDuringInventoryBroadcast);

	FHSRRewardReceipt Replay;
	TestEqual(TEXT("Replay is no-op"), Fixture.Reward->SubmitReward(Request, Replay), EHSRRewardOperationResult::NoOp);
	FHSRInventorySnapshot AfterReplay;
	Fixture.Inventory->GetSnapshot(AfterReplay);
	TestEqual(TEXT("Replay inventory revision stable"), AfterReplay.Revision, AfterFirst.Revision);
	TestEqual(TEXT("Replay reward broadcast stable"), RewardBroadcasts, 1);
	TestEqual(TEXT("Replay inventory broadcast stable"), InventoryBroadcasts, 1);
	TestTrue(TEXT("Replay receipt stable"), SameGrants(First.Grants, Replay.Grants));

	FHSRRewardRequest Conflict = Request;
	Conflict.Seed = 54321;
	FHSRRewardReceipt Ignored;
	TestEqual(TEXT("Changed payload conflicts"), Fixture.Reward->SubmitReward(Conflict, Ignored), EHSRRewardOperationResult::ClaimConflict);
	TestEqual(TEXT("Conflict reward broadcast stable"), RewardBroadcasts, 1);
	TestEqual(TEXT("Conflict inventory broadcast stable"), InventoryBroadcasts, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRRewardDeterminismTest, "HSR.Reward.Determinism", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRRewardDeterminismTest::RunTest(const FString& Parameters)
{
	using namespace HSR::Reward::Tests;
	FFixture A = MakeFixture(*this);
	FFixture B = MakeFixture(*this);
	TestEqual(TEXT("Register A drop"), A.Reward->RegisterDropTable(*MakeDropTable()), EHSRRewardOperationResult::Success);
	UHSRDropTableDefinition* ReversedDrop = MakeDropTable();
	Algo::Reverse(ReversedDrop->Entries);
	TestEqual(TEXT("Register reversed B drop"), B.Reward->RegisterDropTable(*ReversedDrop), EHSRRewardOperationResult::Success);
	TestEqual(TEXT("Register A reward"), A.Reward->RegisterRewardDefinition(*MakeReward()), EHSRRewardOperationResult::Success);
	TestEqual(TEXT("Register B reward"), B.Reward->RegisterRewardDefinition(*MakeReward()), EHSRRewardOperationResult::Success);
	FHSRRewardRequest Request{Id(200), TEXT("Reward.P13.Standard"), 777};
	FHSRRewardReceipt ReceiptA;
	FHSRRewardReceipt ReceiptB;
	TestEqual(TEXT("A succeeds"), A.Reward->SubmitReward(Request, ReceiptA), EHSRRewardOperationResult::Success);
	TestEqual(TEXT("B succeeds"), B.Reward->SubmitReward(Request, ReceiptB), EHSRRewardOperationResult::Success);
	TestTrue(TEXT("Same seed and claim are deterministic"), SameGrants(ReceiptA.Grants, ReceiptB.Grants));
	for (int32 Index = 1; Index < ReceiptA.Grants.Num(); ++Index)
	{
		TestTrue(TEXT("Grants are sorted"), ReceiptA.Grants[Index - 1].ItemId.LexicalLess(ReceiptA.Grants[Index].ItemId));
	}

	FHSRRewardDefinitionRule VariableReward;
	VariableReward.RewardDefinitionId = TEXT("Reward.Variable");
	VariableReward.DropTableId = TEXT("Drop.Variable");
	VariableReward.DropRolls = 1;
	FHSRDropTableRule VariableDrop;
	VariableDrop.DropTableId = TEXT("Drop.Variable");
	VariableDrop.Entries.Add({TEXT("Item.Material.LumenShard"), 1, 100, 1});
	TArray<FHSRRewardItemEntry> SeedOne;
	TestTrue(TEXT("Seed one resolves"), FHSRRewardResolver::Resolve(VariableReward, &VariableDrop, 1, SeedOne));
	bool bDifferentSeedObserved = false;
	for (int32 Seed = 2; Seed <= 16 && !bDifferentSeedObserved; ++Seed)
	{
		TArray<FHSRRewardItemEntry> Candidate;
		TestTrue(TEXT("Alternate seed resolves"), FHSRRewardResolver::Resolve(VariableReward, &VariableDrop, Seed, Candidate));
		bDifferentSeedObserved = Candidate[0].Quantity != SeedOne[0].Quantity;
	}
	TestTrue(TEXT("Different seed produces controlled variation"), bDifferentSeedObserved);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRRewardAtomicFailureTest, "HSR.Reward.AtomicFailure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRRewardAtomicFailureTest::RunTest(const FString& Parameters)
{
	using namespace HSR::Reward::Tests;
	FFixture Fixture = MakeFixture(*this, 1);
	int32 CapacityInventoryBroadcasts = 0;
	int32 CapacityRewardBroadcasts = 0;
	Fixture.Inventory->OnInventoryChanged().AddLambda([&](int64) { ++CapacityInventoryBroadcasts; });
	Fixture.Reward->OnRewardCommitted().AddLambda([&](const FHSRRewardReceipt&) { ++CapacityRewardBroadcasts; });
	UHSRRewardDefinition* Reward = MakeReward(false);
	Reward->FixedItems.Add({TEXT("Item.Unique.ArchiveToken"), 1});
	TestEqual(TEXT("Register mixed reward"), Fixture.Reward->RegisterRewardDefinition(*Reward), EHSRRewardOperationResult::Success);
	FHSRRewardRequest Request{Id(300), TEXT("Reward.P13.Standard"), 1};
	FHSRRewardReceipt Receipt;
	TestEqual(TEXT("Capacity rejects whole reward"), Fixture.Reward->SubmitReward(Request, Receipt), EHSRRewardOperationResult::InventoryRejected);
	FHSRInventorySnapshot Snapshot;
	Fixture.Inventory->GetSnapshot(Snapshot);
	TestEqual(TEXT("No partial stack"), Snapshot.UsedSlots, 0);
	TestEqual(TEXT("No inventory revision"), Snapshot.Revision, int64(0));
	TestFalse(TEXT("Failed claim absent"), Fixture.Reward->GetReceipt(Request.ClaimId, Receipt));
	TestEqual(TEXT("Capacity failure inventory delegate silent"), CapacityInventoryBroadcasts, 0);
	TestEqual(TEXT("Capacity failure reward delegate silent"), CapacityRewardBroadcasts, 0);

	FFixture Retry = MakeFixture(*this, 3);
	int32 RetryInventoryBroadcasts = 0;
	int32 RetryRewardBroadcasts = 0;
	Retry.Inventory->OnInventoryChanged().AddLambda([&](int64) { ++RetryInventoryBroadcasts; });
	Retry.Reward->OnRewardCommitted().AddLambda([&](const FHSRRewardReceipt&) { ++RetryRewardBroadcasts; });
	TestEqual(TEXT("Retry register reward"), Retry.Reward->RegisterRewardDefinition(*Reward), EHSRRewardOperationResult::Success);
	Retry.Reward->SetCommitFailureForAutomation(true);
	TestEqual(TEXT("Injected failure"), Retry.Reward->SubmitReward(Request, Receipt), EHSRRewardOperationResult::InjectedFailure);
	Retry.Inventory->GetSnapshot(Snapshot);
	TestEqual(TEXT("Injected failure inventory unchanged"), Snapshot.Revision, int64(0));
	TestFalse(TEXT("Injected failure claim absent"), Retry.Reward->GetReceipt(Request.ClaimId, Receipt));
	TestEqual(TEXT("Injected failure inventory delegate silent"), RetryInventoryBroadcasts, 0);
	TestEqual(TEXT("Injected failure reward delegate silent"), RetryRewardBroadcasts, 0);
	Retry.Reward->SetCommitFailureForAutomation(false);
	TestEqual(TEXT("Retry converges"), Retry.Reward->SubmitReward(Request, Receipt), EHSRRewardOperationResult::Success);
	TestEqual(TEXT("Retry inventory broadcasts once"), RetryInventoryBroadcasts, 1);
	TestEqual(TEXT("Retry reward broadcasts once"), RetryRewardBroadcasts, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRRewardValidationTest, "HSR.Reward.Validation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRRewardValidationTest::RunTest(const FString& Parameters)
{
	using namespace HSR::Reward::Tests;
	FFixture Fixture = MakeFixture(*this);
	UHSRDropTableDefinition* EmptyDrop = NewObject<UHSRDropTableDefinition>();
	EmptyDrop->DropTableId = TEXT("Drop.Empty");
	TestEqual(TEXT("Empty drop rejected"), Fixture.Reward->RegisterDropTable(*EmptyDrop), EHSRRewardOperationResult::InvalidDefinition);
	UHSRDropTableDefinition* BadDrop = MakeDropTable();
	BadDrop->Entries[0].Weight = 0;
	TestEqual(TEXT("Zero weight rejected"), Fixture.Reward->RegisterDropTable(*BadDrop), EHSRRewardOperationResult::InvalidDefinition);
	BadDrop = MakeDropTable();
	BadDrop->Entries[0].MinQuantity = 3;
	BadDrop->Entries[0].MaxQuantity = 2;
	TestEqual(TEXT("Invalid quantity range rejected"), Fixture.Reward->RegisterDropTable(*BadDrop), EHSRRewardOperationResult::InvalidDefinition);
	BadDrop = MakeDropTable();
	BadDrop->Entries[1].ItemId = BadDrop->Entries[0].ItemId;
	TestEqual(TEXT("Duplicate drop item rejected"), Fixture.Reward->RegisterDropTable(*BadDrop), EHSRRewardOperationResult::InvalidDefinition);
	UHSRDropTableDefinition* OversizedDrop = NewObject<UHSRDropTableDefinition>();
	OversizedDrop->DropTableId = TEXT("Drop.Oversized");
	for (int32 Index = 0; Index <= FHSRRewardResolver::MaxDefinitionEntries; ++Index)
	{
		OversizedDrop->Entries.Add({FName(*FString::Printf(TEXT("Item.Drop.%03d"), Index)), 1, 1, 1});
	}
	TestEqual(TEXT("Oversized drop rejected before copy"), Fixture.Reward->RegisterDropTable(*OversizedDrop), EHSRRewardOperationResult::InvalidDefinition);
	TestEqual(TEXT("Register valid drop"), Fixture.Reward->RegisterDropTable(*MakeDropTable()), EHSRRewardOperationResult::Success);
	TestEqual(TEXT("Identical drop registration is no-op"), Fixture.Reward->RegisterDropTable(*MakeDropTable()), EHSRRewardOperationResult::NoOp);
	UHSRDropTableDefinition* ConflictingDrop = MakeDropTable();
	ConflictingDrop->Entries[0].Weight = 4;
	TestEqual(TEXT("Conflicting drop registration rejected"), Fixture.Reward->RegisterDropTable(*ConflictingDrop), EHSRRewardOperationResult::DuplicateDefinitionId);
	UHSRRewardDefinition* EmptyReward = NewObject<UHSRRewardDefinition>();
	EmptyReward->RewardDefinitionId = TEXT("Reward.Empty");
	TestEqual(TEXT("Empty reward rejected"), Fixture.Reward->RegisterRewardDefinition(*EmptyReward), EHSRRewardOperationResult::InvalidDefinition);
	UHSRRewardDefinition* BadFixedReward = MakeReward(false);
	BadFixedReward->FixedItems[0].Quantity = 0;
	TestEqual(TEXT("Invalid fixed quantity rejected"), Fixture.Reward->RegisterRewardDefinition(*BadFixedReward), EHSRRewardOperationResult::InvalidDefinition);
	UHSRRewardDefinition* DuplicateFixed = MakeReward(false);
	const FHSRRewardItemEntry DuplicateEntry = DuplicateFixed->FixedItems[0];
	DuplicateFixed->FixedItems.Add(DuplicateEntry);
	TestEqual(TEXT("Duplicate fixed item rejected"), Fixture.Reward->RegisterRewardDefinition(*DuplicateFixed), EHSRRewardOperationResult::InvalidDefinition);
	UHSRRewardDefinition* OversizedFixed = NewObject<UHSRRewardDefinition>();
	OversizedFixed->RewardDefinitionId = TEXT("Reward.Oversized");
	for (int32 Index = 0; Index <= FHSRRewardResolver::MaxDefinitionEntries; ++Index)
	{
		OversizedFixed->FixedItems.Add({FName(*FString::Printf(TEXT("Item.Fixed.%03d"), Index)), 1});
	}
	TestEqual(TEXT("Oversized fixed reward rejected"), Fixture.Reward->RegisterRewardDefinition(*OversizedFixed), EHSRRewardOperationResult::InvalidDefinition);
	FHSRRewardDefinitionRule GrantLimitReward;
	GrantLimitReward.RewardDefinitionId = TEXT("Reward.GrantLimit");
	GrantLimitReward.DropTableId = TEXT("Drop.GrantLimit");
	GrantLimitReward.DropRolls = 1;
	for (int32 Index = 0; Index < FHSRRewardResolver::MaxResolvedGrants; ++Index)
	{
		GrantLimitReward.FixedItems.Add({FName(*FString::Printf(TEXT("Item.Grant.%03d"), Index)), 1});
	}
	FHSRDropTableRule GrantLimitDrop;
	GrantLimitDrop.DropTableId = TEXT("Drop.GrantLimit");
	GrantLimitDrop.Entries.Add({TEXT("Item.Grant.Overflow"), 1, 1, 1});
	TArray<FHSRRewardItemEntry> TooManyResolvedGrants;
	TestFalse(TEXT("Resolved grant limit rejected"), FHSRRewardResolver::Resolve(GrantLimitReward, &GrantLimitDrop, 1, TooManyResolvedGrants));
	UHSRRewardDefinition* ExcessiveRolls = MakeReward();
	ExcessiveRolls->RewardDefinitionId = TEXT("Reward.ExcessiveRolls");
	ExcessiveRolls->DropRolls = FHSRRewardResolver::MaxDropRolls + 1;
	TestEqual(TEXT("Excessive rolls rejected"), Fixture.Reward->RegisterRewardDefinition(*ExcessiveRolls), EHSRRewardOperationResult::InvalidDefinition);
	UHSRRewardDefinition* MissingDrop = MakeReward();
	MissingDrop->DropTableId = TEXT("Drop.Missing");
	TestEqual(TEXT("Missing drop rejected"), Fixture.Reward->RegisterRewardDefinition(*MissingDrop), EHSRRewardOperationResult::UnknownDropTable);
	TestEqual(TEXT("Register valid reward"), Fixture.Reward->RegisterRewardDefinition(*MakeReward()), EHSRRewardOperationResult::Success);
	TestEqual(TEXT("Identical reward registration is no-op"), Fixture.Reward->RegisterRewardDefinition(*MakeReward()), EHSRRewardOperationResult::NoOp);
	UHSRRewardDefinition* ConflictingReward = MakeReward();
	ConflictingReward->FixedItems[0].Quantity = 3;
	TestEqual(TEXT("Conflicting reward registration rejected"), Fixture.Reward->RegisterRewardDefinition(*ConflictingReward), EHSRRewardOperationResult::DuplicateDefinitionId);
	UHSRRewardDefinition* UnknownItemReward = MakeReward(false);
	UnknownItemReward->RewardDefinitionId = TEXT("Reward.UnknownItem");
	UnknownItemReward->FixedItems[0].ItemId = TEXT("Item.Missing");
	TestEqual(TEXT("Register unknown item reward rule"), Fixture.Reward->RegisterRewardDefinition(*UnknownItemReward), EHSRRewardOperationResult::Success);
	FHSRRewardReceipt Receipt;
	TestEqual(TEXT("Invalid claim rejected"), Fixture.Reward->SubmitReward({FGuid(), TEXT("Reward.P13.Standard"), 1}, Receipt), EHSRRewardOperationResult::InvalidClaimId);
	TestEqual(TEXT("Unknown reward rejected"), Fixture.Reward->SubmitReward({Id(400), TEXT("Reward.Missing"), 1}, Receipt), EHSRRewardOperationResult::UnknownRewardDefinition);
	TestEqual(TEXT("Unknown item fails resolve"), Fixture.Reward->SubmitReward({Id(401), TEXT("Reward.UnknownItem"), 1}, Receipt), EHSRRewardOperationResult::ResolveFailed);
	return true;
}

#endif
