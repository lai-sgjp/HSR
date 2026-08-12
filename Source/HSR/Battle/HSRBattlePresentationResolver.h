#pragma once

#include "CoreMinimal.h"
#include "../UI/HSRBattleCommandTypes.h"

/** Author-authored identifiers consumed by the presentation layer only. */
struct FHSRBattlePresentationMapping
{
	EHSRPresentationEventType EventType = EHSRPresentationEventType::Damage;
	FName AttackPresentationId;
	FName HitPresentationId;
};

/** Pure-value output. It carries no authority, actor, asset, or gameplay handle. */
struct FHSRBattlePresentationIntent
{
	FGuid EventId;
	FGuid ActionId;
	FName SourceParticipantId;
	FName TargetParticipantId;
	EHSRPresentationEventType EventType = EHSRPresentationEventType::Damage;
	FName AttackPresentationId;
	FName HitPresentationId;
	float Value = 0.0f;
	bool bCritical = false;
	bool bBreak = false;
	bool bFallback = false;
};

/** Resolves battle presentation events without participating in battle authority. */
class FHSRBattlePresentationResolver
{
public:
	void AddMapping(const FHSRBattlePresentationMapping& Mapping);
	bool Resolve(const FHSRBattlePresentationEvent& Event, FHSRBattlePresentationIntent& OutIntent) const;
	bool Consume(const FHSRBattlePresentationEvent& Event, FHSRBattlePresentationIntent& OutIntent);
	int32 GetConsumedEventCount() const { return ConsumedEventIds.Num(); }

private:
	static void CopyEvent(const FHSRBattlePresentationEvent& Event, FHSRBattlePresentationIntent& OutIntent);
	TMap<uint8, FHSRBattlePresentationMapping> Mappings;
	TSet<FGuid> ConsumedEventIds;
};
