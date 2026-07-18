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
	UHSRInteractionViewModel();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void Observe(UHSRInteractionComponent* InComponent);

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void Teardown();

	UFUNCTION(BlueprintPure, Category = "Interaction")
	UHSRInteractionComponent* GetObservedComponent() const { return ObservedComponent.Get(); }

	/** Force broadcast current snapshot to all connected Widgets, bypassing dedup. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void ForceCurrentSnapshot();

	int32 GetInstanceId() const { return InstanceId; }

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FHSROnPromptChanged OnPromptChanged;

private:
	TWeakObjectPtr<UHSRInteractionComponent> ObservedComponent;

	UFUNCTION()
	void OnComponentCandidateChanged(AActor* NewCandidate);

	void BroadcastCurrentState(AActor* Candidate);

	int32 InstanceId;
	bool bLastVisible;
	FText LastPrompt;
	int32 BroadcastCount;
	int32 SkippedDedupCount;
	int32 TeardownCount;
};
