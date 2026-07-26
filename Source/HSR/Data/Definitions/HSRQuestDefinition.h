#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "../../Quest/HSRQuestTypes.h"
#include "HSRQuestDefinition.generated.h"

UCLASS(BlueprintType)
class HSR_API UHSRQuestDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FName QuestId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TArray<FHSRQuestObjectiveDefinition> Objectives;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|Reward")
	FName RewardDefinitionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|Reward")
	int32 RewardSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|Reward")
	bool bAutoClaimReward = true;
};

