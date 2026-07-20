#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemGlobals.h"
#include "HSRAbilitySystemGlobals.generated.h"

UCLASS()
class HSR_API UHSRAbilitySystemGlobals : public UAbilitySystemGlobals
{
	GENERATED_BODY()

public:
	virtual FGameplayEffectContext* AllocGameplayEffectContext() const override;
};
