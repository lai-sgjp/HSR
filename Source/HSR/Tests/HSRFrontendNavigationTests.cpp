#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../UI/Frontend/HSRFrontendRouter.h"
#include "../UI/Frontend/HSRFrontendModuleRootWidget.h"
#include "../UI/HSRUserWidget.h"
#include "../UI/HSRScreenWidget.h"
#include "../UI/HSRUIManagerSubsystem.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "../Player/HSRPlayerController.h"

namespace HSR::P17::FrontendTests
{
	static FHSRFrontendRouteRequest Open(const int64 Token, const EHSRFrontendModule Module)
	{
		FHSRFrontendRouteRequest Request;
		Request.RequestToken = Token;
		Request.Route.Module = Module;
		return Request;
	}

	static FHSRFrontendRouteRequest Navigate(const int64 Token, const EHSRFrontendRouteOperation Operation)
	{
		FHSRFrontendRouteRequest Request;
		Request.RequestToken = Token;
		Request.Operation = Operation;
		return Request;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRFrontendRouterSequenceTest, "HSR.UI.FrontendNavigation.RouterSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRFrontendRouterSequenceTest::RunTest(const FString&)
{
	using namespace HSR::P17::FrontendTests;
	UHSRFrontendRouter* Router = NewObject<UHSRFrontendRouter>();

	TestEqual(TEXT("direct inventory opens through hub"), Router->Submit(Open(1, EHSRFrontendModule::Inventory)),
		EHSRFrontendRouteResult::Success);
	TestEqual(TEXT("hub plus inventory"), Router->GetSnapshot().History.Num(), 2);
	TestEqual(TEXT("inventory active"), Router->GetSnapshot().GetActiveRoute().Module, EHSRFrontendModule::Inventory);
	TestEqual(TEXT("same route is no-op"), Router->Submit(Open(2, EHSRFrontendModule::Inventory)),
		EHSRFrontendRouteResult::NoOp);
	TestEqual(TEXT("no-op consumes token"), Router->GetSnapshot().LastProcessedRequestToken, int64(2));
	TestEqual(TEXT("back reaches hub"), Router->Submit(Navigate(3, EHSRFrontendRouteOperation::Back)),
		EHSRFrontendRouteResult::Success);
	TestEqual(TEXT("hub active"), Router->GetSnapshot().GetActiveRoute().Module, EHSRFrontendModule::PauseHub);
	TestEqual(TEXT("open party"), Router->Submit(Open(4, EHSRFrontendModule::Party)), EHSRFrontendRouteResult::Success);
	TestEqual(TEXT("close to root"), Router->Submit(Navigate(5, EHSRFrontendRouteOperation::CloseToRoot)),
		EHSRFrontendRouteResult::Success);
	TestFalse(TEXT("router closed"), Router->GetSnapshot().IsOpen());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRFrontendRouterFailureTest, "HSR.UI.FrontendNavigation.RouterFailures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRFrontendRouterFailureTest::RunTest(const FString&)
{
	using namespace HSR::P17::FrontendTests;
	UHSRFrontendRouter* Router = NewObject<UHSRFrontendRouter>();
	TestEqual(TEXT("empty back"), Router->Submit(Navigate(1, EHSRFrontendRouteOperation::Back)),
		EHSRFrontendRouteResult::NothingOpen);
	TestEqual(TEXT("invalid module"), Router->Submit(Open(1, EHSRFrontendModule::None)),
		EHSRFrontendRouteResult::InvalidModule);
	TestEqual(TEXT("business rejection consumes token"), Router->GetSnapshot().LastProcessedRequestToken, int64(1));
	TestEqual(TEXT("valid open"), Router->Submit(Open(2, EHSRFrontendModule::PauseHub)),
		EHSRFrontendRouteResult::Success);
	TestEqual(TEXT("duplicate token"), Router->Submit(Open(2, EHSRFrontendModule::Map)),
		EHSRFrontendRouteResult::AlreadyProcessed);
	TestEqual(TEXT("negative token is structurally invalid"), Router->Submit(Open(-1, EHSRFrontendModule::Map)),
		EHSRFrontendRouteResult::InvalidRequest);
	FHSRFrontendRouteRequest PayloadBack = Navigate(3, EHSRFrontendRouteOperation::Back);
	PayloadBack.Route.Module = EHSRFrontendModule::Save;
	TestEqual(TEXT("back rejects payload"), Router->Submit(PayloadBack), EHSRFrontendRouteResult::InvalidRequest);
	TestEqual(TEXT("failure preserves hub"), Router->GetSnapshot().GetActiveRoute().Module,
		EHSRFrontendModule::PauseHub);
	TestEqual(TEXT("invalid structure does not consume token"), Router->GetSnapshot().LastProcessedRequestToken, int64(2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRFrontendRouterReplaceTest, "HSR.UI.FrontendNavigation.RouterReplace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRFrontendRouterReplaceTest::RunTest(const FString&)
{
	using namespace HSR::P17::FrontendTests;
	UHSRFrontendRouter* Router = NewObject<UHSRFrontendRouter>();
	TestEqual(TEXT("direct party"), Router->Submit(Open(1, EHSRFrontendModule::Party)), EHSRFrontendRouteResult::Success);
	TestEqual(TEXT("replace with map"), Router->Submit(Open(2, EHSRFrontendModule::Map)), EHSRFrontendRouteResult::Success);
	TestEqual(TEXT("history remains hub plus module"), Router->GetSnapshot().History.Num(), 2);
	TestEqual(TEXT("map active"), Router->GetSnapshot().GetActiveRoute().Module, EHSRFrontendModule::Map);
	TestEqual(TEXT("same positive token is rejected regardless of payload"),
		Router->Submit(Open(2, EHSRFrontendModule::Challenge)), EHSRFrontendRouteResult::AlreadyProcessed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRFrontendSharedSessionTest, "HSR.UI.FrontendNavigation.SharedSession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRFrontendSharedSessionTest::RunTest(const FString&)
{
	ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	UHSRUIManagerSubsystem* Manager = NewObject<UHSRUIManagerSubsystem>(LocalPlayer);
	Manager->InitializeForAutomation();
	Manager->RegisterHostForAutomation(true, true);
	Manager->ConfigureAutomationBackend(true, true, true, true, true, false);
	Manager->ConfigureAutomationInventoryBackend(true, true, true, true, true, true);
	TestEqual(TEXT("direct inventory opens shared shell and module"),
		Manager->OpenFrontendModule(EHSRFrontendModule::Inventory), EHSRUIScreenResult::Success);
	TestTrue(TEXT("inventory is hosted by the shared module root"), Manager->HasFrontendModuleRootForAutomation());
	TestTrue(TEXT("frontend owns one pause"), Manager->IsPausedForAutomation());
	TestEqual(TEXT("global stack remains root plus shell"), Manager->GetLogicalScreenCount(), 2);
	TestEqual(TEXT("inventory route active"), Manager->GetFrontendRouter()->GetSnapshot().GetActiveRoute().Module,
		EHSRFrontendModule::Inventory);
	TestEqual(TEXT("back returns to hub"), Manager->RequestBack(), EHSRUIScreenResult::Success);
	TestTrue(TEXT("back to hub remains paused"), Manager->IsPausedForAutomation());
	TestEqual(TEXT("hub route active"), Manager->GetFrontendRouter()->GetSnapshot().GetActiveRoute().Module,
		EHSRFrontendModule::PauseHub);
	TestEqual(TEXT("X closes the session"), Manager->CloseFrontendToRoot(), EHSRUIScreenResult::Success);
	TestFalse(TEXT("close releases owned pause"), Manager->IsPausedForAutomation());
	TestEqual(TEXT("exact exploration root"), Manager->GetLogicalScreenCount(), 1);
	Manager->DeinitializeForAutomation();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRFrontendPlaceholderReplaceTest, "HSR.UI.FrontendNavigation.PlaceholderReplace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRFrontendPlaceholderReplaceTest::RunTest(const FString&)
{
	ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	UHSRUIManagerSubsystem* Manager = NewObject<UHSRUIManagerSubsystem>(LocalPlayer);
	Manager->InitializeForAutomation();
	Manager->RegisterHostForAutomation(true, true);
	Manager->ConfigureAutomationBackend(true, true, true, true, true, false);
	TestEqual(TEXT("party placeholder opens"), Manager->OpenFrontendModule(EHSRFrontendModule::Party), EHSRUIScreenResult::Success);
	TestEqual(TEXT("map atomically replaces party"), Manager->OpenFrontendModule(EHSRFrontendModule::Map), EHSRUIScreenResult::Success);
	TestEqual(TEXT("replace preserves root plus shell depth"), Manager->GetLogicalScreenCount(), 2);
	TestEqual(TEXT("map route active"), Manager->GetFrontendRouter()->GetSnapshot().GetActiveRoute().Module, EHSRFrontendModule::Map);
	TestEqual(TEXT("close reaches exact root"), Manager->CloseFrontendToRoot(), EHSRUIScreenResult::Success);
	Manager->DeinitializeForAutomation();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRFrontendCrossTypeReplaceTest, "HSR.UI.FrontendNavigation.CrossTypeReplace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRFrontendCrossTypeReplaceTest::RunTest(const FString&)
{
	ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	UHSRUIManagerSubsystem* Manager = NewObject<UHSRUIManagerSubsystem>(LocalPlayer);
	Manager->InitializeForAutomation(); Manager->RegisterHostForAutomation(true, true);
	Manager->ConfigureAutomationBackend(true, true, true, true, true, false);
	Manager->ConfigureAutomationDetailBackend(true, true, true, true, true);
	Manager->ConfigureAutomationInventoryBackend(true, true, true, true, true, true);
	TestEqual(TEXT("placeholder opens"), Manager->OpenFrontendModule(EHSRFrontendModule::Party), EHSRUIScreenResult::Success);
	TestEqual(TEXT("placeholder to character replaces"), Manager->OpenFrontendModule(EHSRFrontendModule::Character), EHSRUIScreenResult::Success);
	TestTrue(TEXT("character is hosted by the shared module root"), Manager->HasFrontendModuleRootForAutomation());
	Manager->FailNextAutomationRouteSubmit();
	TestEqual(TEXT("failed character to inventory route is controlled"),
		Manager->OpenFrontendModule(EHSRFrontendModule::Inventory), EHSRUIScreenResult::StackRejected);
	TestEqual(TEXT("failed replace preserves character route"),
		Manager->GetFrontendRouter()->GetSnapshot().GetActiveRoute().Module, EHSRFrontendModule::Character);
	TestTrue(TEXT("failed replace preserves character ownership"), Manager->HasOpenCharacterDetailScreen());
	TestEqual(TEXT("failed replace restores character focus"), Manager->GetLastAutomationFocusModule(),
		EHSRFrontendModule::Character);
	TestEqual(TEXT("failed replace preserves root plus shell depth"), Manager->GetLogicalScreenCount(), 2);
	TestEqual(TEXT("character to inventory replaces"), Manager->OpenFrontendModule(EHSRFrontendModule::Inventory), EHSRUIScreenResult::Success);
	TestEqual(TEXT("inventory to map replaces"), Manager->OpenFrontendModule(EHSRFrontendModule::Map), EHSRUIScreenResult::Success);
	TestEqual(TEXT("fixed global root shell depth"), Manager->GetLogicalScreenCount(), 2);
	TestEqual(TEXT("router follows map"), Manager->GetFrontendRouter()->GetSnapshot().GetActiveRoute().Module, EHSRFrontendModule::Map);
	TestEqual(TEXT("cross type close"), Manager->CloseFrontendToRoot(), EHSRUIScreenResult::Success);
	Manager->DeinitializeForAutomation(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRFrontendFailureCompensationTest, "HSR.UI.FrontendNavigation.FailureCompensation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRFrontendFailureCompensationTest::RunTest(const FString&)
{
	ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	UHSRUIManagerSubsystem* Manager = NewObject<UHSRUIManagerSubsystem>(LocalPlayer);
	Manager->InitializeForAutomation(); Manager->RegisterHostForAutomation(true, true);
	Manager->ConfigureAutomationBackend(true, true, true, true, true, true);
	TestEqual(TEXT("external pause rejected"), Manager->OpenFrontendModule(EHSRFrontendModule::PauseHub), EHSRUIScreenResult::ExternalPause);
	TestEqual(TEXT("external pause preserves exact root"), Manager->GetLogicalScreenCount(), 1);
	Manager->ConfigureAutomationBackend(false, true, true, true, true, false);
	TestEqual(TEXT("shell create failure controlled"), Manager->OpenFrontendModule(EHSRFrontendModule::PauseHub), EHSRUIScreenResult::WidgetCreationFailed);
	Manager->ConfigureAutomationBackend(true, false, true, true, true, false);
	TestEqual(TEXT("shell attach failure controlled"), Manager->OpenFrontendModule(EHSRFrontendModule::PauseHub), EHSRUIScreenResult::ViewportAttachFailed);
	TestEqual(TEXT("attach compensation exact root"), Manager->GetLogicalScreenCount(), 1);
	Manager->ConfigureAutomationBackend(true, true, true, true, true, false);
	Manager->FailNextAutomationPolicyApply();
	TestEqual(TEXT("shell policy failure controlled"), Manager->OpenFrontendModule(EHSRFrontendModule::PauseHub), EHSRUIScreenResult::PolicyApplyFailed);
	TestEqual(TEXT("policy compensation exact root"), Manager->GetLogicalScreenCount(), 1);
	Manager->ConfigureAutomationBackend(true, true, true, true, true, false);
	Manager->FailNextAutomationPauseApply();
	TestEqual(TEXT("shell pause failure controlled"), Manager->OpenFrontendModule(EHSRFrontendModule::PauseHub), EHSRUIScreenResult::PauseApplyFailed);
	TestEqual(TEXT("pause compensation exact root"), Manager->GetLogicalScreenCount(), 1);
	Manager->ConfigureAutomationBackend(true, true, true, true, false, false);
	TestEqual(TEXT("hub focus failure controlled"), Manager->OpenFrontendModule(EHSRFrontendModule::PauseHub), EHSRUIScreenResult::FocusApplyFailed);
	TestEqual(TEXT("focus compensation exact root"), Manager->GetLogicalScreenCount(), 1);
	TestFalse(TEXT("focus compensation releases candidate pause"), Manager->IsPausedForAutomation());
	Manager->ConfigureAutomationBackend(true, true, true, true, true, false);
	TestEqual(TEXT("hub retry succeeds"), Manager->OpenFrontendModule(EHSRFrontendModule::PauseHub), EHSRUIScreenResult::Success);
	Manager->ConfigureAutomationDetailBackend(true, true, true, true, false);
	TestEqual(TEXT("detail focus failure controlled"), Manager->OpenFrontendModule(EHSRFrontendModule::Character), EHSRUIScreenResult::FocusApplyFailed);
	TestEqual(TEXT("failed detail preserves hub depth"), Manager->GetLogicalScreenCount(), 2);
	TestEqual(TEXT("failed detail preserves hub route"), Manager->GetFrontendRouter()->GetSnapshot().GetActiveRoute().Module, EHSRFrontendModule::PauseHub);
	Manager->ConfigureAutomationDetailBackend(true, true, true, true, true);
	TestEqual(TEXT("detail opens for close matrix"), Manager->OpenFrontendModule(EHSRFrontendModule::Character), EHSRUIScreenResult::Success);
	Manager->ConfigureAutomationBackend(true, true, true, true, true, true);
	Manager->FailNextAutomationPolicyApply();
	TestEqual(TEXT("close policy failure controlled"), Manager->CloseFrontendToRoot(), EHSRUIScreenResult::PolicyApplyFailed);
	TestEqual(TEXT("close policy failure preserves frontend depth"), Manager->GetLogicalScreenCount(), 2);
	Manager->ConfigureAutomationBackend(true, true, true, true, true, true);
	Manager->FailNextAutomationPauseApply();
	TestEqual(TEXT("close unpause failure controlled"), Manager->CloseFrontendToRoot(), EHSRUIScreenResult::PauseApplyFailed);
	TestEqual(TEXT("close unpause failure preserves frontend depth"), Manager->GetLogicalScreenCount(), 2);
	Manager->ConfigureAutomationBackend(true, true, true, true, false, true);
	TestEqual(TEXT("close focus failure controlled"), Manager->CloseFrontendToRoot(), EHSRUIScreenResult::FocusApplyFailed);
	TestEqual(TEXT("close focus failure preserves frontend depth"), Manager->GetLogicalScreenCount(), 2);
	Manager->ConfigureAutomationBackend(true, true, true, true, true, true);
	TestEqual(TEXT("close succeeds after failures"), Manager->CloseFrontendToRoot(), EHSRUIScreenResult::Success);
	Manager->DeinitializeForAutomation(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRFrontendTravelDiscardTest, "HSR.UI.FrontendNavigation.TravelDiscard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRFrontendTravelDiscardTest::RunTest(const FString&)
{
	ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	UHSRUIManagerSubsystem* Manager = NewObject<UHSRUIManagerSubsystem>(LocalPlayer);
	Manager->InitializeForAutomation(); Manager->RegisterHostIdentityForAutomation(10, true, true);
	Manager->ConfigureAutomationBackend(true, true, true, true, true, false);
	TestEqual(TEXT("map opens before travel"), Manager->OpenFrontendModule(EHSRFrontendModule::Map), EHSRUIScreenResult::Success);
	TestEqual(TEXT("travel teardown closes frontend"), Manager->TeardownHostIdentityForTravelForAutomation(10), EHSRUIScreenResult::Success);
	TestEqual(TEXT("travel exact root"), Manager->GetLogicalScreenCount(), 1);
	TestFalse(TEXT("travel discards route"), Manager->GetFrontendRouter()->GetSnapshot().IsOpen());
	Manager->NotifyArrivalCommittedForAutomation(1);
	TestEqual(TEXT("new host registers"), Manager->RegisterHostIdentityForAutomation(11, true, true), EHSRUIScreenResult::Success);
	TestFalse(TEXT("arrival does not restore frontend route"), Manager->GetFrontendRouter()->GetSnapshot().IsOpen());
	TestEqual(TEXT("stale old host rejected"), Manager->UnregisterHostIdentityForAutomation(10), EHSRUIScreenResult::InvalidHost);
	Manager->DeinitializeForAutomation(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRFrontendInputBindingGuardTest, "HSR.UI.FrontendNavigation.InputBindingGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRFrontendInputBindingGuardTest::RunTest(const FString&)
{
	const UInputComponent* First = reinterpret_cast<const UInputComponent*>(UPTRINT(1));
	const UInputComponent* Replacement = reinterpret_cast<const UInputComponent*>(UPTRINT(2));
	TestFalse(TEXT("null component is never bound"), AHSRPlayerController::ShouldBindFrontendInputComponent(nullptr, nullptr));
	TestTrue(TEXT("first setup binds"), AHSRPlayerController::ShouldBindFrontendInputComponent(nullptr, First));
	TestFalse(TEXT("repeated setup on same component is idempotent"), AHSRPlayerController::ShouldBindFrontendInputComponent(First, First));
	TestTrue(TEXT("recreated input component receives one new binding set"), AHSRPlayerController::ShouldBindFrontendInputComponent(First, Replacement));
	TestTrue(TEXT("first setup restores an unmarked frontend context"),
		AHSRPlayerController::ShouldRestoreFrontendNavigationContext(false, false));
	TestTrue(TEXT("cross-map reset restores a context even when its old controller marked it added"),
		AHSRPlayerController::ShouldRestoreFrontendNavigationContext(true, false));
	TestFalse(TEXT("present frontend context is not added twice"),
		AHSRPlayerController::ShouldRestoreFrontendNavigationContext(true, true));
	UHSRScreenWidget* Widget = NewObject<UHSRScreenWidget>();
	TestFalse(TEXT("unowned widget cannot consume X"), Widget->ShouldConsumeCloseToRootKeyForAutomation(EKeys::X));
	ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	UHSRUIManagerSubsystem* Manager = NewObject<UHSRUIManagerSubsystem>(LocalPlayer);
	Manager->InitializeForAutomation(); Manager->RegisterHostForAutomation(true, true);
	Manager->ConfigureAutomationBackend(true, true, true, true, true, false);
	Manager->ConfigureAutomationDetailBackend(true, true, true, true, true);
	Widget->SetOwningUIManager(Manager);
	TestTrue(TEXT("owned UIOnly widget consumes X for close-to-root"),
		Widget->ShouldConsumeCloseToRootKeyForAutomation(EKeys::X));
	TestFalse(TEXT("Escape remains back rather than close-to-root"),
		Widget->ShouldConsumeCloseToRootKeyForAutomation(EKeys::Escape));
	TestEqual(TEXT("UIOnly X fixture opens character"),
		Manager->OpenFrontendModule(EHSRFrontendModule::Character), EHSRUIScreenResult::Success);
	TestTrue(TEXT("UIOnly X routes the close transaction"), Widget->RouteCloseToRootKeyForAutomation(EKeys::X));
	TestEqual(TEXT("UIOnly X closes to exact root"), Manager->GetLogicalScreenCount(), 1);
	TestFalse(TEXT("UIOnly X closes Router history"), Manager->GetFrontendRouter()->GetSnapshot().IsOpen());
	TestFalse(TEXT("UIOnly X releases pause"), Manager->IsPausedForAutomation());
	Manager->DeinitializeForAutomation();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRFrontendDirectAndBackFailureTest, "HSR.UI.FrontendNavigation.DirectAndBackFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRFrontendDirectAndBackFailureTest::RunTest(const FString&)
{
	ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	UHSRUIManagerSubsystem* Manager = NewObject<UHSRUIManagerSubsystem>(LocalPlayer);
	Manager->InitializeForAutomation(); Manager->RegisterHostForAutomation(true, true);
	Manager->ConfigureAutomationBackend(true, true, true, true, true, false);
	Manager->ConfigureAutomationDetailBackend(false, true, true, true, true);
	TestEqual(TEXT("direct-root missing detail fails"), Manager->OpenFrontendModule(EHSRFrontendModule::Character), EHSRUIScreenResult::MissingWidgetClass);
	TestEqual(TEXT("direct-root failure restores exact root"), Manager->GetLogicalScreenCount(), 1);
	TestFalse(TEXT("direct-root failure unpauses"), Manager->IsPausedForAutomation());
	TestFalse(TEXT("direct-root failure closes router"), Manager->GetFrontendRouter()->GetSnapshot().IsOpen());
	Manager->ConfigureAutomationDetailBackend(true, true, true, true, true);
	TestEqual(TEXT("detail opens"), Manager->OpenFrontendModule(EHSRFrontendModule::Character), EHSRUIScreenResult::Success);
	Manager->ConfigureAutomationDetailBackend(true, true, true, true, false);
	TestEqual(TEXT("detail back focus failure"), Manager->RequestBack(), EHSRUIScreenResult::FocusApplyFailed);
	TestEqual(TEXT("focus failure preserves frontend depth"), Manager->GetLogicalScreenCount(), 2);
	Manager->ConfigureAutomationDetailBackend(true, true, true, true, true);
	Manager->FailNextAutomationRouteSubmit();
	TestEqual(TEXT("detail back route failure"), Manager->RequestBack(), EHSRUIScreenResult::StackRejected);
	TestEqual(TEXT("route failure preserves frontend depth"), Manager->GetLogicalScreenCount(), 2);
	TestTrue(TEXT("route failure preserves detail ownership"), Manager->HasOpenCharacterDetailScreen());
	TestEqual(TEXT("route failure preserves character route"), Manager->GetFrontendRouter()->GetSnapshot().GetActiveRoute().Module, EHSRFrontendModule::Character);
	TestEqual(TEXT("route failure restores character focus"), Manager->GetLastAutomationFocusModule(), EHSRFrontendModule::Character);
	Manager->FailNextAutomationRouteSubmit(); Manager->FailAutomationOldModuleFocusRestore();
	TestEqual(TEXT("failed old-module focus recovery reports compensation"), Manager->RequestBack(), EHSRUIScreenResult::CompensationFailed);
	TestTrue(TEXT("failed old-module focus recovery marks inconsistent"), Manager->IsInconsistent());
	Manager->DeinitializeForAutomation();
	ULocalPlayer* LocalPlayer2 = NewObject<ULocalPlayer>(GEngine);
	UHSRUIManagerSubsystem* Broken = NewObject<UHSRUIManagerSubsystem>(LocalPlayer2);
	Broken->InitializeForAutomation(); Broken->RegisterHostForAutomation(true, true);
	Broken->ConfigureAutomationBackend(true, true, false, true, true, false);
	TestEqual(TEXT("failed primary plus failed recovery reports compensation"),
		Broken->OpenFrontendModule(EHSRFrontendModule::PauseHub), EHSRUIScreenResult::CompensationFailed);
	TestTrue(TEXT("failed recovery marks inconsistent"), Broken->IsInconsistent());
	Broken->DeinitializeForAutomation();
	ULocalPlayer* LocalPlayer3 = NewObject<ULocalPlayer>(GEngine);
	UHSRUIManagerSubsystem* LateRecovery = NewObject<UHSRUIManagerSubsystem>(LocalPlayer3);
	LateRecovery->InitializeForAutomation(); LateRecovery->RegisterHostForAutomation(true, true);
	LateRecovery->ConfigureAutomationBackend(true, true, true, true, true, false);
	TestEqual(TEXT("late recovery fixture opens"), LateRecovery->OpenFrontendModule(EHSRFrontendModule::Map), EHSRUIScreenResult::Success);
	LateRecovery->ConfigureAutomationBackend(true, true, true, true, false, true);
	LateRecovery->FailAutomationPauseRestore();
	TestEqual(TEXT("failed pause recovery reports compensation"), LateRecovery->CloseFrontendToRoot(), EHSRUIScreenResult::CompensationFailed);
	TestTrue(TEXT("failed pause recovery marks inconsistent"), LateRecovery->IsInconsistent());
	LateRecovery->DeinitializeForAutomation(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRFrontendRecoveryMatrixTest, "HSR.UI.FrontendNavigation.RecoveryMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRFrontendRecoveryMatrixTest::RunTest(const FString&)
{
	auto MakeManager = []()
	{
		ULocalPlayer* LP = NewObject<ULocalPlayer>(GEngine);
		UHSRUIManagerSubsystem* M = NewObject<UHSRUIManagerSubsystem>(LP);
		M->InitializeForAutomation(); M->RegisterHostForAutomation(true, true);
		M->ConfigureAutomationBackend(true, true, true, true, true, false);
		return M;
	};
	UHSRUIManagerSubsystem* Hub = MakeManager();
	Hub->ConfigureAutomationBackend(true, true, true, true, false, false); Hub->FailSecondAutomationPauseApply();
	TestEqual(TEXT("hub focus plus failed unpause recovery"), Hub->OpenFrontendModule(EHSRFrontendModule::PauseHub), EHSRUIScreenResult::CompensationFailed);
	TestTrue(TEXT("hub recovery failure inconsistent"), Hub->IsInconsistent()); Hub->DeinitializeForAutomation();
	UHSRUIManagerSubsystem* Placeholder = MakeManager();
	TestEqual(TEXT("placeholder fixture hub"), Placeholder->OpenFrontendModule(EHSRFrontendModule::PauseHub), EHSRUIScreenResult::Success);
	Placeholder->ConfigureAutomationBackend(true, true, true, true, false, true); Placeholder->FailSecondAutomationPolicyApply();
	TestEqual(TEXT("placeholder focus plus failed policy recovery"), Placeholder->OpenFrontendModule(EHSRFrontendModule::Map), EHSRUIScreenResult::CompensationFailed);
	TestTrue(TEXT("placeholder recovery failure inconsistent"), Placeholder->IsInconsistent()); Placeholder->DeinitializeForAutomation();
	UHSRUIManagerSubsystem* Close = MakeManager();
	TestEqual(TEXT("close fixture map"), Close->OpenFrontendModule(EHSRFrontendModule::Map), EHSRUIScreenResult::Success);
	Close->FailNextAutomationPauseApply(); Close->FailSecondAutomationPolicyApply();
	TestEqual(TEXT("X unpause plus failed policy recovery"), Close->CloseFrontendToRoot(), EHSRUIScreenResult::CompensationFailed);
	TestTrue(TEXT("X recovery failure inconsistent"), Close->IsInconsistent()); Close->DeinitializeForAutomation();
	UHSRUIManagerSubsystem* RootPolicy = MakeManager();
	TestEqual(TEXT("root-policy fixture map"), RootPolicy->OpenFrontendModule(EHSRFrontendModule::Map), EHSRUIScreenResult::Success);
	RootPolicy->FailNextAutomationPolicyApply(); RootPolicy->FailSecondAutomationPolicyApply();
	TestEqual(TEXT("X primary policy plus failed old-policy recovery"), RootPolicy->CloseFrontendToRoot(), EHSRUIScreenResult::CompensationFailed);
	TestTrue(TEXT("X old-policy recovery failure inconsistent"), RootPolicy->IsInconsistent()); RootPolicy->DeinitializeForAutomation();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRFrontendDynamicModuleMountTest, "HSR.UI.FrontendDynamicMount.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRFrontendDynamicModuleMountTest::RunTest(const FString&)
{
	ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	UHSRUIManagerSubsystem* Manager = NewObject<UHSRUIManagerSubsystem>(LocalPlayer);
	Manager->InitializeForAutomation();
	Manager->RegisterHostForAutomation(true, true);
	Manager->ConfigureAutomationBackend(true, true, true, true, true, false);
	Manager->ConfigureAutomationFrontendModuleBackend(true, true, true);

	const EHSRFrontendModule Modules[] =
	{
		EHSRFrontendModule::Party,
		EHSRFrontendModule::Map,
		EHSRFrontendModule::Challenge,
		EHSRFrontendModule::Quest,
		EHSRFrontendModule::Save
	};
	for (const EHSRFrontendModule Module : Modules)
	{
		TestEqual(TEXT("module opens through dynamic content path"),
			Manager->OpenFrontendModule(Module), EHSRUIScreenResult::Success);
		TestEqual(TEXT("one dynamic content child is owned"),
			Manager->GetFrontendModuleContentCountForAutomation(), 1);
		TestEqual(TEXT("dynamic content matches route"),
			Manager->GetFrontendModuleContentModuleForAutomation(), Module);
	}

	TestEqual(TEXT("dynamic content closes with frontend root"),
		Manager->CloseFrontendToRoot(), EHSRUIScreenResult::Success);
	TestEqual(TEXT("close releases dynamic content"),
		Manager->GetFrontendModuleContentCountForAutomation(), 0);
	TestEqual(TEXT("frontend depth returns to exploration root"), Manager->GetLogicalScreenCount(), 1);
	Manager->DeinitializeForAutomation();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRFrontendDynamicModuleMountFailureTest, "HSR.UI.FrontendDynamicMount.Failures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRFrontendDynamicModuleMountFailureTest::RunTest(const FString&)
{
	auto MakeManager = []()
	{
		ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
		UHSRUIManagerSubsystem* Manager = NewObject<UHSRUIManagerSubsystem>(LocalPlayer);
		Manager->InitializeForAutomation();
		Manager->RegisterHostForAutomation(true, true);
		Manager->ConfigureAutomationBackend(true, true, true, true, true, false);
		return Manager;
	};

	UHSRUIManagerSubsystem* MissingClass = MakeManager();
	MissingClass->ConfigureAutomationFrontendModuleBackend(false, true, true);
	TestEqual(TEXT("missing module class is rejected"),
		MissingClass->OpenFrontendModule(EHSRFrontendModule::Party), EHSRUIScreenResult::MissingWidgetClass);
	TestEqual(TEXT("missing class restores exact root"), MissingClass->GetLogicalScreenCount(), 1);
	TestEqual(TEXT("missing class owns no content"),
		MissingClass->GetFrontendModuleContentCountForAutomation(), 0);
	MissingClass->DeinitializeForAutomation();

	UHSRUIManagerSubsystem* CreateFailure = MakeManager();
	CreateFailure->ConfigureAutomationFrontendModuleBackend(true, false, true);
	TestEqual(TEXT("module create failure is reported"),
		CreateFailure->OpenFrontendModule(EHSRFrontendModule::Map), EHSRUIScreenResult::WidgetCreationFailed);
	TestEqual(TEXT("create failure restores exact root"), CreateFailure->GetLogicalScreenCount(), 1);
	CreateFailure->DeinitializeForAutomation();

	UHSRUIManagerSubsystem* AttachFailure = MakeManager();
	AttachFailure->ConfigureAutomationFrontendModuleBackend(true, true, false);
	TestEqual(TEXT("module attach failure is reported"),
		AttachFailure->OpenFrontendModule(EHSRFrontendModule::Quest), EHSRUIScreenResult::ViewportAttachFailed);
	TestEqual(TEXT("attach failure owns no content"),
		AttachFailure->GetFrontendModuleContentCountForAutomation(), 0);
	AttachFailure->DeinitializeForAutomation();

	UHSRUIManagerSubsystem* ReplaceFailure = MakeManager();
	ReplaceFailure->ConfigureAutomationFrontendModuleBackend(true, true, true);
	TestEqual(TEXT("failure fixture opens Map"),
		ReplaceFailure->OpenFrontendModule(EHSRFrontendModule::Map), EHSRUIScreenResult::Success);
	ReplaceFailure->ConfigureAutomationBackend(true, true, true, true, false, false);
	TestEqual(TEXT("focus failure is reported"),
		ReplaceFailure->OpenFrontendModule(EHSRFrontendModule::Save), EHSRUIScreenResult::FocusApplyFailed);
	TestEqual(TEXT("focus failure preserves Map route"),
		ReplaceFailure->GetFrontendRouter()->GetSnapshot().GetActiveRoute().Module, EHSRFrontendModule::Map);
	TestEqual(TEXT("focus failure preserves Map content"),
		ReplaceFailure->GetFrontendModuleContentModuleForAutomation(), EHSRFrontendModule::Map);
	ReplaceFailure->ConfigureAutomationBackend(true, true, true, true, true, true);
	ReplaceFailure->FailNextAutomationRouteSubmit();
	TestEqual(TEXT("route failure is reported"),
		ReplaceFailure->OpenFrontendModule(EHSRFrontendModule::Save), EHSRUIScreenResult::StackRejected);
	TestEqual(TEXT("route failure preserves Map content"),
		ReplaceFailure->GetFrontendModuleContentModuleForAutomation(), EHSRFrontendModule::Map);
	ReplaceFailure->DeinitializeForAutomation();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRFrontendDynamicModuleSlotLayoutTest, "HSR.UI.FrontendDynamicMount.SlotLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRFrontendDynamicModuleSlotLayoutTest::RunTest(const FString&)
{
	UHSRFrontendModuleRootWidget* Root = NewObject<UHSRFrontendModuleRootWidget>();
	UOverlay* Host = NewObject<UOverlay>(Root);
	UHSRUserWidget* Content = NewObject<UHSRUserWidget>(Root);
	Content->SetVisibility(ESlateVisibility::Collapsed);
	Root->SetModuleContentHostForAutomation(Host);

	TestTrue(TEXT("dynamic content is attached to the host"), Root->SetModuleContent(Content));
	TestEqual(TEXT("host owns exactly one dynamic child"), Host->GetChildrenCount(), 1);
	TestEqual(TEXT("dynamic content is visible when mounted"), Content->GetVisibility(), ESlateVisibility::Visible);
	UPanelSlot* MountedSlot = Host->GetSlots().Num() == 1 ? Host->GetSlots()[0] : nullptr;
	UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(MountedSlot);
	TestNotNull(TEXT("dynamic content receives an OverlaySlot"), OverlaySlot);
	if (OverlaySlot)
	{
		TestEqual(TEXT("dynamic content fills horizontally"), OverlaySlot->GetHorizontalAlignment(), HAlign_Fill);
		TestEqual(TEXT("dynamic content fills vertically"), OverlaySlot->GetVerticalAlignment(), VAlign_Fill);
		TestEqual(TEXT("dynamic content has no runtime padding"), OverlaySlot->GetPadding(), FMargin(0.f));
	}

	UHSRFrontendModuleRootWidget* CanvasRoot = NewObject<UHSRFrontendModuleRootWidget>();
	UCanvasPanel* CanvasHost = NewObject<UCanvasPanel>(CanvasRoot);
	UHSRUserWidget* CanvasContent = NewObject<UHSRUserWidget>(CanvasRoot);
	CanvasRoot->SetModuleContentHostForAutomation(CanvasHost);
	TestTrue(TEXT("dynamic content attaches to a CanvasPanel host"), CanvasRoot->SetModuleContent(CanvasContent));
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(CanvasHost->GetSlots().Num() == 1 ? CanvasHost->GetSlots()[0] : nullptr);
	TestNotNull(TEXT("dynamic content receives a CanvasPanelSlot"), CanvasSlot);
	if (CanvasSlot)
	{
		const FAnchors Anchors = CanvasSlot->GetAnchors();
		TestEqual(TEXT("CanvasPanel content anchors at the top left"), Anchors.Minimum, FVector2D::ZeroVector);
		TestEqual(TEXT("CanvasPanel content anchors at the bottom right"), Anchors.Maximum, FVector2D::UnitVector);
		TestEqual(TEXT("CanvasPanel content uses zero offsets"), CanvasSlot->GetOffsets(), FMargin(0.f));
		TestEqual(TEXT("CanvasPanel content is not auto-sized"), CanvasSlot->GetAutoSize(), false);
	}
	return true;
}

#endif
