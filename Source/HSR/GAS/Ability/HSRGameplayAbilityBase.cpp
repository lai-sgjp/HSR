#include "HSRGameplayAbilityBase.h"

#include "HSRAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "../Damage/HSRDamageEffectContext.h"
#include "../Attribute/HSRCoreAttributeSet.h"

// 构造函数：能力默认按“每 Actor 实例化”策略创建（每个角色一份实例，可持有跨激活状态）。
UHSRGameplayAbilityBase::UHSRGameplayAbilityBase()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

// 设置待处理目标（基类默认不可用，由具体能力子类覆写）。
bool UHSRGameplayAbilityBase::SetPendingTarget(UAbilitySystemComponent* InTargetAbilitySystem)
{
	return false;
}

bool UHSRGameplayAbilityBase::SetPendingTargets(const TArray<UAbilitySystemComponent*>& InTargetAbilitySystems)
{
	return InTargetAbilitySystems.Num() == 1 && SetPendingTarget(InTargetAbilitySystems[0]);
}

// 清除待处理目标（基类空实现）。
void UHSRGameplayAbilityBase::ClearPendingTarget()
{
}

// 最近一次激活是否成功（基类默认失败，由子类覆写）。
bool UHSRGameplayAbilityBase::DidLastActivationSucceed() const
{
	return false;
}

// 最近一次失败的原因（基类默认笼统返回 EffectFailed）。
EHSRAbilityFailureReason UHSRGameplayAbilityBase::GetLastFailureReason() const
{
	return EHSRAbilityFailureReason::EffectFailed;
}

// 激活前的失败原因（基类默认无阻碍）。
EHSRAbilityFailureReason UHSRGameplayAbilityBase::GetPreActivationFailureReason(const FGameplayAbilitySpecHandle& Handle, const FGameplayAbilityActorInfo* ActorInfo) const
{
	return EHSRAbilityFailureReason::None;
}

// 查询“技能当前是否可用”时的失败原因（用于 UI 灰置）。基类实现：
// - 目标已由 FHSRTargetingPolicy 校验过身份，这里绝不再调用 SetPendingTarget
//   （可用性刷新必须是观察纯的，不能产生副作用）；
// - 目标 ASC 存在则走预激活检查，否则视为无效目标。
EHSRAbilityFailureReason UHSRGameplayAbilityBase::GetAvailabilityFailureReason(const FGameplayAbilitySpecHandle& Handle, const FGameplayAbilityActorInfo* ActorInfo, const UAbilitySystemComponent* CandidateTargetAbilitySystem) const
{
#if WITH_EDITOR
	++AvailabilityQueryCount;
#endif
	return CandidateTargetAbilitySystem ? GetPreActivationFailureReason(Handle, ActorInfo) : EHSRAbilityFailureReason::InvalidTarget;
}

// 从技能定义配置本能力（基类默认直接成功，由子类覆写）。
bool UHSRGameplayAbilityBase::ConfigureFromSkillDefinition(const UHSRSkillDefinition& Definition)
{
	return true;
}

