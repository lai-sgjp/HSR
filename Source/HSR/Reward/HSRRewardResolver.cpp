#include "HSRRewardResolver.h"

// 把一条奖励定义（固定物品 + 可选掉落表）解析成最终发放的物品清单。
// 数据流：固定物品直接计入；若定义声明了 DropRolls>0，则必须提供掉落表，并按
// 权重做多次随机抽取（随机种子 Seed 保证同一奖励在同一 Seed 下可复现，这是
// 存档/结算幂等性的基础）。结果按 ItemId 字典序去重合并并排序后输出。
bool FHSRRewardResolver::Resolve(const FHSRRewardDefinitionRule& Reward, const FHSRDropTableRule* DropTable, int32 Seed, TArray<FHSRRewardItemEntry>& OutItems)
{
	// 定义级校验：掉落次数不能超上限，固定物品条数与掉落表条数也不能超上限。
	if (Reward.DropRolls < 0 || Reward.DropRolls > MaxDropRolls
		|| Reward.FixedItems.Num() > MaxDefinitionEntries
		|| (DropTable && DropTable->Entries.Num() > MaxDefinitionEntries))
	{
		return false;
	}

	// 用一个 TMap<ItemId, Quantity> 把固定物品与掉落结果合并，随后整体去重排序。
	TMap<FName, int32> Quantities;
	// AddQuantity：向合并表累加数量。拒绝空 ID / 非正数量 / int32 溢出 / 超过去重后
	// 的物品种类上限（MaxResolvedGrants）。返回 false 表示本次解析失败。
	auto AddQuantity = [&Quantities](FName ItemId, int32 Quantity)
	{
		if (ItemId.IsNone() || Quantity <= 0)
		{
			return false;
		}
		const int32 Existing = Quantities.FindRef(ItemId);
		if (Quantity > MAX_int32 - Existing)
		{
			return false;
		}
		if (Existing == 0 && Quantities.Num() >= FHSRRewardResolver::MaxResolvedGrants)
		{
			return false;
		}
		Quantities.Add(ItemId, Existing + Quantity);
		return true;
	};

	// 固定物品：原样累加。
	for (const FHSRRewardItemEntry& Entry : Reward.FixedItems)
	{
		if (!AddQuantity(Entry.ItemId, Entry.Quantity))
		{
			return false;
		}
	}

	// 掉落表抽取：只有声明了 DropRolls>0 才需要掉落表；没有声明却缺表不会报错。
	if (Reward.DropRolls > 0)
	{
		if (!DropTable || DropTable->Entries.IsEmpty())
		{
			return false;
		}

		// 先把掉落表条目按 ItemId 排序：保证同一掉落表在不同写入顺序下产生相同的
		// 权重累计顺序，从而让同一 Seed 的随机结果完全可复现（存档校验的基石）。
		TArray<FHSRDropTableEntry> SortedEntries = DropTable->Entries;
		SortedEntries.Sort([](const FHSRDropTableEntry& A, const FHSRDropTableEntry& B)
		{
			return A.ItemId.LexicalLess(B.ItemId);
		});

		// 校验每条掉落并累加总权重；随后用 FRandomStream 按权重区间抽取。
		int32 TotalWeight = 0;
		for (const FHSRDropTableEntry& Entry : SortedEntries)
		{
			if (Entry.ItemId.IsNone() || Entry.MinQuantity <= 0 || Entry.MaxQuantity < Entry.MinQuantity
				|| Entry.Weight <= 0 || Entry.Weight > MAX_int32 - TotalWeight)
			{
				return false;
			}
			TotalWeight += Entry.Weight;
		}

		FRandomStream Stream(Seed);
		for (int32 RollIndex = 0; RollIndex < Reward.DropRolls; ++RollIndex)
		{
			const int32 Roll = Stream.RandRange(1, TotalWeight);
			int32 Accumulated = 0;
			const FHSRDropTableEntry* Selected = nullptr;
			for (const FHSRDropTableEntry& Entry : SortedEntries)
			{
				Accumulated += Entry.Weight;
				if (Roll <= Accumulated)
				{
					Selected = &Entry;
					break;
				}
			}
			// 命中后随机取一个数量并累加；任何一步失败都让整次解析失败。
			if (!Selected || !AddQuantity(Selected->ItemId, Stream.RandRange(Selected->MinQuantity, Selected->MaxQuantity)))
			{
				return false;
			}
		}
	}

	// 把合并表转成最终输出，按 ItemId 排序；空结果视为解析失败。
	OutItems.Reset();
	for (const TPair<FName, int32>& Entry : Quantities)
	{
		OutItems.Add({Entry.Key, Entry.Value});
	}
	OutItems.Sort([](const FHSRRewardItemEntry& A, const FHSRRewardItemEntry& B)
	{
		return A.ItemId.LexicalLess(B.ItemId);
	});
	return !OutItems.IsEmpty();
}
