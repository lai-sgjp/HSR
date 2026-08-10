#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "InputCoreTypes.h"
#include "../UI/HSRInputModeCoordinator.h"
#include "../UI/HSRScreenStack.h"
#include "../UI/HSRUIManagerSubsystem.h"
#include "../UI/HSRScreenWidget.h"
#include "../UI/HSRHUD.h"

namespace HSR::P17::LifecycleTests
{
	static UHSRUIManagerSubsystem* MakeManager()
	{
		ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
		UHSRUIManagerSubsystem* Manager = NewObject<UHSRUIManagerSubsystem>(LocalPlayer);
		Manager->InitializeForAutomation();
		return Manager;
	}

	static int32 EntryCount(const UHSRUIManagerSubsystem* Manager)
	{
		return Manager && Manager->GetScreenStack() ? Manager->GetScreenStack()->GetSnapshot().Entries.Num() : 0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUIScreenLifecycleHappyPathTest, "HSR.UI.ScreenLifecycle.HappyPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUIScreenLifecycleHappyPathTest::RunTest(const FString&)
{
	using namespace HSR::P17::LifecycleTests;
	UHSRUIManagerSubsystem* Manager = MakeManager();
	Manager->RegisterHostForAutomation();
	Manager->ConfigureAutomationBackend(true, true, true, true, true);
	TestEqual(TEXT("root registered"), EntryCount(Manager), 1);
	TestEqual(TEXT("open pause"), Manager->OpenPauseScreen(), EHSRUIScreenResult::Success);
	TestTrue(TEXT("pause instance owned"), Manager->HasOpenPauseScreen());
	TestEqual(TEXT("modal pushed"), EntryCount(Manager), 2);
	TestEqual(TEXT("repeat open rejected"), Manager->OpenPauseScreen(), EHSRUIScreenResult::AlreadyOpen);
	TestEqual(TEXT("repeat open does not duplicate"), EntryCount(Manager), 2);
	TestFalse(TEXT("successful open stays consistent"), Manager->IsInconsistent());
	TestEqual(TEXT("back closes pause"), Manager->RequestBack(), EHSRUIScreenResult::Success);
	TestFalse(TEXT("pause instance cleared"), Manager->HasOpenPauseScreen());
	TestEqual(TEXT("root restored"), EntryCount(Manager), 1);
	TestEqual(TEXT("empty back rejected"), Manager->RequestBack(), EHSRUIScreenResult::NothingOpen);
	Manager->DeinitializeForAutomation();
	TestEqual(TEXT("post deinitialize rejected"), Manager->OpenPauseScreen(), EHSRUIScreenResult::NotInitialized);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUIScreenLifecycleCandidateFailureTest, "HSR.UI.ScreenLifecycle.CandidateFailures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUIScreenLifecycleCandidateFailureTest::RunTest(const FString&)
{
	using namespace HSR::P17::LifecycleTests;
	UHSRUIManagerSubsystem* Manager = MakeManager();
	TestEqual(TEXT("host required"), Manager->OpenPauseScreen(), EHSRUIScreenResult::InvalidHost);
	Manager->RegisterHostForAutomation(true, false);
	TestEqual(TEXT("class required"), Manager->OpenPauseScreen(), EHSRUIScreenResult::MissingWidgetClass);
	TestEqual(TEXT("missing class preserves root"), EntryCount(Manager), 1);
	Manager->RegisterHostForAutomation(true, true);
	Manager->ConfigureAutomationBackend(false, true, true, true, true);
	TestEqual(TEXT("factory failure"), Manager->OpenPauseScreen(), EHSRUIScreenResult::WidgetCreationFailed);
	TestEqual(TEXT("factory failure preserves root"), EntryCount(Manager), 1);
	Manager->ConfigureAutomationBackend(true, false, true, true, true);
	TestEqual(TEXT("attach failure compensated"), Manager->OpenPauseScreen(), EHSRUIScreenResult::ViewportAttachFailed);
	TestEqual(TEXT("attach failure restores root"), EntryCount(Manager), 1);
	TestFalse(TEXT("attach failure owns no widget"), Manager->HasOpenPauseScreen());
	Manager->ConfigureAutomationBackend(true, true, true, false, true);
	TestEqual(TEXT("pause failure compensated"), Manager->OpenPauseScreen(), EHSRUIScreenResult::PauseApplyFailed);
	TestEqual(TEXT("pause failure restores root"), EntryCount(Manager), 1);
	TestFalse(TEXT("pause failure remains consistent"), Manager->IsInconsistent());

	UHSRUIManagerSubsystem* CloseFailure = MakeManager();
	CloseFailure->RegisterHostForAutomation();
	CloseFailure->ConfigureAutomationBackend(true, true, true, true, true);
	TestEqual(TEXT("close failure fixture opens"), CloseFailure->OpenPauseScreen(), EHSRUIScreenResult::Success);
	CloseFailure->ConfigureAutomationBackend(true, true, true, false, true, true);
	TestEqual(TEXT("unpause failure compensated"), CloseFailure->RequestBack(), EHSRUIScreenResult::PauseApplyFailed);
	TestTrue(TEXT("failed close keeps pause instance"), CloseFailure->HasOpenPauseScreen());
	TestEqual(TEXT("failed close restores modal"), EntryCount(CloseFailure), 2);
	TestFalse(TEXT("successful close compensation stays consistent"), CloseFailure->IsInconsistent());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUIScreenLifecycleBoundaryTest, "HSR.UI.ScreenLifecycle.Boundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUIScreenLifecycleBoundaryTest::RunTest(const FString&)
{
	using namespace HSR::P17::LifecycleTests;
	UHSRUIManagerSubsystem* Manager = MakeManager();
	Manager->RegisterHostForAutomation(false, true);
	Manager->ConfigureAutomationBackend(true, true, true, true, false);
	TestEqual(TEXT("battle/non-exploration rejected"), Manager->OpenPauseScreen(), EHSRUIScreenResult::NotExploration);
	TestEqual(TEXT("mode rejection preserves root"), EntryCount(Manager), 1);
	Manager->RegisterHostForAutomation(true, true);
	Manager->ConfigureAutomationBackend(true, true, true, true, false, true);
	TestEqual(TEXT("external pause rejected"), Manager->OpenPauseScreen(), EHSRUIScreenResult::ExternalPause);
	TestEqual(TEXT("external pause preserves root"), EntryCount(Manager), 1);
	Manager->ConfigureAutomationBackend(true, true, false, true, true);
	TestEqual(TEXT("policy and compensation failure is surfaced"), Manager->OpenPauseScreen(),
		EHSRUIScreenResult::CompensationFailed);
	TestTrue(TEXT("failed compensation marks inconsistent"), Manager->IsInconsistent());
	TestEqual(TEXT("inconsistent manager rejects further work"), Manager->OpenPauseScreen(), EHSRUIScreenResult::Inconsistent);

	UHSRInputModeCoordinator* Coordinator = NewObject<UHSRInputModeCoordinator>();
	TestEqual(TEXT("missing focus owner is unavailable"), Coordinator->ApplyFocus(nullptr, nullptr, nullptr),
		EHSRFocusApplyResult::Unavailable);
	TestEqual(TEXT("eligible preferred selected"), UHSRInputModeCoordinator::ChooseFocusTarget(true, true, true),
		EHSRFocusApplyResult::Preferred);
	TestEqual(TEXT("hidden or disabled preferred falls back"), UHSRInputModeCoordinator::ChooseFocusTarget(true, false, true),
		EHSRFocusApplyResult::ScreenFallback);
	TestEqual(TEXT("no eligible focus target"), UHSRInputModeCoordinator::ChooseFocusTarget(true, false, false),
		EHSRFocusApplyResult::Unavailable);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUIHostIdentityLifecycleTest, "HSR.UI.ScreenLifecycle.HostIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUIHostIdentityLifecycleTest::RunTest(const FString&)
{
	using namespace HSR::P17::LifecycleTests;
	UHSRUIManagerSubsystem* Manager = MakeManager();
	TestEqual(TEXT("first host registers"), Manager->RegisterHostIdentityForAutomation(101), EHSRUIScreenResult::Success);
	TestEqual(TEXT("same host register is idempotent"), Manager->RegisterHostIdentityForAutomation(101), EHSRUIScreenResult::NoOp);
	TestEqual(TEXT("different host rejected while current exists"), Manager->RegisterHostIdentityForAutomation(202), EHSRUIScreenResult::InvalidHost);
	TestEqual(TEXT("stale unregister cannot clear current"), Manager->UnregisterHostIdentityForAutomation(202), EHSRUIScreenResult::InvalidHost);
	TestEqual(TEXT("current host remains usable"), Manager->OpenPauseScreen(), EHSRUIScreenResult::Success);
	TestEqual(TEXT("matched unregister tears down open pause"), Manager->UnregisterHostIdentityForAutomation(101), EHSRUIScreenResult::Success);
	TestFalse(TEXT("teardown releases pause widget"), Manager->HasOpenPauseScreen());
	TestEqual(TEXT("teardown leaves logical root only"), EntryCount(Manager), 1);
	TestEqual(TEXT("repeat unregister is rejected without mutation"), Manager->UnregisterHostIdentityForAutomation(101), EHSRUIScreenResult::InvalidHost);
	TestEqual(TEXT("new host can register after teardown"), Manager->RegisterHostIdentityForAutomation(202), EHSRUIScreenResult::Success);
	TestEqual(TEXT("late old host cannot clear new host"), Manager->UnregisterHostIdentityForAutomation(101), EHSRUIScreenResult::InvalidHost);
	TestEqual(TEXT("new host still opens"), Manager->OpenPauseScreen(), EHSRUIScreenResult::Success);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUICharacterDetailLifecycleTest, "HSR.UI.ScreenLifecycle.CharacterDetail",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUICharacterDetailLifecycleTest::RunTest(const FString&)
{
	using namespace HSR::P17::LifecycleTests;
	UHSRUIManagerSubsystem* Manager = MakeManager();
	Manager->RegisterHostForAutomation();
	Manager->ConfigureAutomationDetailBackend(true, true, true, true, true);
	TestEqual(TEXT("detail open"), Manager->OpenCharacterDetailScreen(), EHSRUIScreenResult::Success);
	TestTrue(TEXT("detail instance owned"), Manager->HasOpenCharacterDetailScreen());
	TestEqual(TEXT("detail menu pushed"), EntryCount(Manager), 2);
	// Frontend modules live inside the shared shell, so reopening the active module is a no-op
	// rather than a rejection, and backing out lands on the hub instead of the exploration root.
	TestEqual(TEXT("duplicate detail is a no-op"), Manager->OpenCharacterDetailScreen(), EHSRUIScreenResult::NoOp);
	TestEqual(TEXT("duplicate detail preserves stack"), EntryCount(Manager), 2);
	TestFalse(TEXT("duplicate detail stays consistent"), Manager->IsInconsistent());
	TestEqual(TEXT("detail back"), Manager->RequestBack(), EHSRUIScreenResult::Success);
	TestFalse(TEXT("detail instance cleared"), Manager->HasOpenCharacterDetailScreen());
	TestEqual(TEXT("detail back lands on hub"), EntryCount(Manager), 2);
	TestEqual(TEXT("hub closes to root"), Manager->CloseFrontendToRoot(), EHSRUIScreenResult::Success);
	TestEqual(TEXT("detail restores root"), EntryCount(Manager), 1);

	TestEqual(TEXT("pause fixture open"), Manager->OpenPauseScreen(), EHSRUIScreenResult::Success);
	// Opening a module from the hub routes into it; the hub is not a competing screen.
	TestEqual(TEXT("detail opens from hub"), Manager->OpenCharacterDetailScreen(), EHSRUIScreenResult::Success);
	TestEqual(TEXT("hub routing preserves stack depth"), EntryCount(Manager), 2);
	TestTrue(TEXT("detail owned after hub routing"), Manager->HasOpenCharacterDetailScreen());
	TestFalse(TEXT("hub routing stays consistent"), Manager->IsInconsistent());
	TestEqual(TEXT("pause fixture close"), Manager->RequestBack(), EHSRUIScreenResult::Success);
	TestEqual(TEXT("pause fixture closes to root"), Manager->CloseFrontendToRoot(), EHSRUIScreenResult::Success);

	UHSRUIManagerSubsystem* Failures = MakeManager();
	Failures->RegisterHostForAutomation();
	Failures->ConfigureAutomationDetailBackend(false, true, true, true, true);
	TestEqual(TEXT("detail class required"), Failures->OpenCharacterDetailScreen(), EHSRUIScreenResult::MissingWidgetClass);
	Failures->ConfigureAutomationDetailBackend(true, false, true, true, true);
	TestEqual(TEXT("detail create failure"), Failures->OpenCharacterDetailScreen(), EHSRUIScreenResult::WidgetCreationFailed);
	Failures->ConfigureAutomationDetailBackend(true, true, false, true, true);
	TestEqual(TEXT("detail attach failure compensated"), Failures->OpenCharacterDetailScreen(), EHSRUIScreenResult::ViewportAttachFailed);
	TestEqual(TEXT("attach compensation root"), EntryCount(Failures), 1);
	Failures->ConfigureAutomationDetailBackend(true, true, true, true, true);
	Failures->FailNextAutomationDetailPolicyApply();
	TestEqual(TEXT("detail open policy failure compensated"), Failures->OpenCharacterDetailScreen(), EHSRUIScreenResult::PolicyApplyFailed);
	TestEqual(TEXT("open policy compensation root"), EntryCount(Failures), 1);
	TestEqual(TEXT("close failure fixture opens"), Failures->OpenCharacterDetailScreen(), EHSRUIScreenResult::Success);
	Failures->FailNextAutomationDetailPolicyApply();
	TestEqual(TEXT("detail close policy failure compensated"), Failures->RequestBack(), EHSRUIScreenResult::PolicyApplyFailed);
	TestTrue(TEXT("failed detail close retains instance"), Failures->HasOpenCharacterDetailScreen());
	TestEqual(TEXT("failed detail close restores menu"), EntryCount(Failures), 2);
	TestEqual(TEXT("matched host teardown closes detail"), Failures->UnregisterHostIdentityForAutomation(1), EHSRUIScreenResult::Success);
	TestFalse(TEXT("host teardown clears detail"), Failures->HasOpenCharacterDetailScreen());
	TestEqual(TEXT("new host after detail teardown"), Failures->RegisterHostIdentityForAutomation(2), EHSRUIScreenResult::Success);
	const FHSRScreenStackSnapshot BeforeStaleDetailUnregister = Failures->GetScreenStack()->GetSnapshot();
	TestEqual(TEXT("stale detail host unregister rejected"), Failures->UnregisterHostIdentityForAutomation(1), EHSRUIScreenResult::InvalidHost);
	TestTrue(TEXT("stale detail unregister preserves full snapshot"),
		Failures->GetScreenStack()->GetSnapshot() == BeforeStaleDetailUnregister);
	TestFalse(TEXT("stale detail unregister preserves no instances"),
		Failures->HasOpenCharacterDetailScreen() || Failures->HasOpenPauseScreen());
	TestFalse(TEXT("stale detail unregister stays consistent"), Failures->IsInconsistent());

	UHSRUIManagerSubsystem* ForcedTeardown = MakeManager();
	ForcedTeardown->RegisterHostForAutomation();
	ForcedTeardown->ConfigureAutomationDetailBackend(true, true, true, true, true);
	TestEqual(TEXT("forced teardown fixture opens"), ForcedTeardown->OpenCharacterDetailScreen(), EHSRUIScreenResult::Success);
	// Teardown closes the shell rather than the module, so the failure has to be injected on the
	// shared pause backend the shell path uses.
	ForcedTeardown->FailNextAutomationPauseApply();
	AddExpectedError(TEXT("HSRUI P17 Host teardown required forced cleanup; host references cleared"),
		EAutomationExpectedErrorFlags::Contains, 1);
	TestEqual(TEXT("forced teardown reports inconsistent"), ForcedTeardown->UnregisterHostIdentityForAutomation(1),
		EHSRUIScreenResult::Inconsistent);
	TestTrue(TEXT("forced teardown marks inconsistent"), ForcedTeardown->IsInconsistent());
	TestFalse(TEXT("forced teardown clears detail instance"), ForcedTeardown->HasOpenCharacterDetailScreen());
	TestEqual(TEXT("forced teardown restores logical root"), EntryCount(ForcedTeardown), 1);

	UHSRUIManagerSubsystem* RootOnly = MakeManager();
	RootOnly->RegisterHostForAutomation();
	FHSRScreenRequest ForeignMenu;
	ForeignMenu.RequestToken = 100;
	ForeignMenu.ScreenId = TEXT("UI.Screen.Foreign");
	ForeignMenu.Layer = EHSRUIScreenLayer::Menu;
	ForeignMenu.InputIntent = EHSRUIInputIntent::UIOnly;
	TestEqual(TEXT("foreign menu fixture"), RootOnly->SubmitScreenRequest(ForeignMenu), EHSRScreenStackResult::Success);
	const FHSRScreenStackSnapshot BeforeRootOnlyReject = RootOnly->GetScreenStack()->GetSnapshot();
	TestEqual(TEXT("detail requires root-only stack"), RootOnly->OpenCharacterDetailScreen(), EHSRUIScreenResult::Inconsistent);
	TestTrue(TEXT("root-only rejection preserves snapshot"), RootOnly->GetScreenStack()->GetSnapshot() == BeforeRootOnlyReject);
	TestFalse(TEXT("root-only rejection creates no detail"), RootOnly->HasOpenCharacterDetailScreen());

	UHSRScreenWidget* Unowned = NewObject<UHSRScreenWidget>();
	TestFalse(TEXT("unowned shared screen does not route back"), Unowned->RequestBack());
	TestFalse(TEXT("unowned escape is not consumed"), Unowned->ShouldConsumeBackKeyForAutomation(EKeys::Escape));
	TestFalse(TEXT("unowned gamepad back is not consumed"), Unowned->ShouldConsumeBackKeyForAutomation(EKeys::Gamepad_Special_Right));
	Unowned->SetOwningUIManager(Manager);
	TestTrue(TEXT("owned escape is consumed"), Unowned->ShouldConsumeBackKeyForAutomation(EKeys::Escape));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUIInventoryLifecycleTest, "HSR.UI.ScreenLifecycle.Inventory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUIInventoryLifecycleTest::RunTest(const FString&)
{
	using namespace HSR::P17::LifecycleTests;
	UHSRUIManagerSubsystem* Manager = MakeManager();
	Manager->RegisterHostForAutomation();
	Manager->ConfigureAutomationInventoryBackend(true, true, true, true, true, true);
	TestEqual(TEXT("inventory open"), Manager->OpenInventoryScreen(), EHSRUIScreenResult::Success);
	TestTrue(TEXT("inventory widget owned"), Manager->HasOpenInventoryScreen());
	TestTrue(TEXT("inventory view model owned"), Manager->HasInventoryViewModel());
	TestEqual(TEXT("inventory menu pushed"), EntryCount(Manager), 2);
	TestEqual(TEXT("inventory binds exactly once after SetViewModel-before-attach"),
		Manager->GetInventoryBindCountForAutomation(), 1);
	// Modules replace each other atomically inside the shell, so switching to Character releases
	// the inventory widget/view-model pair instead of being rejected.
	TestEqual(TEXT("duplicate inventory is a no-op"), Manager->OpenInventoryScreen(), EHSRUIScreenResult::NoOp);
	TestTrue(TEXT("duplicate inventory preserves ownership"),
		Manager->HasOpenInventoryScreen() && Manager->HasInventoryViewModel());
	Manager->ConfigureAutomationDetailBackend(true, true, true, true, true);
	TestEqual(TEXT("detail replaces inventory"), Manager->OpenCharacterDetailScreen(), EHSRUIScreenResult::Success);
	TestFalse(TEXT("replaced inventory widget released"), Manager->HasOpenInventoryScreen());
	TestFalse(TEXT("replaced inventory view model shutdown"), Manager->HasInventoryViewModel());
	TestEqual(TEXT("replacement preserves stack depth"), EntryCount(Manager), 2);
	TestFalse(TEXT("replacement stays consistent"), Manager->IsInconsistent());
	TestEqual(TEXT("inventory reopens after replacement"), Manager->OpenInventoryScreen(), EHSRUIScreenResult::Success);
	TestEqual(TEXT("inventory back"), Manager->RequestBack(), EHSRUIScreenResult::Success);
	TestFalse(TEXT("inventory widget released"), Manager->HasOpenInventoryScreen());
	TestFalse(TEXT("inventory view model shutdown"), Manager->HasInventoryViewModel());
	TestEqual(TEXT("inventory back lands on hub"), EntryCount(Manager), 2);
	TestEqual(TEXT("inventory hub closes to root"), Manager->CloseFrontendToRoot(), EHSRUIScreenResult::Success);
	TestEqual(TEXT("inventory restores root"), EntryCount(Manager), 1);
	TestEqual(TEXT("first cycle released one binding"), Manager->GetLastReleasedInventoryBindCountForAutomation(), 1);
	TestEqual(TEXT("first cycle unbound one subscription"), Manager->GetLastReleasedInventoryUnbindCountForAutomation(), 1);
	TestEqual(TEXT("second inventory open"), Manager->OpenInventoryScreen(), EHSRUIScreenResult::Success);
	TestEqual(TEXT("second cycle starts with one fresh binding"), Manager->GetInventoryBindCountForAutomation(), 1);
	TestEqual(TEXT("second inventory back"), Manager->RequestBack(), EHSRUIScreenResult::Success);
	TestEqual(TEXT("second cycle released one binding"), Manager->GetLastReleasedInventoryBindCountForAutomation(), 1);
	TestEqual(TEXT("second cycle leaves no subscription"), Manager->GetLastReleasedInventoryUnbindCountForAutomation(), 1);
	TestEqual(TEXT("second cycle closes to root"), Manager->CloseFrontendToRoot(), EHSRUIScreenResult::Success);

	TestEqual(TEXT("pause fixture opens"), Manager->OpenPauseScreen(), EHSRUIScreenResult::Success);
	TestEqual(TEXT("inventory opens from hub"), Manager->OpenInventoryScreen(), EHSRUIScreenResult::Success);
	TestEqual(TEXT("hub to inventory preserves depth"), EntryCount(Manager), 2);
	TestTrue(TEXT("inventory owned from hub"), Manager->HasOpenInventoryScreen());
	TestEqual(TEXT("pause fixture closes"), Manager->RequestBack(), EHSRUIScreenResult::Success);
	Manager->ConfigureAutomationDetailBackend(true, true, true, true, true);
	TestEqual(TEXT("detail fixture opens"), Manager->OpenCharacterDetailScreen(), EHSRUIScreenResult::Success);
	TestEqual(TEXT("inventory replaces detail"), Manager->OpenInventoryScreen(), EHSRUIScreenResult::Success);
	TestFalse(TEXT("replaced detail released"), Manager->HasOpenCharacterDetailScreen());
	TestEqual(TEXT("detail to inventory preserves depth"), EntryCount(Manager), 2);
	TestEqual(TEXT("detail fixture closes"), Manager->RequestBack(), EHSRUIScreenResult::Success);
	TestEqual(TEXT("inventory fixture closes to root"), Manager->CloseFrontendToRoot(), EHSRUIScreenResult::Success);

	UHSRUIManagerSubsystem* ForeignTop = MakeManager();
	ForeignTop->RegisterHostForAutomation();
	FHSRScreenRequest ForeignMenu;
	ForeignMenu.RequestToken = 9001;
	ForeignMenu.ScreenId = TEXT("UI.Screen.ForeignMenu");
	ForeignMenu.Layer = EHSRUIScreenLayer::Menu;
	ForeignMenu.InputIntent = EHSRUIInputIntent::UIOnly;
	TestEqual(TEXT("foreign menu fixture pushes"), ForeignTop->SubmitScreenRequest(ForeignMenu), EHSRScreenStackResult::Success);
	const FHSRScreenStackSnapshot BeforeForeignInventory = ForeignTop->GetScreenStack()->GetSnapshot();
	TestEqual(TEXT("inventory rejects foreign top"), ForeignTop->OpenInventoryScreen(), EHSRUIScreenResult::Inconsistent);
	TestTrue(TEXT("foreign top rejection preserves full snapshot"),
		ForeignTop->GetScreenStack()->GetSnapshot() == BeforeForeignInventory);
	TestFalse(TEXT("foreign top rejection owns no inventory pair"),
		ForeignTop->HasOpenInventoryScreen() || ForeignTop->HasInventoryViewModel());

	UHSRUIManagerSubsystem* HalfPairBack = MakeManager();
	HalfPairBack->RegisterHostForAutomation();
	HalfPairBack->InjectInventoryHalfPairForAutomation(false);
	const FHSRScreenStackSnapshot BeforeHalfPairBack = HalfPairBack->GetScreenStack()->GetSnapshot();
	TestEqual(TEXT("root-only back detects VM-only ownership"), HalfPairBack->RequestBack(), EHSRUIScreenResult::Inconsistent);
	TestTrue(TEXT("half-pair back preserves snapshot"), HalfPairBack->GetScreenStack()->GetSnapshot() == BeforeHalfPairBack);
	UHSRUIManagerSubsystem* HalfPairRegister = MakeManager();
	HalfPairRegister->InjectInventoryHalfPairForAutomation(true);
	TestEqual(TEXT("register detects widget-only ownership"), HalfPairRegister->RegisterHostIdentityForAutomation(1),
		EHSRUIScreenResult::Inconsistent);
	TestEqual(TEXT("half-pair register leaves stack empty"), EntryCount(HalfPairRegister), 0);

	UHSRUIManagerSubsystem* Failures = MakeManager();
	Failures->RegisterHostForAutomation();
	Failures->ConfigureAutomationInventoryBackend(false, true, true, true, true, true);
	TestEqual(TEXT("inventory class required"), Failures->OpenInventoryScreen(), EHSRUIScreenResult::MissingWidgetClass);
	Failures->ConfigureAutomationInventoryBackend(true, true, false, true, true, true);
	TestEqual(TEXT("inventory VM init required"), Failures->OpenInventoryScreen(), EHSRUIScreenResult::ViewModelInitializationFailed);
	const int32 ShutdownsBeforeSnapshotFailure = Failures->GetInventoryCandidateShutdownCountForAutomation();
	Failures->ConfigureAutomationInventoryBackend(true, true, true, true, true, true);
	Failures->ConfigureAutomationInventoryViewModelStages(true, true, false);
	TestEqual(TEXT("inventory snapshot init required"), Failures->OpenInventoryScreen(), EHSRUIScreenResult::ViewModelInitializationFailed);
	TestEqual(TEXT("snapshot failure shuts down created VM"), Failures->GetInventoryCandidateShutdownCountForAutomation(),
		ShutdownsBeforeSnapshotFailure + 1);
	Failures->ConfigureAutomationInventoryBackend(true, false, true, true, true, true);
	TestEqual(TEXT("inventory widget create failure"), Failures->OpenInventoryScreen(), EHSRUIScreenResult::WidgetCreationFailed);
	TestFalse(TEXT("create failure releases VM"), Failures->HasInventoryViewModel());
	Failures->ConfigureAutomationInventoryBackend(true, true, true, false, true, true);
	TestEqual(TEXT("inventory attach failure compensated"), Failures->OpenInventoryScreen(), EHSRUIScreenResult::ViewportAttachFailed);
	TestEqual(TEXT("attach compensation root"), EntryCount(Failures), 1);
	TestFalse(TEXT("attach compensation clears ownership"), Failures->HasOpenInventoryScreen() || Failures->HasInventoryViewModel());
	Failures->ConfigureAutomationInventoryBackend(true, true, true, true, true, true);
	Failures->FailNextAutomationInventoryPolicyApply();
	TestEqual(TEXT("inventory open policy failure compensated"), Failures->OpenInventoryScreen(), EHSRUIScreenResult::PolicyApplyFailed);
	TestFalse(TEXT("open policy failure clears ownership"), Failures->HasOpenInventoryScreen() || Failures->HasInventoryViewModel());
	TestEqual(TEXT("close fixture opens"), Failures->OpenInventoryScreen(), EHSRUIScreenResult::Success);
	Failures->FailNextAutomationInventoryPolicyApply();
	TestEqual(TEXT("inventory close policy failure compensated"), Failures->RequestBack(), EHSRUIScreenResult::PolicyApplyFailed);
	TestTrue(TEXT("failed close retains pair"), Failures->HasOpenInventoryScreen() && Failures->HasInventoryViewModel());
	TestEqual(TEXT("failed close restores inventory menu"), EntryCount(Failures), 2);
	TestEqual(TEXT("matched teardown closes inventory"), Failures->UnregisterHostIdentityForAutomation(1), EHSRUIScreenResult::Success);
	TestFalse(TEXT("matched teardown clears pair"), Failures->HasOpenInventoryScreen() || Failures->HasInventoryViewModel());
	TestEqual(TEXT("new host after inventory teardown"), Failures->RegisterHostIdentityForAutomation(2), EHSRUIScreenResult::Success);
	const FHSRScreenStackSnapshot BeforeStale = Failures->GetScreenStack()->GetSnapshot();
	TestEqual(TEXT("stale inventory unregister rejected"), Failures->UnregisterHostIdentityForAutomation(1), EHSRUIScreenResult::InvalidHost);
	TestTrue(TEXT("stale inventory unregister preserves snapshot"), Failures->GetScreenStack()->GetSnapshot() == BeforeStale);

	UHSRUIManagerSubsystem* Forced = MakeManager();
	Forced->RegisterHostForAutomation();
	Forced->ConfigureAutomationInventoryBackend(true, true, true, true, true, true);
	TestEqual(TEXT("forced inventory teardown fixture"), Forced->OpenInventoryScreen(), EHSRUIScreenResult::Success);
	// Teardown closes the shell, so inject on the shared pause backend rather than the module one.
	Forced->FailNextAutomationPauseApply();
	AddExpectedError(TEXT("HSRUI P17 Host teardown required forced cleanup; host references cleared"),
		EAutomationExpectedErrorFlags::Contains, 1);
	TestEqual(TEXT("forced inventory teardown inconsistent"), Forced->UnregisterHostIdentityForAutomation(1), EHSRUIScreenResult::Inconsistent);
	TestTrue(TEXT("forced inventory teardown marks inconsistent"), Forced->IsInconsistent());
	TestFalse(TEXT("forced inventory teardown clears pair"), Forced->HasOpenInventoryScreen() || Forced->HasInventoryViewModel());
	TestEqual(TEXT("forced inventory teardown root"), EntryCount(Forced), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUITravelRestoreLifecycleTest, "HSR.UI.ScreenLifecycle.TravelRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUITravelRestoreLifecycleTest::RunTest(const FString&)
{
	using namespace HSR::P17::LifecycleTests;
	UHSRUIManagerSubsystem* Manager = MakeManager();
	TestEqual(TEXT("travel host one registers"), Manager->RegisterHostIdentityForAutomation(1), EHSRUIScreenResult::Success);
	Manager->ConfigureAutomationDetailBackend(true, true, true, true, true);
	TestEqual(TEXT("detail opens before travel"), Manager->OpenCharacterDetailScreen(), EHSRUIScreenResult::Success);
	TestEqual(TEXT("detail travel capture tears down"), Manager->TeardownHostIdentityForTravelForAutomation(1), EHSRUIScreenResult::Success);
	TestTrue(TEXT("descriptor waits after teardown"), Manager->HasPendingTravelRestoreForAutomation());
	TestFalse(TEXT("old detail released"), Manager->HasOpenCharacterDetailScreen());
	Manager->NotifyArrivalCommittedForAutomation(1, TEXT("Map.B"));
	TestTrue(TEXT("arrival-before-host remains pending"), Manager->HasPendingTravelRestoreForAutomation());
	TestEqual(TEXT("new host consumes arrival-before-host"), Manager->RegisterHostIdentityForAutomation(2), EHSRUIScreenResult::Success);
	TestFalse(TEXT("detail descriptor consumed"), Manager->HasPendingTravelRestoreForAutomation());
	TestTrue(TEXT("detail restored on fresh host"), Manager->HasOpenCharacterDetailScreen());
	TestEqual(TEXT("restored detail stack"), EntryCount(Manager), 2);
	TestEqual(TEXT("restored detail closes"), Manager->RequestBack(), EHSRUIScreenResult::Success);
	const FHSRScreenStackSnapshot BeforeDuplicateHost = Manager->GetScreenStack()->GetSnapshot();
	TestEqual(TEXT("duplicate host registration is no-op"), Manager->RegisterHostIdentityForAutomation(2), EHSRUIScreenResult::NoOp);
	TestTrue(TEXT("duplicate host registration is zero-change"), Manager->GetScreenStack()->GetSnapshot() == BeforeDuplicateHost);

	TestEqual(TEXT("root travel capture"), Manager->TeardownHostIdentityForTravelForAutomation(2), EHSRUIScreenResult::Success);
	TestEqual(TEXT("host-before-arrival registers"), Manager->RegisterHostIdentityForAutomation(3), EHSRUIScreenResult::Success);
	TestTrue(TEXT("host-before-arrival waits"), Manager->HasPendingTravelRestoreForAutomation());
	Manager->NotifyArrivalCommittedForAutomation(2, TEXT("Map.A"));
	TestFalse(TEXT("root descriptor consumed after arrival"), Manager->HasPendingTravelRestoreForAutomation());
	TestEqual(TEXT("root-only restore stays root"), EntryCount(Manager), 1);

	Manager->ConfigureAutomationInventoryBackend(true, true, true, true, true, true);
	TestEqual(TEXT("inventory opens before travel"), Manager->OpenInventoryScreen(), EHSRUIScreenResult::Success);
	TestEqual(TEXT("inventory travel capture"), Manager->TeardownHostIdentityForTravelForAutomation(3), EHSRUIScreenResult::Success);
	TestEqual(TEXT("inventory new host first"), Manager->RegisterHostIdentityForAutomation(4), EHSRUIScreenResult::Success);
	Manager->NotifyArrivalCommittedForAutomation(3, TEXT("Map.B"));
	TestFalse(TEXT("inventory descriptor consumed"), Manager->HasPendingTravelRestoreForAutomation());
	TestTrue(TEXT("inventory fresh pair restored"), Manager->HasOpenInventoryScreen() && Manager->HasInventoryViewModel());
	TestEqual(TEXT("fresh inventory binds once"), Manager->GetInventoryBindCountForAutomation(), 1);
	const FHSRScreenStackSnapshot BeforeDuplicateArrival = Manager->GetScreenStack()->GetSnapshot();
	Manager->NotifyArrivalCommittedForAutomation(3, TEXT("Map.B"));
	TestTrue(TEXT("duplicate arrival after consume is zero-change"), Manager->GetScreenStack()->GetSnapshot() == BeforeDuplicateArrival);
	TestEqual(TEXT("stale old host capture rejected"), Manager->TeardownHostIdentityForTravelForAutomation(3), EHSRUIScreenResult::InvalidHost);
	TestEqual(TEXT("inventory closes after restore"), Manager->RequestBack(), EHSRUIScreenResult::Success);

	TestEqual(TEXT("pause opens before travel"), Manager->OpenPauseScreen(), EHSRUIScreenResult::Success);
	TestEqual(TEXT("pause travel capture"), Manager->TeardownHostIdentityForTravelForAutomation(4), EHSRUIScreenResult::Success);
	Manager->NotifyArrivalCommittedForAutomation(4, TEXT("Map.A"));
	TestEqual(TEXT("pause new host registers"), Manager->RegisterHostIdentityForAutomation(5), EHSRUIScreenResult::Success);
	TestFalse(TEXT("pause never restores"), Manager->HasOpenPauseScreen());
	TestEqual(TEXT("pause travel restores root only"), EntryCount(Manager), 1);

	UHSRUIManagerSubsystem* NonTravel = MakeManager();
	NonTravel->RegisterHostForAutomation();
	TestEqual(TEXT("ordinary unregister succeeds"), NonTravel->UnregisterHostIdentityForAutomation(1), EHSRUIScreenResult::Success);
	TestFalse(TEXT("ordinary unregister creates no descriptor"), NonTravel->HasPendingTravelRestoreForAutomation());
	TestFalse(TEXT("removed EndPlay does not capture"), AHSRHUD::ShouldCaptureTravelRestore(EEndPlayReason::RemovedFromWorld, true));
	TestFalse(TEXT("destroyed without travel does not capture"), AHSRHUD::ShouldCaptureTravelRestore(EEndPlayReason::Destroyed, false));
	TestTrue(TEXT("destroyed with authorized travel captures"), AHSRHUD::ShouldCaptureTravelRestore(EEndPlayReason::Destroyed, true));
	TestFalse(TEXT("PIE stop EndPlay does not capture"), AHSRHUD::ShouldCaptureTravelRestore(EEndPlayReason::EndPlayInEditor, true));
	TestTrue(TEXT("level transition EndPlay captures"), AHSRHUD::ShouldCaptureTravelRestore(EEndPlayReason::LevelTransition, false));

	UHSRUIManagerSubsystem* OldArrival = MakeManager();
	OldArrival->RegisterHostForAutomation();
	TestEqual(TEXT("old-arrival fixture captures"), OldArrival->TeardownHostIdentityForTravelForAutomation(1), EHSRUIScreenResult::Success);
	OldArrival->NotifyArrivalCommittedForAutomation(0);
	TestEqual(TEXT("old-arrival new host"), OldArrival->RegisterHostIdentityForAutomation(2), EHSRUIScreenResult::Success);
	TestTrue(TEXT("old arrival cannot consume descriptor"), OldArrival->HasPendingTravelRestoreForAutomation());
	OldArrival->NotifyArrivalCommittedForAutomation(1);
	TestFalse(TEXT("minimum fresh arrival consumes descriptor"), OldArrival->HasPendingTravelRestoreForAutomation());

	UHSRUIManagerSubsystem* Supersede = MakeManager();
	Supersede->NotifyArrivalCommittedForAutomation(1);
	Supersede->RegisterHostForAutomation();
	TestEqual(TEXT("A to B capture"), Supersede->TeardownHostIdentityForTravelForAutomation(1), EHSRUIScreenResult::Success);
	TestEqual(TEXT("B host registers"), Supersede->RegisterHostIdentityForAutomation(2), EHSRUIScreenResult::Success);
	TestEqual(TEXT("B to C supersedes descriptor"), Supersede->TeardownHostIdentityForTravelForAutomation(2), EHSRUIScreenResult::Success);
	Supersede->NotifyArrivalCommittedForAutomation(1);
	TestEqual(TEXT("C host registers after stale arrival"), Supersede->RegisterHostIdentityForAutomation(3), EHSRUIScreenResult::Success);
	TestTrue(TEXT("superseded descriptor rejects stale arrival"), Supersede->HasPendingTravelRestoreForAutomation());
	Supersede->NotifyArrivalCommittedForAutomation(2);
	TestFalse(TEXT("superseded descriptor accepts fresh arrival"), Supersede->HasPendingTravelRestoreForAutomation());

	UHSRUIManagerSubsystem* Foreign = MakeManager();
	Foreign->RegisterHostForAutomation();
	FHSRScreenRequest ForeignMenu;
	ForeignMenu.RequestToken = 100;
	ForeignMenu.ScreenId = TEXT("Screen.ForeignMenu");
	ForeignMenu.Layer = EHSRUIScreenLayer::Menu;
	ForeignMenu.InputIntent = EHSRUIInputIntent::UIOnly;
	TestEqual(TEXT("foreign menu fixture"), Foreign->SubmitScreenRequest(ForeignMenu), EHSRScreenStackResult::Success);
	TestEqual(TEXT("foreign stack travel is flagged"), Foreign->TeardownHostIdentityForTravelForAutomation(1), EHSRUIScreenResult::Inconsistent);
	TestEqual(TEXT("foreign stack is forced to root"), EntryCount(Foreign), 1);
	TestEqual(TEXT("foreign cleanup accepts fresh host"), Foreign->RegisterHostIdentityForAutomation(2), EHSRUIScreenResult::Success);
	Foreign->NotifyArrivalCommittedForAutomation(1);
	TestEqual(TEXT("foreign descriptor restores root only"), EntryCount(Foreign), 1);

	UHSRUIManagerSubsystem* Deep = MakeManager();
	Deep->RegisterHostForAutomation();
	ForeignMenu.RequestToken = 100;
	TestEqual(TEXT("deep menu fixture"), Deep->SubmitScreenRequest(ForeignMenu), EHSRScreenStackResult::Success);
	FHSRScreenRequest ForeignModal = ForeignMenu;
	ForeignModal.RequestToken = 101;
	ForeignModal.ScreenId = TEXT("Screen.ForeignModal");
	ForeignModal.Layer = EHSRUIScreenLayer::Modal;
	TestEqual(TEXT("deep modal fixture"), Deep->SubmitScreenRequest(ForeignModal), EHSRScreenStackResult::Success);
	TestEqual(TEXT("depth greater than two travel is flagged"), Deep->TeardownHostIdentityForTravelForAutomation(1), EHSRUIScreenResult::Inconsistent);
	TestEqual(TEXT("deep stack is forced to root"), EntryCount(Deep), 1);

	UHSRUIManagerSubsystem* HalfPair = MakeManager();
	HalfPair->RegisterHostForAutomation();
	HalfPair->InjectInventoryHalfPairForAutomation(false);
	AddExpectedError(TEXT("HSRUI P17 Host teardown required forced cleanup; host references cleared"),
		EAutomationExpectedErrorFlags::Contains, 1);
	TestEqual(TEXT("half-pair travel is inconsistent"), HalfPair->TeardownHostIdentityForTravelForAutomation(1), EHSRUIScreenResult::Inconsistent);
	TestEqual(TEXT("half-pair travel preserves exact root"), EntryCount(HalfPair), 1);
	TestFalse(TEXT("half-pair travel clears ownership"), HalfPair->HasOpenInventoryScreen() || HalfPair->HasInventoryViewModel());

	UHSRUIManagerSubsystem* NoTeardown = MakeManager();
	NoTeardown->RegisterHostForAutomation();
	const FHSRScreenStackSnapshot BeforeNoTeardown = NoTeardown->GetScreenStack()->GetSnapshot();
	NoTeardown->NotifyArrivalCommittedForAutomation(99);
	TestFalse(TEXT("failed travel without teardown creates no descriptor"), NoTeardown->HasPendingTravelRestoreForAutomation());
	TestTrue(TEXT("failed travel without teardown preserves complete snapshot"), NoTeardown->GetScreenStack()->GetSnapshot() == BeforeNoTeardown);

	UHSRUIManagerSubsystem* MissingRestore = MakeManager();
	MissingRestore->RegisterHostForAutomation();
	MissingRestore->ConfigureAutomationInventoryBackend(true, true, true, true, true, true);
	TestEqual(TEXT("missing restore fixture opens"), MissingRestore->OpenInventoryScreen(), EHSRUIScreenResult::Success);
	TestEqual(TEXT("missing restore fixture captures"), MissingRestore->TeardownHostIdentityForTravelForAutomation(1), EHSRUIScreenResult::Success);
	TestEqual(TEXT("missing restore new host"), MissingRestore->RegisterHostIdentityForAutomation(2), EHSRUIScreenResult::Success);
	MissingRestore->ConfigureAutomationInventoryBackend(false, true, true, true, true, true);
	MissingRestore->NotifyArrivalCommittedForAutomation(1);
	TestFalse(TEXT("missing class consumes descriptor"), MissingRestore->HasPendingTravelRestoreForAutomation());
	TestEqual(TEXT("missing class restore remains root"), EntryCount(MissingRestore), 1);
	TestFalse(TEXT("missing class restore owns no pair"), MissingRestore->HasOpenInventoryScreen() || MissingRestore->HasInventoryViewModel());

	UHSRUIManagerSubsystem* OneShotPolicy = MakeManager();
	OneShotPolicy->RegisterHostForAutomation();
	OneShotPolicy->ConfigureAutomationInventoryBackend(true, true, true, true, true, true);
	TestEqual(TEXT("one-shot fixture opens"), OneShotPolicy->OpenInventoryScreen(), EHSRUIScreenResult::Success);
	TestEqual(TEXT("one-shot fixture captures"), OneShotPolicy->TeardownHostIdentityForTravelForAutomation(1), EHSRUIScreenResult::Success);
	TestEqual(TEXT("one-shot new host"), OneShotPolicy->RegisterHostIdentityForAutomation(2), EHSRUIScreenResult::Success);
	OneShotPolicy->FailNextAutomationInventoryPolicyApply();
	OneShotPolicy->NotifyArrivalCommittedForAutomation(1);
	TestFalse(TEXT("one-shot failure consumes descriptor"), OneShotPolicy->HasPendingTravelRestoreForAutomation());
	TestEqual(TEXT("one-shot compensation restores root"), EntryCount(OneShotPolicy), 1);
	TestFalse(TEXT("one-shot compensation stays consistent"), OneShotPolicy->IsInconsistent());

	UHSRUIManagerSubsystem* PersistentPolicy = MakeManager();
	PersistentPolicy->RegisterHostForAutomation();
	PersistentPolicy->ConfigureAutomationInventoryBackend(true, true, true, true, true, true);
	TestEqual(TEXT("persistent fixture opens"), PersistentPolicy->OpenInventoryScreen(), EHSRUIScreenResult::Success);
	TestEqual(TEXT("persistent fixture captures"), PersistentPolicy->TeardownHostIdentityForTravelForAutomation(1), EHSRUIScreenResult::Success);
	TestEqual(TEXT("persistent new host"), PersistentPolicy->RegisterHostIdentityForAutomation(2), EHSRUIScreenResult::Success);
	PersistentPolicy->ConfigureAutomationInventoryBackend(true, true, true, true, false, true);
	PersistentPolicy->NotifyArrivalCommittedForAutomation(1);
	TestFalse(TEXT("persistent failure consumes descriptor"), PersistentPolicy->HasPendingTravelRestoreForAutomation());
	TestTrue(TEXT("persistent failure reports inconsistent"), PersistentPolicy->IsInconsistent());
	TestEqual(TEXT("persistent failure logical root"), EntryCount(PersistentPolicy), 1);
	TestFalse(TEXT("persistent failure owns no pair"), PersistentPolicy->HasOpenInventoryScreen() || PersistentPolicy->HasInventoryViewModel());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUITravelForcedCleanupRecoveryTest,
	"HSR.UI.ScreenLifecycle.TravelForcedCleanupRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUITravelForcedCleanupRecoveryTest::RunTest(const FString&)
{
	using namespace HSR::P17::LifecycleTests;

	// Regression: a Challenge battle round-trip forced travel cleanup, which used to leave
	// bInconsistent latched forever so pressing Pause after returning was rejected with
	// Inconsistent even though the restored host and stack were perfectly healthy.
	UHSRUIManagerSubsystem* Manager = MakeManager();
	Manager->RegisterHostForAutomation();
	Manager->ConfigureAutomationBackend(true, true, true, true, true);

	// A non-owned screen the manager cannot close forces the root cleanup path during travel
	// capture -- the same shape the Challenge battle round-trip produced in PIE.
	FHSRScreenRequest ForeignModal;
	ForeignModal.RequestToken = 500;
	ForeignModal.ScreenId = TEXT("Screen.ForeignModal");
	ForeignModal.Layer = EHSRUIScreenLayer::Modal;
	ForeignModal.InputIntent = EHSRUIInputIntent::UIOnly;
	TestEqual(TEXT("foreign modal fixture"), Manager->SubmitScreenRequest(ForeignModal), EHSRScreenStackResult::Success);
	TestEqual(TEXT("foreign modal is on the stack"), EntryCount(Manager), 2);

	TestEqual(TEXT("travel capture forces cleanup"),
		Manager->TeardownHostIdentityForTravelForAutomation(1), EHSRUIScreenResult::Inconsistent);
	TestTrue(TEXT("forced cleanup marks inconsistent"), Manager->IsInconsistent());
	TestTrue(TEXT("forced cleanup is travel-recoverable"),
		Manager->IsInconsistencyTravelRecoverableForAutomation());
	TestEqual(TEXT("forced cleanup lands on root"), EntryCount(Manager), 1);

	// Returning from the battle map registers a fresh host, which must clear the flag.
	TestEqual(TEXT("returning host registers"), Manager->RegisterHostIdentityForAutomation(2),
		EHSRUIScreenResult::Success);
	TestFalse(TEXT("fresh host clears inconsistency"), Manager->IsInconsistent());
	TestFalse(TEXT("recoverable marker is consumed"),
		Manager->IsInconsistencyTravelRecoverableForAutomation());

	// The actual user-visible symptom: pressing Pause after returning must work.
	TestEqual(TEXT("pause reopens after battle return"), Manager->OpenFrontendModule(EHSRFrontendModule::PauseHub),
		EHSRUIScreenResult::Success);
	TestTrue(TEXT("reopened pause is owned"), Manager->HasOpenPauseScreen());
	TestEqual(TEXT("reopened pause stack depth"), EntryCount(Manager), 2);

	// The PIE Challenge round-trip actually failed inside TeardownCurrentHost (Teardown=17),
	// not the forced-pop branch above: a failing close made bRecovered false. Recoverability was
	// evaluated after ClearHostReferences() had already zeroed the host, so it read false and the
	// flag latched. Drive that exact path.
	UHSRUIManagerSubsystem* CloseFail = MakeManager();
	CloseFail->RegisterHostForAutomation();
	CloseFail->ConfigureAutomationBackend(true, true, true, true, true);
	TestEqual(TEXT("close-fail fixture opens pause"), CloseFail->OpenPauseScreen(), EHSRUIScreenResult::Success);
	CloseFail->FailNextAutomationPauseApply();
	AddExpectedError(TEXT("HSRUI P17 Host teardown required forced cleanup; host references cleared"),
		EAutomationExpectedErrorFlags::Contains, 1);
	TestEqual(TEXT("teardown reports inconsistent"),
		CloseFail->TeardownHostIdentityForTravelForAutomation(1), EHSRUIScreenResult::Inconsistent);
	TestTrue(TEXT("teardown marks inconsistent"), CloseFail->IsInconsistent());
	TestTrue(TEXT("contained teardown is travel-recoverable"),
		CloseFail->IsInconsistencyTravelRecoverableForAutomation());
	TestEqual(TEXT("returning host registers after close failure"),
		CloseFail->RegisterHostIdentityForAutomation(2), EHSRUIScreenResult::Success);
	TestFalse(TEXT("close-failure inconsistency is cleared"), CloseFail->IsInconsistent());
	TestEqual(TEXT("pause reopens after close-failure teardown"),
		CloseFail->OpenFrontendModule(EHSRFrontendModule::PauseHub), EHSRUIScreenResult::Success);

	// The PIE repro had a Challenge module open, so a module root existed. TeardownCurrentHost
	// cleared the shell but never the module root, and CloseFrontendToRoot() only reaches its own
	// clear on the success path -- so the retired root outlived the host and IsAtCleanExplorationRoot()
	// reported Recoverable=false forever. The two cases above miss it: neither opens a module.
	UHSRUIManagerSubsystem* ModuleOpen = MakeManager();
	ModuleOpen->RegisterHostForAutomation();
	ModuleOpen->ConfigureAutomationBackend(true, true, true, true, true);
	TestEqual(TEXT("module fixture opens pause hub"),
		ModuleOpen->OpenFrontendModule(EHSRFrontendModule::PauseHub), EHSRUIScreenResult::Success);
	TestEqual(TEXT("module fixture routes to challenge"),
		ModuleOpen->OpenFrontendModule(EHSRFrontendModule::Quest), EHSRUIScreenResult::Success);
	TestTrue(TEXT("module root exists before teardown"), ModuleOpen->HasFrontendModuleRootForAutomation());
	ModuleOpen->FailNextAutomationPauseApply();
	AddExpectedError(TEXT("HSRUI P17 Host teardown required forced cleanup; host references cleared"),
		EAutomationExpectedErrorFlags::Contains, 1);
	TestEqual(TEXT("module teardown reports inconsistent"),
		ModuleOpen->TeardownHostIdentityForTravelForAutomation(1), EHSRUIScreenResult::Inconsistent);
	TestFalse(TEXT("forced teardown releases the module root"),
		ModuleOpen->HasFrontendModuleRootForAutomation());
	TestTrue(TEXT("module teardown is travel-recoverable"),
		ModuleOpen->IsInconsistencyTravelRecoverableForAutomation());
	TestEqual(TEXT("returning host registers after module teardown"),
		ModuleOpen->RegisterHostIdentityForAutomation(2), EHSRUIScreenResult::Success);
	TestFalse(TEXT("module teardown inconsistency is cleared"), ModuleOpen->IsInconsistent());
	TestEqual(TEXT("pause reopens after module teardown"),
		ModuleOpen->OpenFrontendModule(EHSRFrontendModule::PauseHub), EHSRUIScreenResult::Success);

	// Genuine corruption must still latch permanently and survive a fresh host, so the
	// recoverable path cannot become a blanket amnesty for real UI damage.
	UHSRUIManagerSubsystem* Corrupt = MakeManager();
	Corrupt->RegisterHostForAutomation();
	Corrupt->InjectInventoryHalfPairForAutomation(true);
	TestEqual(TEXT("ownership mismatch is rejected"), Corrupt->OpenPauseScreen(),
		EHSRUIScreenResult::Inconsistent);
	TestTrue(TEXT("ownership mismatch marks inconsistent"), Corrupt->IsInconsistent());
	TestFalse(TEXT("real corruption is not travel-recoverable"),
		Corrupt->IsInconsistencyTravelRecoverableForAutomation());
	TestTrue(TEXT("real corruption survives a fresh host attempt"), Corrupt->IsInconsistent());
	TestEqual(TEXT("corrupt pause stays rejected"), Corrupt->OpenPauseScreen(),
		EHSRUIScreenResult::Inconsistent);
	return true;
}

#endif
