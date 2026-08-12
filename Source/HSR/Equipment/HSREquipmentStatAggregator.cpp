#include "HSREquipmentStatAggregator.h"

// AddInstance：把单件装备（武器或遗器）的各个词条数值累加进聚合结果 FHSREquipmentAggregate。
// 这是装备属性聚合的最小单元——Loadout 聚合（Aggregate）会逐件调用本函数。
// 校验策略：实例 ID 与定义 ID 必须合法、强化等级不能为负；每个词条的数值必须是
// 有限（非 NaN/Inf）的非负数，且累加后不能溢出为无穷，否则整个 Loadout 视为无效。
bool UHSREquipmentStatAggregator::AddInstance(const FHSREquipmentInstance& I, FHSREquipmentAggregate& O)
{
	if (!I.InstanceId.IsValid() || I.DefinitionId.IsNone() || I.EnhancementLevel < 0)
	{
		return false;
	}

	for (const auto& M : I.Modifiers)
	{
		if (!FMath::IsFinite(M.Value) || M.Value < 0.f)
		{
			return false;
		}

		float* D = nullptr;
		switch (M.Stat)
		{
		case EHSREquipmentStat::MaxHealth:
			D = &O.MaxHealth;
			break;
		case EHSREquipmentStat::Attack:
			D = &O.Attack;
			break;
		case EHSREquipmentStat::Defense:
			D = &O.Defense;
			break;
		case EHSREquipmentStat::Speed:
			D = &O.Speed;
			break;
		}

		if (D == nullptr || !FMath::IsFinite(*D + M.Value))
		{
			return false;
		}
		*D += M.Value;
	}

	return true;
}

// Aggregate：把一个角色的完整 Loadout（武器 + 全部遗器）聚合成一份属性加成。
// 先在一个临时聚合体 T 上累加，全部成功后才写入输出 O——这样任一件装备无效时
// 输出不会被改成半成品状态。Revision 随聚合结果一起携带，用于给 GAS Effect 桥接层
// 做版本比对（只有数值变化才需要重新应用 Effect）。
bool UHSREquipmentStatAggregator::Aggregate(const FHSREquipmentLoadout& L, int64 R, FHSREquipmentAggregate& O)
{
	if (R < 0)
	{
		return false;
	}

	FHSREquipmentAggregate T;
	T.Revision = R;

	for (const auto& P : L.Equipment)
	{
		if (!AddInstance(P.Value, T))
		{
			return false;
		}
	}
	for (const auto& P : L.Relics)
	{
		if (!AddInstance(P.Value, T))
		{
			return false;
		}
	}

	O = T;
	return true;
}
