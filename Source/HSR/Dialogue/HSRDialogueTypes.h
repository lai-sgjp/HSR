#pragma once

#include "CoreMinimal.h"
#include "HSRDialogueTypes.generated.h"

USTRUCT(BlueprintType)
struct FHSRDialogueChoiceDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName ChoiceId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName TargetNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Quest")
	FName QuestEventId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Quest", meta = (ClampMin = "1"))
	int32 EventCount = 1;
};

USTRUCT(BlueprintType)
struct FHSRDialogueNodeDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName NodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TArray<FHSRDialogueChoiceDefinition> Choices;
};

USTRUCT(BlueprintType)
struct FHSRDialogueChoiceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	bool bChoiceAccepted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FName DialogueId;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FName ChoiceId;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FName NextNodeId;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue|Quest")
	FName QuestEventId;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue|Quest")
	bool bQuestEventSubmitted = false;
};

