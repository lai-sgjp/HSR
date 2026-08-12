#include "HSRChallengeDirectoryViewModel.h"

#include "../Challenge/HSRChallengeProgressionSubsystem.h"
#include "../Data/Definitions/HSREncounterDefinition.h"

// Initialize：注入挑战目录的"配置来源"（每个来源携带一个遭遇定义）与进度子系统，
// 并建立 EncounterId -> 定义 的查找表，最后做一次全量刷新。
//
// 数据来源：UHSRChallengeProgressionSubsystem（挑战完成进度）+
//           一组 FHSRChallengeDirectorySource（提供遭遇定义资产）。
// 转换方式：把遭遇定义 + 进度状态整理成 UI 可用的 FHSRChallengeDirectorySnapshot，
//           每个条目附带 可用/完成/锁定/不可用 状态与诊断文案。
// 广播时机：本类不依赖事件广播；进度变化由 Widget 侧监听并调用 Refresh()。
EHSRChallengeDirectoryResult UHSRChallengeDirectoryViewModel::Initialize(
	const TArray<FHSRChallengeDirectorySource>& Sources,
	UHSRChallengeProgressionSubsystem* InProgression)
{
	ConfiguredSources = Sources;
	Progression = InProgression;
	DefinitionsById.Reset();

	// 构建去重的定义查找表：跳过空定义、无 EncounterId 的项以及重复的 EncounterId。
	for (const FHSRChallengeDirectorySource& Source : ConfiguredSources)
	{
		UHSREncounterDefinition* Definition = Source.Definition;
		if (!Definition || Definition->EncounterId.IsNone() || DefinitionsById.Contains(Definition->EncounterId))
		{
			continue;
		}

		DefinitionsById.Add(Definition->EncounterId, Definition);
	}

	return Refresh();
}

// Refresh：根据当前配置与进度，重建整个目录快照并返回整体结果。
// 每个挑战条目按以下优先级判定状态：
//   1) 定义不完整 / 前置定义无效 -> Unavailable（带诊断文案）；
//   2) 已完成 -> Completed；
//   3) 前置尚未全部完成 -> Locked；
//   4) 其余 -> Available，并计入 AvailableIds 供选择校验使用。
EHSRChallengeDirectoryResult UHSRChallengeDirectoryViewModel::Refresh()
{
	Snapshot = FHSRChallengeDirectorySnapshot();
	AvailableIds.Reset();
	TSet<FName> ProjectedIds;

	for (const FHSRChallengeDirectorySource& Source : ConfiguredSources)
	{
		UHSREncounterDefinition* Definition = Source.Definition;
		if (!Definition || Definition->EncounterId.IsNone() || ProjectedIds.Contains(Definition->EncounterId))
		{
			continue;
		}
		ProjectedIds.Add(Definition->EncounterId);

		// 用遭遇定义填充条目：遭遇 ID、敌人定义 ID、战斗地图路径。
		FHSRChallengeDirectoryEntry& Entry = Snapshot.Entries.AddDefaulted_GetRef();
		Entry.EncounterId = Definition->EncounterId;
		Entry.EnemyDefinitionId = Definition->EnemyDefinitionId;
		Entry.BattleMapPath = Definition->BattleMap.IsNull()
			? NAME_None : FName(*Definition->BattleMap.GetLongPackageName());
		const bool bDefinitionValid = !Entry.EnemyDefinitionId.IsNone() && !Entry.BattleMapPath.IsNone();
		bool bPrerequisitesValid = true;
		bool bPrerequisitesComplete = true;
		if (bDefinitionValid)
		{
			// 校验所有前置挑战：前置定义必须存在且本身配置完整（敌人定义 + 地图齐全）。
			for (const FName PrerequisiteId : Definition->PrerequisiteEncounterIds)
			{
				const TObjectPtr<UHSREncounterDefinition>* Prerequisite = DefinitionsById.Find(PrerequisiteId);
				if (PrerequisiteId.IsNone() || !Prerequisite || !Prerequisite->Get()
					|| (*Prerequisite)->EnemyDefinitionId.IsNone() || (*Prerequisite)->BattleMap.IsNull())
				{
					bPrerequisitesValid = false;
					break;
				}
				// 前置未全部完成时，该挑战处于 Locked（但前置定义仍有效）。
				if (!Progression.IsValid() || !Progression->IsCompleted(PrerequisiteId))
				{
					bPrerequisitesComplete = false;
				}
			}
		}

		if (!bDefinitionValid || !bPrerequisitesValid)
		{
			Entry.Status = EHSRChallengeDirectoryStatus::Unavailable;
			Entry.Diagnostic = FText::FromString(TEXT("Challenge is unavailable."));
		}
		else if (Progression.IsValid() && Progression->IsCompleted(Entry.EncounterId))
		{
			Entry.Status = EHSRChallengeDirectoryStatus::Completed;
			Entry.Diagnostic = FText::FromString(TEXT("Challenge is completed."));
		}
		else if (!bPrerequisitesComplete)
		{
			Entry.Status = EHSRChallengeDirectoryStatus::Locked;
			Entry.Diagnostic = FText::FromString(TEXT("Challenge prerequisites are incomplete."));
		}
		else
		{
			Entry.Status = EHSRChallengeDirectoryStatus::Available;
			Entry.Diagnostic = FText::GetEmpty();
			AvailableIds.Add(Entry.EncounterId);
		}
		// 派生三个供 UI 直接使用的布尔标记，避免蓝图里反复比较枚举。
		Entry.bCompleted = Entry.Status == EHSRChallengeDirectoryStatus::Completed;
		Entry.bUnlocked = Entry.Status == EHSRChallengeDirectoryStatus::Available;
		Entry.bAvailable = Entry.Status == EHSRChallengeDirectoryStatus::Available;
	}

	// 按 EncounterId 字典序排序，保证目录显示顺序稳定可预期。
	Snapshot.Entries.Sort([](const FHSRChallengeDirectoryEntry& A, const FHSRChallengeDirectoryEntry& B)
	{
		return A.EncounterId.LexicalLess(B.EncounterId);
	});
	return Snapshot.Entries.IsEmpty() ? EHSRChallengeDirectoryResult::EmptyDirectory
		: EHSRChallengeDirectoryResult::Success;
}

