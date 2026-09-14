#pragma once
#include "CoreMinimal.h"
#include "HSRGrayboxInteractable.h"
#include "HSRSceneInteraction.generated.h"
class UStaticMeshComponent;

/** Authored world interaction: existing travel/quest authorities accept the intent. */
UCLASS()
class HSR_API AHSRSceneInteraction : public AHSRGrayboxInteractable
{
    GENERATED_BODY()
public:
    AHSRSceneInteraction();
    UPROPERTY(EditAnywhere,Category="Scene") FText Prompt;
    UPROPERTY(EditAnywhere,Category="Scene") FName TeleportId;
    UPROPERTY(EditAnywhere,Category="Scene") FName QuestEventId;
    UPROPERTY(EditAnywhere,Category="Scene") FName DiscoveryId;
    UPROPERTY(VisibleAnywhere,Category="Scene") TObjectPtr<UStaticMeshComponent> DisplayMesh;
    virtual FText GetInteractionPrompt_Implementation() const override;
    virtual FHSRInteractionResult ExecuteInteraction_Implementation(const FHSRInteractionContext& Context) override;
};
