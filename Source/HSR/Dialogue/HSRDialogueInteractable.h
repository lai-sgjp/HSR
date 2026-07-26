#pragma once

#include "CoreMinimal.h"
#include "../Exploration/HSRGrayboxInteractable.h"
#include "HSRDialogueTypes.h"
#include "../Quest/HSRQuestTypes.h"
#include "HSRDialogueInteractable.generated.h"

class UHSRQuestDefinition;
class UHSRDialogueDefinition;

UCLASS()
class HSR_API AHSRDialogueInteractable : public AHSRGrayboxInteractable
{
	GENERATED_BODY()

public:
	AHSRDialogueInteractable();
	virtual void BeginPlay() override;

	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual FHSRInteractionResult ExecuteInteraction_Implementation(const FHSRInteractionContext& Context) override;

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	bool GetStartDialogueNode(FHSRDialogueNodeDefinition& OutNode) const;

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	EHSRQuestOperationResult SelectDialogueChoice(FName NodeId, FName ChoiceId, FHSRDialogueChoiceResult& OutResult);

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Dialogue")
	FName DialogueId;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Dialogue")
	TObjectPtr<UHSRQuestDefinition> QuestDefinition;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Dialogue")
	TObjectPtr<UHSRDialogueDefinition> DialogueDefinition;
};
