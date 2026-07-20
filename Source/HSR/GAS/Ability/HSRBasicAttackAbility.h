#pragma once

#include "CoreMinimal.h"
#include "HSRGameplayAbilityBase.h"
#include "HSRBasicAttackAbility.generated.h"

class UAbilitySystemComponent;

/**
 * Minimal, synchronous battle action.  Target selection and turn authority stay
 * in UHSRBattleCoordinator; this ability only applies the configured GAS effect.
 */
UCLASS()
class HSR_API UHSRBasicAttackAbility : public UHSRGameplayAbilityBase
{
	GENERATED_BODY()

public:
	UHSRBasicAttackAbility();

	/** Must be supplied by the coordinator immediately before activation. */
	virtual bool SetPendingTarget(UAbilitySystemComponent* InTargetAbilitySystem) override;
	virtual void ClearPendingTarget() override;
	virtual bool DidLastActivationSucceed() const override { return bLastActivationSucceeded; }

#if WITH_EDITOR
	/** Development-only seam for proving that a missing effect cannot resolve a turn. */
	void SetDamageEffectClassForDevelopmentTest(TSoftClassPtr<UGameplayEffect> InDamageEffectClass) { DamageEffectClass = MoveTemp(InDamageEffectClass); }
	TSoftClassPtr<UGameplayEffect> GetDamageEffectClassForDevelopmentTest() const { return DamageEffectClass; }
#endif

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

private:
	/** User-owned Blueprint GameplayEffect: instant, Health additive -10. */
	TSoftClassPtr<UGameplayEffect> DamageEffectClass;
	TWeakObjectPtr<UAbilitySystemComponent> PendingTargetAbilitySystem;
	bool bLastActivationSucceeded = false;
};
