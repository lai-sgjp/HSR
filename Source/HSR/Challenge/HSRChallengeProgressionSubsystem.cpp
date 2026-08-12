#include "HSRChallengeProgressionSubsystem.h"

namespace
{
	// 名字比较：转小写后按字典序比较，保证挑战 ID 排序稳定且大小写不敏感。
	bool NameLess(const FName& A, const FName& B)
	{
		return A.ToString().ToLower().Compare(B.ToString().ToLower(), ESearchCase::CaseSensitive) < 0;
	}
}

// 重建“已通关遭遇”快照：把内部集合转成排序后的数组。
// 集合遍历顺序不稳定，而快照要用于存档对比/序列化，所以必须统一排序。
void UHSRChallengeProgressionSubsystem::RebuildSnapshot()
{
	Snapshot.CompletedEncounterIds.Reset(CompletedEncounterIds.Num());
	for (const FName& EncounterId : CompletedEncounterIds)
	{
		Snapshot.CompletedEncounterIds.Add(EncounterId);
	}
	Snapshot.CompletedEncounterIds.Sort(NameLess);
}

// 标记一次遭遇通关。重复通关同一遭遇视为 NoOp。
EHSRChallengeProgressionResult UHSRChallengeProgressionSubsystem::CompleteEncounter(const FName EncounterId)
{
	if (!IsValidEncounterId(EncounterId))
	{
		return EHSRChallengeProgressionResult::InvalidEncounterId;
	}
	if (CompletedEncounterIds.Contains(EncounterId))
	{
		return EHSRChallengeProgressionResult::NoOp;
	}

	CompletedEncounterIds.Add(EncounterId);
	++Snapshot.Revision;
	RebuildSnapshot();
	ProgressionChanged.Broadcast(Snapshot);
	return EHSRChallengeProgressionResult::Success;
}

// 查询某遭遇是否已通关。
bool UHSRChallengeProgressionSubsystem::IsCompleted(const FName EncounterId) const
{
	return CompletedEncounterIds.Contains(EncounterId);
}

// 导出存档数据：排序后的通关列表 + 修订号。
void UHSRChallengeProgressionSubsystem::ExportSaveData(FHSRChallengeProgressionSaveData& OutData) const
{
	OutData.CompletedEncounterIds = Snapshot.CompletedEncounterIds;
	OutData.Revision = Snapshot.Revision;
}

// 校验存档数据的合法性：修订号非负，通关 ID 非空且不重复。
bool UHSRChallengeProgressionSubsystem::ValidateSaveData(const FHSRChallengeProgressionSaveData& Data)
{
	if (Data.Revision < 0)
	{
		return false;
	}

	TSet<FName> Seen;
	for (const FName EncounterId : Data.CompletedEncounterIds)
	{
		if (EncounterId.IsNone() || Seen.Contains(EncounterId))
		{
			return false;
		}
		Seen.Add(EncounterId);
	}
	return true;
}

// 准备恢复候选：先校验存档，再把通关列表按统一规则排序后作为候选返回。
bool UHSRChallengeProgressionSubsystem::PrepareRestore(
	const FHSRChallengeProgressionSaveData& Saved,
	FHSRChallengeProgressionSaveData& OutCandidate) const
{
	if (!ValidateSaveData(Saved))
	{
		return false;
	}
	OutCandidate = Saved;
	OutCandidate.CompletedEncounterIds.Sort(NameLess);
	return true;
}

// 判断恢复候选与当前快照是否不同（集合比较与顺序无关）。
bool UHSRChallengeProgressionSubsystem::IsRestoreDifferent(
	const FHSRChallengeProgressionSaveData& Candidate) const
{
	if (Snapshot.Revision != Candidate.Revision
		|| Snapshot.CompletedEncounterIds.Num() != Candidate.CompletedEncounterIds.Num())
	{
		return true;
	}

	TArray<FName> SortedCandidate = Candidate.CompletedEncounterIds;
	SortedCandidate.Sort(NameLess);
	return Snapshot.CompletedEncounterIds != SortedCandidate;
}

// 提交恢复：用候选数据重建内部集合与快照，可选广播变更。
void UHSRChallengeProgressionSubsystem::CommitRestore(
	FHSRChallengeProgressionSaveData&& Candidate, const bool bNotify)
{
	CompletedEncounterIds.Reset();
	for (const FName EncounterId : Candidate.CompletedEncounterIds)
	{
		CompletedEncounterIds.Add(EncounterId);
	}
	Snapshot.Revision = Candidate.Revision;
	RebuildSnapshot();
	if (bNotify)
	{
		ProgressionChanged.Broadcast(Snapshot);
	}
}
