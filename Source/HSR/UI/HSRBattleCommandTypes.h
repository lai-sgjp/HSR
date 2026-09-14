#pragma once

#include "CoreMinimal.h"
#include "../Battle/HSRBattleTypes.h"
#include "../Reward/HSRRewardTypes.h"
#include "../GAS/Ability/HSRAbilityTypes.h"
#include "../Status/HSRStatusTypes.h"
#include "HSRBattleCommandTypes.generated.h"

namespace HSRBattleText
{
	inline FText FailureReason(EHSRAbilityFailureReason Reason)
	{
		switch (Reason)
		{
		case EHSRAbilityFailureReason::None: return FText::GetEmpty();
		case EHSRAbilityFailureReason::InvalidBattle: return NSLOCTEXT("HSRBattle", "InvalidBattle", "战斗尚未就绪");
		case EHSRAbilityFailureReason::DuplicateAction: return NSLOCTEXT("HSRBattle", "DuplicateAction", "此行动已经提交");
		case EHSRAbilityFailureReason::NotCurrentActor: return NSLOCTEXT("HSRBattle", "NotCurrentActor", "尚未轮到此角色行动");
		case EHSRAbilityFailureReason::DefinitionMissing: return NSLOCTEXT("HSRBattle", "DefinitionMissing", "技能配置不可用");
		case EHSRAbilityFailureReason::InvalidTarget: return NSLOCTEXT("HSRBattle", "InvalidTarget", "请选择有效目标");
		case EHSRAbilityFailureReason::AlreadyAtFullHealth: return NSLOCTEXT("HSRBattle", "FullHealth", "目标生命值已满");
		case EHSRAbilityFailureReason::InsufficientSkillPoint: return NSLOCTEXT("HSRBattle", "InsufficientPoints", "战技点不足");
		case EHSRAbilityFailureReason::InsufficientEnergy: return NSLOCTEXT("HSRBattle", "InsufficientEnergy", "能量不足");
		case EHSRAbilityFailureReason::CommitFailed: return NSLOCTEXT("HSRBattle", "CommitFailed", "行动提交失败，请重试");
		case EHSRAbilityFailureReason::EffectFailed: return NSLOCTEXT("HSRBattle", "EffectFailed", "技能执行失败");
		default: return NSLOCTEXT("HSRBattle", "Unavailable", "当前无法使用此技能");
		}
	}
}

UENUM(BlueprintType)
enum class EHSRPresentationEventType : uint8
{
	Damage UMETA(DisplayName = "伤害"),
	Toughness UMETA(DisplayName = "削韧"),
	Break UMETA(DisplayName = "击破"),
	Heal UMETA(DisplayName = "治疗")
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
		// Explicit localized strings survive cooked builds where UENUM metadata is stripped.
		switch (EventType)
		{
		case EHSRPresentationEventType::Damage: return NSLOCTEXT("HSRBattle", "DamageEvent", "伤害");
		case EHSRPresentationEventType::Toughness: return NSLOCTEXT("HSRBattle", "ToughnessEvent", "削韧");
		case EHSRPresentationEventType::Break: return NSLOCTEXT("HSRBattle", "BreakEvent", "击破");
		case EHSRPresentationEventType::Heal: return NSLOCTEXT("HSRBattle", "HealEvent", "治疗");
		default: return NSLOCTEXT("HSRBattle", "StatusEvent", "状态变化");
		}
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
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Result") FName RewardDefinitionId;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Result") TArray<FHSRInventoryGrant> RewardGrants;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Result") int64 RewardRevision = 0;
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
			Parts.Add(FText::Format(NSLOCTEXT("HSRCommand", "EntrySkillPointSpend", "战技点 −{0}"), FText::AsNumber(-SkillPointDelta)));
		}
		else if (SkillPointDelta > 0)
		{
			Parts.Add(FText::Format(NSLOCTEXT("HSRCommand", "EntrySkillPointGain", "战技点 +{0}"), FText::AsNumber(SkillPointDelta)));
		}

		if (bEnergyCostIsKnown && EnergyCost > 0.0f)
		{
			Parts.Add(FText::Format(NSLOCTEXT("HSRCommand", "EntryEnergyCost", "能量 −{0}"), FText::AsNumber(FMath::RoundToInt(EnergyCost))));
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
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") bool bActionPlaying = false;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") FText PlayingSkillName;
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
