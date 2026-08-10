#pragma once

#include "CoreMinimal.h"
#include "../GAS/Ability/HSRAbilityTypes.h"
#include "HSRBattleTypes.generated.h"

class UHSRSkillDefinition;

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
	Finished UMETA(DisplayName = "Finished"),
	Failed UMETA(DisplayName = "Failed")
};

UENUM(BlueprintType)
enum class EHSRBattleOutcome : uint8
{
	None UMETA(DisplayName = "None"),
	PlayerVictory UMETA(DisplayName = "Player Victory"),
	PlayerDefeat UMETA(DisplayName = "Player Defeat")
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

/** One authored slot on a side's roster, before ParticipantIds are minted.  The Coordinator
 * turns each entry into exactly one participant, so roster length drives battle width. */
USTRUCT(BlueprintType)
struct FHSRBattleRosterEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	FName CharacterId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Battle")
	TSubclassOf<APawn> PawnClass;

	/**
	 * Presentation resolved by whoever assembles the roster, because that caller is the one holding
	 * the character definition. Empty is valid and means "no authored name"; the UI falls back to
	 * the participant id. Keeping it on the entry lets the Coordinator stay free of asset lookups.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	TSoftObjectPtr<UTexture2D> Portrait;

	FHSRBattleRosterEntry() = default;
	FHSRBattleRosterEntry(FName InCharacterId, TSubclassOf<APawn> InPawnClass)
		: CharacterId(InCharacterId), PawnClass(InPawnClass) {}

	bool IsValid() const { return !CharacterId.IsNone(); }
};

/** One participant's skill list, in presentation order.  Wrapper struct exists because
 * TMap values cannot be bare TArray in a UPROPERTY. */
USTRUCT()
struct FHSRSkillLoadout
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<UHSRSkillDefinition>> Skills;
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Battle")
	TSubclassOf<APawn> PawnClass;

	/** Carried through from the roster entry; see FHSRBattleRosterEntry::DisplayName. */
	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	TSoftObjectPtr<UTexture2D> Portrait;

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
	FName ExplorationMapId;

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

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Reward")
	FName RewardDefinitionId;

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

USTRUCT(BlueprintType)
struct FHSRTeamResourceState
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Resources") int32 CurrentSkillPoints = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle|Resources") int32 MaxSkillPoints = 3;
};

USTRUCT()
struct FHSRSkillPointReservation
{
	GENERATED_BODY()
	FGuid ActionId;
	int32 Delta = 0;
};

/** Pure battle completion DTO. It deliberately contains no runtime object references. */
USTRUCT(BlueprintType)
struct FHSRBattleResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	EHSRBattleOutcome Outcome = EHSRBattleOutcome::None;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	FName DefeatedParticipantId;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	FName EncounterId;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Reward")
	FName RewardDefinitionId;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	FHSRBattleReturnContext ReturnContext;

	bool IsValid() const { return RequestId.IsValid() && Outcome != EHSRBattleOutcome::None; }
};
