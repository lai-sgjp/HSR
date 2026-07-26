#include "HSRMapArrivalPoint.h"

#include "Components/SceneComponent.h"

AHSRMapArrivalPoint::AHSRMapArrivalPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}
