#include "HSRDamageEffectContext.h"
#include "GameFramework/Actor.h"

// 返回本上下文的确切结构类型（供外部验证是否是 FHSRDamageEffectContext）。
UScriptStruct* FHSRDamageEffectContext::GetScriptStruct() const
{
	return StaticStruct();
}

// 深拷贝本上下文（复制数据，并单独复制命中结果以避免共享）。
FGameplayEffectContext* FHSRDamageEffectContext::Duplicate() const
{
	FHSRDamageEffectContext* NewContext = new FHSRDamageEffectContext();
	*NewContext = *this;
	if (GetHitResult())
	{
		NewContext->AddHitResult(*GetHitResult(), true);
	}
	return NewContext;
}

// 网络序列化（单机场景几乎不用，但保持契约完整）。
bool FHSRDamageEffectContext::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bOutSuccess = true;
	bool bBaseSuccess = true;
	FGameplayEffectContext::NetSerialize(Ar, Map, bBaseSuccess);
	bOutSuccess &= bBaseSuccess;
	Ar << DamageContext.ActionId;
	DamageContext.DamageType.NetSerialize(Ar, Map, bOutSuccess);
	Ar << DamageContext.AbilityMultiplier << DamageContext.CritRoll;
	Ar << DefenseCoefficient << MinDamage;
	uint8 ResultValue = static_cast<uint8>(DamageResult.Result);
	Ar << ResultValue;
	if (Ar.IsLoading())
	{
		DamageResult.Result = static_cast<EHSRDamageResultType>(ResultValue);
	}
	Ar << DamageResult.ActionId;
	DamageResult.DamageType.NetSerialize(Ar, Map, bOutSuccess);
	Ar << DamageResult.Breakdown.NormalizedAttack << DamageResult.Breakdown.NormalizedDefense;
	Ar << DamageResult.Breakdown.RawDamage << DamageResult.Breakdown.CritMultiplier;
	Ar << DamageResult.Breakdown.bCritical << DamageResult.Breakdown.FinalDamage << DamageResult.Breakdown.AppliedDamage;
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	uint8 InjectionValue = static_cast<uint8>(TestInjection);
	Ar << InjectionValue;
	if (Ar.IsLoading())
	{
		TestInjection = static_cast<EHSRDamageTestInjection>(InjectionValue);
	}
#endif
	return true;
}
