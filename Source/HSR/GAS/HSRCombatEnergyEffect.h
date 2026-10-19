#pragma once
#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "HSRCombatEnergyEffect.generated.h"

/** One authoritative Instant Energy modifier shared by battle energy gain sources. */
UCLASS()
class HSR_API UHSRCombatEnergyEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UHSRCombatEnergyEffect();
};
