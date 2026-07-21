#include "HSRTurnManager.h"

#include "AbilitySystemComponent.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"

bool UHSRTurnManager::Initialize(const TArray<FHSRBattleParticipant>& InParticipants)
{
	Reset();
	if (InParticipants.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRTurnManager::Initialize - REJECTED empty participant list"));
		return false;
	}

	OrderedParticipants = InParticipants;
	for (FHSRBattleParticipant& Participant : OrderedParticipants)
	{
		Participant.InitiativeSpeed = ReadInitiativeSpeed(Participant);
	}
	OrderedParticipants.Sort([](const FHSRBattleParticipant& Left, const FHSRBattleParticipant& Right)
	{
		if (!FMath::IsNearlyEqual(Left.InitiativeSpeed, Right.InitiativeSpeed))
		{
			return Left.InitiativeSpeed > Right.InitiativeSpeed;
		}
		return Left.ParticipantId.LexicalLess(Right.ParticipantId);
	});
	++BattleEpochCounter;
	if (BattleEpochCounter == 0)
	{
		++BattleEpochCounter;
	}
	BattleEpoch = BattleEpochCounter;

	UE_LOG(LogTemp, Log, TEXT("UHSRTurnManager::Initialize - Queue built Count=%d"), OrderedParticipants.Num());
	if (!AdvanceToNextValidTurn())
	{
		BattleEpoch = 0;
		TurnSequence = 0;
		return false;
	}
	return true;
}

bool UHSRTurnManager::ResolveAction(FName ResolvingParticipantId)
{
	if (CurrentTurnIndex == INDEX_NONE || State == EHSRTurnManagerState::Finished)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRTurnManager::ResolveAction - REJECTED no active turn ParticipantId=%s"), *ResolvingParticipantId.ToString());
		return false;
	}

	const FHSRBattleParticipant& Current = OrderedParticipants[CurrentTurnIndex];
	if (Current.ParticipantId != ResolvingParticipantId)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRTurnManager::ResolveAction - REJECTED non-current ParticipantId=%s Current=%s"), *ResolvingParticipantId.ToString(), *Current.ParticipantId.ToString());
		return false;
	}
	if (!IsCurrentParticipantValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRTurnManager::ResolveAction - REJECTED invalid current ParticipantId=%s; advancing"), *ResolvingParticipantId.ToString());
		AdvanceToNextValidTurn();
		return false;
	}

	const FName ResolvedId = Current.ParticipantId;
	BroadcastLifecycleEvent(EHSRTurnLifecycleEventType::TurnEnded, ResolvedId);
	if (State == EHSRTurnManagerState::Finished || CurrentTurnIndex == INDEX_NONE)
	{
		ActionResolved.Broadcast(ResolvedId);
		UE_LOG(LogTemp, Log, TEXT("UHSRTurnManager::ResolveAction - SUCCESS terminal ParticipantId=%s Next=None"), *ResolvedId.ToString());
		return true;
	}
	AdvanceToNextValidTurn();
	ActionResolved.Broadcast(ResolvedId);
	UE_LOG(LogTemp, Log, TEXT("UHSRTurnManager::ResolveAction - SUCCESS ParticipantId=%s Next=%s"), *ResolvedId.ToString(), *GetCurrentParticipantId().ToString());
	return true;
}

