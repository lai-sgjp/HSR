#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HSRTeleportDefinition.generated.h"

UCLASS(BlueprintType)
class HSR_API UHSRTeleportDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HSR|Map")
	FName TeleportId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HSR|Map")
	FName SourceMapId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HSR|Map")
	FName DestinationMapId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HSR|Map")
	FName DestinationArrivalId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HSR|Map")
	bool bInitiallyUnlocked = false;
};
