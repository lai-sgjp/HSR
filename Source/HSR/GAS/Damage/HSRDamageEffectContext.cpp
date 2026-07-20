#include "HSRDamageEffectContext.h"
#include "GameFramework/Actor.h"

UScriptStruct* FHSRDamageEffectContext::GetScriptStruct() const { return StaticStruct(); }

FGameplayEffectContext* FHSRDamageEffectContext::Duplicate() const
{
	FHSRDamageEffectContext* NewContext = new FHSRDamageEffectContext();
	*NewContext = *this;
	if (GetHitResult()) { NewContext->AddHitResult(*GetHitResult(), true); }
	return NewContext;
}

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
	if (Ar.IsLoading()) { DamageResult.Result = static_cast<EHSRDamageResultType>(ResultValue); }
	Ar << DamageResult.ActionId;
	DamageResult.DamageType.NetSerialize(Ar, Map, bOutSuccess);
	Ar << DamageResult.Breakdown.NormalizedAttack << DamageResult.Breakdown.NormalizedDefense;
	Ar << DamageResult.Breakdown.RawDamage << DamageResult.Breakdown.CritMultiplier;
	Ar << DamageResult.Breakdown.bCritical << DamageResult.Breakdown.FinalDamage << DamageResult.Breakdown.AppliedDamage;
	return true;
}
