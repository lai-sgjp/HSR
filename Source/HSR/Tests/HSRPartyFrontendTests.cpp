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
	TestEqual(TEXT("widget forwards candidate clear"), Widget->ClearCandidateSlot(1), EHSRPartyResult::Success);
	FHSRPartySnapshot BeforeWidgetConfirm; Party->GetSnapshot(BeforeWidgetConfirm);
	TestFalse(TEXT("widget candidate does not mutate authority"), BeforeWidgetConfirm.Slots[1].IsEmpty());
	TestEqual(TEXT("widget forwards candidate cancel"), Widget->CancelCandidate(), EHSRPartyResult::Success);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRPartyFrontendCandidateEditingTest,
	"HSR.UI.Party.CandidateEditing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRPartyFrontendCandidateEditingTest::RunTest(const FString&)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UHSRCharacterProfileSubsystem* Profiles = NewObject<UHSRCharacterProfileSubsystem>(GameInstance);
	UHSRPartySubsystem* Party = NewObject<UHSRPartySubsystem>(GameInstance);
	auto Register = [Profiles](const TCHAR* Id)
	{
		UHSRCharacterDefinition* Definition = NewObject<UHSRCharacterDefinition>(Profiles);
		Definition->CharacterId = FName(Id); Definition->MaxLevel = 2;
		UCurveFloat* Curve = NewObject<UCurveFloat>(Definition); Curve->FloatCurve.AddKey(2, 100);
		Definition->CumulativeExperienceCurve = Curve;
		return Profiles->RegisterDefinition(Definition);
	};
	TestEqual(TEXT("register B"), Register(TEXT("Character.B")), EHSRCharacterProfileResult::Success);
	TestEqual(TEXT("register A"), Register(TEXT("Character.A")), EHSRCharacterProfileResult::Success);
	TestEqual(TEXT("register C"), Register(TEXT("Character.C")), EHSRCharacterProfileResult::Success);
	Party->InitializeForDevelopmentTest(Profiles);
	TestEqual(TEXT("seed permanent A"), Party->AddCharacter(TEXT("Character.A"), 0), EHSRPartyResult::Success);

	UHSRPartyViewModel* ViewModel = NewObject<UHSRPartyViewModel>();
	ViewModel->Initialize(Party, Profiles);
	FHSRPartyFrontendSnapshot Snapshot;
	TestTrue(TEXT("candidate snapshot available"), ViewModel->GetSnapshot(Snapshot));
	TestEqual(TEXT("available characters sorted"), Snapshot.AvailableCharacterIds[0], FName(TEXT("Character.A")));
	TestFalse(TEXT("initial candidate clean"), Snapshot.bHasPendingChanges);
	TestEqual(TEXT("select B into candidate"), ViewModel->SetCandidateSlot(1, TEXT("Character.B")), EHSRPartyResult::Success);
	ViewModel->GetSnapshot(Snapshot);
	TestTrue(TEXT("candidate becomes dirty"), Snapshot.bHasPendingChanges);
	TestEqual(TEXT("candidate displays B"), Snapshot.Slots[1].CharacterId, FName(TEXT("Character.B")));
	FHSRPartySnapshot Permanent; Party->GetSnapshot(Permanent);
	TestTrue(TEXT("permanent slot remains empty before confirm"), Permanent.Slots[1].IsEmpty());
	TestEqual(TEXT("duplicate candidate rejected"), ViewModel->SetCandidateSlot(1, TEXT("Character.A")), EHSRPartyResult::DuplicateCharacter);
	TestEqual(TEXT("cancel candidate"), ViewModel->CancelCandidate(), EHSRPartyResult::Success);
	ViewModel->GetSnapshot(Snapshot);
	TestFalse(TEXT("cancel restores clean state"), Snapshot.bHasPendingChanges);
	TestFalse(TEXT("cancel restores empty slot"), Snapshot.Slots[1].bOccupied);

	TestEqual(TEXT("reselect B"), ViewModel->SetCandidateSlot(1, TEXT("Character.B")), EHSRPartyResult::Success);
	TestEqual(TEXT("confirm candidate"), ViewModel->ConfirmCandidate(), EHSRPartyResult::Success);
	Party->GetSnapshot(Permanent);
	TestEqual(TEXT("confirm installs B"), Permanent.Slots[1].CharacterId, FName(TEXT("Character.B")));
	ViewModel->GetSnapshot(Snapshot);
	TestFalse(TEXT("confirmed candidate clean"), Snapshot.bHasPendingChanges);

	TestEqual(TEXT("edit from current revision"), ViewModel->ClearCandidateSlot(1), EHSRPartyResult::Success);
	TestEqual(TEXT("external authority mutation"), Party->ReplaceCharacter(1, TEXT("Character.C")), EHSRPartyResult::Success);
	TestEqual(TEXT("stale candidate rejected"), ViewModel->ConfirmCandidate(), EHSRPartyResult::RevisionConflict);
	Party->GetSnapshot(Permanent);
	TestEqual(TEXT("stale confirm preserves external state"), Permanent.Slots[1].CharacterId, FName(TEXT("Character.C")));
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
