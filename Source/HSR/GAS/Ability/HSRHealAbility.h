#pragma once
#include "CoreMinimal.h"
#include "HSRGameplayAbilityBase.h"
#include "HSRHealAbility.generated.h"
class UAbilitySystemComponent; 
class UGameplayEffect;
UCLASS() class HSR_API UHSRHealAbility : public UHSRGameplayAbilityBase
{ 
	GENERATED_BODY() 
public: 
	UHSRHealAbility(); 
	virtual bool ConfigureFromSkillDefinition(const UHSRSkillDefinition& Definition) override; 
	virtual bool SetPendingTarget(UAbilitySystemComponent* Target) override; 
	virtual void ClearPendingTarget() override; 
	virtual bool DidLastActivationSucceed() const override{return bSuccess;} 
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle H,const FGameplayAbilityActorInfo* I,const FGameplayAbilityActivationInfo A,const FGameplayEventData* E) override; 
	private: TSoftClassPtr<UGameplayEffect> Effect; TWeakObjectPtr<UAbilitySystemComponent> Target; bool bSuccess=false; };
