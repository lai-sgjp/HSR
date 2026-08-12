#include "HSRItemEquipmentMappingCatalog.h"

// 新增一条“物品 → 装备”映射：校验物品 ID/装备定义 ID/槽位非空合法，
// 并拒绝物品 ID 或装备定义 ID 已被其他条目占用的情况。
bool UHSRItemEquipmentMappingCatalog::AddMapping(const FHSRItemEquipmentMappingEntry& Entry)
{
	if (Entry.ItemId.IsNone() || Entry.EquipmentDefinitionId.IsNone() || Entry.Slot < 0
		|| Mappings.ContainsByPredicate([&Entry](const FHSRItemEquipmentMappingEntry& Existing)
		{
			return Existing.ItemId == Entry.ItemId || Existing.EquipmentDefinitionId == Entry.EquipmentDefinitionId;
		}))
	{
		return false;
	}
	Mappings.Add(Entry);
	return true;
}

// 按物品 ID 解析映射；命中多条视为配置错误（返回 false）。
bool UHSRItemEquipmentMappingCatalog::Resolve(const FName ItemId, FHSRItemEquipmentMappingEntry& OutEntry) const
{
	const FHSRItemEquipmentMappingEntry* Found = nullptr;
	for (const FHSRItemEquipmentMappingEntry& Entry : Mappings)
	{
		if (Entry.ItemId != ItemId)
		{
			continue;
		}
		if (Found != nullptr)
		{
			return false;
		}
		Found = &Entry;
	}
	if (!Found)
	{
		return false;
	}
	OutEntry = *Found;
	return true;
}

// 按装备定义 ID 反向解析映射；同样拒绝重复。
bool UHSRItemEquipmentMappingCatalog::ResolveEquipmentDefinition(const FName EquipmentDefinitionId,
	FHSRItemEquipmentMappingEntry& OutEntry) const
{
	const FHSRItemEquipmentMappingEntry* Found = nullptr;
	for (const FHSRItemEquipmentMappingEntry& Entry : Mappings)
	{
		if (Entry.EquipmentDefinitionId != EquipmentDefinitionId)
		{
			continue;
		}
		if (Found != nullptr)
		{
			return false;
		}
		Found = &Entry;
	}
	if (!Found)
	{
		return false;
	}
	OutEntry = *Found;
	return true;
}

// 校验式解析：只有“唯一存储”物品才允许映射；解析成功后交给外部校验器
// 验证装备定义的有效性（定义 ID、类型、槽位）。
bool UHSRItemEquipmentMappingCatalog::Validate(const FName ItemId, const EHSRItemStorageKind StorageKind, const TFunctionRef<bool(FName, EHSREquipmentKind, int32)>& DefinitionValidator, FHSRItemEquipmentMappingEntry& OutEntry) const
{
	if (StorageKind != EHSRItemStorageKind::Unique || !Resolve(ItemId, OutEntry))
	{
		return false;
	}
	return DefinitionValidator(OutEntry.EquipmentDefinitionId, OutEntry.Kind, OutEntry.Slot);
}
