#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "../Battle/HSRBattleGameMode.h"
#include "../Battle/HSRBattleTypes.h"
#include "../Battle/HSREncounterTypes.h"
#include "../Challenge/HSRChallengeProgressionSubsystem.h"
#include "../Data/Definitions/HSRCharacterCatalog.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Data/Definitions/HSRRewardDefinition.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Reward/HSRRewardSubsystem.h"
#include "../Reward/HSRSettlementAuthority.h"

namespace HSR::BattleSettlement::Tests
{
	struct FFixture
	{
		UGameInstance* GameInstance = nullptr;
		UWorld* World = nullptr;
		UHSRInventorySubsystem* Inventory = nullptr;
		UHSRCharacterProfileSubsystem* Profiles = nullptr;
		UHSRRewardSubsystem* Reward = nullptr;
		UHSRSettlementAuthority* Authority = nullptr;
		UHSRChallengeProgressionSubsystem* Progression = nullptr;
		FName ItemId = TEXT("Item.BattleSettlement.Automation");
		FName RewardId = TEXT("Reward.BattleSettlement.Automation");
		FName CharacterId = TEXT("Character.A");
		FGuid RequestId = FGuid(301, 302, 303, 304);

		bool Initialize(FAutomationTestBase& Test)
		{
			GameInstance = NewObject<UGameInstance>(GEngine);
			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone(FName(*FString::Printf(TEXT("HSRBattleSettlement_%s"),
				*FGuid::NewGuid().ToString(EGuidFormats::Digits))));
			World = GameInstance->GetWorld();
			Inventory = GameInstance->GetSubsystem<UHSRInventorySubsystem>();
			Profiles = GameInstance->GetSubsystem<UHSRCharacterProfileSubsystem>();
			Reward = GameInstance->GetSubsystem<UHSRRewardSubsystem>();
			Authority = GameInstance->GetSubsystem<UHSRSettlementAuthority>();
			Progression = GameInstance->GetSubsystem<UHSRChallengeProgressionSubsystem>();
			if (!Inventory || !Profiles || !Reward || !Authority || !Progression)
			{
				return false;
			}

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
			Entry.Quantity = 2;
			return Test.TestEqual(TEXT("Register battle-settlement item"), Inventory->RegisterDefinition(*Item),
				EHSRInventoryOperationResult::Success)
				&& Test.TestEqual(TEXT("Register battle-settlement reward"), Reward->RegisterRewardDefinition(*Definition),
					EHSRRewardOperationResult::Success);
		}

		FHSREncounterRequest Encounter(int32 Experience = 10) const
		{
			FHSREncounterRequest Result;
			Result.RequestId = RequestId;
			Result.PlayerCharacterId = CharacterId;
			Result.EncounterId = TEXT("Encounter.BattleSettlement.Automation");
			Result.RewardDefinitionId = RewardId;
			Result.RewardSeed = 17;
			Result.VictoryExperience = Experience;
			return Result;
		}

