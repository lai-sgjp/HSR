#include "HSRInventoryCatalog.h"

bool UHSRInventoryCatalog::Validate(FString* OutError) const
{
	auto Fail = [OutError](const FString& Error)
	{
		if (OutError) *OutError = Error;
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
