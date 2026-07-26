#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "../../Reward/HSRRewardTypes.h"
#include "HSRRewardDefinition.generated.h"

UCLASS(BlueprintType)
class HSR_API UHSRRewardDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	FName RewardDefinitionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	TArray<FHSRRewardItemEntry> FixedItems;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	FName DropTableId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward", meta = (ClampMin = "0"))
	int32 DropRolls = 0;
};
