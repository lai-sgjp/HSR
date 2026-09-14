#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "../UI/HSRQuestWidget.h"
#include "../UI/HSRMapWidget.h"
#include "../UI/HSRMapViewModel.h"
#include "../UI/HSRSaveWidget.h"
#include "../UI/HSRSaveViewModel.h"
#include "../Save/HSRSaveSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRQuestReadableCardTest, "HSR.UI.Presentation.QuestReadableCard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRQuestReadableCardTest::RunTest(const FString&)
{
	FHSRQuestViewData Quest;
	Quest.QuestId = TEXT("Internal.Quest.Id");
	Quest.DisplayName = FText::FromString(TEXT("界域回响"));
	Quest.State = EHSRQuestState::Active;
	FHSRQuestObjectiveViewData Objective;
	Objective.ObjectiveId = TEXT("Internal.Objective.Id");
	Objective.Description = FText::FromString(TEXT("调查广场的异常装置"));
	Objective.CurrentCount = 1;
	Objective.RequiredCount = 3;
	Quest.Objectives.Add(Objective);
	const FString Card = UHSRQuestWidget::FormatQuestCard(Quest).ToString();
	TestTrue(TEXT("title shown"), Card.Contains(TEXT("界域回响")));
	TestTrue(TEXT("objective shown"), Card.Contains(TEXT("调查广场的异常装置")));
	TestTrue(TEXT("progress shown"), Card.Contains(TEXT("1/3")));
	TestFalse(TEXT("stable internal IDs never leak into UI"), Card.Contains(TEXT("Internal.")));
	Quest.DisplayName = FText::GetEmpty();
	Quest.Objectives[0].Description = FText::GetEmpty();
	TestFalse(TEXT("missing localized labels still do not expose IDs"),
		UHSRQuestWidget::FormatQuestCard(Quest).ToString().Contains(TEXT("Internal.")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRDetachedRouteSnapshotTest, "HSR.UI.Presentation.DetachedRouteSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRDetachedRouteSnapshotTest::RunTest(const FString&)
{
	UGameInstance* Instance = NewObject<UGameInstance>();
	UHSRSaveSubsystem* Save = NewObject<UHSRSaveSubsystem>(Instance);
	UHSRSaveViewModel* ViewModel = NewObject<UHSRSaveViewModel>();
	ViewModel->Initialize(Save);
	UHSRSaveWidget* Widget = NewObject<UHSRSaveWidget>();
	Widget->SetViewModel(ViewModel);
	FHSRSaveFrontendResult Snapshot;
	TestTrue(TEXT("attached model supplies result"), Widget->GetCurrentResult(Snapshot));
	Widget->SetViewModel(nullptr);
	TestFalse(TEXT("detached page cannot display stale save state"), Widget->GetCurrentResult(Snapshot));
	ViewModel->Shutdown();
	return true;
}
#endif