// 准备形式化伤害：把请求、GE spec 与目标 ASC 暂存在能力上，供激活时一次性施加。
// 这是“准备”阶段（纯数据装配，不动世界状态）。
bool UHSRGameplayAbilityBase::PrepareFormalDamage(const FHSRFormalDamageRequest& Request, const FGameplayEffectSpecHandle& Spec, UAbilitySystemComponent* TargetAbilitySystem, FHSRFormalDamagePrepareResult& OutResult)
{
	ClearPreparedFormalDamage();
	LastAdditionalFormalDamageResults.Reset();
	OutResult = FHSRFormalDamagePrepareResult();
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

bool UHSRGameplayAbilityBase::PrepareAdditionalFormalDamage(const FHSRFormalDamageRequest& Request,
	const FGameplayEffectSpecHandle& Spec, UAbilitySystemComponent* TargetAbilitySystem, FHSRFormalDamagePrepareResult& OutResult)
{
	OutResult = FHSRFormalDamagePrepareResult();
	OutResult.ActionId = Request.ActionId;
	OutResult.Result = EHSRDamageResultType::SpecCreationFailed;
	if (bApplyingPreparedDamage || !PreparedFormalDamage.bPrepared || !Spec.IsValid() || !IsValid(TargetAbilitySystem)
		|| Request.ActionId != PreparedFormalDamage.Request.ActionId || Request.BattleId != PreparedFormalDamage.Request.BattleId
		|| Request.SourceParticipantId != PreparedFormalDamage.Request.SourceParticipantId
		|| Request.TargetParticipantId.IsNone() || Request.TargetParticipantId == PreparedFormalDamage.Request.TargetParticipantId
		|| TargetAbilitySystem == PreparedFormalDamage.Target.Get()
		|| AdditionalPreparedFormalDamage.ContainsByPredicate([&](const auto& Entry)
		{
			return Entry.Target.Get() == TargetAbilitySystem || Entry.Request.TargetParticipantId == Request.TargetParticipantId;
		})) return false;
	FHSRPreparedFormalDamage& Additional = AdditionalPreparedFormalDamage.AddDefaulted_GetRef();
	Additional.Request = Request;
	Additional.Spec = Spec;
	Additional.Target = TargetAbilitySystem;
	Additional.bPrepared = true;
	OutResult.Result = EHSRDamageResultType::DamageResolved;
	OutResult.bPrepared = true;
	return true;
}

// 清除已准备的正式伤害数据。
void UHSRGameplayAbilityBase::ClearPreparedFormalDamage()
{
	PreparedFormalDamage = FHSRPreparedFormalDamage();
	AdditionalPreparedFormalDamage.Reset();
}

// 施加已准备的正式伤害：把暂存的 GE spec 施加到目标 ASC，并记录执行结果。
bool UHSRGameplayAbilityBase::ApplyPreparedFormalDamage(UAbilitySystemComponent* SourceAbilitySystem)
{
	if (bApplyingPreparedDamage) return false;
	TGuardValue<bool> Applying(bApplyingPreparedDamage, true);
	// Consume prepared specs before invoking external attribute delegates; they cannot
	// re-enter this seam and apply the same effects a second time.
	const FHSRPreparedFormalDamage Primary = MoveTemp(PreparedFormalDamage);
	const TArray<FHSRPreparedFormalDamage> Additional = MoveTemp(AdditionalPreparedFormalDamage);
	ClearPreparedFormalDamage();
	LastAdditionalFormalDamageResults.Reset();
	LastFormalDamageExecutionResult = ApplyOnePreparedFormalDamage(Primary, SourceAbilitySystem);
	int32 SuccessCount = LastFormalDamageExecutionResult.bSucceeded ? 1 : 0;
	for (const auto& Entry : Additional)
	{
		FHSRFormalDamageExecutionResult Result = ApplyOnePreparedFormalDamage(Entry, SourceAbilitySystem);
		if (Result.bSucceeded) ++SuccessCount;
		LastAdditionalFormalDamageResults.Add(Result);
	}
	if (SuccessCount > 0 && SuccessCount != 1 + Additional.Num())
		UE_LOG(LogTemp, Warning, TEXT("UltimateDamage PartialSettlement ActionId=%s Applied=%d Requested=%d PrimaryResult=%d"),
			*Primary.Request.ActionId.ToString(), SuccessCount, 1 + Additional.Num(), int32(LastFormalDamageExecutionResult.DamageResult.Result));
	// DamageResult remains the primary target's result. bSucceeded is the overall
	// transaction flag so a partial AOE cannot refund Cost after applying valid damage.
	LastFormalDamageExecutionResult.bSucceeded = SuccessCount > 0;
	return LastFormalDamageExecutionResult.bSucceeded;
}

FHSRFormalDamageExecutionResult UHSRGameplayAbilityBase::ApplyOnePreparedFormalDamage(
	const FHSRPreparedFormalDamage& Prepared, UAbilitySystemComponent* SourceAbilitySystem)
{
	FHSRFormalDamageExecutionResult Result;
	Result.DamageResult.ActionId = Prepared.Request.ActionId;
	Result.DamageResult.Result = EHSRDamageResultType::EffectApplicationFailed;
	if (!IsValid(SourceAbilitySystem) || !Prepared.bPrepared || !Prepared.Target.IsValid() || !Prepared.Spec.IsValid()
		|| Prepared.Target->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) <= 0.f) return Result;
	UAbilitySystemComponent* Target = Prepared.Target.Get();
	if (!IsValid(SourceAbilitySystem->GetOwnerActor()) || !IsValid(SourceAbilitySystem->GetAvatarActor())
		|| !IsValid(Target->GetOwnerActor()) || !IsValid(Target->GetAvatarActor())) return Result;
	const float HealthBefore = Target->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());
	const FGameplayEffectContext* RawContext = Prepared.Spec.Data->GetContext().Get();
	if (!RawContext || RawContext->GetScriptStruct() != FHSRDamageEffectContext::StaticStruct()) return Result;
	const FHSRDamageEffectContext* DamageContext = static_cast<const FHSRDamageEffectContext*>(RawContext);
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	UE_LOG(LogTemp, Log, TEXT("P7-004 AbilityApply Context ActionId=%s Target=%s Injection=%d"),
		*DamageContext->DamageContext.ActionId.ToString(), *Prepared.Request.TargetParticipantId.ToString(), static_cast<int32>(DamageContext->TestInjection));
	// 开发测试注入：强制“提交成本后施加失败”场景。
	if (DamageContext->TestInjection == EHSRDamageTestInjection::ForcePostCostApplyFailure)
	{
		return Result;
	}
