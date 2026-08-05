#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HSRChallengeDirectoryTypes.h"
#include "HSRChallengeDirectoryViewModel.generated.h"

class UHSREncounterDefinition;

UCLASS()
class HSR_API UHSRChallengeDirectoryViewModel : public UObject
{
	GENERATED_BODY()

public:
	EHSRChallengeDirectoryResult Initialize(const TArray<FHSRChallengeDirectorySource>& Sources);
	const FHSRChallengeDirectorySnapshot& GetSnapshot() const { return Snapshot; }
	EHSRChallengeDirectoryResult ResolveSelection(FName EncounterId, UHSREncounterDefinition*& OutDefinition) const;

private:
	UPROPERTY(Transient)
	FHSRChallengeDirectorySnapshot Snapshot;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UHSREncounterDefinition>> DefinitionsById;

	TSet<FName> AvailableIds;
};
