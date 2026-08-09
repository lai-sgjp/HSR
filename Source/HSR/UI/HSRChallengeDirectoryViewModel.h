#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HSRChallengeDirectoryTypes.h"
#include "HSRChallengeDirectoryViewModel.generated.h"

class UHSREncounterDefinition;
class UHSRChallengeProgressionSubsystem;

UCLASS()
class HSR_API UHSRChallengeDirectoryViewModel : public UObject
{
	GENERATED_BODY()

public:
	EHSRChallengeDirectoryResult Initialize(const TArray<FHSRChallengeDirectorySource>& Sources,
		UHSRChallengeProgressionSubsystem* InProgression = nullptr);
	EHSRChallengeDirectoryResult Refresh();
	const FHSRChallengeDirectorySnapshot& GetSnapshot() const { return Snapshot; }
	EHSRChallengeDirectoryResult ResolveSelection(FName EncounterId, UHSREncounterDefinition*& OutDefinition) const;

private:
	UPROPERTY(Transient)
	FHSRChallengeDirectorySnapshot Snapshot;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UHSREncounterDefinition>> DefinitionsById;

	UPROPERTY(Transient)
	TArray<FHSRChallengeDirectorySource> ConfiguredSources;

	TWeakObjectPtr<UHSRChallengeProgressionSubsystem> Progression;

	TSet<FName> AvailableIds;
};