		FHSRBattleResult Battle(EHSRBattleOutcome Outcome = EHSRBattleOutcome::PlayerVictory) const
		{
			FHSRBattleResult Result;
			Result.RequestId = RequestId;
			Result.EncounterId = TEXT("Encounter.BattleSettlement.Automation");
			Result.Outcome = Outcome;
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRBattleSettlementIntegrationTest,
	"HSR.BattleSettlement.Integration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRBattleSettlementIntegrationTest::RunTest(const FString& Parameters)
{
	using namespace HSR::BattleSettlement::Tests;
	FFixture Fixture;
	if (!TestTrue(TEXT("Battle settlement fixture initializes"), Fixture.Initialize(*this)))
	{
		Fixture.Shutdown();
		return false;
	}

	FHSRBattleSettlementState State;
	const FHSREncounterRequest Encounter = Fixture.Encounter(10);
	const FHSRBattleResult Victory = Fixture.Battle();
	TestEqual(TEXT("Victory becomes ready to return only after settlement"),
		AHSRBattleGameMode::ProcessSettlementForAutomation(Fixture.GameInstance, Encounter, Victory, State),
		EHSRBattleSettlementConfirmResult::ReadyToReturn);
	TestTrue(TEXT("Victory caches an immutable committed request"), State.bSettlementCommitted && State.bHasRequest);
	TestEqual(TEXT("Settlement transaction is encounter request"), State.Request.TransactionId, Encounter.RequestId);
	TestEqual(TEXT("Configured EXP is cached"), State.Request.Experience, 10);
	TestEqual(TEXT("Inventory commits once"), State.Receipt.InventoryRevision, int64(1));
	TestEqual(TEXT("Profile commits once"), State.Receipt.ProfileRevision, int64(1));
	TestEqual(TEXT("Reward commits once"), State.Receipt.RewardRevision, int64(1));
	TestTrue(TEXT("victory marks encounter complete after settlement"), Fixture.Progression->IsCompleted(Encounter.EncounterId));
	TestEqual(TEXT("victory completion revision is one"), Fixture.Progression->GetSnapshot().Revision, int64(1));

	int32 InventoryEvents = 0, ProfileEvents = 0, RewardEvents = 0;
	Fixture.Inventory->OnInventoryChanged().AddLambda([&](int64){ ++InventoryEvents; });
	Fixture.Profiles->OnProfileChanged().AddLambda([&](FName, int64){ ++ProfileEvents; });
	Fixture.Reward->OnRewardCommitted().AddLambda([&](const FHSRRewardReceipt&){ ++RewardEvents; });
	TestEqual(TEXT("Post-return-rejection retry reuses committed settlement"),
		AHSRBattleGameMode::ProcessSettlementForAutomation(Fixture.GameInstance, Encounter, Victory, State),
		EHSRBattleSettlementConfirmResult::ReadyToReturn);
	TestEqual(TEXT("Retry emits no inventory event"), InventoryEvents, 0);
	TestEqual(TEXT("Retry emits no profile event"), ProfileEvents, 0);
	TestEqual(TEXT("Retry emits no reward event"), RewardEvents, 0);
	TestEqual(TEXT("settlement retry does not advance challenge progress"), Fixture.Progression->GetSnapshot().Revision, int64(1));

	FHSREncounterRequest Changed = Encounter;
	Changed.RewardSeed++;
	TestEqual(TEXT("Same transaction with changed payload conflicts"),
		AHSRBattleGameMode::ProcessSettlementForAutomation(Fixture.GameInstance, Changed, Victory, State),
		EHSRBattleSettlementConfirmResult::Rejected);
	Fixture.Shutdown();

	FFixture DefeatFixture;
	if (TestTrue(TEXT("Defeat fixture initializes"), DefeatFixture.Initialize(*this)))
	{
		FHSRBattleSettlementState DefeatState;
		TestEqual(TEXT("Defeat bypasses settlement and may return"),
			AHSRBattleGameMode::ProcessSettlementForAutomation(DefeatFixture.GameInstance, DefeatFixture.Encounter(),
				DefeatFixture.Battle(EHSRBattleOutcome::PlayerDefeat), DefeatState),
			EHSRBattleSettlementConfirmResult::ReadyToReturn);
		TestFalse(TEXT("Defeat has no settlement request"), DefeatState.bHasRequest);
		TestEqual(TEXT("Defeat performs no aggregate install"), DefeatFixture.Authority->GetAutomationSnapshot().AggregateInstallCount, 0);
	}
	DefeatFixture.Shutdown();

	FFixture ZeroFixture;
	if (TestTrue(TEXT("Zero EXP fixture initializes"), ZeroFixture.Initialize(*this)))
	{
		FHSRBattleSettlementState ZeroState;
		TestEqual(TEXT("Zero EXP victory settles"),
			AHSRBattleGameMode::ProcessSettlementForAutomation(ZeroFixture.GameInstance, ZeroFixture.Encounter(0),
				ZeroFixture.Battle(), ZeroState), EHSRBattleSettlementConfirmResult::ReadyToReturn);
		TestEqual(TEXT("Zero EXP preserves profile revision"), ZeroState.Receipt.ProfileRevision, int64(0));
	}
	ZeroFixture.Shutdown();

	FFixture InvalidFixture;
	if (TestTrue(TEXT("Invalid fixture initializes"), InvalidFixture.Initialize(*this)))
	{
		for (int32 Case = 0; Case < 3; ++Case)
		{
			FHSREncounterRequest InvalidEncounter = InvalidFixture.Encounter();
			FHSRBattleResult InvalidResult = InvalidFixture.Battle();
			if (Case == 0) InvalidEncounter.RewardDefinitionId = NAME_None;
			if (Case == 1) InvalidEncounter.PlayerCharacterId = NAME_None;
			if (Case == 2) InvalidResult.RequestId.Invalidate();
			FHSRBattleSettlementState InvalidState;
			TestEqual(TEXT("Invalid settlement identity rejects"),
				AHSRBattleGameMode::ProcessSettlementForAutomation(InvalidFixture.GameInstance, InvalidEncounter, InvalidResult, InvalidState),
				EHSRBattleSettlementConfirmResult::Rejected);
		}
		TestEqual(TEXT("Invalid identities perform no aggregate install"), InvalidFixture.Authority->GetAutomationSnapshot().AggregateInstallCount, 0);
	}
	InvalidFixture.Shutdown();

	for (int32 Domain = 0; Domain < 3; ++Domain)
	{
		FFixture StaleFixture;
		if (TestTrue(TEXT("Stale revision fixture initializes"), StaleFixture.Initialize(*this)))
		{
			FHSRBattleSettlementState StaleState;
			StaleState.bHasRequest = true;
			StaleState.Request.TransactionId = StaleFixture.RequestId;
			StaleState.Request.RewardDefinitionId = StaleFixture.RewardId;
			StaleState.Request.PlayerCharacterId = StaleFixture.CharacterId;
			StaleState.Request.RewardSeed = 17;
			StaleState.Request.Experience = 10;
			if (Domain == 0) StaleState.Request.ExpectedInventoryRevision = 99;
			if (Domain == 1) StaleState.Request.ExpectedProfileRevision = 99;
			if (Domain == 2) StaleState.Request.ExpectedRewardRevision = 99;
			TestEqual(TEXT("Stale domain revision rejects"),
				AHSRBattleGameMode::ProcessSettlementForAutomation(StaleFixture.GameInstance, StaleFixture.Encounter(),
					StaleFixture.Battle(), StaleState), EHSRBattleSettlementConfirmResult::Rejected);
			TestTrue(TEXT("Stale request remains cached for explicit retry policy"), StaleState.bHasRequest);
			TestFalse(TEXT("Stale request is not committed"), StaleState.bSettlementCommitted);
			TestEqual(TEXT("Stale revision performs no aggregate install"),
				StaleFixture.Authority->GetAutomationSnapshot().AggregateInstallCount, 0);
			TestEqual(TEXT("Stale revision performs no publication"),
				StaleFixture.Authority->GetAutomationSnapshot().PublicationCount, 0);
		}
		StaleFixture.Shutdown();
	}

	FFixture PrepareFixture;
	if (TestTrue(TEXT("Prepare failure fixture initializes"), PrepareFixture.Initialize(*this)))
	{
		FHSRBattleSettlementState PrepareState;
		PrepareFixture.Authority->SetPrepareFailureForAutomation(EHSRSettlementPrepareFailurePoint::AfterInventory);
		TestEqual(TEXT("Prepare failure rejects before return"),
			AHSRBattleGameMode::ProcessSettlementForAutomation(PrepareFixture.GameInstance, PrepareFixture.Encounter(),
				PrepareFixture.Battle(), PrepareState), EHSRBattleSettlementConfirmResult::Rejected);
		TestTrue(TEXT("Prepare failure preserves immutable request"), PrepareState.bHasRequest);
		TestFalse(TEXT("Prepare failure does not commit"), PrepareState.bSettlementCommitted);
		TestEqual(TEXT("Prepare failure performs no aggregate install"),
			PrepareFixture.Authority->GetAutomationSnapshot().AggregateInstallCount, 0);
		PrepareFixture.Authority->SetPrepareFailureForAutomation(EHSRSettlementPrepareFailurePoint::None);
		TestEqual(TEXT("Prepare retry reuses request and commits"),
			AHSRBattleGameMode::ProcessSettlementForAutomation(PrepareFixture.GameInstance, PrepareFixture.Encounter(),
				PrepareFixture.Battle(), PrepareState), EHSRBattleSettlementConfirmResult::ReadyToReturn);
	}
	PrepareFixture.Shutdown();
	return true;
}

#endif
