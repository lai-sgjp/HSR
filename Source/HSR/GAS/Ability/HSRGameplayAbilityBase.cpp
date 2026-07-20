#include "HSRGameplayAbilityBase.h"

#include "HSRAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "../Damage/HSRDamageEffectContext.h"

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

bool UHSRGameplayAbilityBase::PrepareFormalDamage(const FHSRFormalDamageRequest& Request, const FGameplayEffectSpecHandle& Spec, UAbilitySystemComponent* TargetAbilitySystem, FHSRFormalDamagePrepareResult& OutResult)
{
	ClearPreparedFormalDamage();
	OutResult.ActionId = Request.ActionId;
	if (!Request.ActionId.IsValid() || !Spec.IsValid() || !TargetAbilitySystem)
	{
		OutResult.Result = EHSRDamageResultType::SpecCreationFailed;
		return false;
	}
	PreparedFormalDamage.Request = Request;
	PreparedFormalDamage.Spec = Spec;
	PreparedFormalDamage.Target = TargetAbilitySystem;
	PreparedFormalDamage.bPrepared = true;
	OutResult.Result = EHSRDamageResultType::DamageResolved;
	OutResult.bPrepared = true;
	return true;
}

void UHSRGameplayAbilityBase::ClearPreparedFormalDamage()
{
	PreparedFormalDamage = FHSRPreparedFormalDamage();
}

bool UHSRGameplayAbilityBase::ApplyPreparedFormalDamage(UAbilitySystemComponent* SourceAbilitySystem)
{
	LastFormalDamageExecutionResult = FHSRFormalDamageExecutionResult();
	if (!SourceAbilitySystem || !PreparedFormalDamage.bPrepared || !PreparedFormalDamage.Target.IsValid() || !PreparedFormalDamage.Spec.IsValid())
	{
		ClearPreparedFormalDamage();
		return false;
	}
	const FActiveGameplayEffectHandle Applied = SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(*PreparedFormalDamage.Spec.Data.Get(), PreparedFormalDamage.Target.Get());
	LastFormalDamageExecutionResult.bSucceeded = Applied.WasSuccessfullyApplied();
	if (const FHSRDamageEffectContext* DamageContext = static_cast<const FHSRDamageEffectContext*>(PreparedFormalDamage.Spec.Data->GetContext().Get()))
	{
		LastFormalDamageExecutionResult.DamageResult = DamageContext->DamageResult;
	}
	if (!LastFormalDamageExecutionResult.bSucceeded)
	{
		LastFormalDamageExecutionResult.DamageResult.Result = EHSRDamageResultType::EffectApplicationFailed;
	}
	ClearPreparedFormalDamage();
	return LastFormalDamageExecutionResult.bSucceeded;
}
