#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/World.h"
#include "HSREncounterDefinition.generated.h"

UCLASS(BlueprintType)
class HSR_API UHSREncounterDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	FName EncounterId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	FName EnemyDefinitionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	TSoftObjectPtr<UWorld> BattleMap;
};
