#pragma once

#include "CoreMinimal.h"
#include "../Reward/HSRRewardTypes.h"
#include "HSRQuestTypes.generated.h"

UENUM(BlueprintType)
enum class EHSRQuestState : uint8
{
	NotStarted,
	Active,
	Completed
};

UENUM(BlueprintType)
enum class EHSRQuestOperationResult : uint8
{
	Success,
	NoOp,
	InvalidDefinitionId,
	DuplicateDefinitionId,
	UnknownQuestDefinition,
	UnknownDialogueDefinition,
	InvalidDefinition,
	InvalidEvent,
	InvalidState,
	RewardRejected,
	InvalidRestoreData
};

USTRUCT(BlueprintType)
struct FHSRQuestObjectiveDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FName ObjectiveId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FName EventId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest", meta = (ClampMin = "1"))
	int32 RequiredCount = 1;
};

USTRUCT(BlueprintType)
struct FHSRQuestDomainEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	FName EventId;

	UPROPERTY(BlueprintReadOnly, Category = "Quest", meta = (ClampMin = "1"))
	int32 Count = 1;
};

USTRUCT(BlueprintType)
struct FHSRQuestRuntimeObjective
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	FName ObjectiveId;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	int32 CurrentCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	int32 RequiredCount = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	bool bCompleted = false;
};

USTRUCT(BlueprintType)
struct FHSRQuestRuntimeState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	FName QuestId;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	EHSRQuestState State = EHSRQuestState::NotStarted;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	TArray<FHSRQuestRuntimeObjective> Objectives;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	bool bRewardClaimed = false;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	FGuid RewardClaimId;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	int64 Revision = 0;
};

USTRUCT(BlueprintType)
struct FHSRQuestSaveData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Quest|Save")
	TArray<FHSRQuestRuntimeState> States;

	UPROPERTY(BlueprintReadWrite, Category = "Quest|Save")
	int64 Revision = 0;
};

struct FHSRQuestRestoreState
{
	TMap<FName, FHSRQuestRuntimeState> States;
	int64 Revision = 0;
};

USTRUCT(BlueprintType)
struct FHSRQuestRewardClaimResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	EHSRQuestOperationResult QuestResult = EHSRQuestOperationResult::NoOp;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	EHSRRewardOperationResult RewardResult = EHSRRewardOperationResult::NoOp;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	FHSRRewardReceipt Receipt;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FHSRQuestChanged, const FHSRQuestRuntimeState&);
DECLARE_MULTICAST_DELEGATE_OneParam(FHSRQuestRestored, int64);

