#pragma once

#include "CoreMinimal.h"
#include "HSREquipmentTypes.h"

class UHSRRelicDefinition;
class UHSRRelicSetDefinition;

struct FHSRRelicSetResolution
{
	FName SetSourceId = NAME_None;
	int32 Count = 0;
	bool bActive = false;
};

class HSR_API FHSRRelicSetResolver
{
public:
	// Piece count a set activates at when no other threshold is available.  Mirrors the authored
	// default on UHSRRelicSetDefinition::Threshold.
	static constexpr int32 DefaultThreshold = 2;

	static TMap<FName, FHSRRelicSetResolution> Resolve(
		const FHSREquipmentLoadout& Loadout,
		const TArray<UHSRRelicDefinition*>& RelicDefinitions,
		const TArray<UHSRRelicSetDefinition*>& SetDefinitions);
};
