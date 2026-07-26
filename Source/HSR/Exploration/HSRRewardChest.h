#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Interaction/HSRInteractableInterface.h"
#include "Components/SphereComponent.h"
#include "HSRRewardChest.generated.h"

class UHSRDropTableDefinition;
class UHSRItemDefinition;
class UHSRRewardDefinition;

UCLASS()
class HSR_API AHSRRewardChest : public AActor, public IHSRInteractableInterface
{
	GENERATED_BODY()

public:
	AHSRRewardChest();
	virtual bool IsInteractionAvailable_Implementation() const override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual FHSRInteractionResult ExecuteInteraction_Implementation(const FHSRInteractionContext& Context) override;

protected:
	virtual void BeginPlay() override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collision")
	TObjectPtr<USphereComponent> CollisionComponent;
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Reward")
	FGuid StableClaimId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	TArray<TObjectPtr<UHSRItemDefinition>> ItemDefinitions;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	TObjectPtr<UHSRDropTableDefinition> DropTableDefinition;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	TObjectPtr<UHSRRewardDefinition> RewardDefinition;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	int32 RewardSeed = 0;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Reward")
	bool bClaimed = false;
	bool bClaimConfigurationValid = true;
	bool bRewardBundleRegistered = false;
};
