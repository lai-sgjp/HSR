#pragma once

#include "CoreMinimal.h"
#include "HSRBattleTypes.h"
#include "GameplayTagContainer.h"

class AActor;
class UAbilitySystemComponent;

struct FHSRBattleParticipant
{
	FName ParticipantId;
	FName DefinitionId;
	EHSRBattleParticipantTeam Team = EHSRBattleParticipantTeam::Player;
	FGameplayTagContainer WeaknessTags;

	/** Legacy diagnostic speed snapshot; action-distance state below is owned battle-locally by TurnManager. */
	float InitiativeSpeed = 0.0f;
	float EffectiveSpeed = 0.0f;
	float BaseActionDistance = 0.0f;
	float RemainingActionDistance = 0.0f;
	bool bDefeated = false;
	TWeakObjectPtr<AActor> Actor;
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	FHSRBattleParticipant() = default;

	bool IsValid() const
	{
		return Actor.IsValid() && AbilitySystemComponent.IsValid();
	}

	/**
	 * Single authority for "can this participant still act or be acted upon".
	 *
	 * Health and bDefeated must both be consulted: HandleHealthChanged defers ResolveDefeat while a
	 * formal damage transaction is open, so a participant can sit at Health == 0 with bDefeated still
	 * false. Checking either flag alone made targeting offer a corpse that the turn order had already
	 * skipped.
	 */
	bool IsAlive() const;

	FHSRBattleParticipantDefinition GetDefinition() const
	{
		FHSRBattleParticipantDefinition Def;
		Def.ParticipantId = ParticipantId;
		Def.DefinitionId = DefinitionId;
		Def.Team = Team;
		return Def;
	}
};
