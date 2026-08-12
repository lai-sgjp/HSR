#include "HSRAbilitySystemGlobals.h"
#include "Damage/HSRDamageEffectContext.h"

// GAS 全局对象的上下文分配钩子：所有 GameplayEffect 的上下文
// 一律用我们自定义的 FHSRDamageEffectContext，以携带伤害公式所需的数据
//（倍率、防御系数、最小伤害、暴击骰、伤害结果等）。
FGameplayEffectContext* UHSRAbilitySystemGlobals::AllocGameplayEffectContext() const
{
	return new FHSRDamageEffectContext();
}
