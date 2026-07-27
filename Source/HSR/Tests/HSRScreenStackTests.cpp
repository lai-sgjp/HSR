#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../UI/HSRInputModeCoordinator.h"
#include "../UI/HSRScreenStack.h"

namespace HSR::P17::Tests
{
	static FHSRScreenRequest MakeRequest(const int64 Token, const EHSRScreenStackOperation Operation,
		const TCHAR* ScreenId = TEXT(""), const EHSRUIScreenLayer Layer = EHSRUIScreenLayer::Menu,
		const EHSRUIInputIntent InputIntent = EHSRUIInputIntent::UIOnly, const TCHAR* FocusToken = TEXT(""))
	{
		FHSRScreenRequest Request;
		Request.RequestToken = Token;
		Request.Operation = Operation;
		Request.ScreenId = FName(ScreenId);
		Request.Layer = Layer;
		Request.InputIntent = InputIntent;
		Request.FocusToken = FName(FocusToken);
		return Request;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRScreenStackSequenceTest, "HSR.UI.ScreenStack.Sequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRScreenStackSequenceTest::RunTest(const FString&)
{
	using namespace HSR::P17::Tests;
	UHSRScreenStack* Stack = NewObject<UHSRScreenStack>();
	UHSRInputModeCoordinator* Coordinator = NewObject<UHSRInputModeCoordinator>();

	TestEqual(TEXT("push HUD root"), Stack->SubmitRequest(MakeRequest(1, EHSRScreenStackOperation::Push,
		TEXT("Screen.HUD"), EHSRUIScreenLayer::HUD, EHSRUIInputIntent::GameOnly)), EHSRScreenStackResult::Success);
	TestEqual(TEXT("push menu"), Stack->SubmitRequest(MakeRequest(2, EHSRScreenStackOperation::Push,
		TEXT("Screen.Menu"), EHSRUIScreenLayer::Menu, EHSRUIInputIntent::GameAndUI, TEXT("Focus.Menu"))), EHSRScreenStackResult::Success);
	TestEqual(TEXT("push modal"), Stack->SubmitRequest(MakeRequest(3, EHSRScreenStackOperation::Push,
		TEXT("Screen.Modal"), EHSRUIScreenLayer::Modal, EHSRUIInputIntent::UIOnly, TEXT("Focus.Modal"))), EHSRScreenStackResult::Success);
	FHSRInputModePolicy Policy = Coordinator->ResolvePolicy(Stack);
	TestEqual(TEXT("modal owns policy"), Policy.OwningScreenId, FName(TEXT("Screen.Modal")));
	TestEqual(TEXT("modal is UI only"), Policy.InputIntent, EHSRUIInputIntent::UIOnly);
	TestTrue(TEXT("modal shows cursor"), Policy.bShowMouseCursor);

	TestEqual(TEXT("pop modal"), Stack->SubmitRequest(MakeRequest(4, EHSRScreenStackOperation::Pop)), EHSRScreenStackResult::Success);
	Policy = Coordinator->ResolvePolicy(Stack);
	TestEqual(TEXT("menu restored"), Policy.OwningScreenId, FName(TEXT("Screen.Menu")));
	TestEqual(TEXT("menu focus restored"), Policy.PreferredFocusToken, FName(TEXT("Focus.Menu")));

	TestEqual(TEXT("replace active menu"), Stack->SubmitRequest(MakeRequest(5, EHSRScreenStackOperation::Replace,
		TEXT("Screen.Inventory"), EHSRUIScreenLayer::Menu, EHSRUIInputIntent::UIOnly, TEXT("Focus.Inventory"))), EHSRScreenStackResult::Success);
	Policy = Coordinator->ResolvePolicy(Stack);
	TestEqual(TEXT("replacement owns policy"), Policy.OwningScreenId, FName(TEXT("Screen.Inventory")));
	TestEqual(TEXT("close to root"), Stack->SubmitRequest(MakeRequest(6, EHSRScreenStackOperation::CloseToRoot)), EHSRScreenStackResult::Success);
	Policy = Coordinator->ResolvePolicy(Stack);
	TestEqual(TEXT("root restored"), Policy.OwningScreenId, FName(TEXT("Screen.HUD")));
	TestEqual(TEXT("root game only"), Policy.InputIntent, EHSRUIInputIntent::GameOnly);
	TestFalse(TEXT("root hides cursor"), Policy.bShowMouseCursor);
	TestEqual(TEXT("close root again no-op"), Stack->SubmitRequest(MakeRequest(7, EHSRScreenStackOperation::CloseToRoot)), EHSRScreenStackResult::NoOp);
	TestEqual(TEXT("no-op consumes request token"), Stack->GetSnapshot().LastProcessedRequestToken, int64(7));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRScreenStackFailureTest, "HSR.UI.ScreenStack.Failures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRScreenStackFailureTest::RunTest(const FString&)
{
	using namespace HSR::P17::Tests;
	UHSRScreenStack* Stack = NewObject<UHSRScreenStack>();
	TestEqual(TEXT("empty pop rejected"), Stack->SubmitRequest(MakeRequest(1, EHSRScreenStackOperation::Pop)), EHSRScreenStackResult::EmptyStack);
	TestEqual(TEXT("empty replace rejected"), Stack->SubmitRequest(MakeRequest(1, EHSRScreenStackOperation::Replace,
		TEXT("Screen.Menu"))), EHSRScreenStackResult::EmptyStack);
	TestEqual(TEXT("empty close rejected"), Stack->SubmitRequest(MakeRequest(1, EHSRScreenStackOperation::CloseToRoot)),
		EHSRScreenStackResult::EmptyStack);
	FHSRScreenRequest InvalidOperation = MakeRequest(1, EHSRScreenStackOperation::Push, TEXT("Screen.InvalidOperation"),
		EHSRUIScreenLayer::HUD);
	InvalidOperation.Operation = static_cast<EHSRScreenStackOperation>(255);
	TestEqual(TEXT("invalid operation rejected"), Stack->SubmitRequest(InvalidOperation), EHSRScreenStackResult::InvalidRequest);
	TestEqual(TEXT("pop payload rejected"), Stack->SubmitRequest(MakeRequest(1, EHSRScreenStackOperation::Pop,
		TEXT("Screen.Unexpected"))), EHSRScreenStackResult::InvalidRequest);
	TestEqual(TEXT("failed request token not consumed"), Stack->GetSnapshot().LastProcessedRequestToken, int64(0));
	TestEqual(TEXT("first push requires HUD root"), Stack->SubmitRequest(MakeRequest(1, EHSRScreenStackOperation::Push,
		TEXT("Screen.OrphanMenu"), EHSRUIScreenLayer::Menu)), EHSRScreenStackResult::RootRequired);
	TestEqual(TEXT("root failure token not consumed"), Stack->GetSnapshot().LastProcessedRequestToken, int64(0));
	TestEqual(TEXT("root succeeds after failure"), Stack->SubmitRequest(MakeRequest(1, EHSRScreenStackOperation::Push,
		TEXT("Screen.HUD"), EHSRUIScreenLayer::HUD, EHSRUIInputIntent::GameOnly)), EHSRScreenStackResult::Success);

	const FHSRScreenStackSnapshot Baseline = Stack->GetSnapshot();
	TestEqual(TEXT("duplicate screen rejected"), Stack->SubmitRequest(MakeRequest(2, EHSRScreenStackOperation::Push,
		TEXT("Screen.HUD"), EHSRUIScreenLayer::Menu)), EHSRScreenStackResult::DuplicateScreen);
	TestTrue(TEXT("duplicate is zero pollution"), Stack->GetSnapshot() == Baseline);
	TestEqual(TEXT("empty screen rejected"), Stack->SubmitRequest(MakeRequest(2, EHSRScreenStackOperation::Push)), EHSRScreenStackResult::InvalidScreenId);
	TestTrue(TEXT("empty id is zero pollution"), Stack->GetSnapshot() == Baseline);
	FHSRScreenRequest InvalidLayer = MakeRequest(2, EHSRScreenStackOperation::Push, TEXT("Screen.InvalidLayer"));
	InvalidLayer.Layer = static_cast<EHSRUIScreenLayer>(255);
	TestEqual(TEXT("invalid layer rejected"), Stack->SubmitRequest(InvalidLayer), EHSRScreenStackResult::InvalidRequest);
	TestTrue(TEXT("invalid layer is zero pollution"), Stack->GetSnapshot() == Baseline);
	FHSRScreenRequest InvalidIntent = MakeRequest(2, EHSRScreenStackOperation::Push, TEXT("Screen.InvalidIntent"));
	InvalidIntent.InputIntent = static_cast<EHSRUIInputIntent>(255);
	TestEqual(TEXT("invalid input intent rejected"), Stack->SubmitRequest(InvalidIntent), EHSRScreenStackResult::InvalidRequest);
	TestTrue(TEXT("invalid intent is zero pollution"), Stack->GetSnapshot() == Baseline);
	TestEqual(TEXT("second HUD root rejected"), Stack->SubmitRequest(MakeRequest(2, EHSRScreenStackOperation::Push,
		TEXT("Screen.OtherHUD"), EHSRUIScreenLayer::HUD)), EHSRScreenStackResult::RootAlreadyExists);
	TestTrue(TEXT("second root is zero pollution"), Stack->GetSnapshot() == Baseline);
	TestEqual(TEXT("root pop protected"), Stack->SubmitRequest(MakeRequest(2, EHSRScreenStackOperation::Pop)),
		EHSRScreenStackResult::RootProtected);
	TestTrue(TEXT("root pop is zero pollution"), Stack->GetSnapshot() == Baseline);
	TestEqual(TEXT("root replace protected"), Stack->SubmitRequest(MakeRequest(2, EHSRScreenStackOperation::Replace,
		TEXT("Screen.NewRoot"), EHSRUIScreenLayer::HUD)), EHSRScreenStackResult::RootProtected);
	TestTrue(TEXT("root replace is zero pollution"), Stack->GetSnapshot() == Baseline);
	TestEqual(TEXT("zero token rejected"), Stack->SubmitRequest(MakeRequest(0, EHSRScreenStackOperation::Pop)), EHSRScreenStackResult::InvalidRequest);
	TestTrue(TEXT("zero token is zero pollution"), Stack->GetSnapshot() == Baseline);

	TestEqual(TEXT("valid second request"), Stack->SubmitRequest(MakeRequest(2, EHSRScreenStackOperation::Push,
		TEXT("Screen.Menu"), EHSRUIScreenLayer::Menu)), EHSRScreenStackResult::Success);
	const FHSRScreenStackSnapshot AfterSecond = Stack->GetSnapshot();
	TestEqual(TEXT("same token replay idempotent"), Stack->SubmitRequest(MakeRequest(2, EHSRScreenStackOperation::Pop)), EHSRScreenStackResult::AlreadyProcessed);
	TestTrue(TEXT("replay is zero pollution"), Stack->GetSnapshot() == AfterSecond);
	TestEqual(TEXT("older token stale"), Stack->SubmitRequest(MakeRequest(1, EHSRScreenStackOperation::Pop)), EHSRScreenStackResult::StaleRequest);
	TestTrue(TEXT("stale is zero pollution"), Stack->GetSnapshot() == AfterSecond);
	TestEqual(TEXT("replace duplicate rejected"), Stack->SubmitRequest(MakeRequest(3, EHSRScreenStackOperation::Replace,
		TEXT("Screen.HUD"), EHSRUIScreenLayer::Menu)), EHSRScreenStackResult::DuplicateScreen);
	TestTrue(TEXT("failed replace is zero pollution"), Stack->GetSnapshot() == AfterSecond);
	TestEqual(TEXT("replace menu with second HUD rejected"), Stack->SubmitRequest(MakeRequest(3,
		EHSRScreenStackOperation::Replace, TEXT("Screen.OtherHUD"), EHSRUIScreenLayer::HUD)),
		EHSRScreenStackResult::RootAlreadyExists);
	TestTrue(TEXT("second root replace is zero pollution"), Stack->GetSnapshot() == AfterSecond);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRScreenStackLayerPrecedenceTest, "HSR.UI.ScreenStack.LayerPrecedence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRScreenStackLayerPrecedenceTest::RunTest(const FString&)
{
	using namespace HSR::P17::Tests;
	UHSRScreenStack* Stack = NewObject<UHSRScreenStack>();
	UHSRInputModeCoordinator* Coordinator = NewObject<UHSRInputModeCoordinator>();
	Stack->SubmitRequest(MakeRequest(1, EHSRScreenStackOperation::Push, TEXT("Screen.HUD"), EHSRUIScreenLayer::HUD,
		EHSRUIInputIntent::GameOnly));
	Stack->SubmitRequest(MakeRequest(2, EHSRScreenStackOperation::Push, TEXT("Screen.Modal"), EHSRUIScreenLayer::Modal));
	Stack->SubmitRequest(MakeRequest(3, EHSRScreenStackOperation::Push, TEXT("Screen.LateMenu"), EHSRUIScreenLayer::Menu));
	TestEqual(TEXT("higher layer remains active"), Coordinator->ResolvePolicy(Stack).OwningScreenId, FName(TEXT("Screen.Modal")));
	TestEqual(TEXT("pop removes active higher layer"), Stack->SubmitRequest(MakeRequest(4, EHSRScreenStackOperation::Pop)), EHSRScreenStackResult::Success);
	TestEqual(TEXT("late menu becomes active"), Coordinator->ResolvePolicy(Stack).OwningScreenId, FName(TEXT("Screen.LateMenu")));
	return true;
}

#endif
