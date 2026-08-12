#include "HSRBattlePresentationResolver.h"

namespace
{
	static FName FallbackAttackId()
	{
		return TEXT("Demo.Presentation.Fallback.Attack");
	}

	static FName FallbackHitId()
	{
		return TEXT("Demo.Presentation.Fallback.Hit");
	}
}

void FHSRBattlePresentationResolver::AddMapping(const FHSRBattlePresentationMapping& Mapping)
{
	Mappings.Add(static_cast<uint8>(Mapping.EventType), Mapping);
}

void FHSRBattlePresentationResolver::CopyEvent(const FHSRBattlePresentationEvent& Event, FHSRBattlePresentationIntent& OutIntent)
{
	OutIntent.EventId = Event.EventId;
	OutIntent.ActionId = Event.ActionId;
	OutIntent.SourceParticipantId = Event.SourceParticipantId;
	OutIntent.TargetParticipantId = Event.TargetParticipantId;
	OutIntent.EventType = Event.EventType;
	OutIntent.Value = Event.Value;
	OutIntent.bCritical = Event.bCritical;
	OutIntent.bBreak = Event.bBreak;
}

bool FHSRBattlePresentationResolver::Resolve(const FHSRBattlePresentationEvent& Event, FHSRBattlePresentationIntent& OutIntent) const
{
	CopyEvent(Event, OutIntent);
	if (const FHSRBattlePresentationMapping* Mapping = Mappings.Find(static_cast<uint8>(Event.EventType)))
	{
		OutIntent.AttackPresentationId = Mapping->AttackPresentationId;
		OutIntent.HitPresentationId = Mapping->HitPresentationId;
		OutIntent.bFallback = OutIntent.AttackPresentationId.IsNone() || OutIntent.HitPresentationId.IsNone();
		if (OutIntent.bFallback)
		{
			OutIntent.AttackPresentationId = FallbackAttackId();
			OutIntent.HitPresentationId = FallbackHitId();
		}
		return true;
	}

	OutIntent.AttackPresentationId = FallbackAttackId();
	OutIntent.HitPresentationId = FallbackHitId();
	OutIntent.bFallback = true;
	return true;
}

bool FHSRBattlePresentationResolver::Consume(const FHSRBattlePresentationEvent& Event, FHSRBattlePresentationIntent& OutIntent)
{
	if (!Event.EventId.IsValid() || ConsumedEventIds.Contains(Event.EventId))
	{
		return false;
	}
	if (!Resolve(Event, OutIntent))
	{
		return false;
	}
	ConsumedEventIds.Add(Event.EventId);
	return true;
}
