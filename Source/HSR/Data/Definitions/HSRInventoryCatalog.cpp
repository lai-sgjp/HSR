#include "HSRInventoryCatalog.h"

// 校验物品目录：每个条目的 ItemId 非空、有显示名、且全局唯一。
bool UHSRInventoryCatalog::Validate(FString* OutError) const
{
	// 局部失败回调：有 OutError 就写入原因，统一返回 false。
	auto Fail = [OutError](const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
		return false;
	};

	TSet<FName> SeenItemIds;
	for (const FHSRInventoryCatalogEntry& Entry : Entries)
	{
		if (Entry.ItemId.IsNone())
		{
			return Fail(TEXT("Inventory catalog contains an empty ItemId"));
		}
		if (Entry.DisplayName.IsEmpty())
		{
			return Fail(FString::Printf(TEXT("Inventory catalog entry %s has no DisplayName"),
				*Entry.ItemId.ToString()));
		}
		if (SeenItemIds.Contains(Entry.ItemId))
		{
			return Fail(FString::Printf(TEXT("Inventory catalog duplicates ItemId %s"),
				*Entry.ItemId.ToString()));
		}
		SeenItemIds.Add(Entry.ItemId);
	}

	return true;
}

// 按物品 ID 查找目录条目；找不到返回 false。
bool UHSRInventoryCatalog::FindEntry(const FName ItemId, FHSRInventoryCatalogEntry& OutEntry) const
{
	for (const FHSRInventoryCatalogEntry& Entry : Entries)
	{
		if (Entry.ItemId == ItemId)
		{
			OutEntry = Entry;
			return true;
		}
	}
	return false;
}
