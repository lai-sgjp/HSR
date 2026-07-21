#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "../HSRBreakTypes.h"
#include "HSREnemyDefinition.generated.h"

class UHSREncounterDefinition;

UCLASS(BlueprintType)
class HSR_API UHSREnemyDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	FName EnemyDefinitionId;

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
};