bool UHSRTurnManager::ConsumeBreakDelay(const FHSRTurnDelayRequest& Request)
{
	if (!Request.ActionId.IsValid() || Request.TargetParticipantId.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("P8-004 Delay ActionId=%s Target=%s Applied=0 Reason=InvalidRequest"), *Request.ActionId.ToString(), *Request.TargetParticipantId.ToString());
		return false;
	}
	if (ConsumedBreakDelayActionIds.Contains(Request.ActionId))
	{
		UE_LOG(LogTemp, Log, TEXT("P8-004 Delay ActionId=%s Target=%s Applied=0 Reason=Replay"), *Request.ActionId.ToString(), *Request.TargetParticipantId.ToString());
		return false;
	}
	ConsumedBreakDelayActionIds.Add(Request.ActionId);
	if (State == EHSRTurnManagerState::Finished)
	{
		UE_LOG(LogTemp, Log, TEXT("P8-004 Delay ActionId=%s Target=%s Applied=0 Reason=Finished"), *Request.ActionId.ToString(), *Request.TargetParticipantId.ToString());
		return false;
	}
	const int32 TargetIndex = OrderedParticipants.IndexOfByPredicate([&Request](const FHSRBattleParticipant& Participant)
	{
		return Participant.ParticipantId == Request.TargetParticipantId;
	});
	if (!OrderedParticipants.IsValidIndex(TargetIndex) || !IsParticipantTurnEligible(OrderedParticipants[TargetIndex]))
	{
		UE_LOG(LogTemp, Log, TEXT("P8-004 Delay ActionId=%s Target=%s Applied=0 Reason=InvalidTarget"), *Request.ActionId.ToString(), *Request.TargetParticipantId.ToString());
		return false;
	}

	PendingBreakDelayActionIds.Add(Request.TargetParticipantId, Request.ActionId);
	UE_LOG(LogTemp, Log, TEXT("P8-004 Delay ActionId=%s Target=%s Registered=1 Consumed=0 Next=%s Reason=None"), *Request.ActionId.ToString(), *Request.TargetParticipantId.ToString(), *GetCurrentParticipantId().ToString());
	return true;
}

void UHSRTurnManager::FinishBattle()
{
	State = EHSRTurnManagerState::Finished;
	CurrentTurnIndex = INDEX_NONE;
	UE_LOG(LogTemp, Log, TEXT("UHSRTurnManager::FinishBattle - Turn progression stopped"));
}

void UHSRTurnManager::Reset()
{
	OrderedParticipants.Empty();
	ConsumedBreakDelayActionIds.Empty();
	PendingBreakDelayActionIds.Empty();
	CurrentTurnIndex = INDEX_NONE;
	BattleEpoch = 0;
	TurnSequence = 0;
	State = EHSRTurnManagerState::Waiting;
}

FName UHSRTurnManager::GetCurrentParticipantId() const
{
	return OrderedParticipants.IsValidIndex(CurrentTurnIndex) ? OrderedParticipants[CurrentTurnIndex].ParticipantId : NAME_None;
}

