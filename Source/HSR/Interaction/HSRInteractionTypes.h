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

USTRUCT(BlueprintType)
struct FHSRInteractionContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TWeakObjectPtr<AActor> TargetActor;

	FHSRInteractionContext() {}
	explicit FHSRInteractionContext(AActor* InTarget) : TargetActor(InTarget) {}
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
};
