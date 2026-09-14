#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "../HSRBreakTypes.h"
#include "HSREnemyDefinition.generated.h"

class UHSREncounterDefinition;
class UBehaviorTree;
class UBlackboardData;
class UStaticMesh;

UCLASS(BlueprintType)
class HSR_API UHSREnemyDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Elite single-target attacks favor wounded opponents; ordinary enemies are uniform. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Tactics")
	bool bPreferWoundedTargets = false;
	/** Opt-in combat values; legacy enemy assets continue using their initialization effect. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Stats") bool bUseAuthoredBaseStats = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Stats", meta=(EditCondition="bUseAuthoredBaseStats", ClampMin="1.0")) float BaseMaxHealth = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Stats", meta=(EditCondition="bUseAuthoredBaseStats", ClampMin="0.0")) float BaseAttack = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Stats", meta=(EditCondition="bUseAuthoredBaseStats", ClampMin="0.0")) float BaseDefense = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Stats", meta=(EditCondition="bUseAuthoredBaseStats", ClampMin="0.001")) float BaseSpeed = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Presentation") TSoftObjectPtr<UStaticMesh> BattleMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Presentation", meta=(ClampMin="0.1")) float BattleMeshScale=1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy|Formation", meta=(ClampMin="1",ClampMax="5")) int32 FormationCount=1;
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
