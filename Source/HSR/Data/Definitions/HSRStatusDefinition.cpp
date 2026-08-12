#include "HSRStatusDefinition.h"

// 校验状态定义的所有字段约束，返回第一个不通过项。
// 检查点：StatusId 命名、授予标签、分类（Buff/Debuff）、效果种类、源失效策略、
// 免疫/可净化规则、持续时间、堆叠契约、触发时机、效果 GE、DoT 的伤害配置完整性。
EHSRStatusOperationResult UHSRStatusDefinition::Validate() const
{
	// StatusId 必须形如 "Status.Xxx"（前缀 + 至少一个字符）。
	const FString StatusIdString = StatusId.ToString();
	if (StatusId.IsNone() || !StatusIdString.StartsWith(TEXT("Status.")) || StatusIdString.Len() <= 7)
	{
		return EHSRStatusOperationResult::InvalidStatusId;
	}
	// 授予标签必须存在，且其名称必须与 StatusId 一致（标签即状态身份）。
	if (!GrantedStatusTag.IsValid() || GrantedStatusTag.GetTagName() != StatusId)
	{
		return EHSRStatusOperationResult::InvalidDefinition;
	}
	// 分类必须是 Buff 或 Debuff。
	if (Classification != EHSRStatusClassification::Buff && Classification != EHSRStatusClassification::Debuff)
	{
		return EHSRStatusOperationResult::InvalidDefinition;
	}
	// 效果种类必须是 TagOnly（纯标签）或 DamageOverTime。
	if (EffectKind != EHSRStatusEffectKind::TagOnly && EffectKind != EHSRStatusEffectKind::DamageOverTime)
	{
		return EHSRStatusOperationResult::InvalidDefinition;
	}
	// 源失效策略必须是 Keep 或 Remove。
	if (SourceInvalidPolicy != EHSRSourceInvalidPolicy::Keep && SourceInvalidPolicy != EHSRSourceInvalidPolicy::Remove)
	{
		return EHSRStatusOperationResult::InvalidDefinition;
	}
	// Buff 不允许可净化或带免疫标签（免疫是 Debuff 专属）。
	if (Classification == EHSRStatusClassification::Buff && (bDispellable || ImmunityTag.IsValid()))
	{
		return EHSRStatusOperationResult::InvalidDefinition;
	}
	// Debuff 必须有免疫标签（否则无法被免疫机制拦截）。
	if (Classification == EHSRStatusClassification::Debuff && !ImmunityTag.IsValid())
	{
		return EHSRStatusOperationResult::InvalidDefinition;
	}
	if (DurationTurns <= 0)
	{
		return EHSRStatusOperationResult::InvalidDuration;
	}
	// 堆叠契约：续期策略要求 MaxStacks==1；叠层策略要求 MaxStacks>=2。
	const bool bValidStackContract = RefreshPolicy == EHSRStatusRefreshPolicy::RefreshDuration ? MaxStacks == 1
		: RefreshPolicy == EHSRStatusRefreshPolicy::AddStack && MaxStacks >= 2;
	if (TriggerTiming != EHSRStatusTriggerTiming::TargetTurnEnded || !bValidStackContract)
	{
		return EHSRStatusOperationResult::InvalidPolicy;
	}
	if (InfiniteGameplayEffectClass.IsNull())
	{
		return EHSRStatusOperationResult::MissingGameplayEffect;
	}
	// DoT 必须配齐伤害配置：伤害类型、伤害规则、伤害 GE，且倍率有限为正、必须是 Debuff。
	const bool bHasDamageConfiguration = DamageType.IsValid() || !DamageRule.IsNull() || !DamageGameplayEffectClass.IsNull();
	if (EffectKind == EHSRStatusEffectKind::DamageOverTime
		&& (!DamageType.IsValid() || DamageRule.IsNull() || DamageGameplayEffectClass.IsNull()
			|| !FMath::IsFinite(DamageAbilityMultiplier) || DamageAbilityMultiplier <= 0.0f
			|| Classification != EHSRStatusClassification::Debuff))
	{
		return EHSRStatusOperationResult::InvalidDefinition;
	}
	// 纯标签状态不允许携带任何伤害配置。
	if (EffectKind == EHSRStatusEffectKind::TagOnly && bHasDamageConfiguration)
	{
		return EHSRStatusOperationResult::InvalidDefinition;
	}
	return EHSRStatusOperationResult::Success;
}
