#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "InputCoreTypes.h"
#include "../Interaction/HSRInteractionTypes.h"
#include "../UI/Dialogue/HSRDialogueOverlayWidget.h"
#include "../UI/HSRUIManagerSubsystem.h"

namespace HSR::P17::DialogueOverlayTests
{
	static UHSRUIManagerSubsystem* MakeManager()
	{
		ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
		UHSRUIManagerSubsystem* Manager = NewObject<UHSRUIManagerSubsystem>(LocalPlayer);
		Manager->InitializeForAutomation();
		Manager->RegisterHostForAutomation(true, true);
		Manager->ConfigureAutomationBackend(true, true, true, true, true, false);
		return Manager;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRDialogueInteractionPayloadTest,
	"HSR.Dialogue.Overlay.InteractionPayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRDialogueInteractionPayloadTest::RunTest(const FString&)
{
	const FHSRInteractionResult Result = FHSRInteractionResult::MakeDialogueSuccess(
		TEXT("Dialogue.P17.Demo"), TEXT("Node.Start"));
	TestTrue(TEXT("Dialogue interaction remains successful"), Result.bSuccess);
	TestEqual(TEXT("typed Dialogue payload is explicit"), Result.PayloadType,
		EHSRInteractionPayloadType::Dialogue);
	TestEqual(TEXT("payload carries stable DialogueId"), Result.DialogueId,
		FName(TEXT("Dialogue.P17.Demo")));
	TestEqual(TEXT("payload carries stable start NodeId"), Result.DialogueNodeId,
		FName(TEXT("Node.Start")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRDialogueOverlayWidgetSeamsTest,
	"HSR.Dialogue.Overlay.WidgetSeams",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRDialogueOverlayWidgetSeamsTest::RunTest(const FString&)
{
	UHSRDialogueOverlayWidget* Widget = NewObject<UHSRDialogueOverlayWidget>();
	FHSRDialoguePresentationSnapshot Snapshot;
	FHSRDialoguePresentationChoice Choice;
	TestFalse(TEXT("unbound Overlay has no snapshot"), Widget->GetSnapshot(Snapshot));
	TestEqual(TEXT("unbound Overlay exposes zero choices"), Widget->GetChoiceCount(), 0);
	TestFalse(TEXT("out-of-range choice access is bounds safe"), Widget->GetChoiceAt(0, Choice));
	TestTrue(TEXT("Escape is a Dialogue back key"), Widget->ShouldConsumeBackKeyForAutomation(EKeys::Escape));
	TestTrue(TEXT("Gamepad Back is a Dialogue back key"),
		Widget->ShouldConsumeBackKeyForAutomation(EKeys::Gamepad_Special_Right));
	TestTrue(TEXT("X is a Dialogue close-to-root key"),
		Widget->ShouldConsumeCloseToRootKeyForAutomation(EKeys::X));
	TestFalse(TEXT("unrelated key is not consumed"), Widget->ShouldConsumeBackKeyForAutomation(EKeys::T));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRDialogueOverlayLifecycleTest,
	"HSR.Dialogue.Overlay.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRDialogueOverlayLifecycleTest::RunTest(const FString&)
{
	using namespace HSR::P17::DialogueOverlayTests;
	UHSRUIManagerSubsystem* Manager = MakeManager();
	Manager->ConfigureAutomationDialogueOverlayBackend(true, true, true);

	TestEqual(TEXT("first Dialogue opens"), Manager->OpenDialogueOverlay(
		TEXT("Dialogue.P17.Demo"), TEXT("Node.Start")), EHSRUIScreenResult::Success);
	TestTrue(TEXT("one Overlay is owned"), Manager->HasOpenDialogueOverlay());
	TestEqual(TEXT("second Dialogue does not create a second Overlay"),
		Manager->OpenDialogueOverlay(TEXT("Dialogue.P17.Other"), TEXT("Node.Start")),
		EHSRUIScreenResult::AlreadyOpen);
	TestTrue(TEXT("duplicate-open failure preserves Overlay"), Manager->HasOpenDialogueOverlay());
	TestEqual(TEXT("Escape/X close transaction succeeds"), Manager->CloseDialogueOverlay(),
		EHSRUIScreenResult::Success);
	TestFalse(TEXT("close releases Overlay"), Manager->HasOpenDialogueOverlay());
	TestEqual(TEXT("repeat close is a no-op boundary"), Manager->CloseDialogueOverlay(),
		EHSRUIScreenResult::NothingOpen);

	TestEqual(TEXT("Overlay can reopen after clean close"), Manager->OpenDialogueOverlay(
		TEXT("Dialogue.P17.Demo"), TEXT("Node.Start")), EHSRUIScreenResult::Success);
	TestEqual(TEXT("ordinary host teardown closes Overlay"),
		Manager->UnregisterHostIdentityForAutomation(1), EHSRUIScreenResult::Success);
	TestFalse(TEXT("ordinary teardown leaves no stale Overlay"), Manager->HasOpenDialogueOverlay());
	Manager->DeinitializeForAutomation();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRDialogueOverlayFailureAndTravelTest,
	"HSR.Dialogue.Overlay.FailureAndTravel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRDialogueOverlayFailureAndTravelTest::RunTest(const FString&)
{
	using namespace HSR::P17::DialogueOverlayTests;

	UHSRUIManagerSubsystem* MissingClass = MakeManager();
	MissingClass->ConfigureAutomationDialogueOverlayBackend(false, true, true);
	TestEqual(TEXT("missing Overlay class is typed"), MissingClass->OpenDialogueOverlay(
		TEXT("Dialogue.P17.Demo"), TEXT("Node.Start")), EHSRUIScreenResult::MissingWidgetClass);
	TestFalse(TEXT("missing class keeps Overlay closed"), MissingClass->HasOpenDialogueOverlay());
	MissingClass->DeinitializeForAutomation();

	UHSRUIManagerSubsystem* CreateFailure = MakeManager();
	CreateFailure->ConfigureAutomationDialogueOverlayBackend(true, false, true);
	TestEqual(TEXT("Overlay create failure is typed"), CreateFailure->OpenDialogueOverlay(
		TEXT("Dialogue.P17.Demo"), TEXT("Node.Start")), EHSRUIScreenResult::WidgetCreationFailed);
	TestFalse(TEXT("create failure leaves no Overlay"), CreateFailure->HasOpenDialogueOverlay());
	CreateFailure->DeinitializeForAutomation();

	UHSRUIManagerSubsystem* AttachFailure = MakeManager();
	AttachFailure->ConfigureAutomationDialogueOverlayBackend(true, true, false);
	TestEqual(TEXT("Overlay attach failure is typed"), AttachFailure->OpenDialogueOverlay(
		TEXT("Dialogue.P17.Demo"), TEXT("Node.Start")), EHSRUIScreenResult::ViewportAttachFailed);
	TestFalse(TEXT("attach failure leaves no Overlay"), AttachFailure->HasOpenDialogueOverlay());
	AttachFailure->DeinitializeForAutomation();

	UHSRUIManagerSubsystem* PolicyFailure = MakeManager();
	PolicyFailure->ConfigureAutomationDialogueOverlayBackend(true, true, true);
	PolicyFailure->FailNextAutomationPolicyApply();
	TestEqual(TEXT("Overlay policy failure is typed"), PolicyFailure->OpenDialogueOverlay(
		TEXT("Dialogue.P17.Demo"), TEXT("Node.Start")), EHSRUIScreenResult::PolicyApplyFailed);
	TestFalse(TEXT("policy failure leaves no Overlay"), PolicyFailure->HasOpenDialogueOverlay());
	PolicyFailure->DeinitializeForAutomation();

	UHSRUIManagerSubsystem* FocusFailure = MakeManager();
	FocusFailure->ConfigureAutomationDialogueOverlayBackend(true, true, true);
	FocusFailure->ConfigureAutomationBackend(true, true, true, true, false, false);
	TestEqual(TEXT("Overlay focus failure is typed"), FocusFailure->OpenDialogueOverlay(
		TEXT("Dialogue.P17.Demo"), TEXT("Node.Start")), EHSRUIScreenResult::FocusApplyFailed);
	TestFalse(TEXT("focus failure leaves no Overlay"), FocusFailure->HasOpenDialogueOverlay());
	FocusFailure->DeinitializeForAutomation();

	UHSRUIManagerSubsystem* Travel = MakeManager();
	Travel->ConfigureAutomationDialogueOverlayBackend(true, true, true);
	TestEqual(TEXT("travel fixture opens Overlay"), Travel->OpenDialogueOverlay(
		TEXT("Dialogue.P17.Demo"), TEXT("Node.Start")), EHSRUIScreenResult::Success);
	TestEqual(TEXT("travel teardown succeeds"), Travel->TeardownHostIdentityForTravelForAutomation(1),
		EHSRUIScreenResult::Success);
	TestFalse(TEXT("travel teardown releases Overlay"), Travel->HasOpenDialogueOverlay());
	Travel->NotifyArrivalCommittedForAutomation(1);
	TestEqual(TEXT("arrival host registers cleanly"), Travel->RegisterHostIdentityForAutomation(2),
		EHSRUIScreenResult::Success);
	TestFalse(TEXT("arrival does not restore stale Dialogue"), Travel->HasOpenDialogueOverlay());
	Travel->DeinitializeForAutomation();
	return true;
}

#endif
