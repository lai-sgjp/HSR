#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HSRBattleTestConsumer.generated.h"

UCLASS()
class HSR_API AHSRBattleTestConsumer : public AActor
{
	GENERATED_BODY()

public:
	AHSRBattleTestConsumer();

	virtual void BeginPlay() override;
};
