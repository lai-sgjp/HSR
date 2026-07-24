#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HSRCharacterDefinition.generated.h"

class UHSRSkillDefinition;
class USkeletalMesh;
class UTexture2D;
class AHSRCharacterBase;

/** Immutable authored input for a playable character. Runtime progression never writes this asset. */
UCLASS(BlueprintType)
class HSR_API UHSRCharacterDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Stable project identifier. It is the only character identity persisted by progression. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	FName CharacterId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TSoftObjectPtr<USkeletalMesh> CharacterMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TSoftObjectPtr<UTexture2D> Portrait;

	/** Soft references preserve the Definition/Runtime boundary and avoid loading abilities just to inspect progression. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skills")
	TArray<TSoftObjectPtr<UHSRSkillDefinition>> SkillDefinitions;

	/** Stable SkillId -> maximum level authority. Runtime validation never loads SkillDefinitions. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skills")
	TMap<FName, int32> SkillMaxLevels;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats", meta=(ClampMin="0.0")) float BaseMaxHealth = 100.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats", meta=(ClampMin="0.0")) float BaseAttack = 10.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats", meta=(ClampMin="0.0")) float BaseDefense = 10.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats", meta=(ClampMin="0.000001")) float BaseSpeed = 100.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats", meta=(ClampMin="0.0")) float MaxHealthPerLevel = 10.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats", meta=(ClampMin="0.0")) float AttackPerLevel = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats", meta=(ClampMin="0.0")) float DefensePerLevel = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats", meta=(ClampMin="0.0")) float SpeedPerLevel = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character") TSoftClassPtr<AHSRCharacterBase> CharacterClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progression", meta = (ClampMin = "1"))
	int32 MaxLevel = 80;

	/** Float curve where X is the destination level and Y is total cumulative experience required for that level. Level 1 is always zero. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progression")
	TSoftObjectPtr<UCurveFloat> CumulativeExperienceCurve;
};
