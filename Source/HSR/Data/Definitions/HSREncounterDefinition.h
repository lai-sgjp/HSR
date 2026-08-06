#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/World.h"
#include "HSREncounterDefinition.generated.h"

class UHSRDropTableDefinition;
class UHSRItemDefinition;
class UHSRRewardDefinition;
class UHSRStageBuffDefinition;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter|Preparation")
	TArray<TObjectPtr<UHSRStageBuffDefinition>> StageBuffDefinitions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter|Reward")
	TArray<TObjectPtr<UHSRItemDefinition>> RewardItemDefinitions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter|Reward")
	TObjectPtr<UHSRDropTableDefinition> RewardDropTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter|Reward")
	TObjectPtr<UHSRRewardDefinition> VictoryRewardDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter|Reward")
	int32 RewardSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter|Reward", meta = (ClampMin = "0"))
	int32 VictoryExperience = 0;
};
