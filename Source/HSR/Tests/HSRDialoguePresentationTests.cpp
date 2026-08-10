#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../Data/Definitions/HSRDialogueDefinition.h"
#include "../Dialogue/HSRDialogueSubsystem.h"
#include "../UI/Dialogue/HSRDialoguePresentationTypes.h"
#include "../UI/Dialogue/HSRDialoguePresentationViewModel.h"

namespace HSR::P17::DialogueTests
{
	static UHSRDialogueDefinition* MakeDefinition()
	{
		UHSRDialogueDefinition* Definition = NewObject<UHSRDialogueDefinition>();
		Definition->DialogueId = TEXT("Dialogue.P17.Contract");
		Definition->StartNodeId = TEXT("Node.Start");

		FHSRDialogueNodeDefinition Start;
		Start.NodeId = TEXT("Node.Start");
		Start.SpeakerText = FText::FromString(TEXT("Archivist"));
		Start.Text = FText::FromString(TEXT("A contract dialogue body."));
		FHSRDialogueChoiceDefinition Continue;
		Continue.ChoiceId = TEXT("Choice.Continue");
		Continue.TargetNodeId = TEXT("Node.End");
		Continue.QuestEventId = TEXT("QuestEvent.P17.Continue");
		Continue.DisplayText = FText::FromString(TEXT("Continue"));
		Start.Choices.Add(Continue);

		FHSRDialogueNodeDefinition End;
		End.NodeId = TEXT("Node.End");
		End.SpeakerText = FText::FromString(TEXT("Archivist"));
		End.Text = FText::FromString(TEXT("The dialogue ends."));
		Definition->Nodes = {Start, End};
		return Definition;
	}