#endif
	const FActiveGameplayEffectHandle Applied = SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(*Prepared.Spec.Data.Get(), Target);
	Result.bSucceeded = Applied.WasSuccessfullyApplied();
	{
		Result.DamageResult = DamageContext->DamageResult;
		#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
		const bool bInjectedFailure = DamageContext->TestInjection == EHSRDamageTestInjection::ForceCaptureFailed
			|| DamageContext->TestInjection == EHSRDamageTestInjection::ForceInvalidCapturedValue
			|| DamageContext->TestInjection == EHSRDamageTestInjection::ForcePostCostApplyFailure;
		#else
		const bool bInjectedFailure = false;
		#endif
		// GAS 可能重复求值执行上下文，导致纯诊断结果停留在默认值而 Apply 句柄却有效。
		// 正常路径的权威成功标志是句柄；只有注入式失败必须保留非成功结果。
		if (Result.bSucceeded && !bInjectedFailure
			&& Result.DamageResult.Result != EHSRDamageResultType::CaptureFailed
			&& Result.DamageResult.Result != EHSRDamageResultType::InvalidCapturedValue)
		{
			Result.DamageResult.Result = EHSRDamageResultType::DamageResolved;
		}
	}
	// 最终成功 = 句柄有效 且 结果为 DamageResolved。
	Result.bSucceeded = Result.bSucceeded && Result.DamageResult.Result == EHSRDamageResultType::DamageResolved;
	if (Result.bSucceeded && IsValid(Target))
	{
		// GAS executes a copied effect context. Its diagnostic result may not be
		// written back to the submitted spec even though HP changed authoritatively.
		// Only this target's applied damage is recovered from its actual attributes;
		// FinalDamage/crit and the other targets' breakdowns remain independent.
		const float HealthAfter = Target->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());
		Result.DamageResult.Breakdown.AppliedDamage = FMath::Max(0.f, HealthBefore - HealthAfter);
	}
	// 句柄失败但结果不是已知的诊断失败时，归一化为 EffectApplicationFailed。
	if (!Result.bSucceeded
		&& Result.DamageResult.Result != EHSRDamageResultType::CaptureFailed
		&& Result.DamageResult.Result != EHSRDamageResultType::InvalidCapturedValue)
	{
		Result.DamageResult.Result = EHSRDamageResultType::EffectApplicationFailed;
	}
	return Result;
}

