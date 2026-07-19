#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "HSRBasicAttackAbility.generated.h"

class UAbilitySystemComponent;

/**
 * Minimal, synchronous battle action.  Target selection and turn authority stay
 * in UHSRBattleCoordinator; this ability only applies the configured GAS effect.
 */
UCLASS()
class HSR_API UHSRBasicAttackAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UHSRBasicAttackAbility();

	/** Must be supplied by the coordinator immediately before activation. */
	bool SetPendingTarget(UAbilitySystemComponent* InTargetAbilitySystem);
	bool DidLastActivationSucceed() const { return bLastActivationSucceeded; }

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
