#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Curves/CurveFloat.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Party/HSRPartySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../UI/HSRPartyViewModel.h"
#include "../UI/HSRPartyWidget.h"
#include "../UI/HSRUIManagerSubsystem.h"
#include "../UI/Frontend/HSRFrontendRouter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRPartyFrontendProjectionTest,
	"HSR.UI.Party.Projection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRPartyFrontendProjectionTest::RunTest(const FString&)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UHSRCharacterProfileSubsystem* Profiles = NewObject<UHSRCharacterProfileSubsystem>(GameInstance);
	UHSRPartySubsystem* Party = NewObject<UHSRPartySubsystem>(GameInstance);
	UHSRCharacterDefinition* Definition = NewObject<UHSRCharacterDefinition>();
	Definition->CharacterId = TEXT("Character.Party.A");
	Definition->MaxLevel = 2;
	UCurveFloat* Curve = NewObject<UCurveFloat>(Definition);
	Curve->FloatCurve.AddKey(2, 100);
	Definition->CumulativeExperienceCurve = Curve;
	TestEqual(TEXT("profile registers"), Profiles->RegisterDefinition(Definition), EHSRCharacterProfileResult::Success);
	Party->InitializeForDevelopmentTest(Profiles);
	TestEqual(TEXT("character enters party"), Party->AddCharacter(Definition->CharacterId, 1), EHSRPartyResult::Success);

	UHSRPartyViewModel* ViewModel = NewObject<UHSRPartyViewModel>();
	ViewModel->Initialize(Party);
	FHSRPartyFrontendSnapshot Snapshot;
	TestTrue(TEXT("snapshot available"), ViewModel->GetSnapshot(Snapshot));
	TestEqual(TEXT("ready state"), Snapshot.Status, EHSRPartyFrontendStatus::Ready);
	TestEqual(TEXT("fixed two slots"), Snapshot.Slots.Num(), UHSRPartySubsystem::Capacity);
	TestFalse(TEXT("slot zero empty"), Snapshot.Slots[0].bOccupied);
	TestEqual(TEXT("slot one index"), Snapshot.Slots[1].SlotIndex, 1);
	TestTrue(TEXT("slot one occupied"), Snapshot.Slots[1].bOccupied);
	TestEqual(TEXT("character projected"), Snapshot.Slots[1].CharacterId, Definition->CharacterId);
	UHSRPartyWidget* Widget = NewObject<UHSRPartyWidget>();
	Widget->SetViewModel(ViewModel);
	Widget->AttachForAutomation();
	TestEqual(TEXT("widget binds once"), Widget->GetBindCountForAutomation(), 1);
	Widget->SetViewModel(nullptr);
	TestEqual(TEXT("widget unbinds once"), Widget->GetUnbindCountForAutomation(), 1);
	ViewModel->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRPartyFrontendStableStatesTest,
	"HSR.UI.Party.StableStates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRPartyFrontendStableStatesTest::RunTest(const FString&)
{
	UHSRPartyViewModel* Unavailable = NewObject<UHSRPartyViewModel>();
	Unavailable->Initialize(nullptr);
	FHSRPartyFrontendSnapshot Snapshot;
	TestTrue(TEXT("unavailable readable"), Unavailable->GetSnapshot(Snapshot));
	TestEqual(TEXT("unavailable explicit"), Snapshot.Status, EHSRPartyFrontendStatus::Unavailable);

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UHSRCharacterProfileSubsystem* Profiles = NewObject<UHSRCharacterProfileSubsystem>(GameInstance);
	UHSRPartySubsystem* Party = NewObject<UHSRPartySubsystem>(GameInstance);
	Party->InitializeForDevelopmentTest(Profiles);
	UHSRPartyViewModel* Empty = NewObject<UHSRPartyViewModel>();
	Empty->Initialize(Party);
	TestTrue(TEXT("empty readable"), Empty->GetSnapshot(Snapshot));
	TestEqual(TEXT("empty explicit"), Snapshot.Status, EHSRPartyFrontendStatus::Empty);
	TestEqual(TEXT("empty still exposes fixed slots"), Snapshot.Slots.Num(), UHSRPartySubsystem::Capacity);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRPartyFrontendRouteLifecycleTest,
	"HSR.UI.Party.RouteLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRPartyFrontendRouteLifecycleTest::RunTest(const FString&)
{
	ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	UHSRUIManagerSubsystem* Manager = NewObject<UHSRUIManagerSubsystem>(LocalPlayer);
	Manager->InitializeForAutomation();
	Manager->RegisterHostForAutomation();
	TestEqual(TEXT("party opens"), Manager->OpenFrontendModule(EHSRFrontendModule::Party), EHSRUIScreenResult::Success);
	TestEqual(TEXT("party active"), Manager->GetFrontendRouter()->GetSnapshot().GetActiveRoute().Module, EHSRFrontendModule::Party);
	TestEqual(TEXT("repeat open no-op"), Manager->OpenFrontendModule(EHSRFrontendModule::Party), EHSRUIScreenResult::NoOp);
	TestEqual(TEXT("back to hub"), Manager->RequestBack(), EHSRUIScreenResult::Success);
	TestEqual(TEXT("close to exploration"), Manager->RequestBack(), EHSRUIScreenResult::Success);
	TestEqual(TEXT("party reopens"), Manager->OpenFrontendModule(EHSRFrontendModule::Party), EHSRUIScreenResult::Success);
	TestEqual(TEXT("travel teardown"), Manager->TeardownHostIdentityForTravelForAutomation(1), EHSRUIScreenResult::Success);
	Manager->NotifyArrivalCommittedForAutomation(1);
	TestEqual(TEXT("new host"), Manager->RegisterHostIdentityForAutomation(2), EHSRUIScreenResult::Success);
	TestEqual(TEXT("party opens after travel"), Manager->OpenFrontendModule(EHSRFrontendModule::Party), EHSRUIScreenResult::Success);
	Manager->DeinitializeForAutomation();
	return true;
}

#endif
