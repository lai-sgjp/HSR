#include "HSRUltimateAbility.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "../Attribute/HSRCoreAttributeSet.h"
#include "../../Data/HSRSkillDefinition.h"

namespace
{
	// 检查效果是否含“指定属性上的负 Additive 修饰符”（用于识别能量消耗效果）。
	// 命中时通过 OutMagnitude 返回该修饰符的数值。
	bool HasNegativeAttributeModifier(const UGameplayEffect* Effect, const FGameplayAttribute& Attribute, float& OutMagnitude)
	{
		OutMagnitude = 0.0f;
		if (!Effect)
		{
			return false;
		}

		for (const FGameplayModifierInfo& Modifier : Effect->Modifiers)
		{
			float Magnitude = 0.0f;
			if (Modifier.Attribute == Attribute
				&& Modifier.ModifierOp == EGameplayModOp::Additive
				&& Modifier.ModifierMagnitude.GetStaticMagnitudeIfPossible(1.0f, Magnitude)
				&& Magnitude < 0.0f)
			{
				OutMagnitude = Magnitude;
				return true;
			}
		}
		return false;
	}
}

// 构造函数：把默认动作上下文固定为 Ultimate。
UHSRUltimateAbility::UHSRUltimateAbility()
{
	SetActionContext(FGuid(), FName(TEXT("Ultimate")));
}

