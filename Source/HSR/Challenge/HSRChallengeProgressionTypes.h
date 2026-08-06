#pragma once

#include "CoreMinimal.h"
#include "HSRChallengeProgressionTypes.generated.h"

UENUM(BlueprintType)
enum class EHSRChallengeProgressionResult : uint8
{
	Success,
	NoOp,
	InvalidEncounterId,
	InvalidRestoreData
};

USTRUCT(BlueprintType)
struct HSR_API FHSRChallengeProgressionSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "HSR|ChallengeProgression")
	TArray<FName> CompletedEncounterIds;

	UPROPERTY(BlueprintReadOnly, Category = "HSR|ChallengeProgression")
	int64 Revision = 0;
};

USTRUCT(BlueprintType)
struct HSR_API FHSRChallengeProgressionSaveData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "HSR|ChallengeProgression|Save")
	TArray<FName> CompletedEncounterIds;

	UPROPERTY(BlueprintReadWrite, Category = "HSR|ChallengeProgression|Save")
	int64 Revision = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FHSRChallengeProgressionChanged,
	const FHSRChallengeProgressionSnapshot&);
