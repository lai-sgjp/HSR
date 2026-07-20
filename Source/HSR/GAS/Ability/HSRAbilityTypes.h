#pragma once

#include "CoreMinimal.h"
#include "../Damage/HSRDamageTypes.h"
#include "HSRAbilityTypes.generated.h"

UENUM(BlueprintType)
enum class EHSRSkillCategory : uint8
{
	BasicAttack,
	Skill,
	Ultimate,
	Heal
};

UENUM(BlueprintType)
enum class EHSRTargetType : uint8
{
	SingleEnemy,
	SingleAlly,
	Self
};

UENUM(BlueprintType)
enum class EHSRAbilityResolutionStatus : uint8
{
	Succeeded,
	Rejected,
	Cancelled,
	Failed
};

UENUM(BlueprintType)
enum class EHSRAbilityFailureReason : uint8
{
	None,
	InvalidBattle,
	DuplicateAction,
	NotCurrentActor,
	DefinitionMissing,
	AbilityUnavailable,
	InvalidTarget,
	AlreadyAtFullHealth,
	InsufficientSkillPoint,
	InsufficientEnergy,
	CommitFailed,
	EffectFailed
};

USTRUCT(BlueprintType)
struct FHSRBattleActionCommand
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Battle")
	FGuid ActionId;

	UPROPERTY(BlueprintReadWrite, Category = "Battle")
	FGuid BattleId;

	UPROPERTY(BlueprintReadWrite, Category = "Battle")
	FName ActorParticipantId;

	UPROPERTY(BlueprintReadWrite, Category = "Battle")
	FName SkillId;

	UPROPERTY(BlueprintReadWrite, Category = "Battle")
	TArray<FName> TargetParticipantIds;
};

USTRUCT(BlueprintType)
struct FHSRAbilityResolution
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	FGuid ActionId;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	EHSRAbilityResolutionStatus Status = EHSRAbilityResolutionStatus::Rejected;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	EHSRAbilityFailureReason FailureReason = EHSRAbilityFailureReason::None;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	FName ActorParticipantId;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	FName SkillId;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	bool bHasDamageResult = false;

	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	FHSRDamageResult DamageResult;

	bool Succeeded() const { return Status == EHSRAbilityResolutionStatus::Succeeded; }
};
