#include "HSRStatusDefinition.h"

EHSRStatusOperationResult UHSRStatusDefinition::Validate() const
{
	const FString StatusIdString = StatusId.ToString();
	if (StatusId.IsNone() || !StatusIdString.StartsWith(TEXT("Status.")) || StatusIdString.Len() <= 7)
	{
		return EHSRStatusOperationResult::InvalidStatusId;
	}
	if (!GrantedStatusTag.IsValid() || GrantedStatusTag.GetTagName() != StatusId)
	{
		return EHSRStatusOperationResult::InvalidDefinition;
	}
	if (Classification != EHSRStatusClassification::Buff && Classification != EHSRStatusClassification::Debuff)
	{
		return EHSRStatusOperationResult::InvalidDefinition;
	}
	if (EffectKind != EHSRStatusEffectKind::TagOnly && EffectKind != EHSRStatusEffectKind::DamageOverTime)
	{
		return EHSRStatusOperationResult::InvalidDefinition;
	}
	if (SourceInvalidPolicy != EHSRSourceInvalidPolicy::Keep && SourceInvalidPolicy != EHSRSourceInvalidPolicy::Remove)
	{
		return EHSRStatusOperationResult::InvalidDefinition;
	}
	if (Classification == EHSRStatusClassification::Buff && (bDispellable || ImmunityTag.IsValid())) return EHSRStatusOperationResult::InvalidDefinition;
	if (Classification == EHSRStatusClassification::Debuff && !ImmunityTag.IsValid()) return EHSRStatusOperationResult::InvalidDefinition;
	if (DurationTurns <= 0) return EHSRStatusOperationResult::InvalidDuration;
	const bool bValidStackContract = RefreshPolicy == EHSRStatusRefreshPolicy::RefreshDuration ? MaxStacks == 1
		: RefreshPolicy == EHSRStatusRefreshPolicy::AddStack && MaxStacks >= 2;
	if (TriggerTiming != EHSRStatusTriggerTiming::TargetTurnEnded || !bValidStackContract)
	{
		return EHSRStatusOperationResult::InvalidPolicy;
	}
	if (InfiniteGameplayEffectClass.IsNull()) return EHSRStatusOperationResult::MissingGameplayEffect;
	const bool bHasDamageConfiguration = DamageType.IsValid() || !DamageRule.IsNull() || !DamageGameplayEffectClass.IsNull();
	if (EffectKind == EHSRStatusEffectKind::DamageOverTime && (!DamageType.IsValid() || DamageRule.IsNull() || DamageGameplayEffectClass.IsNull()
		|| !FMath::IsFinite(DamageAbilityMultiplier) || DamageAbilityMultiplier <= 0.0f
		|| Classification != EHSRStatusClassification::Debuff)) return EHSRStatusOperationResult::InvalidDefinition;
	if (EffectKind == EHSRStatusEffectKind::TagOnly && bHasDamageConfiguration) return EHSRStatusOperationResult::InvalidDefinition;
	return EHSRStatusOperationResult::Success;
}
