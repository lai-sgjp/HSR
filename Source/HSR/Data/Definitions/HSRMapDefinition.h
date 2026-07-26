#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HSRMapDefinition.generated.h"

class UWorld;

UCLASS(BlueprintType)
class HSR_API UHSRMapDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HSR|Map")
	FName MapId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HSR|Map")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HSR|Map")
	TSoftObjectPtr<UWorld> World;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HSR|Map")
	FName RegionId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HSR|Map")
	FName DefaultArrivalId = NAME_None;
};
