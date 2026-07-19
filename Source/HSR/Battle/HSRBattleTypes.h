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

};