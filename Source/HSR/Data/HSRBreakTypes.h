#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HSRBreakTypes.generated.h"

UENUM(BlueprintType)
enum class EHSRElementToughnessContractResult : uint8 { Valid, MissingElement, InvalidElementTag, EmptyWeaknesses, InvalidWeaknessTag, InvalidToughnessDamage, InvalidInitialToughness, InvalidMaxToughness };

UENUM(BlueprintType)
enum class EHSRToughnessFailureReason : uint8 { None, MissingElement, EmptyWeaknesses, NoWeaknessMatch, InvalidDamage, MissingGameplayEffect, EffectApplicationFailed };

UENUM(BlueprintType)
enum class EHSRBreakFailureReason : uint8 { None, ToughnessNotDepleted, AlreadyPublished, InvalidTarget, BattleFinished };

USTRUCT(BlueprintType)
struct HSR_API FHSRToughnessResult
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bMatched = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Before = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Damage = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float After = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bReachedZero = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EHSRToughnessFailureReason FailureReason = EHSRToughnessFailureReason::None;
};

/** Pure P8-003 publication record. It has no runtime Actor, ASC, or UObject state. */
USTRUCT(BlueprintType)
struct HSR_API FHSRBreakResult
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FGuid ActionId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName TargetParticipantId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float ToughnessBefore = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float ToughnessAfter = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bTriggered = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EHSRBreakFailureReason FailureReason = EHSRBreakFailureReason::None;
};

/** Pure request emitted by a triggered BreakResult. TurnManager is its only consumer. */
USTRUCT(BlueprintType)
struct HSR_API FHSRTurnDelayRequest
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FGuid ActionId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName TargetParticipantId;
};

/** Battle-local, pure-value action-distance adjustment.  It deliberately has no Actor/ASC state. */
enum class EHSRActionDistanceAdjustmentKind : uint8
{
	Advance,
	Delay
};

enum class EHSRActionDistanceAdjustmentResult : uint8
{
	Accepted,
	DuplicateOperation,
	InvalidRequest,
	InvalidEpoch,
	InvalidTarget,
	DefeatedTarget,
	Finished,
	ArithmeticFailure
};

struct HSR_API FHSRActionDistanceRequest
{
	uint64 BattleEpoch = 0;
	FGuid OperationId;
	FName TargetParticipantId;
	float Ratio = 0.0f;
	EHSRActionDistanceAdjustmentKind Kind = EHSRActionDistanceAdjustmentKind::Advance;
};

/** Diagnostic result; callers can verify rejection and replay without querying runtime objects. */
struct HSR_API FHSRActionDistanceResult
{
	EHSRActionDistanceAdjustmentResult Result = EHSRActionDistanceAdjustmentResult::InvalidRequest;
	float OldBase = 0.0f;
	float NewBase = 0.0f;
	float OldRemaining = 0.0f;
	float NewRemaining = 0.0f;
	int32 OldPendingOperationCount = 0;
	int32 NewPendingOperationCount = 0;
	int32 PendingOperationCount = 0;
	FName CurrentParticipantId;
	FName NextParticipantId;
	uint64 BattleEpoch = 0;
	uint64 TurnSequence = 0;
};

/**
 * One upcoming action slot on the turn-order bar. It describes a predicted future action rather than
 * present state, so any speed change or advance/delay invalidates a previously built forecast.
 * Pure value: the UI reads it without ever touching a participant Actor or ASC.
 */
USTRUCT(BlueprintType)
struct HSR_API FHSRTurnForecastEntry
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|TurnOrder")
	FName ParticipantId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|TurnOrder")
	bool bPlayerTeam = false;

	/** 0 for the participant acting now, 1 for the next one, and so on. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|TurnOrder")
	int32 SlotIndex = 0;

	/** Action-distance units until this action, relative to now. 0 means acting immediately. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|TurnOrder")
	float DistanceUntilAction = 0.0f;

	/** True when this participant already occupies an earlier slot of the same forecast. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|TurnOrder")
	bool bRepeatAction = false;
};

/** Pure Phase 8 authoring validation; it has no runtime battle side effects. */
struct HSR_API FHSRToughnessConfiguration
{
	/**
	 * Single source for the Element.X -> Weakness.X naming rule. Callers used to open-code this
	 * string surgery, so a new element only worked if every copy agreed and the derived tag had
	 * actually been authored. Returns an invalid tag when the input is not an Element.* tag or
	 * when no matching Weakness.* tag exists, which callers should treat as "no weakness match"
	 * rather than assuming the tag is present.
	 */
	static FGameplayTag GetWeaknessTagFor(const FGameplayTag& ElementTag)
	{
		if (ValidateElement(ElementTag) != EHSRElementToughnessContractResult::Valid)
		{
			return FGameplayTag();
		}

		static const FString ElementPrefix(TEXT("Element."));
		const FString ElementName = ElementTag.ToString();
		if (!ElementName.StartsWith(ElementPrefix))
		{
			return FGameplayTag();
		}

		const FString Leaf = ElementName.RightChop(ElementPrefix.Len());
		return Leaf.IsEmpty()
			? FGameplayTag()
			: FGameplayTag::RequestGameplayTag(FName(*FString::Printf(TEXT("Weakness.%s"), *Leaf)), false);
	}

	static EHSRElementToughnessContractResult ValidateElement(const FGameplayTag& Tag)
	{
		if (!Tag.IsValid()) return EHSRElementToughnessContractResult::MissingElement;
		const FGameplayTag Root = FGameplayTag::RequestGameplayTag(TEXT("Element"), false);
		return Root.IsValid() && Tag.MatchesTag(Root) ? EHSRElementToughnessContractResult::Valid : EHSRElementToughnessContractResult::InvalidElementTag;
	}
	static EHSRElementToughnessContractResult ValidateWeaknesses(const FGameplayTagContainer& Tags)
	{
		if (Tags.IsEmpty()) return EHSRElementToughnessContractResult::EmptyWeaknesses;
		const FGameplayTag Root = FGameplayTag::RequestGameplayTag(TEXT("Weakness"), false);
		for (const FGameplayTag& Tag : Tags) if (!Root.IsValid() || !Tag.IsValid() || !Tag.MatchesTag(Root)) return EHSRElementToughnessContractResult::InvalidWeaknessTag;
		return EHSRElementToughnessContractResult::Valid;
	}
	static EHSRElementToughnessContractResult ValidateToughnessDamage(float Value) { return FMath::IsFinite(Value) && Value > 0.0f ? EHSRElementToughnessContractResult::Valid : EHSRElementToughnessContractResult::InvalidToughnessDamage; }
	static EHSRElementToughnessContractResult ValidateInitialToughness(float Current, float Maximum)
	{
		if (!FMath::IsFinite(Maximum) || Maximum <= 0.0f) return EHSRElementToughnessContractResult::InvalidMaxToughness;
		return FMath::IsFinite(Current) && Current > 0.0f && Current <= Maximum ? EHSRElementToughnessContractResult::Valid : EHSRElementToughnessContractResult::InvalidInitialToughness;
	}
};
