#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "HSRInteractionViewModel.generated.h"

class UHSRInteractionComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHSROnPromptChanged, bool, bVisible, const FText&, PromptText);

UCLASS(BlueprintType)
class HSR_API UHSRInteractionViewModel : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void Observe(UHSRInteractionComponent* InComponent);

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void Teardown();

	UFUNCTION(BlueprintPure, Category = "Interaction")
	UHSRInteractionComponent* GetObservedComponent() const { return ObservedComponent.Get(); }

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FHSROnPromptChanged OnPromptChanged;

private:
	TWeakObjectPtr<UHSRInteractionComponent> ObservedComponent;

	UFUNCTION()
	void OnComponentCandidateChanged(AActor* NewCandidate);

	void BroadcastCurrentState(AActor* Candidate);
};