bool UHSRTurnManager::AdvanceToNextValidTurn()
{
	if (OrderedParticipants.IsEmpty())
	{
		State = EHSRTurnManagerState::Finished;
		CurrentTurnIndex = INDEX_NONE;
		return false;
	}

	const int32 StartIndex = CurrentTurnIndex == INDEX_NONE ? 0 : (CurrentTurnIndex + 1) % OrderedParticipants.Num();
	FName ConsumedDelayTarget = NAME_None;
	FGuid ConsumedDelayActionId;
	for (int32 Offset = 0; Offset < OrderedParticipants.Num(); ++Offset)
	{
		const int32 CandidateIndex = (StartIndex + Offset) % OrderedParticipants.Num();
		const FHSRBattleParticipant& Candidate = OrderedParticipants[CandidateIndex];
		if (!IsParticipantTurnEligible(Candidate))
		{
			if (const FGuid* PendingActionId = PendingBreakDelayActionIds.Find(Candidate.ParticipantId))
			{
				const FGuid ClearedActionId = *PendingActionId;
				PendingBreakDelayActionIds.Remove(Candidate.ParticipantId);
				UE_LOG(LogTemp, Log, TEXT("P8-004 Delay ActionId=%s Target=%s Registered=0 Consumed=1 Next=%s Reason=InvalidTarget"), *ClearedActionId.ToString(), *Candidate.ParticipantId.ToString(), *GetCurrentParticipantId().ToString());
			}
			continue;
		}
		if (const FGuid* PendingActionId = PendingBreakDelayActionIds.Find(Candidate.ParticipantId))
		{
			ConsumedDelayTarget = Candidate.ParticipantId;
			ConsumedDelayActionId = *PendingActionId;
			PendingBreakDelayActionIds.Remove(Candidate.ParticipantId);
			continue;
		}
		if (IsParticipantTurnEligible(Candidate))
		{
			CurrentTurnIndex = CandidateIndex;
			State = Candidate.Team == EHSRBattleParticipantTeam::Player ? EHSRTurnManagerState::PlayerTurn : EHSRTurnManagerState::EnemyTurn;
			++TurnSequence;
			if (!ConsumedDelayTarget.IsNone())
			{
				UE_LOG(LogTemp, Log, TEXT("P8-004 Delay ActionId=%s Target=%s Registered=0 Consumed=1 Next=%s Reason=None"), *ConsumedDelayActionId.ToString(), *ConsumedDelayTarget.ToString(), *Candidate.ParticipantId.ToString());
			}
			BroadcastLifecycleEvent(EHSRTurnLifecycleEventType::TurnStarted, Candidate.ParticipantId);
			return true;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("UHSRTurnManager::AdvanceToNextValidTurn - FINISHED no valid participants"));
	State = EHSRTurnManagerState::Finished;
	CurrentTurnIndex = INDEX_NONE;
	if (!ConsumedDelayTarget.IsNone())
	{
		UE_LOG(LogTemp, Log, TEXT("P8-004 Delay ActionId=%s Target=%s Registered=0 Consumed=1 Next=None Reason=NoValidParticipant"), *ConsumedDelayActionId.ToString(), *ConsumedDelayTarget.ToString());
	}
	return false;
}

void UHSRTurnManager::BroadcastLifecycleEvent(EHSRTurnLifecycleEventType EventType, FName ParticipantId)
{
	if (BattleEpoch == 0 || TurnSequence == 0 || ParticipantId.IsNone() || State == EHSRTurnManagerState::Finished)
	{
		return;
	}

	FHSRTurnLifecycleEvent Event;
	Event.BattleEpoch = BattleEpoch;
	Event.ParticipantId = ParticipantId;
	Event.TurnSequence = TurnSequence;
	Event.EventType = EventType;
	if (EventType == EHSRTurnLifecycleEventType::TurnStarted)
	{
		TurnStarted.Broadcast(Event);
	}
	else
	{
		TurnEnded.Broadcast(Event);
	}
}

bool UHSRTurnManager::IsCurrentParticipantValid() const
{
	return OrderedParticipants.IsValidIndex(CurrentTurnIndex) && IsParticipantTurnEligible(OrderedParticipants[CurrentTurnIndex]);
}

bool UHSRTurnManager::IsParticipantTurnEligible(const FHSRBattleParticipant& Participant)
{
	return Participant.IsValid()
		&& Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) > 0.0f;
}

float UHSRTurnManager::ReadInitiativeSpeed(const FHSRBattleParticipant& Participant)
{
	return Participant.AbilitySystemComponent.IsValid() ? FMath::Max(0.0f, Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetSpeedAttribute())) : 0.0f;
}

#if WITH_EDITOR
bool UHSRTurnManager::InvalidateCurrentParticipantForDevelopmentTest()
{
	if (!OrderedParticipants.IsValidIndex(CurrentTurnIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRTurnManager::InvalidateCurrentParticipantForDevelopmentTest - REJECTED no active turn"));
		return false;
	}

	OrderedParticipants[CurrentTurnIndex].Actor.Reset();
	UE_LOG(LogTemp, Log, TEXT("UHSRTurnManager::InvalidateCurrentParticipantForDevelopmentTest - SUCCESS ParticipantId=%s"), *GetCurrentParticipantId().ToString());
	return true;
}
#endif
