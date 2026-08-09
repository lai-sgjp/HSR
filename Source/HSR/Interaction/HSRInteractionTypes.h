#pragma once

#include "CoreMinimal.h"
#include "HSRInteractionTypes.generated.h"

UENUM(BlueprintType)
enum class EHSRInteractionFailureReason : uint8
{
	None UMETA(DisplayName = "None / Success"),
	NoCandidate UMETA(DisplayName = "No Candidate"),
	TargetInvalid UMETA(DisplayName = "Target Invalid / Expired"),
	OutOfRange UMETA(DisplayName = "Out of Range"),
	Unavailable UMETA(DisplayName = "Unavailable"),
	ExecutionFailed UMETA(DisplayName = "Execution Failed")
};

UENUM(BlueprintType)
enum class EHSRInteractionPayloadType : uint8
{
	None,
	Dialogue
};

USTRUCT(BlueprintType)
struct FHSRInteractionContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TWeakObjectPtr<AActor> InteractorActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FVector InteractionLocation = FVector::ZeroVector;

	FHSRInteractionContext() = default;
	FHSRInteractionContext(AActor* InInteractor, const FVector& InLocation)
		: InteractorActor(InInteractor), InteractionLocation(InLocation) {}
};

USTRUCT(BlueprintType)
struct FHSRInteractionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	EHSRInteractionFailureReason FailureReason = EHSRInteractionFailureReason::None;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FText Message;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction|Payload")
	EHSRInteractionPayloadType PayloadType = EHSRInteractionPayloadType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction|Payload")
	FName DialogueId;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction|Payload")
	FName DialogueNodeId;

	FHSRInteractionResult() {}

	static FHSRInteractionResult MakeSuccess()
	{
		FHSRInteractionResult Result;
		Result.bSuccess = true;
		Result.FailureReason = EHSRInteractionFailureReason::None;
		return Result;
	}

	static FHSRInteractionResult MakeFailure(EHSRInteractionFailureReason Reason, const FText& InMessage = FText())
	{
		FHSRInteractionResult Result;
		Result.bSuccess = false;
		Result.FailureReason = Reason;
		Result.Message = InMessage;
		return Result;
	}

	static FHSRInteractionResult MakeDialogueSuccess(FName InDialogueId, FName InNodeId)
	{
		FHSRInteractionResult Result = MakeSuccess();
		Result.PayloadType = EHSRInteractionPayloadType::Dialogue;
		Result.DialogueId = InDialogueId;
		Result.DialogueNodeId = InNodeId;
		return Result;
	}

	bool HasDialoguePayload() const
	{
		return bSuccess && PayloadType == EHSRInteractionPayloadType::Dialogue
			&& !DialogueId.IsNone() && !DialogueNodeId.IsNone();
	}
};
