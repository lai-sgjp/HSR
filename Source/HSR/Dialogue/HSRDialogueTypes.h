#pragma once

#include "CoreMinimal.h"
#include "../Battle/HSREncounterTypes.h"
#include "../Quest/HSRQuestTypes.h"
#include "HSRDialogueTypes.generated.h"

UENUM(BlueprintType)
enum class EHSRDialogueChoiceBranch : uint8
{
	None UMETA(DisplayName = "No Authority Branch"),
	Quest UMETA(DisplayName = "Quest"),
	Encounter UMETA(DisplayName = "Encounter"),
	Reward UMETA(DisplayName = "Reward")
};

UENUM(BlueprintType)
enum class EHSRDialogueChoiceOperationResult : uint8
{
	Success,
	NoOp,
	InvalidRequest,
	UnknownDialogueDefinition,
	InvalidChoice,
	InvalidDefinition,
	AuthorityUnavailable,
	AuthorityRejected,
	OperationIdConflict
};

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FText DisplayText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Branch")
	EHSRDialogueChoiceBranch Branch = EHSRDialogueChoiceBranch::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Branch")
	FGuid BranchOperationId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Encounter")
	FHSREncounterRequest EncounterRequest;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Reward")
	FName RewardDefinitionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue|Reward", meta = (ClampMin = "0"))
	int32 RewardSeed = 0;
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FText SpeakerText;
};

USTRUCT(BlueprintType)
struct FHSRDialogueChoiceRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Dialogue")
	FName DialogueId;

	UPROPERTY(BlueprintReadWrite, Category = "Dialogue")
	FName NodeId;

	UPROPERTY(BlueprintReadWrite, Category = "Dialogue")
	FName ChoiceId;

	bool IsValid() const
	{
		return !DialogueId.IsNone() && !NodeId.IsNone() && !ChoiceId.IsNone();
	}
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

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue|Quest")
	int32 EventCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue|Branch")
	EHSRDialogueChoiceBranch Branch = EHSRDialogueChoiceBranch::None;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue|Branch")
	FGuid OperationId;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue|Branch")
	bool bBranchSubmitted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue|Quest")
	EHSRQuestOperationResult QuestResult = EHSRQuestOperationResult::NoOp;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue|Encounter")
	FHSREncounterResult EncounterResponse;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue|Encounter")
	FHSREncounterRequest EncounterRequest;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue|Reward")
	EHSRRewardOperationResult RewardResult = EHSRRewardOperationResult::NoOp;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue|Reward")
	FHSRRewardReceipt RewardReceipt;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue|Reward")
	FName RewardDefinitionId;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue|Reward")
	int32 RewardSeed = 0;
};
