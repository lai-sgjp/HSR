#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HSRExplorationReturnConsumer.generated.h"

UCLASS()
class HSR_API AHSRExplorationReturnConsumer : public AActor
{
	GENERATED_BODY()

public:
	AHSRExplorationReturnConsumer();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	void TryConsumeAndReturn();

	FTimerHandle ReturnTimerHandle;
	int32 RetryCount;

	static constexpr int32 MaxRetries = 10;
	static constexpr float RetryInterval = 0.2f;
};
