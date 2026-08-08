#include "HSREquipmentEnhancementCatalog.h"

#include <cmath>

bool UHSREquipmentEnhancementCatalog::AddRule(const FHSREquipmentEnhancementRule& Rule)
{
	if (Rule.DefinitionId.IsNone() || Rule.MaterialItemId.IsNone() || Rule.TargetLevel < 0
		|| Rule.MaterialCost <= 0 || !IsValidModifierSnapshot(Rule.TargetModifiers))
	{
		return false;
	}
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

bool UHSREquipmentEnhancementCatalog::ResolveRule(const FName DefinitionId, const EHSREquipmentKind Kind,
	const int32 TargetLevel, FHSREquipmentEnhancementRule& OutRule) const
{
	const FHSREquipmentEnhancementRule* Found = nullptr;
	for (const FHSREquipmentEnhancementRule& Rule : Rules)
	{
		if (Rule.DefinitionId != DefinitionId || Rule.Kind != Kind || Rule.TargetLevel != TargetLevel) continue;
		if (Found != nullptr) return false;
		Found = &Rule;
	}
	if (Found == nullptr) return false;
	OutRule = *Found;
	return true;
}

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
