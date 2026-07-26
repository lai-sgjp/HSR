#if WITH_DEV_AUTOMATION_TESTS

#include "../Battle/HSRBattleCoordinator.h"
#include "../Battle/HSREncounterTypes.h"
#include "../Data/Definitions/HSRDropTableDefinition.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Data/Definitions/HSRRewardDefinition.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Reward/HSRRewardSubsystem.h"
#include "../Reward/HSRRewardTypes.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRRewardContextIntegrationTest, "HSR.Reward.Integration.Context", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRRewardContextIntegrationTest::RunTest(const FString& Parameters)
{
	UHSRBattleCoordinator* Coordinator = NewObject<UHSRBattleCoordinator>();
	FHSREncounterRequest Request;
	Request.RequestId = FGuid(1, 2, 3, 4);
	Request.EncounterId = TEXT("Encounter.P13");
	Request.EnemyDefinitionId = TEXT("Enemy.P13");
	Request.BattleMapPath = TEXT("/Game/Maps/Map_BattleTest");
	Request.RewardDefinitionId = TEXT("Reward.P13.Standard");
	Request.RewardSeed = 777;
	TestTrue(TEXT("Coordinator accepts reward request context"), Coordinator->SubmitBattleRequest(Request));
	TestEqual(TEXT("Reward id copied"), Coordinator->GetCurrentRewardDefinitionId(), Request.RewardDefinitionId);
	TestEqual(TEXT("Reward seed copied"), Coordinator->GetCurrentRewardSeed(), Request.RewardSeed);
	FHSRBattleResult VictoryResult;
	VictoryResult.RequestId = Request.RequestId;
	VictoryResult.Outcome = EHSRBattleOutcome::PlayerVictory;
	FHSRRewardRequest RewardRequest;
	TestTrue(TEXT("Victory creates a reward request"), Coordinator->BuildVictoryRewardRequest(VictoryResult, RewardRequest));
	TestEqual(TEXT("Battle request id is the stable claim id"), RewardRequest.ClaimId, Request.RequestId);
	TestEqual(TEXT("Reward definition preserved"), RewardRequest.RewardDefinitionId, Request.RewardDefinitionId);
	TestEqual(TEXT("Reward seed preserved"), RewardRequest.Seed, Request.RewardSeed);

	FHSRBattleResult DefeatResult = VictoryResult;
	DefeatResult.Outcome = EHSRBattleOutcome::PlayerDefeat;
	TestFalse(TEXT("Defeat skips reward submission"), Coordinator->BuildVictoryRewardRequest(DefeatResult, RewardRequest));
	FHSRBattleResult StaleResult = VictoryResult;
	StaleResult.RequestId = FGuid(5, 6, 7, 8);
	TestFalse(TEXT("Stale battle result skips reward submission"), Coordinator->BuildVictoryRewardRequest(StaleResult, RewardRequest));
	Coordinator->Reset();
	TestTrue(TEXT("Reward id reset"), Coordinator->GetCurrentRewardDefinitionId().IsNone());
	TestEqual(TEXT("Reward seed reset"), Coordinator->GetCurrentRewardSeed(), 0);
	TestFalse(TEXT("Reset context cannot create a reward request"), Coordinator->BuildVictoryRewardRequest(VictoryResult, RewardRequest));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRRewardBundleAtomicityIntegrationTest, "HSR.Reward.Integration.BundleAtomicity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRRewardBundleAtomicityIntegrationTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UHSRInventorySubsystem* Inventory = NewObject<UHSRInventorySubsystem>(GameInstance);
	UHSRRewardSubsystem* Reward = NewObject<UHSRRewardSubsystem>(GameInstance);
	Reward->InitializeForAutomation(Inventory);

	UHSRRewardDefinition* ExistingReward = NewObject<UHSRRewardDefinition>();
	ExistingReward->RewardDefinitionId = TEXT("Reward.Bundle.Atomic");
	ExistingReward->FixedItems.Add({TEXT("Item.Existing"), 1});
	TestEqual(TEXT("Seed existing reward definition"), Reward->RegisterRewardDefinition(*ExistingReward), EHSRRewardOperationResult::Success);

	UHSRItemDefinition* NewItem = NewObject<UHSRItemDefinition>();
	NewItem->ItemId = TEXT("Item.Bundle.New");
	NewItem->StorageKind = EHSRItemStorageKind::Stackable;
	NewItem->MaxStack = 10;
	TArray<TObjectPtr<UHSRItemDefinition>> Items{NewItem};
	UHSRDropTableDefinition* NewDrop = NewObject<UHSRDropTableDefinition>();
	NewDrop->DropTableId = TEXT("Drop.Bundle.New");
	NewDrop->Entries.Add({NewItem->ItemId, 1, 1, 1});
	UHSRRewardDefinition* ConflictingReward = NewObject<UHSRRewardDefinition>();
	ConflictingReward->RewardDefinitionId = ExistingReward->RewardDefinitionId;
	ConflictingReward->FixedItems.Add({NewItem->ItemId, 2});
	ConflictingReward->DropTableId = NewDrop->DropTableId;
	ConflictingReward->DropRolls = 1;

	TestEqual(TEXT("Conflicting bundle rejected before commit"), Reward->RegisterBundle(Items, *NewDrop, *ConflictingReward), EHSRRewardOperationResult::DuplicateDefinitionId);
	TestEqual(TEXT("Item was not partially registered"), Inventory->RegisterDefinition(*NewItem), EHSRInventoryOperationResult::Success);
	TestEqual(TEXT("Drop was not partially registered"), Reward->RegisterDropTable(*NewDrop), EHSRRewardOperationResult::Success);
	return true;
}

#endif
