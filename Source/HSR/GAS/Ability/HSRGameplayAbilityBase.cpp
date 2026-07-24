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

EHSRAbilityFailureReason UHSRGameplayAbilityBase::GetAvailabilityFailureReason(const FGameplayAbilitySpecHandle& Handle, const FGameplayAbilityActorInfo* ActorInfo, const UAbilitySystemComponent* CandidateTargetAbilitySystem) const
{
#if WITH_EDITOR
	++AvailabilityQueryCount;
#endif
	// Target identity is already validated by FHSRTargetingPolicy.  Do not call
	// SetPendingTarget here: availability refreshes must be observationally pure.
	return CandidateTargetAbilitySystem ? GetPreActivationFailureReason(Handle, ActorInfo) : EHSRAbilityFailureReason::InvalidTarget;
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
#if WITH_EDITOR
	if (const FHSRDamageEffectContext* DamageContext = static_cast<const FHSRDamageEffectContext*>(PreparedFormalDamage.Spec.Data->GetContext().Get()))
	{
		UE_LOG(LogTemp, Log, TEXT("P7-004 AbilityApply Context ActionId=%s Injection=%d"), *DamageContext->DamageContext.ActionId.ToString(), static_cast<int32>(DamageContext->TestInjection));
	}
	if (const FHSRDamageEffectContext* DamageContext = static_cast<const FHSRDamageEffectContext*>(PreparedFormalDamage.Spec.Data->GetContext().Get()); DamageContext && DamageContext->TestInjection == EHSRDamageTestInjection::ForcePostCostApplyFailure)
	{
		LastFormalDamageExecutionResult.DamageResult.Result = EHSRDamageResultType::EffectApplicationFailed;
		ClearPreparedFormalDamage();
		return false;
	}
#endif
	const FActiveGameplayEffectHandle Applied = SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(*PreparedFormalDamage.Spec.Data.Get(), PreparedFormalDamage.Target.Get());
	LastFormalDamageExecutionResult.bSucceeded = Applied.WasSuccessfullyApplied();
	if (const FHSRDamageEffectContext* DamageContext = static_cast<const FHSRDamageEffectContext*>(PreparedFormalDamage.Spec.Data->GetContext().Get()))
	{
		LastFormalDamageExecutionResult.DamageResult = DamageContext->DamageResult;
		#if WITH_EDITOR
		const bool bInjectedFailure = DamageContext->TestInjection == EHSRDamageTestInjection::ForceCaptureFailed || DamageContext->TestInjection == EHSRDamageTestInjection::ForceInvalidCapturedValue || DamageContext->TestInjection == EHSRDamageTestInjection::ForcePostCostApplyFailure;
		#else
		const bool bInjectedFailure = false;
		#endif
		// GAS may evaluate a duplicated execution context and leave the pure
		// diagnostic result at its default while the Apply handle is valid. The
		// authoritative normal-path success is the handle; injected failures are
		// the only cases that must preserve a non-success result.
		if (LastFormalDamageExecutionResult.bSucceeded && !bInjectedFailure
			&& LastFormalDamageExecutionResult.DamageResult.Result != EHSRDamageResultType::CaptureFailed
			&& LastFormalDamageExecutionResult.DamageResult.Result != EHSRDamageResultType::InvalidCapturedValue)
		{
			LastFormalDamageExecutionResult.DamageResult.Result = EHSRDamageResultType::DamageResolved;
		}
	}
	LastFormalDamageExecutionResult.bSucceeded = LastFormalDamageExecutionResult.bSucceeded
		&& LastFormalDamageExecutionResult.DamageResult.Result == EHSRDamageResultType::DamageResolved;
	if (!LastFormalDamageExecutionResult.bSucceeded
		&& LastFormalDamageExecutionResult.DamageResult.Result != EHSRDamageResultType::CaptureFailed
		&& LastFormalDamageExecutionResult.DamageResult.Result != EHSRDamageResultType::InvalidCapturedValue)
	{
		LastFormalDamageExecutionResult.DamageResult.Result = EHSRDamageResultType::EffectApplicationFailed;
	}
	ClearPreparedFormalDamage();
	return LastFormalDamageExecutionResult.bSucceeded;
}
