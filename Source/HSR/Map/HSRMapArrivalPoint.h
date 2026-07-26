#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HSRMapArrivalPoint.generated.h"

UCLASS(BlueprintType)
class HSR_API AHSRMapArrivalPoint : public AActor
{
	GENERATED_BODY()

public:
	AHSRMapArrivalPoint();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="HSR|Map")
	FName ArrivalId = NAME_None;
};
