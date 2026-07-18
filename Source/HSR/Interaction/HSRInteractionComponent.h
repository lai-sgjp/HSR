#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HSRInteractionTypes.h"
#include "HSRInteractionComponent.generated.h"

class IHSRInteractableInterface;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSROnCandidateChanged, AActor*, NewCandidate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSROnInteractionCompleted, const FHSRInteractionResult&, Result);

UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class HSR_API UHSRInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSRInteractionComponent();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	FHSRInteractionResult TryInteract();

	void RegisterCandidate(AActor* Candidate);
	void UnregisterCandidate(AActor* Candidate);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	AActor* GetCurrentCandidate() const { return CurrentCandidate.Get(); }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool HasValidCandidate() const;

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FHSROnCandidateChanged OnCandidateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FHSROnInteractionCompleted OnInteractionCompleted;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TWeakObjectPtr<AActor> CurrentCandidate;

	bool IsCandidateValid() const;
};
