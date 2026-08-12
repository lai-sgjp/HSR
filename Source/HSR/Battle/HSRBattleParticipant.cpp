#include "HSRBattleParticipant.h"

#include "AbilitySystemComponent.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"

// 参与者是否存活：结构有效、未被标记为已败、且 ASC 血量 > 0。
// 注意 ASC 消失（如 Pawn 被销毁）也视为不存活，防止“名义上存活”的队伍残留。
bool FHSRBattleParticipant::IsAlive() const
{
	if (!IsValid() || bDefeated)
	{
		return false;
	}

	return AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) > 0.0f;
}
