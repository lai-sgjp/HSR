#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Battle/HSRBattleStage.h"
#include "../Data/Definitions/HSRQuestDefinition.h"
#include "../Quest/HSRQuestSubsystem.h"
#include "../UI/HSRQuestViewModel.h"
#include "Engine/GameInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRStageLayoutTest, "HSR.Presentation.StageLayout", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRStageLayoutTest::RunTest(const FString&)
{
    const AHSRBattleStage* Stage = GetDefault<AHSRBattleStage>();
    FTransform A, B;
    TestTrue(TEXT("Four players supported"), Stage->ResolveSlot(true, 3, false, A));
    TestTrue(TEXT("Five enemies supported"), Stage->ResolveSlot(false, 4, false, B));
    TestTrue(TEXT("Sides separated"), FVector::Dist(A.GetLocation(), B.GetLocation()) > 500.f);
    TestFalse(TEXT("Reject missing slot instead of spawning at origin"), Stage->ResolveSlot(true, 4, false, A));
    TestFalse(TEXT("Reject negative index"), Stage->ResolveSlot(false, -1, false, A));
    TestTrue(TEXT("Boss has dedicated slot"), Stage->ResolveSlot(false, 0, true, A));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRQuestDisplayTest, "HSR.Presentation.QuestDisplay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRQuestDisplayTest::RunTest(const FString&)
{
    UGameInstance* GI = NewObject<UGameInstance>();
    UHSRQuestSubsystem* Quest = NewObject<UHSRQuestSubsystem>(GI);
    UHSRQuestDefinition* Definition = NewObject<UHSRQuestDefinition>();
    Definition->QuestId = TEXT("Presentation.Quest");
    Definition->DisplayName = FText::FromString(TEXT("City echoes"));
    Definition->bAutoClaimReward = false;
    FHSRQuestObjectiveDefinition Objective;
    Objective.ObjectiveId = TEXT("Survey"); Objective.EventId = TEXT("Survey.Event");
    Objective.Description = FText::FromString(TEXT("Inspect the plaza"));
    Definition->Objectives.Add(Objective);
    TestEqual(TEXT("Register"), Quest->RegisterQuestDefinition(*Definition), EHSRQuestOperationResult::Success);
    FHSRQuestRuntimeState State;
    Quest->StartQuest(Definition->QuestId, State);
    UHSRQuestViewModel* VM = NewObject<UHSRQuestViewModel>(); VM->Initialize(Quest);
    FHSRQuestFrontendSnapshot Snapshot; VM->GetSnapshot(Snapshot);
    TestEqual(TEXT("One quest"), Snapshot.Quests.Num(), 1);
    if (Snapshot.Quests.Num() == 1)
    {
        TestEqual(TEXT("Authored title reaches UI"), Snapshot.Quests[0].DisplayName.ToString(), FString(TEXT("City echoes")));
        if (TestEqual(TEXT("One objective"), Snapshot.Quests[0].Objectives.Num(), 1))
            TestEqual(TEXT("Objective description reaches UI"), Snapshot.Quests[0].Objectives[0].Description.ToString(), FString(TEXT("Inspect the plaza")));
    }
    VM->Shutdown();
    return true;
}
#endif
