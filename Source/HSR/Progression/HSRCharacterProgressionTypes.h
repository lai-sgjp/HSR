#pragma once

#include "CoreMinimal.h"
#include "HSRCharacterProgressionTypes.generated.h"

UENUM(BlueprintType)
enum class EHSRCharacterProgressionResult : uint8
{
	Success,
	MissingDefinition,
	InvalidCharacterId,
	InvalidRuntimeState,
	MissingExperienceCurve,
	InvalidExperienceCurve,
	NegativeExperience,
	ExperienceOverflow,
	InvalidSkillId,
	InvalidSkillLevel
};

/** Persistable, pure-value progression state. It deliberately contains neither derived combat values nor UObject references. */
USTRUCT(BlueprintType)
struct HSR_API FHSRCharacterRuntimeState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	FName CharacterId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression", meta = (ClampMin = "1"))
	int32 Level = 1;

	/** Total cumulative experience, not experience remaining until a level-up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression", meta = (ClampMin = "0"))
	int32 Experience = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression", meta = (ClampMin = "0"))
	int32 Ascension = 0;

	/** Stable SkillId -> authored/runtime level mapping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skills")
	TMap<FName, int32> SkillLevels;
};
