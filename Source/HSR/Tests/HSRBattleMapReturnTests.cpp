#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "../Battle/HSRBattleTypes.h"
#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "../Battle/HSREncounterTypes.h"
#include "../Data/Definitions/HSRMapDefinition.h"
#include "../Map/HSRMapSubsystem.h"
#include "../Player/HSRPlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRBattleMapReturnContractTest, "HSR.BattleReturn.MapContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRBattleMapReturnContractTest::RunTest(const FString&)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UHSRMapSubsystem* Maps = NewObject<UHSRMapSubsystem>(GameInstance);
	UHSRMapDefinition* MapB = NewObject<UHSRMapDefinition>();
	MapB->MapId = TEXT("Map.B");
	MapB->World = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/Map_Exploration_P15_B.Map_Exploration_P15_B")));
	MapB->RegionId = TEXT("Region.B");
	MapB->DefaultArrivalId = TEXT("Arrival.B");
	TestEqual(TEXT("register return map"), Maps->RegisterMapDefinition(*MapB), EHSRMapOperationResult::Success);

	FName ResolvedMapId;
	TestTrue(TEXT("resolve canonical package"), Maps->ResolveMapIdByPackage(TEXT("/Game/Maps/Map_Exploration_P15_B"), ResolvedMapId));
	TestEqual(TEXT("canonical package resolves stable id"), ResolvedMapId, FName(TEXT("Map.B")));
	TestTrue(TEXT("resolve PIE package"), Maps->ResolveMapIdByPackage(TEXT("/Game/Maps/UEDPIE_0_Map_Exploration_P15_B"), ResolvedMapId));
	TestEqual(TEXT("PIE package resolves stable id"), ResolvedMapId, FName(TEXT("Map.B")));
	TestFalse(TEXT("unknown package rejected"), Maps->ResolveMapIdByPackage(TEXT("/Game/Maps/Map_Unknown"), ResolvedMapId));
	TestTrue(TEXT("unknown package clears output"), ResolvedMapId.IsNone());

	FHSRBattleReturnContext BattleContext;
	BattleContext.RequestId = FGuid::NewGuid();
	BattleContext.ExplorationMapPath = TEXT("/Game/Maps/Map_Exploration_P15_B");
	BattleContext.ExplorationMapId = TEXT("Map.B");
	BattleContext.ReturnTransform = FTransform(FRotator::ZeroRotator, FVector(125.0, 250.0, 50.0));
	FHSRExplorationReturnContext ExplorationContext;
	ExplorationContext.RequestId = BattleContext.RequestId;
	ExplorationContext.ExplorationMapPath = BattleContext.ExplorationMapPath;
	ExplorationContext.ExplorationMapId = BattleContext.ExplorationMapId;
	ExplorationContext.ReturnTransform = BattleContext.ReturnTransform;
	TestTrue(TEXT("return DTO request remains stable"), ExplorationContext.RequestId == BattleContext.RequestId);
	TestEqual(TEXT("return DTO carries stable map id"), ExplorationContext.ExplorationMapId, FName(TEXT("Map.B")));
	TestTrue(TEXT("return DTO transform remains pure value"), ExplorationContext.ReturnTransform.Equals(BattleContext.ReturnTransform));
	TestTrue(TEXT("victory resolves encounter"), UHSRBattleTransitionSubsystem::ShouldResolveEncounter(EHSRBattleOutcome::PlayerVictory));
	TestFalse(TEXT("defeat remains retryable"), UHSRBattleTransitionSubsystem::ShouldResolveEncounter(EHSRBattleOutcome::PlayerDefeat));
	TestFalse(TEXT("incomplete result remains retryable"), UHSRBattleTransitionSubsystem::ShouldResolveEncounter(EHSRBattleOutcome::None));
	TestTrue(TEXT("source-world travel failure belongs to transaction"), UHSRBattleTransitionSubsystem::DoesTravelFailureMatch(
		TEXT("/Game/Maps/UEDPIE_0_Map_Battle"), TEXT("/Game/Maps/Map_Battle"), TEXT("/Game/Maps/Map_Exploration_P15_B")));
	TestTrue(TEXT("target-world travel failure belongs to transaction"), UHSRBattleTransitionSubsystem::DoesTravelFailureMatch(
		TEXT("/Game/Maps/UEDPIE_0_Map_Exploration_P15_B"), TEXT("/Game/Maps/Map_Battle"), TEXT("/Game/Maps/Map_Exploration_P15_B")));
	TestFalse(TEXT("unrelated travel failure is ignored"), UHSRBattleTransitionSubsystem::DoesTravelFailureMatch(
		TEXT("/Game/Maps/Map_Unrelated"), TEXT("/Game/Maps/Map_Battle"), TEXT("/Game/Maps/Map_Exploration_P15_B")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRBattleReturnConsumerRecoveryContractTest,
	"HSR.BattleReturn.ConsumerRecoveryContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRBattleReturnConsumerRecoveryContractTest::RunTest(const FString&)
{
	TestTrue(TEXT("pending return without map consumer requests fallback"),
		AHSRPlayerController::ShouldEnsureBattleReturnConsumer(true, false));
	TestFalse(TEXT("pending return with map consumer does not duplicate fallback"),
		AHSRPlayerController::ShouldEnsureBattleReturnConsumer(true, true));
	TestFalse(TEXT("empty return does not spawn a consumer"),
		AHSRPlayerController::ShouldEnsureBattleReturnConsumer(false, false));
	return true;
}

#endif
