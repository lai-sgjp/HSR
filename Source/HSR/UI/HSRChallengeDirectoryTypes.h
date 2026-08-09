#pragma once

#include "CoreMinimal.h"
#include "HSRChallengeDirectoryTypes.generated.h"

class UHSREncounterDefinition;

UENUM(BlueprintType)
enum class EHSRChallengeDirectoryResult : uint8
{
	Success,
	EmptyDirectory,
	UnknownChallenge,
	Locked,
	Completed,
	InvalidDefinition
};

UENUM(BlueprintType)
enum class EHSRChallengeDirectoryStatus : uint8
{
	Available,
	Locked,
	Completed,
	Unavailable
};

USTRUCT(BlueprintType)
struct HSR_API FHSRChallengeDirectorySource
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HSR|Challenge")
	TObjectPtr<UHSREncounterDefinition> Definition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HSR|Challenge")
	bool bUnlocked = true;
};

USTRUCT(BlueprintType)
struct HSR_API FHSRChallengeDirectoryEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "HSR|Challenge")
	FName EncounterId;

	UPROPERTY(BlueprintReadOnly, Category = "HSR|Challenge")
	FName EnemyDefinitionId;

	UPROPERTY(BlueprintReadOnly, Category = "HSR|Challenge")
	FName BattleMapPath;

	UPROPERTY(BlueprintReadOnly, Category = "HSR|Challenge")
	bool bUnlocked = false;

	UPROPERTY(BlueprintReadOnly, Category = "HSR|Challenge")
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "HSR|Challenge")
	bool bCompleted = false;

	UPROPERTY(BlueprintReadOnly, Category = "HSR|Challenge")
	EHSRChallengeDirectoryStatus Status = EHSRChallengeDirectoryStatus::Unavailable;

	UPROPERTY(BlueprintReadOnly, Category = "HSR|Challenge")
	FText Diagnostic;
};

USTRUCT(BlueprintType)
struct HSR_API FHSRChallengeDirectorySnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "HSR|Challenge")
	TArray<FHSRChallengeDirectoryEntry> Entries;
};
