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

	UE_LOG(LogTemp, Log, TEXT("UHSRTurnManager::Initialize - Queue built Count=%d"), OrderedParticipants.Num());
	return AdvanceToNextValidTurn();
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
	AdvanceToNextValidTurn();
	ActionResolved.Broadcast(ResolvedId);
	UE_LOG(LogTemp, Log, TEXT("UHSRTurnManager::ResolveAction - SUCCESS ParticipantId=%s Next=%s"), *ResolvedId.ToString(), *GetCurrentParticipantId().ToString());
	return true;
}

void UHSRTurnManager::Reset()
{
	OrderedParticipants.Empty();
	CurrentTurnIndex = INDEX_NONE;
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
	for (int32 Offset = 0; Offset < OrderedParticipants.Num(); ++Offset)
	{
		const int32 CandidateIndex = (StartIndex + Offset) % OrderedParticipants.Num();
		if (OrderedParticipants[CandidateIndex].IsValid())
		{
			CurrentTurnIndex = CandidateIndex;
			State = OrderedParticipants[CandidateIndex].Team == EHSRBattleParticipantTeam::Player ? EHSRTurnManagerState::PlayerTurn : EHSRTurnManagerState::EnemyTurn;
			return true;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("UHSRTurnManager::AdvanceToNextValidTurn - FINISHED no valid participants"));
	State = EHSRTurnManagerState::Finished;
	CurrentTurnIndex = INDEX_NONE;
	return false;
}

bool UHSRTurnManager::IsCurrentParticipantValid() const
{
	return OrderedParticipants.IsValidIndex(CurrentTurnIndex) && OrderedParticipants[CurrentTurnIndex].IsValid();
}

float UHSRTurnManager::ReadInitiativeSpeed(const FHSRBattleParticipant& Participant)
{
	return Participant.AbilitySystemComponent.IsValid() ? FMath::Max(0.0f, Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetSpeedAttribute())) : 0.0f;
}
