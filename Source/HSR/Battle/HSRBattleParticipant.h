#pragma once

#include "CoreMinimal.h"
#include "HSRBattleTypes.h"

class AActor;
class UAbilitySystemComponent;

struct FHSRBattleParticipant
{
	FName ParticipantId;
	FName DefinitionId;
	EHSRBattleParticipantTeam Team = EHSRBattleParticipantTeam::Player;

	/** Snapshot when the turn order is built. TurnManager never polls this every frame. */
	float InitiativeSpeed = 0.0f;

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
