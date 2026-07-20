#pragma once

#include "CoreMinimal.h"

class UHSRSkillDefinition;
struct FHSRBattleParticipant;

/**
 * Pure, battle-local target policy. Commands carry only stable participant IDs;
 * this policy regenerates candidates and validates submitted IDs against the
 * current runtime participant list.
 */
class HSR_API FHSRTargetingPolicy
{
public:
	static TArray<FName> BuildCandidateTargetIds(
		const UHSRSkillDefinition& SkillDefinition,
		const FHSRBattleParticipant& Actor,
		const TArray<FHSRBattleParticipant>& Participants);

	static bool ValidateTargetIds(
		const UHSRSkillDefinition& SkillDefinition,
		const FHSRBattleParticipant& Actor,
		const TArray<FHSRBattleParticipant>& Participants,
		const TArray<FName>& TargetParticipantIds);
};
