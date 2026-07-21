#include "HSREnemyDefinition.h"

EHSRElementToughnessContractResult UHSREnemyDefinition::GetElementToughnessContractResult() const
{
	const EHSRElementToughnessContractResult WeaknessResult = FHSRToughnessConfiguration::ValidateWeaknesses(WeaknessTags);
	return WeaknessResult != EHSRElementToughnessContractResult::Valid ? WeaknessResult : FHSRToughnessConfiguration::ValidateInitialToughness(InitialToughness, InitialMaxToughness);
}
