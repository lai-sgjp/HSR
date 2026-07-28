#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "../Data/Definitions/HSRCharacterCatalog.h"
#include "../Data/Definitions/HSRDropTableDefinition.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Data/Definitions/HSRRewardDefinition.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Reward/HSRRewardSubsystem.h"
#include "../Reward/HSRSettlementAuthority.h"

namespace HSR::Settlement::Tests
{
	struct FState
	{
		FHSRInventorySnapshot Inventory;
		FHSRCharacterProfileSnapshot Profile;
		int32 RewardReceiptCount = 0;
	};

	struct FFixture
	{
		UGameInstance* GameInstance = nullptr;
		UWorld* World = nullptr;
		UHSRSettlementAuthority* Authority = nullptr;
		UHSRInventorySubsystem* Inventory = nullptr;
		UHSRCharacterProfileSubsystem* Profiles = nullptr;
		UHSRRewardSubsystem* Reward = nullptr;
		FName ItemId = TEXT("Item.Settlement.Automation");
		FName RewardId = TEXT("Reward.Settlement.Automation");
		FName CharacterId = TEXT("Character.A");

		bool Initialize(FAutomationTestBase& Test, int32 Capacity = 10, int32 Quantity = 2)
		{
			GameInstance = NewObject<UGameInstance>(GEngine);
			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone(FName(*FString::Printf(TEXT("HSRSettlement_%s"),
				*FGuid::NewGuid().ToString(EGuidFormats::Digits))));
			World = GameInstance->GetWorld();
			Authority = GameInstance->GetSubsystem<UHSRSettlementAuthority>();
			Inventory = GameInstance->GetSubsystem<UHSRInventorySubsystem>();
			Profiles = GameInstance->GetSubsystem<UHSRCharacterProfileSubsystem>();
			Reward = GameInstance->GetSubsystem<UHSRRewardSubsystem>();
			if (!Authority || !Inventory || !Profiles || !Reward)
			{
				return false;
			}

			Inventory->SetCapacityForAutomation(Capacity);
			UHSRCharacterCatalog* Catalog = LoadObject<UHSRCharacterCatalog>(nullptr,
				TEXT("/Game/Data/Progression/DA_CharacterCatalog_P11.DA_CharacterCatalog_P11"));
			if (!Catalog || Profiles->RegisterLoadedCatalog(Catalog) != EHSRCharacterProfileResult::Success)
			{
				return false;
			}

			UHSRItemDefinition* Item = NewObject<UHSRItemDefinition>(GameInstance);
			Item->ItemId = ItemId;
			Item->StorageKind = EHSRItemStorageKind::Stackable;
			Item->MaxStack = 99;
			UHSRRewardDefinition* Definition = NewObject<UHSRRewardDefinition>(GameInstance);
			Definition->RewardDefinitionId = RewardId;
			FHSRRewardItemEntry& Entry = Definition->FixedItems.AddDefaulted_GetRef();
			Entry.ItemId = ItemId;
			Entry.Quantity = Quantity;
			return Test.TestEqual(TEXT("Register settlement item"), Inventory->RegisterDefinition(*Item),
				EHSRInventoryOperationResult::Success)
				&& Test.TestEqual(TEXT("Register settlement reward"), Reward->RegisterRewardDefinition(*Definition),
					EHSRRewardOperationResult::Success);
		}

		FHSRSettlementRequest Request(uint32 Salt = 1, int32 Experience = 10) const
		{
			FHSRSettlementRequest Result;
			Result.TransactionId = FGuid(Salt, Salt + 1, Salt + 2, Salt + 3);
			Result.RewardDefinitionId = RewardId;
			Result.PlayerCharacterId = CharacterId;
			Result.RewardSeed = 17;
			Result.Experience = Experience;
			return Result;
		}

		FState Snapshot() const
		{
			FState Result;
			Inventory->GetSnapshot(Result.Inventory);
			Profiles->GetProfileSnapshot(CharacterId, Result.Profile);
			TArray<FHSRRewardReceipt> Receipts;
			Reward->GetReceipts(Receipts);
			Result.RewardReceiptCount = Receipts.Num();
			return Result;
		}

		void Shutdown() const
		{
			if (!GameInstance) return;
			GameInstance->Shutdown();
			if (World)
			{
				World->DestroyWorld(false);
				GEngine->DestroyWorldContext(World);
			}
			GameInstance->RemoveFromRoot();
		}
	};

