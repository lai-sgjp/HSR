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

/** Pure Phase 8 authoring validation; it has no runtime battle side effects. */
struct HSR_API FHSRToughnessConfiguration
{
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
