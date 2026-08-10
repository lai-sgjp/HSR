#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "../HSRBreakTypes.h"
#include "HSREnemyDefinition.generated.h"

class UHSREncounterDefinition;
class UBehaviorTree;
class UBlackboardData;

UCLASS(BlueprintType)
class HSR_API UHSREnemyDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|AI")
	TSoftObjectPtr<UBehaviorTree> BehaviorTreeAsset = TSoftObjectPtr<UBehaviorTree>(FSoftObjectPath(TEXT("/Game/AI/Enemy/BT_HSREnemy_Exploration.BT_HSREnemy_Exploration")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|AI")
	TSoftObjectPtr<UBlackboardData> BlackboardAsset = TSoftObjectPtr<UBlackboardData>(FSoftObjectPath(TEXT("/Game/AI/Enemy/BB_HSREnemy_Exploration.BB_HSREnemy_Exploration")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	FName EnemyDefinitionId;

	/**
	 * Player-facing name. Leave empty and the UI falls back to the participant id, which is how
	 * every existing asset behaves today -- authoring this is opt-in, not a migration.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	TSoftObjectPtr<UTexture2D> Portrait;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	TObjectPtr<UHSREncounterDefinition> EncounterDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Element")
	FGameplayTagContainer WeaknessTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Toughness", meta = (ClampMin = "0.000001", ClampMax = "100000.0"))
	float InitialToughness = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Toughness", meta = (ClampMin = "0.000001", ClampMax = "100000.0"))
	float InitialMaxToughness = 1.0f;

	EHSRElementToughnessContractResult GetElementToughnessContractResult() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Patrol", meta = (ClampMin = "0.0", UIMin = "100.0"))
	float PatrolRadius = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Patrol", meta = (ClampMin = "0.0"))
	float PatrolWaitTime = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Chase", meta = (ClampMin = "0.0", UIMin = "10.0"))
	float ChaseAcceptanceRadius = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Perception", meta = (ClampMin = "0.0", UIMin = "100.0"))
	float SightRadius = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Perception", meta = (ClampMin = "0.0", UIMin = "100.0"))
	float LoseSightRadius = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Encounter", meta = (ClampMin = "0.0", UIMin = "10.0"))
	float EncounterRadius = 200.0f;
};
