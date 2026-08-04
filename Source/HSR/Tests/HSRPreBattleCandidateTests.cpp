#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Curves/CurveFloat.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Party/HSRPartySubsystem.h"
#include "../UI/HSRPreBattleCandidateViewModel.h"
#include "../Battle/HSRBattleTransitionSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRPreBattleCandidateTest,
	"HSR.UI.PreBattleCandidate.PureRequest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRPreBattleCandidateTest::RunTest(const FString&)
{
	UGameInstance* GI = NewObject<UGameInstance>();
	UHSRCharacterProfileSubsystem* Profiles = NewObject<UHSRCharacterProfileSubsystem>(GI);
	UHSRPartySubsystem* Party = NewObject<UHSRPartySubsystem>(GI);
	auto Register = [Profiles](const TCHAR* Id)
	{
		UHSRCharacterDefinition* D = NewObject<UHSRCharacterDefinition>(Profiles); D->CharacterId = FName(Id); D->MaxLevel = 2;
		UCurveFloat* C = NewObject<UCurveFloat>(D); C->FloatCurve.AddKey(2, 100); D->CumulativeExperienceCurve = C;
		return Profiles->RegisterDefinition(D);
	};
	Register(TEXT("Character.A")); Register(TEXT("Character.B")); Register(TEXT("Character.C"));
	Party->InitializeForDevelopmentTest(Profiles);
	TestEqual(TEXT("permanent slot 0 seeded"), Party->AddCharacter(TEXT("Character.A"), 0), EHSRPartyResult::Success);
	FHSRPartySnapshot Before; Party->GetSnapshot(Before);

	FHSREncounterRequest Template;
	Template.EncounterId = TEXT("Encounter.Test"); Template.EnemyDefinitionId = TEXT("Enemy.Test"); Template.BattleMapPath = TEXT("/Game/Maps/Map_Battle");
	UHSRPreBattleCandidateViewModel* ViewModel = NewObject<UHSRPreBattleCandidateViewModel>();
	ViewModel->Initialize(Party, Profiles, Template);
	TestEqual(TEXT("candidate edits locally"), ViewModel->SetCandidateSlot(1, TEXT("Character.B")), EHSRPreBattleCandidateResult::Success);
	FHSRPartySnapshot AfterEdit; Party->GetSnapshot(AfterEdit);
	TestEqual(TEXT("party revision unchanged before confirm"), AfterEdit.Revision, Before.Revision);
	TestEqual(TEXT("duplicate rejected"), ViewModel->SetCandidateSlot(1, TEXT("Character.A")), EHSRPreBattleCandidateResult::DuplicateCharacter);
	TestEqual(TEXT("buff selection local"), ViewModel->SetBuff(TEXT("Buff.Test")), EHSRPreBattleCandidateResult::Success);
	FHSREncounterRequest Request;
	TestEqual(TEXT("confirm builds pure request"), ViewModel->ConfirmCandidate(Request), EHSRPreBattleCandidateResult::Success);
	TestEqual(TEXT("request uses candidate leader"), Request.PlayerCharacterId, FName(TEXT("Character.A")));
	TestEqual(TEXT("request carries encounter"), Request.EncounterId, Template.EncounterId);
	TestEqual(TEXT("request carries buff metadata"), ViewModel->GetSnapshot().BuffIds.Num(), 1);
	TestEqual(TEXT("request carries buff ids"), Request.BuffIds.Num(), 1);
	Party->GetSnapshot(AfterEdit);
	TestEqual(TEXT("confirm does not mutate permanent party"), AfterEdit.Revision, Before.Revision);
	TestEqual(TEXT("cancel succeeds"), ViewModel->CancelCandidate(), EHSRPreBattleCandidateResult::Success);
	TestFalse(TEXT("cancel clears pending edits"), ViewModel->GetSnapshot().bHasPendingChanges);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRPreBattleAdmissionBuildTest,
	"HSR.UI.PreBattleCandidate.PureAdmissionBuild", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRPreBattleAdmissionBuildTest::RunTest(const FString&)
{
	FHSRPreBattleAdmissionInput Input;
	Input.Template.EncounterId = TEXT("Encounter.Build");
	Input.Template.EnemyDefinitionId = TEXT("Enemy.Build");
	Input.Template.BattleMapPath = TEXT("/Game/Maps/Map_Battle");
	Input.CandidateParty = { TEXT("Character.A"), TEXT("Character.B") };
	Input.BuffIds = { TEXT("Buff.Test") };
	FHSREncounterRequest Out;
	TestEqual(TEXT("pure admission build succeeds"), UHSRBattleTransitionSubsystem::BuildEncounterRequest(Input, Out), EHSREncounterResultType::Success);
	TestEqual(TEXT("pure build copies leader"), Out.PlayerCharacterId, FName(TEXT("Character.A")));
	TestEqual(TEXT("pure build copies buffs"), Out.BuffIds.Num(), 1);
	Input.CandidateParty[1] = TEXT("Character.A");
	TestEqual(TEXT("pure build rejects duplicate candidate"), UHSRBattleTransitionSubsystem::BuildEncounterRequest(Input, Out), EHSREncounterResultType::InvalidRequest);
	return true;
}

#endif
