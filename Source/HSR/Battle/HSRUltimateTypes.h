#pragma once

#include "CoreMinimal.h"
#include "../GAS/Ability/HSRAbilityTypes.h"

enum class EHSRUltimateRequestState : uint8 { Accepted, Executing, Completed, Cancelled };

/** Battle-local values only. Never serialized into the save game. */
struct FHSRUltimateRequestRecord
{
	FHSRBattleActionCommand Command;
	uint64 BattleGeneration = 0;
	uint64 AcceptedSequence = 0;
	EHSRUltimateRequestState State = EHSRUltimateRequestState::Accepted;
	EHSRAbilityFailureReason FailureReason = EHSRAbilityFailureReason::None;
};
