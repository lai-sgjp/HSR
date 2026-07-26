#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "../../Dialogue/HSRDialogueTypes.h"
#include "HSRDialogueDefinition.generated.h"

UCLASS(BlueprintType)
class HSR_API UHSRDialogueDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName DialogueId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Quest")
	FName QuestId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName StartNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TArray<FHSRDialogueNodeDefinition> Nodes;
};

