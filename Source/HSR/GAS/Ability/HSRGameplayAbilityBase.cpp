#include "HSRGameplayAbilityBase.h"

#include "HSRAbilityTypes.h"

UHSRGameplayAbilityBase::UHSRGameplayAbilityBase()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

bool UHSRGameplayAbilityBase::SetPendingTarget(UAbilitySystemComponent* InTargetAbilitySystem)
{
	return false;
}

void UHSRGameplayAbilityBase::ClearPendingTarget()
{
}

bool UHSRGameplayAbilityBase::DidLastActivationSucceed() const
{
	return false;
}

EHSRAbilityFailureReason UHSRGameplayAbilityBase::GetLastFailureReason() const
{
	return EHSRAbilityFailureReason::EffectFailed;
}

EHSRAbilityFailureReason UHSRGameplayAbilityBase::GetPreActivationFailureReason(const FGameplayAbilitySpecHandle& Handle, const FGameplayAbilityActorInfo* ActorInfo) const
{
	return EHSRAbilityFailureReason::None;
}

bool UHSRGameplayAbilityBase::ConfigureFromSkillDefinition(const UHSRSkillDefinition& Definition)
{
	return true;
}
