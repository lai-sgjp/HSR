#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HSRDamageTypes.generated.h"

UENUM(BlueprintType)
enum class EHSRDamageResultType : uint8
{
	InvalidSource,
	InvalidTarget,
	MissingDamageRule,
	InvalidDamageType,
	CaptureFailed,
	InvalidCapturedValue,
	SpecCreationFailed,
	EffectApplicationFailed,
	DuplicateAction,
	BattleTerminal,
	DamageResolved
};

/** Pure input data passed to a future damage calculation; it owns no runtime objects. */
USTRUCT(BlueprintType)
struct HSR_API FHSRDamageContext
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	FGuid ActionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	FGameplayTag DamageType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float AbilityMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float CritRoll = 0.0f;
};

/** Pure formula trace. FinalDamage and AppliedDamage remain intentionally distinct. */
USTRUCT(BlueprintType)
struct HSR_API FHSRDamageBreakdown
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
	float NormalizedAttack = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
	float NormalizedDefense = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
	float RawDamage = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
	float CritMultiplier = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
	bool bCritical = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
	float FinalDamage = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
	float AppliedDamage = 0.0f;
};

/** Pure resolution result for future ability-resolution mapping. */
USTRUCT(BlueprintType)
struct HSR_API FHSRDamageResult
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
	EHSRDamageResultType Result = EHSRDamageResultType::InvalidSource;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
	FGuid ActionId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
	FGameplayTag DamageType;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
	FHSRDamageBreakdown Breakdown;
};
