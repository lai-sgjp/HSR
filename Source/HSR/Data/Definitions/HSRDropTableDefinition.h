#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "../../Reward/HSRRewardTypes.h"
#include "HSRDropTableDefinition.generated.h"

UCLASS(BlueprintType)
class HSR_API UHSRDropTableDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop")
	FName DropTableId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop")
	TArray<FHSRDropTableEntry> Entries;
};
