#include "HSREnemyDefinition.h"

// 校验敌人定义的“韧性契约”：先校验弱点标签集合，
// 合法时再校验初始韧性/最大韧性的数值关系。返回首个不通过项。
EHSRElementToughnessContractResult UHSREnemyDefinition::GetElementToughnessContractResult() const
{
	const EHSRElementToughnessContractResult WeaknessResult = FHSRToughnessConfiguration::ValidateWeaknesses(WeaknessTags);
	return WeaknessResult != EHSRElementToughnessContractResult::Valid
		? WeaknessResult
		: FHSRToughnessConfiguration::ValidateInitialToughness(InitialToughness, InitialMaxToughness);
}
