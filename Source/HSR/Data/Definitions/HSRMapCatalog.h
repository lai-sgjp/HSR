#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HSRMapCatalog.generated.h"

class UHSRMapDefinition;
class UHSRTeleportDefinition;

/**
 * Authoring catalog for the exploration map graph. GameMode bootstrap registers every
 * contained map and teleport into UHSRMapSubsystem once at spawn, so map identity and
 * travel links are data-driven instead of hardcoded in level blueprints.
 */
UCLASS(BlueprintType)
class HSR_API UHSRMapCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HSR|Map")
	TArray<TObjectPtr<UHSRMapDefinition>> Maps;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HSR|Map")
	TArray<TObjectPtr<UHSRTeleportDefinition>> Teleports;
};
