#include "HSRItemEquipmentMappingCatalog.h"

bool UHSRItemEquipmentMappingCatalog::AddMapping(const FHSRItemEquipmentMappingEntry& Entry)
{
	if (Entry.ItemId.IsNone() || Entry.EquipmentDefinitionId.IsNone() || Entry.Slot < 0 || Mappings.ContainsByPredicate([&Entry](const FHSRItemEquipmentMappingEntry& Existing)
		{
			return Existing.ItemId == Entry.ItemId || Existing.EquipmentDefinitionId == Entry.EquipmentDefinitionId;
		}))
	{
		return false;
	}
	Mappings.Add(Entry);
	return true;
}

bool UHSRItemEquipmentMappingCatalog::Resolve(const FName ItemId, FHSRItemEquipmentMappingEntry& OutEntry) const
{
	const FHSRItemEquipmentMappingEntry* Found = nullptr;
	for (const FHSRItemEquipmentMappingEntry& Entry : Mappings)
	{
		if (Entry.ItemId != ItemId) continue;
		if (Found != nullptr) return false;
		Found = &Entry;
	}
	if (!Found) return false;
	OutEntry = *Found;
	return true;
}

bool UHSRItemEquipmentMappingCatalog::ResolveEquipmentDefinition(const FName EquipmentDefinitionId,
	FHSRItemEquipmentMappingEntry& OutEntry) const
{
	const FHSRItemEquipmentMappingEntry* Found = nullptr;
	for (const FHSRItemEquipmentMappingEntry& Entry : Mappings)
	{
		if (Entry.EquipmentDefinitionId != EquipmentDefinitionId) continue;
		if (Found != nullptr) return false;
		Found = &Entry;
	}
	if (!Found) return false;
	OutEntry = *Found;
	return true;
}

bool UHSRItemEquipmentMappingCatalog::Validate(const FName ItemId, const EHSRItemStorageKind StorageKind, const TFunctionRef<bool(FName, EHSREquipmentKind, int32)>& DefinitionValidator, FHSRItemEquipmentMappingEntry& OutEntry) const
{
	if (StorageKind != EHSRItemStorageKind::Unique || !Resolve(ItemId, OutEntry))
	{
		return false;
	}
	return DefinitionValidator(OutEntry.EquipmentDefinitionId, OutEntry.Kind, OutEntry.Slot);
}
