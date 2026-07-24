#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "GameplayEffect.h"
#include "../Battle/HSRBattleCoordinator.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"
#include "../Progression/HSRProgressionGameplayTags.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRProgressionEffectContractTest,"HSR.Progression.Effect.Contract",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHSRProgressionEffectContractTest::RunTest(const FString& Parameters)
{
	UGameplayEffect* GE=NewObject<UGameplayEffect>(); GE->DurationPolicy=EGameplayEffectDurationType::Infinite;
	const auto Add=[GE](FGameplayAttribute Attribute,FGameplayTag Tag){FSetByCallerFloat SBC;SBC.DataTag=Tag;FGameplayModifierInfo M;M.Attribute=Attribute;M.ModifierOp=EGameplayModOp::Additive;M.ModifierMagnitude=FGameplayEffectModifierMagnitude(SBC);GE->Modifiers.Add(M);};
	Add(UHSRCoreAttributeSet::GetMaxHealthAttribute(),HSRProgressionTags::BonusMaxHealth);Add(UHSRCoreAttributeSet::GetAttackAttribute(),HSRProgressionTags::BonusAttack);Add(UHSRCoreAttributeSet::GetDefenseAttribute(),HSRProgressionTags::BonusDefense);Add(UHSRCoreAttributeSet::GetSpeedAttribute(),HSRProgressionTags::BonusSpeed);
	TestTrue(TEXT("Exact contract accepted"),UHSRBattleCoordinator::ValidateCharacterProgressionEffectContract(GE)); GE->Modifiers[0].ModifierOp=EGameplayModOp::Override; TestFalse(TEXT("Override rejected"),UHSRBattleCoordinator::ValidateCharacterProgressionEffectContract(GE));
	FHSRCharacterProgressionContext A,B;A.CharacterId=B.CharacterId=TEXT("Character.A"); TestTrue(TEXT("Same fingerprint"),UHSRBattleCoordinator::HasSameProgressionFingerprint(A,B)); B.ProgressionBonuses.Attack=1; TestFalse(TEXT("Bonus changes fingerprint"),UHSRBattleCoordinator::HasSameProgressionFingerprint(A,B)); return true;
}
#endif
