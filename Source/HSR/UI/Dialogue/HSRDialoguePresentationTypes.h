#pragma once

#include "CoreMinimal.h"
#include "../../Dialogue/HSRDialogueTypes.h"
#include "../../Quest/HSRQuestTypes.h"
#include "HSRDialoguePresentationTypes.generated.h"

UENUM(BlueprintType)
enum class EHSRDialoguePresentationStatus : uint8
{
	Unavailable,
	Active,
	Closed
};

UENUM(BlueprintType)
enum class EHSRDialoguePresentationResult : uint8
{
	Success,
	NoOp,
	InvalidRequest,
	Unavailable,
	AlreadyActive,
	StaleRequest,
	UnknownDialogue,
	InvalidNode,
	InvalidChoice,
	AuthorityUnavailable,
	AuthorityRejected,
	OperationIdConflict,
	Closed
};

USTRUCT(BlueprintType)
struct HSR_API FHSRDialoguePresentationRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "HSR|Dialogue")
	FGuid QueryId;

	UPROPERTY(BlueprintReadWrite, Category = "HSR|Dialogue")
	FName DialogueId;

	UPROPERTY(BlueprintReadWrite, Category = "HSR|Dialogue")
	FName NodeId;

	bool IsValid() const
	{
		return QueryId.IsValid() && !DialogueId.IsNone() && !NodeId.IsNone();
	}
};

USTRUCT(BlueprintType)
struct HSR_API FHSRDialoguePresentationChoiceRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "HSR|Dialogue")
	FGuid QueryId;

	UPROPERTY(BlueprintReadWrite, Category = "HSR|Dialogue")
	FName DialogueId;

	UPROPERTY(BlueprintReadWrite, Category = "HSR|Dialogue")
	FName NodeId;

	UPROPERTY(BlueprintReadWrite, Category = "HSR|Dialogue")
	FName ChoiceId;

	bool IsValid() const
	{
		return QueryId.IsValid() && !DialogueId.IsNone() && !NodeId.IsNone() && !ChoiceId.IsNone();
	}
};

USTRUCT(BlueprintType)
struct HSR_API FHSRDialoguePresentationChoice
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "HSR|Dialogue")
	FName ChoiceId;

	UPROPERTY(BlueprintReadOnly, Category = "HSR|Dialogue")
	FText DisplayText;

	UPROPERTY(BlueprintReadOnly, Category = "HSR|Dialogue")
	FName TargetNodeId;

	UPROPERTY(BlueprintReadOnly, Category = "HSR|Dialogue")
	bool bEnabled = true;

	UPROPERTY(BlueprintReadOnly, Category = "HSR|Dialogue|Branch")
	EHSRDialogueChoiceBranch Branch = EHSRDialogueChoiceBranch::None;

	UPROPERTY(BlueprintReadOnly, Category = "HSR|Dialogue|Branch")
	FGuid OperationId;
};

USTRUCT(BlueprintType)
struct HSR_API FHSRDialoguePresentationSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "HSR|Dialogue")
	EHSRDialoguePresentationStatus Status = EHSRDialoguePresentationStatus::Unavailable;

	UPROPERTY(BlueprintReadOnly, Category = "HSR|Dialogue")
	bool bIsValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "HSR|Dialogue")
	bool bIsActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "HSR|Dialogue")
	FGuid QueryId;

	UPROPERTY(BlueprintReadOnly, Category = "HSR|Dialogue")
	FName DialogueId;

	UPROPERTY(BlueprintReadOnly, Category = "HSR|Dialogue")
	FName NodeId;

	UPROPERTY(BlueprintReadOnly, Category = "HSR|Dialogue")
	FText SpeakerText;

	UPROPERTY(BlueprintReadOnly, Category = "HSR|Dialogue")
	FText BodyText;

	UPROPERTY(BlueprintReadOnly, Category = "HSR|Dialogue")
	TArray<FHSRDialoguePresentationChoice> Choices;

	UPROPERTY(BlueprintReadOnly, Category = "HSR|Dialogue")
	EHSRQuestOperationResult AuthorityResult = EHSRQuestOperationResult::NoOp;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FHSRDialoguePresentationChanged, const FHSRDialoguePresentationSnapshot&);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSRDialoguePresentationBlueprintChanged, const FHSRDialoguePresentationSnapshot&, Snapshot);
