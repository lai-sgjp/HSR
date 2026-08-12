#include "HSRTargetingPolicy.h"

#include "HSRBattleParticipant.h"
#include "../Data/HSRSkillDefinition.h"

// 按技能的“目标类型”从存活参与者中构建候选目标 ID 列表。
// 这是纯静态策略：不持有状态，只根据技能定义 + 行动者 + 参与者集合计算。
TArray<FName> FHSRTargetingPolicy::BuildCandidateTargetIds(
	const UHSRSkillDefinition& SkillDefinition,
	const FHSRBattleParticipant& Actor,
	const TArray<FHSRBattleParticipant>& Participants)
{
	TArray<FName> CandidateIds;
	// 行动者已败则没有候选目标。
	if (!Actor.IsAlive())
	{
		return CandidateIds;
	}

	for (const FHSRBattleParticipant& Candidate : Participants)
	{
		// 已败的候选不能作为目标。
		if (!Candidate.IsAlive())
		{
			continue;
		}

		// 按目标类型过滤：
		// SingleEnemy  → 与行动者不同阵营的存活者；
		// SingleAlly   → 与行动者同阵营但不包括自己的存活者；
		// Self         → 只有行动者自己。
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

// 校验提交的目标 ID 是否合法：必须恰好一个、非空，且出现在候选目标列表里。
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

