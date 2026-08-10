#pragma once

#include "CoreMinimal.h"
#include "../Battle/HSRBattleTypes.h"
#include "../GAS/Ability/HSRAbilityTypes.h"
#include "../Status/HSRStatusTypes.h"
#include "HSRBattleCommandTypes.generated.h"

UENUM(BlueprintType)
enum class EHSRPresentationEventType : uint8
{
	Damage UMETA(DisplayName = "Damage"),
	Toughness UMETA(DisplayName = "Toughness"),
	Break UMETA(DisplayName = "Break"),
	Heal UMETA(DisplayName = "Heal")
};

USTRUCT(BlueprintType)
struct FHSRBattlePresentationEvent
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Presentation") FGuid EventId;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Presentation") FGuid ActionId;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Presentation") FName SourceParticipantId;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Presentation") FName TargetParticipantId;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Presentation") EHSRPresentationEventType EventType = EHSRPresentationEventType::Damage;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Presentation") float Value = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Presentation") bool bCritical = false;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Presentation") bool bBreak = false;

	/**
	 * Display label for EventType, read from the UENUM's DisplayName metadata rather than a
	 * hand-written switch. A new event kind gets a label from its own UMETA the moment it is
	 * declared, so no UI site has to be edited to keep up. Returns the raw numeric value when
	 * reflection is unavailable, which is visibly wrong rather than silently mislabelled -- the
	 * ternary chain this replaced fell through to "Heal" for every unrecognised kind.
	 */
	FText GetEventTypeLabel() const
	{
		if (const UEnum* EnumType = StaticEnum<EHSRPresentationEventType>())
		{
			return EnumType->GetDisplayNameTextByValue(static_cast<int64>(EventType));
		}
		return FText::AsNumber(static_cast<int32>(EventType));
	}
};

/** Read-only terminal result for the result panel. It contains no runtime objects or return authority. */
USTRUCT(BlueprintType)
struct FHSRBattleResultViewState
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Result") FGuid RequestId;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Result") EHSRBattleOutcome Outcome = EHSRBattleOutcome::None;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Result") FName DefeatedParticipantId;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Result") bool bVisible = false;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Result") bool bConfirmPending = false;
};

USTRUCT(BlueprintType)
struct FHSRBattleCommandSkillView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") FName SkillId;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") EHSRSkillCategory Category = EHSRSkillCategory::BasicAttack;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") EHSRTargetType TargetType = EHSRTargetType::SingleEnemy;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") FText DisplayName;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") FText Description;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") bool bDescriptionIsPlaceholder = true;
	/** Points consumed, always >= 0. Zero for skills that grant points -- read SkillPointDelta for
	    the signed value. */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") int32 SkillPointCost = 0;

	/** Signed skill-point change, straight from the DataAsset: negative spends, positive grants. */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") int32 SkillPointDelta = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") float EnergyCost = 0.0f;
	/** False means EnergyCost is unknown, never that the command is free. */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") bool bEnergyCostIsKnown = false;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") TArray<FName> CandidateTargetIds;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") bool bAvailable = false;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") EHSRAbilityFailureReason DisabledReason = EHSRAbilityFailureReason::None;

	/**
	 * Player-facing cost line, derived from the authored numbers rather than from Category. A skill
	 * authored to spend two points reads "SP -2" whatever category it carries; the old per-category
	 * chain hardcoded "SP +1" for BasicAttack and would have misreported any other authored value.
	 * Lives here so every command surface renders one cost string from one rule.
	 */
	FText BuildCostText() const
	{
		TArray<FText> Parts;
		if (SkillPointDelta < 0)
		{
			Parts.Add(FText::Format(NSLOCTEXT("HSRCommand", "EntrySkillPointSpend", "SP -{0}"), FText::AsNumber(-SkillPointDelta)));
		}
		else if (SkillPointDelta > 0)
		{
			Parts.Add(FText::Format(NSLOCTEXT("HSRCommand", "EntrySkillPointGain", "SP +{0}"), FText::AsNumber(SkillPointDelta)));
		}

		if (bEnergyCostIsKnown && EnergyCost > 0.0f)
		{
			Parts.Add(FText::Format(NSLOCTEXT("HSRCommand", "EntryEnergyCost", "Energy -{0}"), FText::AsNumber(FMath::RoundToInt(EnergyCost))));
		}

		return Parts.IsEmpty() ? FText::GetEmpty() : FText::Join(FText::FromString(TEXT("  ")), Parts);
	}
};

