#pragma once

#include "CoreMinimal.h"
#include "HSREnemyTypes.generated.h"

UENUM(BlueprintType)
enum class EHSREnemyExplorationState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	PatrolWaiting UMETA(DisplayName = "Patrol Waiting"),
	MovingToPatrol UMETA(DisplayName = "Moving to Patrol Point"),
	Alert UMETA(DisplayName = "Alert"),
	Chasing UMETA(DisplayName = "Chasing"),
	EncounterPending UMETA(DisplayName = "Encounter Pending"),
	MoveFailed UMETA(DisplayName = "Move Failed"),
	LostTarget UMETA(DisplayName = "Lost Target")
};
