#include "HSRStatusDefinition.h"

EHSRStatusOperationResult UHSRStatusDefinition::Validate() const
{
	if (StatusId != FName(TEXT("Status.Buff.AttackUp")) && StatusId != FName(TEXT("Status.Debuff.DamageOverTime")) && StatusId != FName(TEXT("Status.Debuff.Break"))) return EHSRStatusOperationResult::InvalidStatusId;
	const bool bDamageOverTimeId = StatusId == FName(TEXT("Status.Debuff.DamageOverTime"));
	const bool bBreakId = StatusId == FName(TEXT("Status.Debuff.Break"));
	const bool bAttackUpId = StatusId == FName(TEXT("Status.Buff.AttackUp"));
	if ((bDamageOverTimeId && EffectKind != EHSRStatusEffectKind::DamageOverTime)
		|| (!bDamageOverTimeId && EffectKind != EHSRStatusEffectKind::TagOnly)) return EHSRStatusOperationResult::InvalidDefinition;
	if ((bAttackUpId && Classification != EHSRStatusClassification::Buff)
		|| ((bDamageOverTimeId || bBreakId) && Classification != EHSRStatusClassification::Debuff)) return EHSRStatusOperationResult::InvalidDefinition;
	if (Classification == EHSRStatusClassification::Buff && (bDispellable || ImmunityTag.IsValid())) return EHSRStatusOperationResult::InvalidDefinition;
	if (Classification == EHSRStatusClassification::Debuff && (!ImmunityTag.IsValid() || ImmunityTag.GetTagName() != FName(TEXT("Status.Immunity.Debuff")))) return EHSRStatusOperationResult::InvalidDefinition;
	if ((bDamageOverTimeId && (!bDispellable || SourceInvalidPolicy != EHSRSourceInvalidPolicy::Remove))
		|| (bBreakId && (bDispellable || SourceInvalidPolicy != EHSRSourceInvalidPolicy::Keep))) return EHSRStatusOperationResult::InvalidDefinition;
	if (DurationTurns <= 0) return EHSRStatusOperationResult::InvalidDuration;
	const bool bValidStackContract = RefreshPolicy == EHSRStatusRefreshPolicy::RefreshDuration ? MaxStacks == 1
		: RefreshPolicy == EHSRStatusRefreshPolicy::AddStack && MaxStacks == 2;
	if (TriggerTiming != EHSRStatusTriggerTiming::TargetTurnEnded || !bValidStackContract)
	{
		return EHSRStatusOperationResult::InvalidPolicy;
	}
	if (!GrantedStatusTag.IsValid() || GrantedStatusTag.GetTagName() != StatusId) return EHSRStatusOperationResult::InvalidDefinition;
	if (InfiniteGameplayEffectClass.IsNull()) return EHSRStatusOperationResult::MissingGameplayEffect;
	const bool bHasDamageConfiguration = DamageType.IsValid() || !DamageRule.IsNull() || !DamageGameplayEffectClass.IsNull();
	if (EffectKind == EHSRStatusEffectKind::DamageOverTime && (!DamageType.IsValid() || DamageRule.IsNull() || DamageGameplayEffectClass.IsNull()
		|| !FMath::IsFinite(DamageAbilityMultiplier) || DamageAbilityMultiplier <= 0.0f)) return EHSRStatusOperationResult::InvalidDefinition;
	if (EffectKind == EHSRStatusEffectKind::TagOnly && bHasDamageConfiguration) return EHSRStatusOperationResult::InvalidDefinition;
	return EHSRStatusOperationResult::Success;
}
