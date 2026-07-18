#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Interaction/HSRInteractionTypes.h"
#include "../Interaction/HSRInteractableInterface.h"
#include "Components/SphereComponent.h"
#include "HSRGrayboxInteractable.generated.h"

UCLASS()
class HSR_API AHSRGrayboxInteractable : public AActor, public IHSRInteractableInterface
{
	GENERATED_BODY()

public:
	AHSRGrayboxInteractable();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetAvailable(bool bInAvailable);

	virtual bool IsInteractionAvailable_Implementation() const override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual FHSRInteractionResult ExecuteInteraction_Implementation(const FHSRInteractionContext& Context) override;

protected:
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collision", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> CollisionComponent;

public:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Interaction|Debug")
	bool bAvailable;
};


