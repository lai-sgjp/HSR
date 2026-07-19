#pragma once

#include "CoreMinimal.h"
#include "HSRBattleTypes.generated.h"

UENUM(BlueprintType)
enum class EHSRBattleParticipantTeam : uint8
{
	Player UMETA(DisplayName = "Player"),
	Enemy UMETA(DisplayName = "Enemy")
};

UENUM(BlueprintType)
enum class EHSRBattleCoordinatorState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	Consuming UMETA(DisplayName = "Consuming"),
	Spawned UMETA(DisplayName = "Spawned"),
	Failed UMETA(DisplayName = "Failed")
};

UENUM(BlueprintType)
enum class EHSRBattleInitFailureType : uint8
{
	None UMETA(DisplayName = "None"),
	DefinitionNotFound UMETA(DisplayName = "Definition Not Found"),
	DefinitionTypeMismatch UMETA(DisplayName = "Definition Type Mismatch"),
	ClassLoadFailed UMETA(DisplayName = "Class Load Failed"),
	SpawnFailed UMETA(DisplayName = "Spawn Failed"),
	InitFailed UMETA(DisplayName = "ASC Init Failed")
};

UENUM(BlueprintType)
USTRUCT(BlueprintType)
struct FHSRBattleParticipantDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	FName ParticipantId;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	FName DefinitionId;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	EHSRBattleParticipantTeam Team = EHSRBattleParticipantTeam::Player;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Battle")
	TSubclassOf<APawn> PawnClass;


	FHSRBattleParticipantDefinition() = default;
};

USTRUCT(BlueprintType)
struct FHSRBattleReturnContext
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	FName ExplorationMapPath;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	FTransform ReturnTransform = FTransform::Identity;

	FHSRBattleReturnContext() = default;
};

USTRUCT(BlueprintType)
struct FHSRBattleRequestContext
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	FName EncounterId;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	FName EnemyDefinitionId;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	FName BattleMapPath;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	FHSRBattleReturnContext ReturnContext;

	FHSRBattleRequestContext() = default;
};

USTRUCT(BlueprintType)
struct FHSRBattleInitResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	EHSRBattleInitFailureType FailureType = EHSRBattleInitFailureType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	FName TargetDefinitionId;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	FText Message;

	FHSRBattleInitResult() = default;

	bool IsSuccess() const { return FailureType == EHSRBattleInitFailureType::None; }

	static FHSRBattleInitResult MakeSuccess()
	{
		return FHSRBattleInitResult();
	}

	static FHSRBattleInitResult MakeFailure(EHSRBattleInitFailureType InType, const FText& InMessage, FName InDefId = NAME_None)
	{
		FHSRBattleInitResult Result;
		Result.FailureType = InType;
		Result.Message = InMessage;
		Result.TargetDefinitionId = InDefId;
		return Result;
	}
};