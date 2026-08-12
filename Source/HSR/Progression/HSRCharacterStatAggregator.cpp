#include "HSRCharacterStatAggregator.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"

// BuildContext：根据角色定义与当前运行时状态，构建一份纯数值的「成长上下文」。
// 数据流：定义 D 提供基础属性（Base*）与每级成长（*PerLevel），运行时状态 R 提供
// 当前等级 Level。成长加成 = 每级成长 * (Level - 1)；派生属性 = 基础属性 + 成长加成。
// 这份上下文后续会被刷新到存档/战斗系统，作为该角色各项派生属性的单一数据来源。
bool UHSRCharacterStatAggregator::BuildContext(const UHSRCharacterDefinition* D, const FHSRCharacterRuntimeState& R, int64 Revision, FHSRCharacterProgressionContext& Out)
{
	// 入参合法性校验：定义与状态必须匹配同一角色，等级必须在 1..MaxLevel 区间内，
	// 版本号不能为负。任何一项不满足都直接拒绝，避免脏数据进入派生属性计算。
	if (!D || D->CharacterId.IsNone() || R.CharacterId != D->CharacterId
		|| R.Level < 1 || R.Level > D->MaxLevel || Revision < 0)
	{
		return false;
	}

	// 基础属性与每级成长必须是有限、非负的实数；速度额外要求严格为正（0 速无法行动）。
	const float Values[] = {
		D->BaseMaxHealth, D->BaseAttack, D->BaseDefense, D->BaseSpeed,
		D->MaxHealthPerLevel, D->AttackPerLevel, D->DefensePerLevel, D->SpeedPerLevel
	};
	for (float V : Values)
	{
		if (!FMath::IsFinite(V) || V < 0.0f)
		{
			return false;
		}
	}
	if (D->BaseSpeed <= 0.0f)
	{
		return false;
	}

	// 等级每提升 1 级，属性按对应 PerLevel 线性增长一层。
	const float L = static_cast<float>(R.Level - 1);
	FHSRCharacterProgressionContext C;
	C.CharacterId = R.CharacterId;
	C.RuntimeRevision = Revision;

	// 成长加成（Growth）：按等级累计的增量，clamp 到非负 float 上限。
	C.ProgressionBonuses.MaxHealth = FMath::Clamp(D->MaxHealthPerLevel * L, 0.0f, MAX_flt);
	C.ProgressionBonuses.Attack = FMath::Clamp(D->AttackPerLevel * L, 0.0f, MAX_flt);
	C.ProgressionBonuses.Defense = FMath::Clamp(D->DefensePerLevel * L, 0.0f, MAX_flt);
	C.ProgressionBonuses.Speed = FMath::Clamp(D->SpeedPerLevel * L, 0.0f, MAX_flt);

	// 派生属性（Derived）：基础属性 + 成长加成。速度下限用 UE_SMALL_NUMBER 兜底，
	// 保证任何情况下都不会出现 0 或负速度。
	C.DerivedStats.MaxHealth = FMath::Clamp(D->BaseMaxHealth + C.ProgressionBonuses.MaxHealth, 0.0f, MAX_flt);
	C.DerivedStats.Attack = FMath::Clamp(D->BaseAttack + C.ProgressionBonuses.Attack, 0.0f, MAX_flt);
	C.DerivedStats.Defense = FMath::Clamp(D->BaseDefense + C.ProgressionBonuses.Defense, 0.0f, MAX_flt);
	C.DerivedStats.Speed = FMath::Clamp(D->BaseSpeed + C.ProgressionBonuses.Speed, UE_SMALL_NUMBER, MAX_flt);

	// 兜底：即使上面做了 Clamp，仍显式确认四项派生属性都是有限值，双保险防止 Inf 泄漏。
	if (!FMath::IsFinite(C.DerivedStats.MaxHealth) || !FMath::IsFinite(C.DerivedStats.Attack)
		|| !FMath::IsFinite(C.DerivedStats.Defense) || !FMath::IsFinite(C.DerivedStats.Speed))
	{
		return false;
	}

	Out = C;
	return true;
}
