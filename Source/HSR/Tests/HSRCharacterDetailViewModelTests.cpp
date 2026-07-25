#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Curves/CurveFloat.h"
#include "../UI/HSRCharacterDetailViewModel.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Save/HSRSaveSubsystem.h"
#include "../Party/HSRPartySubsystem.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRCharacterDetailViewModelTest,"HSR.UI.CharacterDetail.ViewModel",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FHSRCharacterDetailViewModelTest::RunTest(const FString&)
{
	UGameInstance* GI=NewObject<UGameInstance>();
	auto* Profiles=NewObject<UHSRCharacterProfileSubsystem>(GI); auto* Save=NewObject<UHSRSaveSubsystem>(GI); auto* Party=NewObject<UHSRPartySubsystem>(GI);
	auto* D=NewObject<UHSRCharacterDefinition>(); D->CharacterId=TEXT("A"); D->DisplayName=FText::FromString(TEXT("Alpha")); D->MaxLevel=2; D->BaseMaxHealth=500; D->BaseAttack=50; D->SkillMaxLevels.Add(TEXT("Z"),3); D->SkillMaxLevels.Add(TEXT("A"),2); auto* C=NewObject<UCurveFloat>(D); C->FloatCurve.AddKey(2,100); D->CumulativeExperienceCurve=C;
	auto* Other=NewObject<UHSRCharacterDefinition>(); Other->CharacterId=TEXT("B"); Other->MaxLevel=2; auto* OtherCurve=NewObject<UCurveFloat>(Other); OtherCurve->FloatCurve.AddKey(2,100); Other->CumulativeExperienceCurve=OtherCurve;
	Profiles->RegisterDefinitions({D,Other}); Party->InitializeForDevelopmentTest(Profiles); Save->InitializeForDevelopmentTest(Profiles,Party); Party->AddCharacter(TEXT("A"));
	auto* VM=NewObject<UHSRCharacterDetailViewModel>(); VM->Initialize(Profiles,Save,Party); int32 Events=0; VM->OnChanged().AddLambda([&](const auto&){++Events;});
	TestEqual(TEXT("party select"),VM->SelectPartySlot0(),EHSRCharacterDetailResult::Success);
	FHSRCharacterDetailSnapshot S; TestTrue(TEXT("snapshot"),VM->GetSnapshot(S)); TestTrue(TEXT("snapshot valid"),S.bIsValid); TestEqual(TEXT("snapshot failure clear"),S.FailureReason,EHSRCharacterDetailResult::Success); TestEqual(TEXT("skill stable first"),S.Skills[0].SkillId,FName(TEXT("A"))); TestEqual(TEXT("base stat"),S.BaseStats.MaxHealth,500.0f); TestFalse(TEXT("empty portrait absent"),S.bHasPortrait); TestTrue(TEXT("empty portrait path"),S.PortraitPath.IsNull()); TestEqual(TEXT("current cumulative"),S.ExperienceForCurrentLevel,0); TestEqual(TEXT("next cumulative"),S.ExperienceForNextLevel,100); TestFalse(TEXT("not max"),S.bAtMaxLevel);
	const int32 Before=Events; TestEqual(TEXT("none candidate"),VM->SelectCharacter(NAME_None),EHSRCharacterDetailResult::InvalidCharacterId); TestEqual(TEXT("invalid candidate"),VM->SelectCharacter(TEXT("Missing")),EHSRCharacterDetailResult::ProfileNotFound); FHSRCharacterDetailSnapshot Still; VM->GetSnapshot(Still); TestEqual(TEXT("old selection retained"),Still.CharacterId,FName(TEXT("A"))); TestEqual(TEXT("failed selection has no event"),Events,Before);
	Profiles->GrantExperience(TEXT("B"),100); TestEqual(TEXT("other profile no refresh"),Events,Before); Party->AddCharacter(TEXT("B")); TestEqual(TEXT("party change keeps explicit selection quiet"),Events,Before);
	Profiles->GrantExperience(TEXT("A"),100); TestEqual(TEXT("selected profile refresh once"),Events,Before+1); VM->GetSnapshot(S); TestEqual(TEXT("level refreshed"),S.Level,2); TestTrue(TEXT("max level"),S.bAtMaxLevel); TestEqual(TEXT("max current threshold"),S.ExperienceForCurrentLevel,100); TestEqual(TEXT("max next threshold stable"),S.ExperienceForNextLevel,100); TestEqual(TEXT("progression bonus"),S.ProgressionBonuses.MaxHealth,10.0f); TestEqual(TEXT("derived stat"),S.DerivedStats.MaxHealth,510.0f);
	FHSRSaveData Baseline; TestEqual(TEXT("save baseline"),Save->SaveSnapshot(Baseline),EHSRSaveResult::Success); Profiles->SetSkillLevel(TEXT("A"),TEXT("A"),1); const int32 BeforeLoad=Events; TestEqual(TEXT("load refreshes at most once"),Save->LoadSnapshot(Baseline),EHSRSaveResult::Success); TestEqual(TEXT("load exactly once"),Events,BeforeLoad+1); TestEqual(TEXT("repeat load success"),Save->LoadSnapshot(Baseline),EHSRSaveResult::Success); TestEqual(TEXT("repeat load no refresh"),Events,BeforeLoad+1);
	S.CharacterId=TEXT("Mutated"); VM->GetSnapshot(Still); TestEqual(TEXT("copy isolated"),Still.CharacterId,FName(TEXT("A")));
	VM->Initialize(Profiles,Save,Party); VM->Initialize(Profiles,Save,Party); const int32 BeforeRebindMutation=Events; Profiles->SetSkillLevel(TEXT("A"),TEXT("A"),1); TestEqual(TEXT("reinitialize avoids duplicate profile binding"),Events,BeforeRebindMutation+1); VM->Uninitialize(); VM->Uninitialize(); const int32 After=Events; Profiles->GrantExperience(TEXT("A"),0); TestEqual(TEXT("uninitialized no event"),Events,After); return true;
}
#endif
