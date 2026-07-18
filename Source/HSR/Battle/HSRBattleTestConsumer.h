#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HSREncounterTypes.h"
#include "HSRBattleTestConsumer.generated.h"

UCLASS()
class HSR_API AHSRBattleTestConsumer : public AActor
{
	GENERATED_BODY()

public:
	AHSRBattleTestConsumer();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test|Return")
	bool bEnableTestReturn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test|Return")
	float ReturnDelay;

protected:
	void RequestTestReturn();

	FHSREncounterRequest StoredConsumedRequest;

	FTimerHandle ReturnTimerHandle;
};
