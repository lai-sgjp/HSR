#include "HSRHealAbility.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "../../Data/HSRSkillDefinition.h"
UHSRHealAbility::UHSRHealAbility(){
	SetActionContext(FGuid(),FName(TEXT("Heal")));
} 
bool UHSRHealAbility::ConfigureFromSkillDefinition(const UHSRSkillDefinition& D)
{
	if(!D.IsValidHealDefinition())
		return false;
	Effect=D.EffectGameplayEffectClass;
	return !Effect.IsNull();
}
bool UHSRHealAbility::SetPendingTarget(UAbilitySystemComponent* T){
	if(!T)return false;
	Target=T;
	return true;
}
void UHSRHealAbility::ClearPendingTarget(){
	Target.Reset();
}
void UHSRHealAbility::ActivateAbility(const FGameplayAbilitySpecHandle H,const FGameplayAbilityActorInfo* I,const FGameplayAbilityActivationInfo A,const FGameplayEventData* E){
	bSuccess=false;
	auto*S=I?I->AbilitySystemComponent.Get():nullptr;
	auto*T=Target.Get();
	auto C=Effect.LoadSynchronous();
	if(S&&T&&C){
		auto X=S->MakeEffectContext();
		auto Spec=S->MakeOutgoingSpec(C,1,X);
		if(Spec.IsValid())
			bSuccess=S->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(),T).WasSuccessfullyApplied();
	}
	ClearPendingTarget();
	EndAbility(H,I,A,true,!bSuccess);
}
