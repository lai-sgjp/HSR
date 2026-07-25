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
	static TMap<FName, FHSRRelicSetResolution> Resolve(
		const FHSREquipmentLoadout& Loadout,
		const TArray<UHSRRelicDefinition*>& RelicDefinitions,
		const TArray<UHSRRelicSetDefinition*>& SetDefinitions);
};