USTRUCT(BlueprintType)
struct FHSRBattleParticipantView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Participants") FName ParticipantId;

	/**
	 * Authored player-facing name, empty when the definition left it unset. Read it through
	 * GetDisplayLabel() rather than directly so the id fallback stays in one place.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Participants") FText DisplayName;

	/** Authored portrait, unset when the definition has none. The UI decides what to show then. */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Participants") TSoftObjectPtr<UTexture2D> Portrait;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Participants") bool bPlayerTeam = false;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Participants") bool bDefeated = false;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Participants") float Health = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Participants") float MaxHealth = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Participants") float Energy = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Participants") float MaxEnergy = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Participants") float Toughness = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Participants") float MaxToughness = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Participants") TArray<FGameplayTag> WeaknessTags;
	/** False means the numeric fields above were never read from an ASC, not that they are zero. */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Participants") bool bHasAttributes = false;

	/**
	 * Name to render: the authored DisplayName, or the participant id when none was authored. Every
	 * UI site should call this instead of choosing a fallback itself, so an unauthored definition
	 * degrades to a readable id in exactly one way.
	 */
	FText GetDisplayLabel() const
	{
		return DisplayName.IsEmpty() ? FText::FromName(ParticipantId) : DisplayName;
	}
};

USTRUCT(BlueprintType)
struct FHSRBattleCommandViewState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") FGuid BattleId;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") FName CurrentActorId;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") TArray<FName> TurnOrderParticipantIds;

	/** Action-distance forecast for the turn-order bar. Empty outside an active battle. */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|TurnOrder") TArray<FHSRTurnForecastEntry> TurnForecast;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Participants") TArray<FHSRBattleParticipantView> Participants;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") bool bCurrentActorPlayerControlled = false;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") int32 SkillPoints = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") int32 MaxSkillPoints = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") float Energy = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") float MaxEnergy = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") TArray<FHSRBattleCommandSkillView> Skills;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") FHSRAbilityResolution LastResolution;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Status") TArray<FHSRStatusPublicSnapshot> Statuses;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Status") FHSRStatusPublicOperationEvent LastStatusOperation;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Presentation") TArray<FHSRBattlePresentationEvent> PresentationEvents;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Result") FHSRBattleResultViewState ResultViewState;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") FName SelectedSkillId;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") FName SelectedTargetId;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") bool bCanSubmit = false;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") bool bCommandPending = false;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") bool bPresentationLocked = false;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") FGuid PendingActionId;

	/** Participant lookup by id, or null when the id is not in this snapshot. */
	const FHSRBattleParticipantView* FindParticipant(FName ParticipantId) const
	{
		return Participants.FindByPredicate([ParticipantId](const FHSRBattleParticipantView& Candidate)
		{
			return Candidate.ParticipantId == ParticipantId;
		});
	}

	/**
	 * Name to render for a participant id. Falls back to the raw id when the id is absent from the
	 * snapshot, which happens for a participant that left the battle between publishes.
	 */
	FText GetParticipantLabel(FName ParticipantId) const
	{
		const FHSRBattleParticipantView* Participant = FindParticipant(ParticipantId);
		return Participant ? Participant->GetDisplayLabel() : FText::FromName(ParticipantId);
	}

	/** Skill lookup by stable id. Prefer this over category matching: a loadout may hold several
	    skills of the same category, and only the id distinguishes them. */
	const FHSRBattleCommandSkillView* FindSkill(FName SkillId) const
	{
		return Skills.FindByPredicate([SkillId](const FHSRBattleCommandSkillView& Candidate)
		{
			return Candidate.SkillId == SkillId;
		});
	}

	/** First skill of a category, or null. First-match-wins, so it cannot address a second skill
	    sharing the category -- present for legacy four-button UI paths only. */
	const FHSRBattleCommandSkillView* FindSkillByCategory(EHSRSkillCategory Category) const
	{
		return Skills.FindByPredicate([Category](const FHSRBattleCommandSkillView& Candidate)
		{
			return Candidate.Category == Category;
		});
	}

	const FHSRBattleCommandSkillView* FindSelectedSkill() const
	{
		return FindSkill(SelectedSkillId);
	}
};
