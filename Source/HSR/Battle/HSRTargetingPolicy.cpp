#include "HSRTargetingPolicy.h"

#include "HSRBattleParticipant.h"
#include "../Data/HSRSkillDefinition.h"

TArray<FName> FHSRTargetingPolicy::BuildCandidateTargetIds(
	const UHSRSkillDefinition& SkillDefinition,
	const FHSRBattleParticipant& Actor,
	const TArray<FHSRBattleParticipant>& Participants)
{
	TArray<FName> CandidateIds;
	if (!Actor.IsValid() || Actor.bDefeated)
	{
		return CandidateIds;
	}

	for (const FHSRBattleParticipant& Candidate : Participants)
	{
		if (!Candidate.IsValid() || Candidate.bDefeated)
		{
			continue;
		}

		switch (SkillDefinition.TargetType)
		{
		case EHSRTargetType::SingleEnemy:
			if (Candidate.Team != Actor.Team)
			{
				CandidateIds.Add(Candidate.ParticipantId);
			}
			break;
		case EHSRTargetType::SingleAlly:
			if (Candidate.Team == Actor.Team && Candidate.ParticipantId != Actor.ParticipantId)
			{
				CandidateIds.Add(Candidate.ParticipantId);
			}
			break;
		case EHSRTargetType::Self:
			if (Candidate.ParticipantId == Actor.ParticipantId)
			{
				CandidateIds.Add(Candidate.ParticipantId);
			}
			break;
		default:
			break;
		}
	}

	return CandidateIds;
}

bool FHSRTargetingPolicy::ValidateTargetIds(
	const UHSRSkillDefinition& SkillDefinition,
	const FHSRBattleParticipant& Actor,
	const TArray<FHSRBattleParticipant>& Participants,
	const TArray<FName>& TargetParticipantIds)
{
	if (TargetParticipantIds.Num() != 1 || TargetParticipantIds[0].IsNone())
	{
		return false;
	}

	return BuildCandidateTargetIds(SkillDefinition, Actor, Participants).Contains(TargetParticipantIds[0]);
}
