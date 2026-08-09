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

	// Piece count required to activate the set.  Authored per set; the resolver and the equipment
	// subsystem both read this field rather than assuming the 2-piece default.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic Set", meta = (ClampMin = "1", ClampMax = "6"))
	int32 Threshold = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic Set")
	TSubclassOf<UGameplayEffect> SetGameplayEffectClass;
};
