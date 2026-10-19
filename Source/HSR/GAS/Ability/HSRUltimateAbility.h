#pragma once

#include "CoreMinimal.h"
#include "HSRGameplayAbilityBase.h"
#include "HSRUltimateAbility.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UHSRSkillDefinition;

/**
 * P6-002 synchronous Ultimate. Cost is committed exclusively by GAS through
 * the configured Cost GameplayEffect; Coordinator never writes Energy.
 */
UCLASS()
class HSR_API UHSRUltimateAbility : public UHSRGameplayAbilityBase
{
	GENERATED_BODY()

public:
	UHSRUltimateAbility();

	virtual bool ConfigureFromSkillDefinition(const UHSRSkillDefinition& Definition) override;
	virtual bool SetPendingTarget(UAbilitySystemComponent* InTargetAbilitySystem) override;
	virtual bool SetPendingTargets(const TArray<UAbilitySystemComponent*>& InTargetAbilitySystems) override;
	virtual void ClearPendingTarget() override;
	virtual bool DidLastActivationSucceed() const override { return bLastActivationSucceeded; }
	virtual EHSRAbilityFailureReason GetLastFailureReason() const override { return LastFailureReason; }
	virtual EHSRAbilityFailureReason GetPreActivationFailureReason(const FGameplayAbilitySpecHandle& Handle, const FGameplayAbilityActorInfo* ActorInfo) const override;
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

private:
	TSoftClassPtr<UGameplayEffect> UltimateEffectClass;
	TSoftClassPtr<UGameplayEffect> EnergyRefundEffectClass;
	TArray<TWeakObjectPtr<UAbilitySystemComponent>> PendingTargetAbilitySystems;
	float ReleaseThreshold = 0.0f;
	float EnergyCost = 0.0f;
	bool bRestoresHealth = false;
	bool bConfigured = false;
	bool bActivationInProgress = false;
	mutable bool bCostAttempted = false;
	mutable bool bCostApplied = false;
	FGuid LastExecutedActionId;
	bool bLastActivationSucceeded = false;
	EHSRAbilityFailureReason LastFailureReason = EHSRAbilityFailureReason::EffectFailed;
};
