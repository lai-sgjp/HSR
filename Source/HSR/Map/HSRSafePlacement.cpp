#include "HSRSafePlacement.h"
#include "HSRMapArrivalPoint.h"
#include "../Exploration/HSRSceneContent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "EngineUtils.h"

bool HSRSafePlacement::Resolve(APawn* Pawn, const FTransform& Requested, FTransform& Out,
    const FName FallbackArrivalId, FName* OutFallbackArrivalId)
{
    if (!IsValid(Pawn) || Requested.ContainsNaN()) return false;
    UWorld* World = Pawn->GetWorld();
    if (!World) return false;
    if (!TActorIterator<AHSRSceneContent>(World))
    {
        Out = Requested; // Historical regression maps preserve their original placement contract.
        if (OutFallbackArrivalId) *OutFallbackArrivalId = NAME_None;
        return true;
    }
    const auto* Capsule = Pawn->FindComponentByClass<UCapsuleComponent>();
    const auto* Movement = Pawn->FindComponentByClass<UCharacterMovementComponent>();
    const float Half = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.f;
    const float Radius = Capsule ? Capsule->GetScaledCapsuleRadius() : 34.f;
    const float FloorTolerance = Movement ? FMath::Max(5.f, Movement->MaxStepHeight) : 45.f;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(HSRSafePlacement), false, Pawn);
    const auto Walkable = [Movement](const FHitResult& Hit)
    {
        return Hit.bBlockingHit && !Hit.bStartPenetrating
            && (Movement ? Movement->IsWalkable(Hit) : Hit.ImpactNormal.Z >= 0.707f);
    };
    const auto Fit = [&](const FTransform& Candidate, FTransform& Result)
    {
        if (Candidate.ContainsNaN()) return false;
        FHitResult Hit;
        const FVector Location = Candidate.GetLocation();
        // Use pawn collision: visibility-only decoration must not count as supporting ground.
        if (!World->LineTraceSingleByChannel(Hit, Location + FVector(0, 0, 5),
            Location - FVector(0, 0, Half + FloorTolerance), ECC_Pawn, Params) || !Walkable(Hit)) return false;
        const FVector Centre = Hit.ImpactPoint + FVector(0, 0, Half + 3);
        if (FMath::Abs(Centre.Z - Location.Z) > FloorTolerance
            || World->OverlapBlockingTestByChannel(Centre, FQuat::Identity, ECC_Pawn,
                FCollisionShape::MakeCapsule(Radius, Half), Params)) return false;
        // Check the footprint, not just one ray on a floor sliver outside a remodeled boundary.
        const FVector Offsets[] = {FVector(Radius * .75f, 0, 0), FVector(-Radius * .75f, 0, 0),
            FVector(0, Radius * .75f, 0), FVector(0, -Radius * .75f, 0)};
        for (const FVector& Offset : Offsets)
        {
            const FVector Support = Hit.ImpactPoint + Offset;
            FHitResult Edge;
            if (!World->LineTraceSingleByChannel(Edge, Support + FVector(0, 0, FloorTolerance),
                Support - FVector(0, 0, FloorTolerance), ECC_Pawn, Params) || !Walkable(Edge)) return false;
        }
        // Saved cosmetic scale/tilt must not invalidate the capsule size checked above.
        Result = FTransform(FRotator(0, Candidate.Rotator().Yaw, 0), Centre, Pawn->GetActorScale3D());
        return true;
    };
    FTransform Resolved;
    if (Fit(Requested, Resolved))
    {
        Out = Resolved;
        if (OutFallbackArrivalId) *OutFallbackArrivalId = NAME_None;
        return true;
    }
    AHSRMapArrivalPoint* Fallback = nullptr;
    for (TActorIterator<AHSRMapArrivalPoint> It(World); It; ++It)
    {
        if (!FallbackArrivalId.IsNone() && It->ArrivalId != FallbackArrivalId) continue;
        if (Fallback) return false;
        Fallback = *It;
    }
    if (!Fallback || !Fit(Fallback->GetActorTransform(), Resolved)) return false;
    Out = Resolved;
    if (OutFallbackArrivalId) *OutFallbackArrivalId = Fallback->ArrivalId;
    return true;
}
