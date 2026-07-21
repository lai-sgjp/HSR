#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "HSRStatusTypes.generated.h"

UENUM(BlueprintType)
enum class EHSRStatusTriggerTiming : uint8
{
	TargetTurnEnded,
	Unsupported
};

UENUM(BlueprintType)
enum class EHSRStatusRefreshPolicy : uint8
{
	RefreshDuration,
	AddStack,
	Unsupported
};

UENUM(BlueprintType)
enum class EHSRStatusEffectKind : uint8
{
	TagOnly,
	DamageOverTime
};

UENUM(BlueprintType)
enum class EHSRStatusClassification : uint8
{
	Buff,
	Debuff
};

UENUM(BlueprintType)
enum class EHSRSourceInvalidPolicy : uint8
{
	Keep,
	Remove
};

UENUM(BlueprintType)
enum class EHSRStatusPublicOperation : uint8
{
	None,
	Add,
	Stack,
	Refresh,
	Replace,
	Trigger,
	Dispel,
	Expire,
	Clear
};

UENUM(BlueprintType)
enum class EHSRStatusOperationResult : uint8
{
	Success,
	Refreshed,
	StackAdded,
	AtMaxRefreshed,
	Replaced,
	InvalidDefinition,
	InvalidStatusId,
	InvalidDuration,
	InvalidPolicy,
	InvalidSource,
	InvalidTarget,
	InvalidEpoch,
	MissingAbilitySystem,
	DefeatedTarget,
	MissingGameplayEffect,
	GameplayEffectNotInfinite,
	ApplyFailed,
	InvalidRuntimeInstance,
	RemoveFailed,
	IgnoredEvent,
	Immune,
	Dispelled,
	NoDispelCandidate,
	Triggered
};

USTRUCT()
struct HSR_API FHSRStatusInstance
{
	GENERATED_BODY()

	FName StatusId;
	FName SourceParticipantId;
	FName TargetParticipantId;
	uint64 BattleEpoch = 0;
	int32 Stacks = 0;
	int32 RemainingTurns = 0;
	uint64 LastConsumedTurnSequence = 0;
	FActiveGameplayEffectHandle ActiveGameplayEffectHandle;
	uint64 LastDamageTurnSequence = 0;
	EHSRStatusOperationResult LastOperationResult = EHSRStatusOperationResult::Success;
};

USTRUCT()
struct HSR_API FHSRStatusRuntimeSnapshot
{
	GENERATED_BODY()

	EHSRStatusOperationResult Result = EHSRStatusOperationResult::Success;
	FName StatusId;
	FName SourceParticipantId;
	FName TargetParticipantId;
	uint64 BattleEpoch = 0;
	uint64 LastConsumedTurnSequence = 0;
	int32 RemainingTurns = 0;
	int32 Stacks = 0;
	int32 InstanceCount = 0;
	int32 ApplyCount = 0;
	int32 RemoveCount = 0;
	bool bHandleValid = false;
	FString ActiveHandleIdentity;
	bool bHandleActiveInAbilitySystem = false;
	int32 GameplayEffectStackCount = 0;
	bool bSecondaryHandleValid = false;
	FString SecondaryHandleIdentity;
	bool bSecondaryHandleActiveInAbilitySystem = false;
	int32 LastRemovedRemainingTurns = 0;
	uint64 LastRemovedTurnSequence = 0;
};

USTRUCT(BlueprintType)
struct HSR_API FHSRStatusPublicSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Status") FName StatusId;
	UPROPERTY(BlueprintReadOnly, Category = "Status") FName TargetParticipantId;
	UPROPERTY(BlueprintReadOnly, Category = "Status") FText DisplayName;
	UPROPERTY(BlueprintReadOnly, Category = "Status") EHSRStatusClassification Classification = EHSRStatusClassification::Buff;
	UPROPERTY(BlueprintReadOnly, Category = "Status") int32 Stacks = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Status") int32 RemainingTurns = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Status") EHSRStatusOperationResult LastResult = EHSRStatusOperationResult::Success;
};

USTRUCT(BlueprintType)
struct HSR_API FHSRStatusPublicOperationEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Status") EHSRStatusPublicOperation Operation = EHSRStatusPublicOperation::None;
	UPROPERTY(BlueprintReadOnly, Category = "Status") EHSRStatusOperationResult Result = EHSRStatusOperationResult::Success;
	UPROPERTY(BlueprintReadOnly, Category = "Status") FName StatusId;
	UPROPERTY(BlueprintReadOnly, Category = "Status") FName TargetParticipantId;
	UPROPERTY(BlueprintReadOnly, Category = "Status") int64 Sequence = 0;
};
