#pragma once
#include "CoreMinimal.h"

class UWorld;

struct FHSRWorldMarker
{
	FName Id;
	FVector Location = FVector::ZeroVector;
	FText Label;
	bool bQuest = false;
};

/** Shared read-only projection for map, minimap and world guidance. */
namespace HSRWorldMarkers
{
	HSR_API TArray<FHSRWorldMarker> Collect(UWorld* World);
}
