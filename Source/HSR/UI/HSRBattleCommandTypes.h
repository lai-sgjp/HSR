#pragma once

#include "CoreMinimal.h"
#include "../GAS/Ability/HSRAbilityTypes.h"
#include "../Status/HSRStatusTypes.h"
#include "HSRBattleCommandTypes.generated.h"

USTRUCT(BlueprintType)
struct FHSRBattleCommandSkillView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") FName SkillId;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") EHSRSkillCategory Category = EHSRSkillCategory::BasicAttack;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") EHSRTargetType TargetType = EHSRTargetType::SingleEnemy;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") TArray<FName> CandidateTargetIds;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") bool bAvailable = false;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") EHSRAbilityFailureReason DisabledReason = EHSRAbilityFailureReason::None;
};

USTRUCT(BlueprintType)
struct FHSRBattleCommandViewState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") FGuid BattleId;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") FName CurrentActorId;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") int32 SkillPoints = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") int32 MaxSkillPoints = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") float Energy = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") float MaxEnergy = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") TArray<FHSRBattleCommandSkillView> Skills;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") FHSRAbilityResolution LastResolution;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Status") TArray<FHSRStatusPublicSnapshot> Statuses;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Status") FHSRStatusPublicOperationEvent LastStatusOperation;
};
