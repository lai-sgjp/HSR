#pragma once

#include "CoreMinimal.h"
#include "HSRPreBattleCandidateTypes.generated.h"

UENUM(BlueprintType)
enum class EHSRPreBattleCandidateResult : uint8
{
	Success,
	InvalidSlot,
	ProfileNotFound,
	DuplicateCharacter,
	EmptyLeader,
	InvalidEncounter,
	InvalidCandidate
};

USTRUCT(BlueprintType)
struct HSR_API FHSRPreBattleCandidateSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "HSR|PreBattle")
	TArray<FName> CandidateCharacterIds;

	UPROPERTY(BlueprintReadOnly, Category = "HSR|PreBattle")
	TArray<FName> BuffIds;

	UPROPERTY(BlueprintReadOnly, Category = "HSR|PreBattle")
	FName EncounterId;

	UPROPERTY(BlueprintReadOnly, Category = "HSR|PreBattle")
	int64 PartyRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "HSR|PreBattle")
	bool bHasPendingChanges = false;
};
