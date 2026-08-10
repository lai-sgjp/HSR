#include "HSRBattleParticipant.h"

#include "AbilitySystemComponent.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"

bool FHSRBattleParticipant::IsAlive() const
{
	if (!IsValid() || bDefeated)
	{
		return false;
	}

	return AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) > 0.0f;
}
