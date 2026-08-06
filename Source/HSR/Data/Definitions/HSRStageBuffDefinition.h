#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HSRStageBuffDefinition.generated.h"

class UGameplayEffect;

UCLASS(BlueprintType)
class HSR_API UHSRStageBuffDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage Buff")
	FName BuffId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage Buff")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage Buff|Effect")
	TSubclassOf<UGameplayEffect> GameplayEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage Buff|Resource")
	FName ResourceItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage Buff|Resource", meta = (ClampMin = "0"))
	int32 ResourceCost = 0;
};
