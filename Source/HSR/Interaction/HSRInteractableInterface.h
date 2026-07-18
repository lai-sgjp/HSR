#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HSRInteractionTypes.h"
#include "HSRInteractableInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UHSRInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

class HSR_API IHSRInteractableInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool IsInteractionAvailable() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractionPrompt() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FHSRInteractionResult ExecuteInteraction(const FHSRInteractionContext& Context);
};
