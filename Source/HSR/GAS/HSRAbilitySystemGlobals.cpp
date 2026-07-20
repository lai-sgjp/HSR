#include "HSRAbilitySystemGlobals.h"
#include "Damage/HSRDamageEffectContext.h"

FGameplayEffectContext* UHSRAbilitySystemGlobals::AllocGameplayEffectContext() const
{
	return new FHSRDamageEffectContext();
}