// 从技能定义配置本能力：
// - 必须满足“合法的终结技定义”；
// - 解析能量消耗 GE、终结技效果 GE 与能量返还 GE；
// - 校验消耗 GE 含负能量修饰符、返还 GE 为 Instant 且含与消耗等额的正能量修饰符。
bool UHSRUltimateAbility::ConfigureFromSkillDefinition(const UHSRSkillDefinition& Definition)
{
	if (!Definition.IsValidUltimateDefinition())
	{
		return false;
	}

	CostGameplayEffectClass = Definition.CostGameplayEffectClass.LoadSynchronous();
	UltimateEffectClass = Definition.EffectGameplayEffectClass;
	EnergyRefundEffectClass = Definition.EnergyRefundGameplayEffectClass;
	const TSubclassOf<UGameplayEffect> LoadedRefund = EnergyRefundEffectClass.LoadSynchronous();
	float CostMagnitude = 0.0f;
	const bool bValidCost = CostGameplayEffectClass
		&& HasNegativeAttributeModifier(CostGameplayEffectClass->GetDefaultObject<UGameplayEffect>(), UHSRCoreAttributeSet::GetEnergyAttribute(), CostMagnitude);
	float RefundMagnitude = 0.0f;
	const bool bValidRefund = LoadedRefund
		&& LoadedRefund->GetDefaultObject<UGameplayEffect>()->DurationPolicy == EGameplayEffectDurationType::Instant
		&& HasNegativeAttributeModifier(LoadedRefund->GetDefaultObject<UGameplayEffect>(), UHSRCoreAttributeSet::GetEnergyAttribute(), RefundMagnitude) == false;
	// 返还效果必须是“等于消耗量”的正能量 Additive 修饰符。
	bool bPositiveRefund = false;
	if (LoadedRefund)
	{
		for (const FGameplayModifierInfo& Modifier : LoadedRefund->GetDefaultObject<UGameplayEffect>()->Modifiers)
		{
			float Magnitude = 0.0f;
			if (Modifier.Attribute == UHSRCoreAttributeSet::GetEnergyAttribute()
				&& Modifier.ModifierOp == EGameplayModOp::Additive
				&& Modifier.ModifierMagnitude.GetStaticMagnitudeIfPossible(1.0f, Magnitude)
				&& FMath::IsNearlyEqual(Magnitude, -CostMagnitude))
			{
				bPositiveRefund = true;
				break;
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("UHSRUltimateAbility::ConfigureFromSkillDefinition - CostGE=%s CostEnergyMagnitude=%.2f ValidCost=%d RefundGE=%s ValidRefund=%d"),
		CostGameplayEffectClass ? *CostGameplayEffectClass->GetName() : TEXT("null"), CostMagnitude, bValidCost ? 1 : 0,
		LoadedRefund ? *LoadedRefund->GetName() : TEXT("null"), (bValidRefund && bPositiveRefund) ? 1 : 0);
	return bValidCost && bValidRefund && bPositiveRefund;
}

// 预激活检查：能量足够（CheckCost 通过）才允许激活。
EHSRAbilityFailureReason UHSRUltimateAbility::GetPreActivationFailureReason(const FGameplayAbilitySpecHandle& Handle, const FGameplayAbilityActorInfo* ActorInfo) const
{
	return CheckCost(Handle, ActorInfo, nullptr) ? EHSRAbilityFailureReason::None : EHSRAbilityFailureReason::InsufficientEnergy;
}

// 设置待处理目标：校验目标 ASC 非空后暂存。
bool UHSRUltimateAbility::SetPendingTarget(UAbilitySystemComponent* InTargetAbilitySystem)
{
	if (!InTargetAbilitySystem)
	{
		return false;
	}
	PendingTargetAbilitySystem = InTargetAbilitySystem;
	return true;
}

// 清除待处理目标。
void UHSRUltimateAbility::ClearPendingTarget()
{
	PendingTargetAbilitySystem.Reset();
}

// 激活能力：先做能量消耗的“提交成本”，再应用已准备的正式伤害；
// 若伤害应用失败则用返还 GE 退还能量，保证消耗与返还对称。
void UHSRUltimateAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bLastActivationSucceeded = false;
	LastFailureReason = EHSRAbilityFailureReason::EffectFailed;
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const TSubclassOf<UGameplayEffect> LoadedRefund = EnergyRefundEffectClass.LoadSynchronous();
	if (!SourceASC || !CostGameplayEffectClass || !LoadedRefund)
	{
		ClearPendingTarget();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const float EnergyBeforeCommit = SourceASC->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute());
	// 预先构造返还 GE spec（若伤害失败时退还能量用）。
	FGameplayEffectContextHandle RefundContext = SourceASC->MakeEffectContext();
	FGameplayEffectSpecHandle RefundSpec = SourceASC->MakeOutgoingSpec(LoadedRefund, 1.0f, RefundContext);
	if (!RefundSpec.IsValid())
	{
		ClearPendingTarget();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 能量不足：拒绝激活。
	if (!CheckCost(Handle, ActorInfo, nullptr))
	{
		LastFailureReason = EHSRAbilityFailureReason::InsufficientEnergy;
		ClearPendingTarget();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// 提交成本（扣能量）。
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		LastFailureReason = EHSRAbilityFailureReason::CommitFailed;
		ClearPendingTarget();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// 应用正式伤害。
	bLastActivationSucceeded = ApplyPreparedFormalDamage(SourceASC);
	FHSRFormalDamageExecutionResult& ExecutionResult = GetMutableFormalDamageExecutionResult();
	ExecutionResult.bCostCommitted = true;
	ExecutionResult.EnergyBefore = EnergyBeforeCommit;
	const float EnergyAfterCommit = SourceASC->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute());
	// 伤害失败：退还能量（返还 GE 必须是等额正能量），避免白白扣能量。
	if (!bLastActivationSucceeded)
	{
		const FActiveGameplayEffectHandle RefundApplied = SourceASC->ApplyGameplayEffectSpecToSelf(*RefundSpec.Data.Get());
		ExecutionResult.bRefundApplied = RefundApplied.WasSuccessfullyApplied();
		LastFailureReason = EHSRAbilityFailureReason::EffectFailed;
	}
	ExecutionResult.EnergyAfter = SourceASC->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute());
	UE_LOG(LogTemp, Log, TEXT("UHSRUltimateAbility::ActivateAbility - CostCommitted=1 DamageApplied=%d RefundApplied=%d EnergyBefore=%.2f EnergyAfterCost=%.2f EnergyFinal=%.2f"), bLastActivationSucceeded ? 1 : 0, ExecutionResult.bRefundApplied ? 1 : 0, EnergyBeforeCommit, EnergyAfterCommit, ExecutionResult.EnergyAfter);

	ClearPendingTarget();
	EndAbility(Handle, ActorInfo, ActivationInfo, true, !bLastActivationSucceeded);
}
