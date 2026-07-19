#pragma once

#include "CoreMinimal.h"
#include "HSREncounterTypes.generated.h"

UENUM(BlueprintType)
enum class EHSREncounterState : uint8
{
	Empty UMETA(DisplayName = "Empty"),
	Pending UMETA(DisplayName = "Pending"),
	Traveling UMETA(DisplayName = "Traveling"),
	Consumed UMETA(DisplayName = "Consumed")
};

UENUM(BlueprintType)
enum class EHSREncounterInitiative : uint8
{
	Player UMETA(DisplayName = "Player"),
	Enemy UMETA(DisplayName = "Enemy"),
	Neutral UMETA(DisplayName = "Neutral")
};

UENUM(BlueprintType)
enum class EHSREncounterResultType : uint8
{
	Success UMETA(DisplayName = "Success"),
	AlreadyPending UMETA(DisplayName = "Already Pending / Traveling"),
	InvalidDefinition UMETA(DisplayName = "Invalid Definition"),
	InvalidRequest UMETA(DisplayName = "Invalid Request"),
	InvalidMap UMETA(DisplayName = "Invalid Battle Map"),
	NothingPending UMETA(DisplayName = "Nothing Pending"),
	AlreadyConsumed UMETA(DisplayName = "Already Consumed"),
	TravelInitiationFailed UMETA(DisplayName = "Travel Initiation Failed")
};

UENUM(BlueprintType)
enum class EHSREncounterReturnResultType : uint8
{
	Success UMETA(DisplayName = "Success"),
	NothingPending UMETA(DisplayName = "Nothing Pending"),
	AlreadyPending UMETA(DisplayName = "Already Pending"),
	AlreadyConsumed UMETA(DisplayName = "Already Consumed"),
	InvalidReturnContext UMETA(DisplayName = "Invalid Return Context")
};

USTRUCT(BlueprintType)
struct FHSREncounterRequest
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName EncounterId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName EnemyDefinitionId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	EHSREncounterInitiative Initiative = EHSREncounterInitiative::Neutral;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName BattleMapPath;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FTransform ReturnTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FName ExplorationMapPath;

	FHSREncounterRequest() = default;
};

USTRUCT(BlueprintType)
struct FHSREncounterResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	EHSREncounterResultType ResultType = EHSREncounterResultType::InvalidRequest;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FText Message;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	FHSREncounterRequest ConsumedRequest;

	FHSREncounterResult() = default;

	static FHSREncounterResult MakeSuccess(const FGuid& InRequestId)
	{
		FHSREncounterResult Result;
		Result.ResultType = EHSREncounterResultType::Success;
		Result.RequestId = InRequestId;
		return Result;
	}

	static FHSREncounterResult MakeFailure(EHSREncounterResultType Type, const FText& InMessage = FText())
	{
		FHSREncounterResult Result;
		Result.ResultType = Type;
		Result.Message = InMessage;
		return Result;
	}
};

USTRUCT(BlueprintType)
struct FHSRExplorationReturnContext
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "Return")
	FName ExplorationMapPath;

	UPROPERTY(BlueprintReadOnly, Category = "Return")
	FTransform ReturnTransform = FTransform::Identity;

	FHSRExplorationReturnContext() = default;
};

USTRUCT(BlueprintType)
struct FHSRExplorationReturnResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Return")
	EHSREncounterReturnResultType ResultType = EHSREncounterReturnResultType::NothingPending;

	UPROPERTY(BlueprintReadOnly, Category = "Return")
	FText Message;

	FHSRExplorationReturnResult() = default;

	UPROPERTY(BlueprintReadOnly, Category = "Return")
	FHSRExplorationReturnContext ConsumedContext;

	static FHSRExplorationReturnResult MakeSuccess()
	{
		FHSRExplorationReturnResult Result;
		Result.ResultType = EHSREncounterReturnResultType::Success;
		return Result;
	}

	static FHSRExplorationReturnResult MakeFailure(EHSREncounterReturnResultType Type, const FText& InMessage = FText())
	{
		FHSRExplorationReturnResult Result;
		Result.ResultType = Type;
		Result.Message = InMessage;
		return Result;
	}
};
