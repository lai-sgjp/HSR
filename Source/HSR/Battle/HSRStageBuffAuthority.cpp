#include "HSRStageBuffAuthority.h"

#include "../Data/Definitions/HSRStageBuffDefinition.h"
#include "GameplayEffect.h"

// 为指定遭遇注册一组关卡 Buff 定义：
// 校验每个定义（BuffId 非空、无重复、有效果 GE、资源成本合法），
// 然后整体替换该遭遇的注册表。失败时不做任何写入。
bool UHSRStageBuffAuthority::RegisterEncounterBuffs(FName EncounterId,
	const TArray<UHSRStageBuffDefinition*>& Definitions)
{
	if (EncounterId.IsNone())
	{
		return false;
	}

	// 先逐项校验并收集候选。
	TArray<TObjectPtr<UHSRStageBuffDefinition>> Candidate;
	TSet<FName> Seen;
	for (UHSRStageBuffDefinition* Definition : Definitions)
	{
		// BuffId 非空、不重复、且必须有效果 GE。
		if (!Definition || Definition->BuffId.IsNone() || Seen.Contains(Definition->BuffId)
			|| !Definition->GameplayEffectClass)
		{
			return false;
		}
		Seen.Add(Definition->BuffId);
		// 资源成本必须合法：非负；有成本就必须有资源物品 ID。
		if (Definition->ResourceCost < 0
			|| (Definition->ResourceCost > 0 && Definition->ResourceItemId.IsNone()))
		{
			return false;
		}
		Candidate.Add(Definition);
	}

	// 已有该遭遇的注册表则替换，否则新建。
	FHSRStageBuffEncounterRegistry* Existing = EncounterRegistries.FindByPredicate(
		[EncounterId](const FHSRStageBuffEncounterRegistry& Entry)
		{
			return Entry.EncounterId == EncounterId;
		});
	if (Existing)
	{
		Existing->Definitions = MoveTemp(Candidate);
	}
	else
	{
		FHSRStageBuffEncounterRegistry& Entry = EncounterRegistries.AddDefaulted_GetRef();
		Entry.EncounterId = EncounterId;
		Entry.Definitions = MoveTemp(Candidate);
	}
	return true;
}

// 校验一组 BuffId 对给定遭遇是否全部有效且不重复。
bool UHSRStageBuffAuthority::ValidateBuffIds(FName EncounterId, const TArray<FName>& BuffIds) const
{
	if (BuffIds.IsEmpty())
	{
		return true;
	}

	// 遭遇必须已注册。
	const FHSRStageBuffEncounterRegistry* Registry = EncounterRegistries.FindByPredicate(
		[EncounterId](const FHSRStageBuffEncounterRegistry& Entry)
		{
			return Entry.EncounterId == EncounterId;
		});
	if (!Registry)
	{
		return false;
	}

	// 逐个校验：非空、不重复、且能在注册表里找到“启用 + 有效果”的定义。
	TSet<FName> Seen;
	for (const FName BuffId : BuffIds)
	{
		if (BuffId.IsNone() || Seen.Contains(BuffId))
		{
			return false;
		}
		Seen.Add(BuffId);
		const TObjectPtr<UHSRStageBuffDefinition>* Found = Registry->Definitions.FindByPredicate(
			[BuffId](const TObjectPtr<UHSRStageBuffDefinition>& Definition)
			{
				return Definition && Definition->BuffId == BuffId && Definition->bEnabled
					&& Definition->GameplayEffectClass;
			});
		if (!Found)
		{
			return false;
		}
	}
	return true;
}

// 按遭遇 + BuffId 查找定义；找不到返回空。
const UHSRStageBuffDefinition* UHSRStageBuffAuthority::FindBuff(FName EncounterId, FName BuffId) const
{
	const FHSRStageBuffEncounterRegistry* Registry = EncounterRegistries.FindByPredicate(
		[EncounterId](const FHSRStageBuffEncounterRegistry& Entry)
		{
			return Entry.EncounterId == EncounterId;
		});
	if (!Registry)
	{
		return nullptr;
	}
	const TObjectPtr<UHSRStageBuffDefinition>* Found = Registry->Definitions.FindByPredicate(
		[BuffId](const TObjectPtr<UHSRStageBuffDefinition>& Definition)
		{
			return Definition && Definition->BuffId == BuffId;
		});
	return Found ? Found->Get() : nullptr;
}

// 清空所有遭遇的 Buff 注册表。
void UHSRStageBuffAuthority::Reset()
{
	EncounterRegistries.Reset();
}