// ResolveSelection：校验用户对某个挑战的选择是否合法，并输出对应的遭遇定义。
// 校验顺序：定义是否存在 -> 是否可用（在 AvailableIds 中）-> 若锁定/已完成则返回对应错误。
// 只有真正 Available 的挑战才会被授予定义，防止绕过前置条件直接开战。
EHSRChallengeDirectoryResult UHSRChallengeDirectoryViewModel::ResolveSelection(
	const FName EncounterId, UHSREncounterDefinition*& OutDefinition) const
{
	OutDefinition = nullptr;
	const TObjectPtr<UHSREncounterDefinition>* Found = DefinitionsById.Find(EncounterId);
	if (!Found)
	{
		return EHSRChallengeDirectoryResult::UnknownChallenge;
	}
	if (!AvailableIds.Contains(EncounterId))
	{
		// 不在可用列表里：回查快照条目以区分"锁定/已完成/定义损坏"三种情况，给出精确错误。
		const FHSRChallengeDirectoryEntry* Entry = Snapshot.Entries.FindByPredicate(
			[EncounterId](const FHSRChallengeDirectoryEntry& Candidate)
			{
				return Candidate.EncounterId == EncounterId;
			});
		if (!Entry)
		{
			return EHSRChallengeDirectoryResult::InvalidDefinition;
		}
		if (Entry->Status == EHSRChallengeDirectoryStatus::Locked)
		{
			return EHSRChallengeDirectoryResult::Locked;
		}
		if (Entry->Status == EHSRChallengeDirectoryStatus::Completed)
		{
			return EHSRChallengeDirectoryResult::Completed;
		}
		return EHSRChallengeDirectoryResult::InvalidDefinition;
	}
	OutDefinition = Found->Get();
	return EHSRChallengeDirectoryResult::Success;
}
