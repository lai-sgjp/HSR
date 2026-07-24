#pragma once

#include "CoreMinimal.h"
#include "../Battle/HSRBattleTypes.h"
#include "../GAS/Ability/HSRAbilityTypes.h"
#include "../Status/HSRStatusTypes.h"
#include "HSRBattleCommandTypes.generated.h"

UENUM(BlueprintType)
enum class EHSRPresentationEventType : uint8
{
	Damage,
	Toughness,
	Break,
	Heal
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
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") int32 SkillPointCost = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") float EnergyCost = 0.0f;
	/** False means EnergyCost is unknown, never that the command is free. */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") bool bEnergyCostIsKnown = false;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") TArray<FName> CandidateTargetIds;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") bool bAvailable = false;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") EHSRAbilityFailureReason DisabledReason = EHSRAbilityFailureReason::None;
};

USTRUCT(BlueprintType)
struct FHSRBattleParticipantView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Participants") FName ParticipantId;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Participants") bool bPlayerTeam = false;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Participants") bool bDefeated = false;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Participants") float Health = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Participants") float MaxHealth = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Participants") float Energy = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Participants") float MaxEnergy = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Participants") float Toughness = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Participants") float MaxToughness = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Participants") TArray<FGameplayTag> WeaknessTags;
};

USTRUCT(BlueprintType)
struct FHSRBattleCommandViewState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") FGuid BattleId;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") FName CurrentActorId;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") TArray<FName> TurnOrderParticipantIds;
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
};
