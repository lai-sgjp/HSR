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

	FHSRBattleParticipantDefinition GetDefinition() const
	{
		FHSRBattleParticipantDefinition Def;
		Def.ParticipantId = ParticipantId;
		Def.DefinitionId = DefinitionId;
		Def.Team = Team;
		return Def;
	}
};