	static UHSRDialoguePresentationViewModel* MakeViewModel(UHSRDialogueSubsystem*& OutDialogue)
	{
		UGameInstance* GameInstance = NewObject<UGameInstance>();
		OutDialogue = NewObject<UHSRDialogueSubsystem>(GameInstance);
		OutDialogue->InitializeForAutomation(nullptr);
		if (UHSRDialogueDefinition* Definition = MakeDefinition())
		{
			OutDialogue->RegisterDialogueDefinition(*Definition);
		}

		UHSRDialoguePresentationViewModel* ViewModel = NewObject<UHSRDialoguePresentationViewModel>(OutDialogue);
		ViewModel->Initialize(OutDialogue);
		return ViewModel;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRDialoguePresentationActiveQueryTest,
	"HSR.Dialogue.Presentation.ActiveQuery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRDialoguePresentationActiveQueryTest::RunTest(const FString&)
{
	using namespace HSR::P17::DialogueTests;
	UHSRDialogueSubsystem* Dialogue = nullptr;
	UHSRDialoguePresentationViewModel* ViewModel = MakeViewModel(Dialogue);
	TestNotNull(TEXT("dialogue authority exists"), Dialogue);
	TestNotNull(TEXT("presentation ViewModel exists"), ViewModel);

	FHSRDialoguePresentationRequest Request;
	Request.QueryId = FGuid(17, 1, 0, 1);
	Request.DialogueId = TEXT("Dialogue.P17.Contract");
	Request.NodeId = TEXT("Node.Start");
	TestTrue(TEXT("request has stable active-query identity"), Request.IsValid());
	TestEqual(TEXT("begin dialogue"), ViewModel->BeginDialogue(Request), EHSRDialoguePresentationResult::Success);

	FHSRDialoguePresentationSnapshot Snapshot;
	TestTrue(TEXT("snapshot available"), ViewModel->GetSnapshot(Snapshot));
	TestTrue(TEXT("snapshot valid"), Snapshot.bIsValid);
	TestEqual(TEXT("snapshot keeps query id"), Snapshot.QueryId, Request.QueryId);
	TestEqual(TEXT("snapshot keeps dialogue id"), Snapshot.DialogueId, Request.DialogueId);
	TestEqual(TEXT("snapshot keeps node id"), Snapshot.NodeId, Request.NodeId);
	TestEqual(TEXT("snapshot speaker"), Snapshot.SpeakerText.ToString(), FString(TEXT("Archivist")));
	TestEqual(TEXT("snapshot body"), Snapshot.BodyText.ToString(), FString(TEXT("A contract dialogue body.")));
	TestEqual(TEXT("snapshot choice count"), Snapshot.Choices.Num(), 1);
	if (Snapshot.Choices.Num() == 1)
	{
		TestEqual(TEXT("choice identity is stable"), Snapshot.Choices[0].ChoiceId, FName(TEXT("Choice.Continue")));
		TestEqual(TEXT("choice display text is authored"), Snapshot.Choices[0].DisplayText.ToString(), FString(TEXT("Continue")));
	}

	TestEqual(TEXT("repeating the active query is a no-op"),
		ViewModel->BeginDialogue(Request), EHSRDialoguePresentationResult::NoOp);
	FHSRDialoguePresentationSnapshot AfterRepeat;
	ViewModel->GetSnapshot(AfterRepeat);
	TestEqual(TEXT("repeat keeps active query"), AfterRepeat.QueryId, Snapshot.QueryId);
	TestEqual(TEXT("repeat keeps current node"), AfterRepeat.NodeId, Snapshot.NodeId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRDialoguePresentationFailurePreservesSnapshotTest,
	"HSR.Dialogue.Presentation.FailurePreservesSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRDialoguePresentationFailurePreservesSnapshotTest::RunTest(const FString&)
{
	using namespace HSR::P17::DialogueTests;
	UHSRDialogueSubsystem* Dialogue = nullptr;
	UHSRDialoguePresentationViewModel* ViewModel = MakeViewModel(Dialogue);

	FHSRDialoguePresentationRequest Request;
	Request.QueryId = FGuid(17, 1, 0, 2);
	Request.DialogueId = TEXT("Dialogue.P17.Contract");
	Request.NodeId = TEXT("Node.Start");
	TestEqual(TEXT("baseline begin"), ViewModel->BeginDialogue(Request), EHSRDialoguePresentationResult::Success);

	FHSRDialoguePresentationSnapshot BeforeFailure;
	ViewModel->GetSnapshot(BeforeFailure);
	FHSRDialoguePresentationRequest InvalidRequest = Request;
	InvalidRequest.QueryId.Invalidate();
	TestEqual(TEXT("invalid query is rejected"),
		ViewModel->BeginDialogue(InvalidRequest), EHSRDialoguePresentationResult::InvalidRequest);

	FHSRDialoguePresentationSnapshot AfterFailure;
	ViewModel->GetSnapshot(AfterFailure);
	TestEqual(TEXT("failure preserves query id"), AfterFailure.QueryId, BeforeFailure.QueryId);
	TestEqual(TEXT("failure preserves dialogue id"), AfterFailure.DialogueId, BeforeFailure.DialogueId);
	TestEqual(TEXT("failure preserves node id"), AfterFailure.NodeId, BeforeFailure.NodeId);
	TestEqual(TEXT("failure preserves body"), AfterFailure.BodyText.ToString(), BeforeFailure.BodyText.ToString());
	TestEqual(TEXT("failure preserves choices"), AfterFailure.Choices.Num(), BeforeFailure.Choices.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRDialoguePresentationSelectionTest,
	"HSR.Dialogue.Presentation.Selection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRDialoguePresentationSelectionTest::RunTest(const FString&)
{
	using namespace HSR::P17::DialogueTests;
	UHSRDialogueSubsystem* Dialogue = nullptr;
	UHSRDialoguePresentationViewModel* ViewModel = MakeViewModel(Dialogue);

	FHSRDialoguePresentationRequest Request;
	Request.QueryId = FGuid(17, 1, 0, 3);
	Request.DialogueId = TEXT("Dialogue.P17.Contract");
	Request.NodeId = TEXT("Node.Start");
	TestEqual(TEXT("begin before selection"), ViewModel->BeginDialogue(Request), EHSRDialoguePresentationResult::Success);

	FHSRDialoguePresentationChoiceRequest ChoiceRequest;
	ChoiceRequest.QueryId = Request.QueryId;
	ChoiceRequest.DialogueId = Request.DialogueId;
	ChoiceRequest.NodeId = Request.NodeId;
	ChoiceRequest.ChoiceId = TEXT("Choice.Continue");
	TestTrue(TEXT("choice request is stable"), ChoiceRequest.IsValid());
	TestEqual(TEXT("choice advances through Dialogue authority"),
		ViewModel->SubmitChoice(ChoiceRequest), EHSRDialoguePresentationResult::Success);

	FHSRDialoguePresentationSnapshot Snapshot;
	TestTrue(TEXT("advanced snapshot available"), ViewModel->GetSnapshot(Snapshot));
	TestEqual(TEXT("selection keeps query id"), Snapshot.QueryId, Request.QueryId);
	TestEqual(TEXT("selection advances node"), Snapshot.NodeId, FName(TEXT("Node.End")));
	TestEqual(TEXT("advanced speaker"), Snapshot.SpeakerText.ToString(), FString(TEXT("Archivist")));
	TestEqual(TEXT("advanced body"), Snapshot.BodyText.ToString(), FString(TEXT("The dialogue ends.")));
	TestTrue(TEXT("terminal node has no choices"), Snapshot.Choices.IsEmpty());
	TestFalse(TEXT("presentation preview does not submit quest branch"), ViewModel->GetLastChoiceResult().bQuestEventSubmitted);

	FHSRDialoguePresentationChoiceRequest StaleRequest = ChoiceRequest;
	TestEqual(TEXT("stale node request is rejected"),
		ViewModel->SubmitChoice(StaleRequest), EHSRDialoguePresentationResult::StaleRequest);
	FHSRDialoguePresentationSnapshot AfterStale;
	ViewModel->GetSnapshot(AfterStale);
	TestEqual(TEXT("stale request preserves node"), AfterStale.NodeId, Snapshot.NodeId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRDialoguePresentationExitAndUnavailableTest,
	"HSR.Dialogue.Presentation.ExitAndUnavailable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRDialoguePresentationExitAndUnavailableTest::RunTest(const FString&)
{
	using namespace HSR::P17::DialogueTests;
	UHSRDialoguePresentationViewModel* UnavailableViewModel = NewObject<UHSRDialoguePresentationViewModel>();
	UnavailableViewModel->Initialize(nullptr);

	FHSRDialoguePresentationRequest Request;
	Request.QueryId = FGuid(17, 1, 0, 4);
	Request.DialogueId = TEXT("Dialogue.P17.Contract");
	Request.NodeId = TEXT("Node.Start");
	TestEqual(TEXT("missing Dialogue authority is unavailable"),
		UnavailableViewModel->BeginDialogue(Request), EHSRDialoguePresentationResult::Unavailable);
	FHSRDialoguePresentationSnapshot UnavailableSnapshot;
	TestFalse(TEXT("unavailable snapshot is not active"), UnavailableViewModel->GetSnapshot(UnavailableSnapshot));
	TestEqual(TEXT("unavailable status is explicit"), UnavailableSnapshot.Status, EHSRDialoguePresentationStatus::Unavailable);

	UHSRDialogueSubsystem* Dialogue = nullptr;
	UHSRDialoguePresentationViewModel* ViewModel = MakeViewModel(Dialogue);
	TestEqual(TEXT("begin before exit"), ViewModel->BeginDialogue(Request), EHSRDialoguePresentationResult::Success);
	TestEqual(TEXT("matching query exits"), ViewModel->ExitDialogue(Request.QueryId), EHSRDialoguePresentationResult::Success);
	FHSRDialoguePresentationSnapshot ClosedSnapshot;
	TestFalse(TEXT("closed snapshot is not active"), ViewModel->GetSnapshot(ClosedSnapshot));
	TestEqual(TEXT("closed status is explicit"), ClosedSnapshot.Status, EHSRDialoguePresentationStatus::Closed);
	TestEqual(TEXT("closed query identity is retained"), ClosedSnapshot.QueryId, Request.QueryId);
	return true;
}

#endif
