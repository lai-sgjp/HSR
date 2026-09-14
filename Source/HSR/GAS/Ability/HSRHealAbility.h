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
	virtual EHSRAbilityFailureReason GetLastFailureReason() const override { return FailureReason; }
	virtual EHSRAbilityFailureReason GetPreActivationFailureReason(const FGameplayAbilitySpecHandle& Handle, const FGameplayAbilityActorInfo* ActorInfo) const override;
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle H,const FGameplayAbilityActorInfo* I,const FGameplayAbilityActivationInfo A,const FGameplayEventData* E) override; 
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;
	private:
	TSoftClassPtr<UGameplayEffect> Effect;
	TSoftClassPtr<UGameplayEffect> RefundEffect;
	TWeakObjectPtr<UAbilitySystemComponent> Target;
	EHSRAbilityFailureReason FailureReason = EHSRAbilityFailureReason::EffectFailed;
	bool bSuccess=false;
	mutable bool bCostApplied = false;
};