	bool SameState(const FState& Left, const FState& Right)
	{
		return Left.Inventory.Revision == Right.Inventory.Revision
			&& Left.Inventory.Stacks.Num() == Right.Inventory.Stacks.Num()
			&& Left.Inventory.UniqueItems.Num() == Right.Inventory.UniqueItems.Num()
			&& Left.Inventory.UsedSlots == Right.Inventory.UsedSlots
			&& Left.Profile.RuntimeRevision == Right.Profile.RuntimeRevision
			&& Left.Profile.RuntimeState.Experience == Right.Profile.RuntimeState.Experience
			&& Left.Profile.RuntimeState.Level == Right.Profile.RuntimeState.Level
			&& Left.RewardReceiptCount == Right.RewardReceiptCount;
	}

	bool ExpectRejectedWithoutMutation(FAutomationTestBase& Test, FFixture& Fixture,
		const TCHAR* Label, const FHSRSettlementRequest& Request, EHSRSettlementResult Expected)
	{
		const FState Before = Fixture.Snapshot();
		const FHSRSettlementAutomationSnapshot CountersBefore = Fixture.Authority->GetAutomationSnapshot();
		FHSRSettlementReceipt Receipt;
		const EHSRSettlementResult Actual = Fixture.Authority->SubmitSettlement(Request, Receipt);
		const FState After = Fixture.Snapshot();
		const FHSRSettlementAutomationSnapshot CountersAfter = Fixture.Authority->GetAutomationSnapshot();
		return Test.TestEqual(Label, Actual, Expected)
			&& Test.TestTrue(TEXT("Rejected settlement preserves all domain state"), SameState(Before, After))
			&& Test.TestEqual(TEXT("Rejected settlement performs no install"), CountersAfter.AggregateInstallCount, CountersBefore.AggregateInstallCount)
			&& Test.TestEqual(TEXT("Rejected settlement performs no publication"), CountersAfter.PublicationCount, CountersBefore.PublicationCount);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRSettlementFoundationTest,
	"HSR.Settlement.Foundation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRSettlementFoundationTest::RunTest(const FString& Parameters)
{
	using namespace HSR::Settlement::Tests;
	FFixture Fixture;
	if (!TestTrue(TEXT("Settlement fixture initializes"), Fixture.Initialize(*this)))
	{
		Fixture.Shutdown();
		return false;
	}

	FHSRSettlementRequest Valid = Fixture.Request();
	FState Before = Fixture.Snapshot();
	FHSRSettlementCandidate Candidate;
	FHSRSettlementReceipt Existing;
	TestEqual(TEXT("Pure aggregate prepare succeeds"), Fixture.Authority->PrepareSettlement(Valid, Candidate, Existing), EHSRSettlementResult::Success);
	TestTrue(TEXT("Prepare leaves all live domains unchanged"), SameState(Before, Fixture.Snapshot()));
	TestEqual(TEXT("Inventory candidate contains complete grant"), Candidate.Inventory.Stacks.FindRef(Fixture.ItemId), 2);
	TestEqual(TEXT("Profile candidate contains granted EXP"), Candidate.Profile.Profiles.FindChecked(Fixture.CharacterId).RuntimeState.Experience, 10);
	TestTrue(TEXT("Reward candidate prebuilds ledger entry"), Candidate.Reward.SettlementLedger.Contains(Valid.TransactionId));
	TestEqual(TEXT("Inventory candidate carries aggregate transaction"), Candidate.Inventory.TransactionId, Valid.TransactionId);
	TestEqual(TEXT("Profile candidate carries aggregate transaction"), Candidate.Profile.TransactionId, Valid.TransactionId);
	TestEqual(TEXT("Reward candidate carries aggregate transaction"), Candidate.Reward.TransactionId, Valid.TransactionId);

	TArray<FString> PublicationOrder;
	int32 InventoryEvents = 0, ProfileEvents = 0, RewardEvents = 0;
	int64 RewardRevisionObservedFromInventoryEvent = -1;
	int32 RewardReceiptsObservedFromInventoryEvent = -1;
	Fixture.Inventory->OnInventoryChanged().AddLambda([&](int64)
	{
		++InventoryEvents;
		PublicationOrder.Add(TEXT("Inventory"));
		FHSRRewardSaveData RewardState;
		Fixture.Reward->ExportSaveData(RewardState);
		RewardRevisionObservedFromInventoryEvent = RewardState.Revision;
		RewardReceiptsObservedFromInventoryEvent = RewardState.Receipts.Num();
	});
	Fixture.Profiles->OnProfileChanged().AddLambda([&](FName, int64){ ++ProfileEvents; PublicationOrder.Add(TEXT("Profile")); });
	Fixture.Reward->OnRewardCommitted().AddLambda([&](const FHSRRewardReceipt&){ ++RewardEvents; PublicationOrder.Add(TEXT("Reward")); });
	FHSRSettlementReceipt Receipt;
	TestEqual(TEXT("Aggregate settlement succeeds"), Fixture.Authority->SubmitSettlement(Valid, Receipt), EHSRSettlementResult::Success);
	TestEqual(TEXT("Inventory revision committed"), Receipt.InventoryRevision, int64(1));
	TestEqual(TEXT("Profile revision committed"), Receipt.ProfileRevision, int64(1));
	TestEqual(TEXT("Reward revision committed"), Receipt.RewardRevision, int64(1));
	TestEqual(TEXT("Inventory publishes once"), InventoryEvents, 1);
	TestEqual(TEXT("Profile publishes once"), ProfileEvents, 1);
	TestEqual(TEXT("Reward publishes once"), RewardEvents, 1);
	TestEqual(TEXT("Inventory callback sees coherent Reward revision"), RewardRevisionObservedFromInventoryEvent, int64(1));
	TestEqual(TEXT("Inventory callback sees coherent Reward receipt"), RewardReceiptsObservedFromInventoryEvent, 1);
	TestEqual(TEXT("First publication is Inventory"), PublicationOrder[0], FString(TEXT("Inventory")));
	TestEqual(TEXT("Second publication is Profile"), PublicationOrder[1], FString(TEXT("Profile")));
	TestEqual(TEXT("Third publication is Reward"), PublicationOrder[2], FString(TEXT("Reward")));
	const FState Committed = Fixture.Snapshot();
	TestEqual(TEXT("Inventory quantity committed"), Committed.Inventory.Stacks[0].Quantity, 2);
	TestEqual(TEXT("EXP committed"), Committed.Profile.RuntimeState.Experience, 10);
	TestEqual(TEXT("One reward receipt committed"), Committed.RewardReceiptCount, 1);

	FHSRSettlementReceipt DuplicateReceipt;
	TestEqual(TEXT("Matching transaction is idempotent"), Fixture.Authority->SubmitSettlement(Valid, DuplicateReceipt), EHSRSettlementResult::NoOp);
	TestTrue(TEXT("Duplicate preserves committed state"), SameState(Committed, Fixture.Snapshot()));
	TestEqual(TEXT("Duplicate returns original reward revision"), DuplicateReceipt.RewardRevision, Receipt.RewardRevision);
	TestEqual(TEXT("Duplicate emits no event"), PublicationOrder.Num(), 3);
	TestEqual(TEXT("Duplicate performs no second install"), Fixture.Authority->GetAutomationSnapshot().AggregateInstallCount, 1);

	FHSRSettlementRequest Conflict = Valid;
	Conflict.RewardSeed++;
	ExpectRejectedWithoutMutation(*this, Fixture, TEXT("Changed payload conflicts"), Conflict, EHSRSettlementResult::TransactionConflict);
	Conflict = Valid;
	Conflict.ExpectedInventoryRevision++;
	ExpectRejectedWithoutMutation(*this, Fixture, TEXT("Changed expected revision conflicts"), Conflict, EHSRSettlementResult::TransactionConflict);

	FHSRSettlementRequest Invalid = Fixture.Request(20);
	Invalid.TransactionId.Invalidate();
	ExpectRejectedWithoutMutation(*this, Fixture, TEXT("Invalid transaction rejected"), Invalid, EHSRSettlementResult::InvalidTransactionId);
	Invalid = Fixture.Request(21); Invalid.RewardDefinitionId = NAME_None;
	ExpectRejectedWithoutMutation(*this, Fixture, TEXT("Empty reward rejected"), Invalid, EHSRSettlementResult::InvalidRequest);
	Invalid = Fixture.Request(25); Invalid.RewardDefinitionId = TEXT("Reward.Unknown");
	Invalid.ExpectedInventoryRevision = 1; Invalid.ExpectedProfileRevision = 1; Invalid.ExpectedRewardRevision = 1;
	ExpectRejectedWithoutMutation(*this, Fixture, TEXT("Unknown reward rejected"), Invalid, EHSRSettlementResult::RewardRejected);
	Invalid = Fixture.Request(22); Invalid.PlayerCharacterId = TEXT("Character.Unknown");
	Invalid.ExpectedInventoryRevision = 1;
	Invalid.ExpectedProfileRevision = 1;
	Invalid.ExpectedRewardRevision = 1;
	ExpectRejectedWithoutMutation(*this, Fixture, TEXT("Unknown character rejected"), Invalid, EHSRSettlementResult::ProfileRejected);
	Invalid = Fixture.Request(23); Invalid.Experience = -1;
	ExpectRejectedWithoutMutation(*this, Fixture, TEXT("Negative EXP rejected"), Invalid, EHSRSettlementResult::InvalidRequest);
	Invalid = Fixture.Request(24); Invalid.ExpectedRewardRevision = 99;
	ExpectRejectedWithoutMutation(*this, Fixture, TEXT("Stale reward revision rejected"), Invalid, EHSRSettlementResult::StaleRevision);
	Invalid = Fixture.Request(26); Invalid.ExpectedInventoryRevision = 99; Invalid.ExpectedProfileRevision = 1; Invalid.ExpectedRewardRevision = 1;
	ExpectRejectedWithoutMutation(*this, Fixture, TEXT("Stale inventory revision rejected"), Invalid, EHSRSettlementResult::StaleRevision);
	Invalid = Fixture.Request(27); Invalid.ExpectedInventoryRevision = 1; Invalid.ExpectedProfileRevision = 99; Invalid.ExpectedRewardRevision = 1;
	ExpectRejectedWithoutMutation(*this, Fixture, TEXT("Stale profile revision rejected"), Invalid, EHSRSettlementResult::StaleRevision);

	for (EHSRSettlementPrepareFailurePoint Point : {EHSRSettlementPrepareFailurePoint::AfterReward,
		EHSRSettlementPrepareFailurePoint::AfterInventory, EHSRSettlementPrepareFailurePoint::AfterProfile})
	{
		Fixture.Authority->SetPrepareFailureForAutomation(Point);
		FHSRSettlementRequest Injected = Fixture.Request(100 + static_cast<uint32>(Point));
		Injected.ExpectedInventoryRevision = 1;
		Injected.ExpectedProfileRevision = 1;
		Injected.ExpectedRewardRevision = 1;
		ExpectRejectedWithoutMutation(*this, Fixture, TEXT("Injected prepare failure is atomic"), Injected, EHSRSettlementResult::InjectedPrepareFailure);
	}
	Fixture.Authority->SetPrepareFailureForAutomation(EHSRSettlementPrepareFailurePoint::None);
	Fixture.Shutdown();

	for (EHSRSettlementCandidateMismatchDomain Domain : {EHSRSettlementCandidateMismatchDomain::Inventory,
		EHSRSettlementCandidateMismatchDomain::Profile, EHSRSettlementCandidateMismatchDomain::Reward})
	{
		FFixture MismatchFixture;
		if (TestTrue(TEXT("Candidate mismatch fixture initializes"), MismatchFixture.Initialize(*this)))
		{
			MismatchFixture.Authority->SetCandidateMismatchForAutomation(Domain);
			ExpectRejectedWithoutMutation(*this, MismatchFixture, TEXT("Mismatched candidate transaction rejected"),
				MismatchFixture.Request(150 + static_cast<uint32>(Domain)), EHSRSettlementResult::CandidateMismatch);
		}
		MismatchFixture.Shutdown();
	}

	FFixture ZeroExperienceFixture;
	if (TestTrue(TEXT("Zero EXP fixture initializes"), ZeroExperienceFixture.Initialize(*this)))
	{
		int32 ZeroProfileEvents = 0;
		ZeroExperienceFixture.Profiles->OnProfileChanged().AddLambda([&](FName, int64){ ++ZeroProfileEvents; });
		FHSRSettlementReceipt ZeroReceipt;
		TestEqual(TEXT("Zero EXP settlement succeeds"),
			ZeroExperienceFixture.Authority->SubmitSettlement(ZeroExperienceFixture.Request(160, 0), ZeroReceipt),
			EHSRSettlementResult::Success);
		TestEqual(TEXT("Zero EXP preserves profile revision"), ZeroReceipt.ProfileRevision, int64(0));
		TestEqual(TEXT("Zero EXP emits no profile event"), ZeroProfileEvents, 0);
	}
	ZeroExperienceFixture.Shutdown();

	FFixture CapacityFixture;
	if (TestTrue(TEXT("Capacity fixture initializes"), CapacityFixture.Initialize(*this, 1)))
	{
		UHSRItemDefinition* Occupied = NewObject<UHSRItemDefinition>(CapacityFixture.GameInstance);
		Occupied->ItemId = TEXT("Item.Settlement.CapacityOccupied");
		Occupied->StorageKind = EHSRItemStorageKind::Stackable;
		Occupied->MaxStack = 1;
		TestEqual(TEXT("Register capacity occupant"), CapacityFixture.Inventory->RegisterDefinition(*Occupied), EHSRInventoryOperationResult::Success);
		TestEqual(TEXT("Occupy capacity"), CapacityFixture.Inventory->AddStack(Occupied->ItemId, 1), EHSRInventoryOperationResult::Success);
		FHSRSettlementRequest CapacityRequest = CapacityFixture.Request(200);
		CapacityRequest.ExpectedInventoryRevision = 1;
		ExpectRejectedWithoutMutation(*this, CapacityFixture, TEXT("Capacity failure rejected"),
			CapacityRequest, EHSRSettlementResult::InventoryRejected);
	}
	CapacityFixture.Shutdown();

	FFixture OverflowFixture;
	if (TestTrue(TEXT("Overflow fixture initializes"), OverflowFixture.Initialize(*this, 10, MAX_int32)))
	{
		TestEqual(TEXT("Prime maximum stack"), OverflowFixture.Inventory->AddStack(OverflowFixture.ItemId, 1), EHSRInventoryOperationResult::Success);
		FHSRSettlementRequest Overflow = OverflowFixture.Request(201);
		Overflow.ExpectedInventoryRevision = 1;
		ExpectRejectedWithoutMutation(*this, OverflowFixture, TEXT("Quantity overflow rejected"), Overflow, EHSRSettlementResult::InventoryRejected);
	}
	OverflowFixture.Shutdown();

	FFixture AmountFixture;
	if (TestTrue(TEXT("Amount fixture initializes"), AmountFixture.Initialize(*this, 10, 100)))
	{
		ExpectRejectedWithoutMutation(*this, AmountFixture, TEXT("Stack amount above definition limit rejected"),
			AmountFixture.Request(202), EHSRSettlementResult::InventoryRejected);
	}
	AmountFixture.Shutdown();

	FFixture UnknownItemFixture;
	if (TestTrue(TEXT("Unknown-item fixture initializes"), UnknownItemFixture.Initialize(*this)))
	{
		UHSRRewardDefinition* UnknownItemReward = NewObject<UHSRRewardDefinition>(UnknownItemFixture.GameInstance);
		UnknownItemReward->RewardDefinitionId = TEXT("Reward.Settlement.UnknownItem");
		FHSRRewardItemEntry& UnknownEntry = UnknownItemReward->FixedItems.AddDefaulted_GetRef();
		UnknownEntry.ItemId = TEXT("Item.Settlement.Unknown");
		UnknownEntry.Quantity = 1;
		TestEqual(TEXT("Register reward with unresolved inventory definition"),
			UnknownItemFixture.Reward->RegisterRewardDefinition(*UnknownItemReward), EHSRRewardOperationResult::Success);
		FHSRSettlementRequest UnknownItemRequest = UnknownItemFixture.Request(203);
		UnknownItemRequest.RewardDefinitionId = UnknownItemReward->RewardDefinitionId;
		ExpectRejectedWithoutMutation(*this, UnknownItemFixture, TEXT("Unknown item definition rejected"),
			UnknownItemRequest, EHSRSettlementResult::RewardRejected);
	}
	UnknownItemFixture.Shutdown();
	return true;
}

#endif
