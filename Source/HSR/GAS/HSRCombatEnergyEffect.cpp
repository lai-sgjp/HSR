#include "HSRCombatEnergyEffect.h"
#include "Attribute/HSRCoreAttributeSet.h"

UHSRCombatEnergyEffect::UHSRCombatEnergyEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UHSRCoreAttributeSet::GetEnergyAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat Magnitude;
	Magnitude.DataTag = FGameplayTag::RequestGameplayTag(TEXT("Resource.EnergyDelta"));
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
}
