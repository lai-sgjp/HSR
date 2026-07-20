#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "HSRDamageTypes.h"
#include "HSRDamageEffectContext.generated.h"

USTRUCT()
struct HSR_API FHSRDamageEffectContext : public FGameplayEffectContext
{
	GENERATED_BODY()

	FHSRDamageContext DamageContext;
	float DefenseCoefficient = 0.5f;
	float MinDamage = 1.0f;
	FHSRDamageResult DamageResult;

	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FGameplayEffectContext* Duplicate() const override;
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
};

template<>
struct TStructOpsTypeTraits<FHSRDamageEffectContext> : public TStructOpsTypeTraitsBase2<FHSRDamageEffectContext>
{
	enum { WithNetSerializer = true, WithCopy = true };
};
