#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HSREnemyCatalog.generated.h"

class UHSREnemyDefinition;

/**
 * Authored catalog of enemy definitions keyed by EnemyDefinitionId. The battle coordinator
 * resolves the enemy for an encounter from this catalog instead of a single GameMode-authored
 * reference, so multiple encounters (Inspector, Boss) can each field their own enemy.
 */
UCLASS(BlueprintType)
class HSR_API UHSREnemyCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HSR|Enemy")
	TArray<TObjectPtr<UHSREnemyDefinition>> Enemies;
};
