#pragma once

#include "CoreMinimal.h"
#include "HSRGameplayAbilityBase.h"
#include "HSRSkillAbility.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;

UCLASS()
class HSR_API UHSRSkillAbility : public UHSRGameplayAbilityBase
{
	GENERATED_BODY()
public:
	UHSRSkillAbility();
	virtual bool ConfigureFromSkillDefinition(const UHSRSkillDefinition& Definition) override;
	virtual bool SetPendingTarget(UAbilitySystemComponent* InTargetAbilitySystem) override;
	virtual void ClearPendingTarget() override;
	virtual bool DidLastActivationSucceed() const override { return bLastActivationSucceeded; }
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
#if WITH_EDITOR
	void SetEffectClassForDevelopmentTest(TSoftClassPtr<UGameplayEffect> InClass) { EffectClass = MoveTemp(InClass); }
	TSoftClassPtr<UGameplayEffect> GetEffectClassForDevelopmentTest() const { return EffectClass; }
#endif
private:
	TSoftClassPtr<UGameplayEffect> EffectClass;
	TWeakObjectPtr<UAbilitySystemComponent> PendingTargetAbilitySystem;
	bool bLastActivationSucceeded = false;
};
