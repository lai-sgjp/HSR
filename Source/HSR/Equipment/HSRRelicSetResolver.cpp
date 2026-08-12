#include "HSRRelicSetResolver.h"
#include "../Data/Definitions/HSRRelicDefinition.h"
#include "../Data/Definitions/HSRRelicSetDefinition.h"

// 解析一个角色的遗器 Loadout，统计每个遗器套装的装备件数，并依据每套的
// Threshold（激活门槛）判定该套装当前是否处于激活状态。
// 返回的 TMap 以 SetId 为键；bActive 表示「件数 >= 门槛」，SetSourceId 在激活时
// 回填 SetId（用于把套装加成作为独立的 GameplayEffect Source 应用）。
TMap<FName, FHSRRelicSetResolution> FHSRRelicSetResolver::Resolve(
	const FHSREquipmentLoadout& Loadout,
	const TArray<UHSRRelicDefinition*>& RelicDefinitions,
	const TArray<UHSRRelicSetDefinition*>& SetDefinitions)
{
	TMap<FName, FHSRRelicSetResolution> Result;
	TMap<FName, FName> DefinitionToSet;

	// 第一步：建立「遗器定义 ID -> 所属套装 ID」的查表。
	// 只有定义 ID、套装 ID 都非空的遗器才会进入查表；重复的定义 ID 直接忽略，
	// 因为同一 ID 不可能同时属于两套（这是数据错误，宁可少算也不能错算）。
	for (const UHSRRelicDefinition* Definition : RelicDefinitions)
	{
		if (!Definition || Definition->DefinitionId.IsNone() || Definition->SetId.IsNone()
			|| DefinitionToSet.Contains(Definition->DefinitionId))
		{
			continue;
		}
		DefinitionToSet.Add(Definition->DefinitionId, Definition->SetId);
	}

	// 第二步：收集每个套装的激活门槛，并为每个套装预置一个空的解析结果槽。
	// 门槛是每个套装各自配置的数值。这里的历史坑：旧代码只允许门槛恰好为 2，
	// 却在下文比较时硬编码字面量 2，导致策划把某套门槛调高后，此处把它静默丢弃、
	// 而装备子系统却仍按新值生效——同一个改动出现了两套不一致的答案。
	// 现在统一走 SetThresholds 查表，保证两处读到的都是同一个值。
	TMap<FName, int32> SetThresholds;
	for (const UHSRRelicSetDefinition* Set : SetDefinitions)
	{
		if (!Set || Set->SetId.IsNone() || Set->Threshold <= 0 || !Set->SetGameplayEffectClass
			|| Result.Contains(Set->SetId))
		{
			continue;
		}
		SetThresholds.Add(Set->SetId, Set->Threshold);
		Result.Add(Set->SetId, FHSRRelicSetResolution());
	}

	// 第三步：遍历 Loadout 的遗器槽位，按「遗器定义 ID -> 套装 ID」累加各套件数。
	// 未注册到任何套装的遗器自然不计入任何套装。
	for (const TPair<EHSRRelicSlot, FHSREquipmentInstance>& Pair : Loadout.Relics)
	{
		const FName* SetId = DefinitionToSet.Find(Pair.Value.DefinitionId);
		if (SetId)
		{
			if (FHSRRelicSetResolution* Resolution = Result.Find(*SetId))
			{
				++Resolution->Count;
			}
		}
	}

	// 第四步：用每套的门槛判定激活状态；只有达到门槛的套装才会获得有效的 SetSourceId。
	for (TPair<FName, FHSRRelicSetResolution>& Pair : Result)
	{
		Pair.Value.bActive = Pair.Value.Count >= SetThresholds.FindRef(Pair.Key);
		Pair.Value.SetSourceId = Pair.Value.bActive ? Pair.Key : NAME_None;
	}

	return Result;
}
