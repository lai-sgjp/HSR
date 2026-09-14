#pragma once

#include "CoreMinimal.h"

class UAbilitySystemComponent;
class UHSRCharacterDefinition;
class UHSREnemyDefinition;
class UWorld;

/** The base layer only. Growth and equipment remain separately owned GAS effects. */
struct HSR_API FHSRBattleBaseAttributes
{
	float MaxHealth = 100.f;
	float MaxEnergy = 100.f;
	float Attack = 10.f;
	float Defense = 10.f;
	float Speed = 100.f;

	static FHSRBattleBaseAttributes FromCharacter(const UHSRCharacterDefinition& Definition);
	static FHSRBattleBaseAttributes FromEnemy(const UHSREnemyDefinition& Definition, float ExistingMaxEnergy);
	static bool IsFormalArena(const UWorld* World);
	bool Apply(UAbilitySystemComponent& ASC) const;
	static void FillHealth(UAbilitySystemComponent& ASC);
};
