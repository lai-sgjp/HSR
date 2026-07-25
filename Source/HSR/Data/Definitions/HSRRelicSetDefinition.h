#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayEffect.h"
#include "HSRRelicSetDefinition.generated.h"

UCLASS(BlueprintType)
class HSR_API UHSRRelicSetDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic Set")
	FName SetId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic Set", meta = (ClampMin = "2", ClampMax = "2"))
	int32 Threshold = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic Set")
	TSubclassOf<UGameplayEffect> SetGameplayEffectClass;
};
