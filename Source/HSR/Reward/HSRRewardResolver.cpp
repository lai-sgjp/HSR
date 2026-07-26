#include "HSRRewardResolver.h"

bool FHSRRewardResolver::Resolve(const FHSRRewardDefinitionRule& Reward, const FHSRDropTableRule* DropTable, int32 Seed, TArray<FHSRRewardItemEntry>& OutItems)
{
	if (Reward.DropRolls < 0 || Reward.DropRolls > MaxDropRolls || Reward.FixedItems.Num() > MaxDefinitionEntries || (DropTable && DropTable->Entries.Num() > MaxDefinitionEntries))
	{
		return false;
	}
	TMap<FName, int32> Quantities;
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

	for (const FHSRRewardItemEntry& Entry : Reward.FixedItems)
	{
		if (!AddQuantity(Entry.ItemId, Entry.Quantity))
		{
			return false;
		}
	}

	if (Reward.DropRolls > 0)
	{
		if (!DropTable || DropTable->Entries.IsEmpty())
		{
			return false;
		}
		TArray<FHSRDropTableEntry> SortedEntries = DropTable->Entries;
		SortedEntries.Sort([](const FHSRDropTableEntry& A, const FHSRDropTableEntry& B)
		{
			return A.ItemId.LexicalLess(B.ItemId);
		});
		int32 TotalWeight = 0;
		for (const FHSRDropTableEntry& Entry : SortedEntries)
		{
			if (Entry.ItemId.IsNone() || Entry.MinQuantity <= 0 || Entry.MaxQuantity < Entry.MinQuantity || Entry.Weight <= 0 || Entry.Weight > MAX_int32 - TotalWeight)
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
			if (!Selected || !AddQuantity(Selected->ItemId, Stream.RandRange(Selected->MinQuantity, Selected->MaxQuantity)))
			{
				return false;
			}
		}
	}

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
