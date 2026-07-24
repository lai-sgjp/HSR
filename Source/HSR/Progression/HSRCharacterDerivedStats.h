#pragma once
#include "CoreMinimal.h"
#include "HSRCharacterDerivedStats.generated.h"
USTRUCT(BlueprintType)
struct HSR_API FHSRCharacterDerivedStats
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) float MaxHealth = 0.0f;
	UPROPERTY(BlueprintReadOnly) float Attack = 0.0f;
	UPROPERTY(BlueprintReadOnly) float Defense = 0.0f;
	UPROPERTY(BlueprintReadOnly) float Speed = 0.0f;
};
USTRUCT(BlueprintType)
struct HSR_API FHSRCharacterProgressionContext
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName CharacterId;
	UPROPERTY(BlueprintReadOnly) int64 RuntimeRevision = 0;
	UPROPERTY(BlueprintReadOnly) FHSRCharacterDerivedStats DerivedStats;
	/** Additive level-growth layer; base values remain owned by the existing Instant initialization GE. */
	UPROPERTY(BlueprintReadOnly) FHSRCharacterDerivedStats ProgressionBonuses;
};
