#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "../Data/Definitions/HSRQuestDefinition.h"
#include "../Quest/HSRQuestSubsystem.h"
#include "../UI/HSRQuestViewModel.h"
#include "../UI/HSRQuestWidget.h"
#include "../UI/HSRUIManagerSubsystem.h"
#include "../UI/Frontend/HSRFrontendRouter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRQuestFrontendProjectionTest,
	"HSR.UI.Quest.Projection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRQuestFrontendProjectionTest::RunTest(const FString&)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UHSRQuestSubsystem* Quest = NewObject<UHSRQuestSubsystem>(GameInstance);
	UHSRQuestDefinition* Definition = NewObject<UHSRQuestDefinition>();
	Definition->QuestId = TEXT("Quest.UI.Projection");
	Definition->Objectives.Add({TEXT("Objective.UI.First"), TEXT("Event.UI.First"), 2});
	Definition->Objectives.Add({TEXT("Objective.UI.Second"), TEXT("Event.UI.Second"), 1});
	Definition->RewardDefinitionId = TEXT("Reward.UI.Projection");
	Definition->bAutoClaimReward = false;
	TestEqual(TEXT("definition registers"), Quest->RegisterQuestDefinition(*Definition), EHSRQuestOperationResult::Success);

	FHSRQuestRuntimeState Runtime;
	TestEqual(TEXT("quest starts"), Quest->StartQuest(Definition->QuestId, Runtime), EHSRQuestOperationResult::Success);
	FHSRQuestDomainEvent Event;
	Event.EventId = TEXT("Event.UI.First");
	Event.Count = 1;
	TArray<FHSRQuestRuntimeState> Changed;
	TestEqual(TEXT("objective advances"), Quest->SubmitEvent(Event, Changed), EHSRQuestOperationResult::Success);

	UHSRQuestViewModel* ViewModel = NewObject<UHSRQuestViewModel>();
	ViewModel->Initialize(Quest);
	FHSRQuestFrontendSnapshot Snapshot;
	TestTrue(TEXT("snapshot is available"), ViewModel->GetSnapshot(Snapshot));
	TestEqual(TEXT("snapshot is ready"), Snapshot.Status, EHSRQuestFrontendStatus::Ready);
	TestEqual(TEXT("one quest projected"), Snapshot.Quests.Num(), 1);
	TestEqual(TEXT("two objectives projected"), Snapshot.Quests[0].Objectives.Num(), 2);
	TestEqual(TEXT("objective progress projected"), Snapshot.Quests[0].Objectives[0].CurrentCount, 1);
	TestFalse(TEXT("reward is not claimed"), Snapshot.Quests[0].bRewardClaimed);
	UHSRQuestWidget* Widget = NewObject<UHSRQuestWidget>();
	Widget->SetViewModel(ViewModel);
	Widget->AttachForAutomation();
	TestEqual(TEXT("widget binds once"), Widget->GetBindCountForAutomation(), 1);
	Widget->SetViewModel(nullptr);
	TestEqual(TEXT("widget unbinds old view model once"), Widget->GetUnbindCountForAutomation(), 1);
	ViewModel->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRQuestFrontendStableStatesTest,
	"HSR.UI.Quest.StableStates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRQuestFrontendStableStatesTest::RunTest(const FString&)
{
	UHSRQuestViewModel* Unavailable = NewObject<UHSRQuestViewModel>();
	Unavailable->Initialize(nullptr);
	FHSRQuestFrontendSnapshot Snapshot;
	TestTrue(TEXT("unavailable snapshot remains readable"), Unavailable->GetSnapshot(Snapshot));
	TestEqual(TEXT("unavailable status is explicit"), Snapshot.Status, EHSRQuestFrontendStatus::Unavailable);

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UHSRQuestSubsystem* EmptyQuest = NewObject<UHSRQuestSubsystem>(GameInstance);
	UHSRQuestViewModel* Empty = NewObject<UHSRQuestViewModel>();
	Empty->Initialize(EmptyQuest);
	TestTrue(TEXT("empty snapshot remains readable"), Empty->GetSnapshot(Snapshot));
	TestEqual(TEXT("empty status is explicit"), Snapshot.Status, EHSRQuestFrontendStatus::Empty);
	TestTrue(TEXT("empty quest list is stable"), Snapshot.Quests.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRQuestFrontendRouteLifecycleTest,
	"HSR.UI.Quest.RouteLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRQuestFrontendRouteLifecycleTest::RunTest(const FString&)
{
	ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	UHSRUIManagerSubsystem* Manager = NewObject<UHSRUIManagerSubsystem>(LocalPlayer);
	Manager->InitializeForAutomation();
	Manager->RegisterHostForAutomation();
	TestEqual(TEXT("quest route opens"), Manager->OpenFrontendModule(EHSRFrontendModule::Quest), EHSRUIScreenResult::Success);
	TestEqual(TEXT("quest route owns one module"), Manager->GetFrontendRouter()->GetSnapshot().GetActiveRoute().Module, EHSRFrontendModule::Quest);
	TestEqual(TEXT("repeat open is idempotent"), Manager->OpenFrontendModule(EHSRFrontendModule::Quest), EHSRUIScreenResult::NoOp);
	TestEqual(TEXT("back returns to hub"), Manager->RequestBack(), EHSRUIScreenResult::Success);
	TestEqual(TEXT("hub is restored"), Manager->GetFrontendRouter()->GetSnapshot().GetActiveRoute().Module, EHSRFrontendModule::PauseHub);
	TestEqual(TEXT("close restores exploration"), Manager->RequestBack(), EHSRUIScreenResult::Success);

	TestEqual(TEXT("quest reopens before travel"), Manager->OpenFrontendModule(EHSRFrontendModule::Quest), EHSRUIScreenResult::Success);
	TestEqual(TEXT("travel teardown succeeds"), Manager->TeardownHostIdentityForTravelForAutomation(1), EHSRUIScreenResult::Success);
	Manager->NotifyArrivalCommittedForAutomation(1);
	TestEqual(TEXT("new host registers"), Manager->RegisterHostIdentityForAutomation(2), EHSRUIScreenResult::Success);
	TestEqual(TEXT("quest reopens after travel"), Manager->OpenFrontendModule(EHSRFrontendModule::Quest), EHSRUIScreenResult::Success);
	TestEqual(TEXT("post-travel route is quest"), Manager->GetFrontendRouter()->GetSnapshot().GetActiveRoute().Module, EHSRFrontendModule::Quest);
	Manager->DeinitializeForAutomation();
	return true;
}

#endif
