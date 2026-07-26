#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HSRMapArrivalConsumer.generated.h"

UCLASS()
class HSR_API AHSRMapArrivalConsumer : public AActor
{
	GENERATED_BODY()

public:
	AHSRMapArrivalConsumer();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="HSR|Map")
	FName MapId = NAME_None;

private:
	void TryCommitArrival();
	void RetryOrCancel(const TCHAR* Reason);

	FTimerHandle RetryTimer;
	int32 RetryCount = 0;
	static constexpr int32 MaxRetries = 10;
	static constexpr float RetryInterval = 0.2f;
};
