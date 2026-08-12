#include "HSREquipmentEnhancementCatalog.h"

#include <cmath>

// 新增一条强化规则：校验定义/材料/目标等级/材料成本/属性快照，并拒绝重复规则。
bool UHSREquipmentEnhancementCatalog::AddRule(const FHSREquipmentEnhancementRule& Rule)
{
	// 基础字段合法性。
	if (Rule.DefinitionId.IsNone() || Rule.MaterialItemId.IsNone() || Rule.TargetLevel < 0
		|| Rule.MaterialCost <= 0 || !IsValidModifierSnapshot(Rule.TargetModifiers))
	{
		return false;
	}
	// 同一定义 + 类型 + 目标等级只允许一条规则。
	if (Rules.ContainsByPredicate([&Rule](const FHSREquipmentEnhancementRule& Existing)
		{
			return Existing.DefinitionId == Rule.DefinitionId && Existing.Kind == Rule.Kind
				&& Existing.TargetLevel == Rule.TargetLevel;
		}))
	{
		return false;
	}
	Rules.Add(Rule);
	return true;
}

// 按定义 + 类型 + 目标等级精确解析一条规则；命中多条视为配置错误（返回 false）。
bool UHSREquipmentEnhancementCatalog::ResolveRule(const FName DefinitionId, const EHSREquipmentKind Kind,
	const int32 TargetLevel, FHSREquipmentEnhancementRule& OutRule) const
{
	const FHSREquipmentEnhancementRule* Found = nullptr;
	for (const FHSREquipmentEnhancementRule& Rule : Rules)
	{
		if (Rule.DefinitionId != DefinitionId || Rule.Kind != Kind || Rule.TargetLevel != TargetLevel)
		{
			continue;
		}
		if (Found != nullptr)
		{
			return false;
		}
		Found = &Rule;
	}
	if (Found == nullptr)
	{
		return false;
	}
	OutRule = *Found;
	return true;
}

// 取某定义+类型在“目标等级 > 当前等级”的全部规则，按目标等级升序返回。
void UHSREquipmentEnhancementCatalog::GetRulesFor(const FName DefinitionId, const EHSREquipmentKind Kind,
	const int32 CurrentLevel, TArray<FHSREquipmentEnhancementRule>& OutRules) const
{
	OutRules.Reset();
	for (const FHSREquipmentEnhancementRule& Rule : Rules)
	{
		if (Rule.DefinitionId == DefinitionId && Rule.Kind == Kind && Rule.TargetLevel > CurrentLevel)
		{
			OutRules.Add(Rule);
		}
	}
	OutRules.Sort([](const FHSREquipmentEnhancementRule& A, const FHSREquipmentEnhancementRule& B)
	{
		return A.TargetLevel < B.TargetLevel;
	});
}

// 校验属性快照：每个修饰符的属性索引合法、数值有限非负，且各属性累计总和
// 有限且不超出 float 可表示范围（用 double 累加以避免中间溢出）。
bool UHSREquipmentEnhancementCatalog::IsValidModifierSnapshot(
	const TArray<FHSREquipmentModifier>& Modifiers)
{
	double Totals[4] = {0.0, 0.0, 0.0, 0.0};
	for (const FHSREquipmentModifier& Modifier : Modifiers)
	{
		const int32 StatIndex = static_cast<int32>(Modifier.Stat);
		if (StatIndex < 0 || StatIndex >= UE_ARRAY_COUNT(Totals) || !FMath::IsFinite(Modifier.Value)
			|| Modifier.Value < 0.0f)
		{
			return false;
		}
		Totals[StatIndex] += static_cast<double>(Modifier.Value);
		if (!std::isfinite(Totals[StatIndex])
			|| Totals[StatIndex] > static_cast<double>(TNumericLimits<float>::Max()))
		{
			return false;
		}
	}
	return true;
}
