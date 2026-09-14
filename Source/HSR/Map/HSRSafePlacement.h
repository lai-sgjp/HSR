#pragma once
#include "CoreMinimal.h"
class APawn;
/** Opt-in rebuilt worlds validate saved locations against real floor/capsule geometry. */
namespace HSRSafePlacement
{
    /** Fallback must match the map's default arrival uniquely. Outputs change only on success. */
    bool Resolve(APawn* Pawn, const FTransform& Requested, FTransform& Out,
        FName FallbackArrivalId = NAME_None, FName* OutFallbackArrivalId = nullptr);
}
