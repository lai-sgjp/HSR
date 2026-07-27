#include "HSRTurnManager.h"

#include "AbilitySystemComponent.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"

bool UHSRTurnManager::MakeBaseActionDistance(float Speed, float& OutBase)
{
	if (!FMath::IsFinite(Speed)) return false;
	OutBase = MaximumBaseActionDistance / FMath::Max(Speed, 1.0f);
	return FMath::IsFinite(OutBase) && OutBase > 0.0f;
}

bool UHSRTurnManager::Initialize(const TArray<FHSRBattleParticipant>& InParticipants)
{
	Reset();
	if (InParticipants.IsEmpty()) return false;
	TArray<FHSRBattleParticipant> Candidate = InParticipants;
	for (FHSRBattleParticipant& Participant : Candidate)
	{
		if (!Participant.IsValid() || Participant.ParticipantId.IsNone()) return false;
		const float Speed = Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetSpeedAttribute());
		float Base = 0.0f;
		if (!MakeBaseActionDistance(Speed, Base)) return false;
		Participant.InitiativeSpeed = FMath::Max(Speed, 1.0f);
		Participant.EffectiveSpeed = FMath::Max(Speed, 1.0f);
		Participant.BaseActionDistance = Base;
		Participant.RemainingActionDistance = Base;
	}
	Candidate.Sort([](const FHSRBattleParticipant& A, const FHSRBattleParticipant& B) { return A.ParticipantId.LexicalLess(B.ParticipantId); });
	for (int32 Index = 1; Index < Candidate.Num(); ++Index) if (Candidate[Index - 1].ParticipantId == Candidate[Index].ParticipantId) return false;
	OrderedParticipants = MoveTemp(Candidate);
	++BattleEpochCounter;
	if (BattleEpochCounter == 0) ++BattleEpochCounter;
	BattleEpoch = BattleEpochCounter;
	if (!BindSpeedDelegates()) { Reset(); return false; }
	if (!AdvanceToNextValidTurn()) { Reset(); return false; }
	return true;
}

bool UHSRTurnManager::BindSpeedDelegates()
{
	for (const FHSRBattleParticipant& Participant : OrderedParticipants)
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (SpeedDelegateBindFailureAfter != INDEX_NONE && SpeedDelegateBindings.Num() >= SpeedDelegateBindFailureAfter) { UnbindSpeedDelegates(); return false; }
#endif
		UAbilitySystemComponent* ASC = Participant.AbilitySystemComponent.Get();
		if (!ASC) { UnbindSpeedDelegates(); return false; }
		const FName Id = Participant.ParticipantId;
		const uint64 Epoch = BattleEpoch;
		TWeakObjectPtr<UHSRTurnManager> WeakThis(this);
		FDelegateHandle Handle = ASC->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetSpeedAttribute()).AddLambda(
			[WeakThis, Id, WeakASC = TWeakObjectPtr<UAbilitySystemComponent>(ASC), Epoch](const FOnAttributeChangeData& Data)
			{ if (UHSRTurnManager* This = WeakThis.Get()) This->HandleSpeedChanged(Id, WeakASC.Get(), Epoch, Data.NewValue); });
		if (!Handle.IsValid()) { UnbindSpeedDelegates(); return false; }
		FSpeedDelegateBinding& Binding = SpeedDelegateBindings.AddDefaulted_GetRef();
		Binding.ParticipantId = Id; Binding.AbilitySystemComponent = ASC; Binding.Handle = Handle; Binding.Epoch = Epoch;
	}
	return true;
}

void UHSRTurnManager::UnbindSpeedDelegates()
{
	for (const FSpeedDelegateBinding& Binding : SpeedDelegateBindings)
		if (UAbilitySystemComponent* ASC = Binding.AbilitySystemComponent.Get()) if (Binding.Handle.IsValid()) ASC->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetSpeedAttribute()).Remove(Binding.Handle);
	SpeedDelegateBindings.Empty();
}

void UHSRTurnManager::HandleSpeedChanged(FName ParticipantId, UAbilitySystemComponent* SourceASC, uint64 BoundEpoch, float NewSpeed)
{
	if (BoundEpoch == 0 || BoundEpoch != BattleEpoch || State == EHSRTurnManagerState::Finished || !SourceASC) return;
	if (!FMath::IsFinite(NewSpeed)) { UE_LOG(LogTemp, Warning, TEXT("ActionDistance SpeedRejected Participant=%s Reason=NonFinite Epoch=%llu Sequence=%llu"), *ParticipantId.ToString(), BoundEpoch, TurnSequence); return; }
	const int32 Index = FindParticipantIndex(ParticipantId);
	if (!OrderedParticipants.IsValidIndex(Index) || OrderedParticipants[Index].AbilitySystemComponent.Get() != SourceASC) return;
	FHSRBattleParticipant& Participant = OrderedParticipants[Index];
	float NewBase = 0.0f;
	if (!MakeBaseActionDistance(NewSpeed, NewBase)) return;
	if (Index == CurrentTurnIndex) { Participant.EffectiveSpeed = FMath::Max(NewSpeed, 1.0f); Participant.BaseActionDistance = NewBase; return; }
	const float NewRemaining = Participant.RemainingActionDistance * NewBase / Participant.BaseActionDistance;
	if (!FMath::IsFinite(NewRemaining) || NewRemaining < 0.0f) return;
	Participant.EffectiveSpeed = FMath::Max(NewSpeed, 1.0f);
	Participant.BaseActionDistance = NewBase;
	Participant.RemainingActionDistance = FMath::Abs(NewRemaining) <= DistanceEpsilon ? 0.0f : NewRemaining;
}

bool UHSRTurnManager::ResolveAction(FName ResolvingParticipantId)
{
	if (bSelectingOrResolving || CurrentTurnIndex == INDEX_NONE || State == EHSRTurnManagerState::Finished || GetCurrentParticipantId() != ResolvingParticipantId || !IsCurrentParticipantValid()) return false;
	bSelectingOrResolving = true;
	const FName ResolvedId = ResolvingParticipantId;
	BroadcastLifecycleEvent(EHSRTurnLifecycleEventType::TurnEnded, ResolvedId);
	bool bSucceeded = ApplyCurrentPendingAfterRecharge();
	if (bSucceeded) AdvanceToNextValidTurn();
	bSelectingOrResolving = false;
	if (bSucceeded) ActionResolved.Broadcast(ResolvedId);
	return bSucceeded;
}

bool UHSRTurnManager::ApplyCurrentPendingAfterRecharge()
{
	if (!OrderedParticipants.IsValidIndex(CurrentTurnIndex)) return false;
	FHSRBattleParticipant& Current = OrderedParticipants[CurrentTurnIndex];
	float Candidate = Current.RemainingActionDistance + Current.BaseActionDistance;
	if (!FMath::IsFinite(Candidate)) return false;
	for (const FPendingPostActionOperation& Pending : PendingPostActionOperations)
	{
		Candidate = Pending.Kind == EHSRActionDistanceAdjustmentKind::Advance ? FMath::Max(0.0f, Candidate - Pending.Distance) : Candidate + Pending.Distance;
		if (!FMath::IsFinite(Candidate)) return false;
	}
	Current.RemainingActionDistance = FMath::Abs(Candidate) <= DistanceEpsilon ? 0.0f : Candidate;
	PendingPostActionOperations.Empty();
	return true;
}

FHSRActionDistanceResult UHSRTurnManager::RequestActionDistanceAdjustment(const FHSRActionDistanceRequest& Request)
{
	return RequestActionDistanceAdjustmentInternal(Request, false);
}

FHSRActionDistanceResult UHSRTurnManager::RequestActionDistanceAdjustmentInternal(const FHSRActionDistanceRequest& Request, bool bAllowAdmittedDeferredDefeat)
{
	if (!Request.OperationId.IsValid() || Request.TargetParticipantId.IsNone() || !FMath::IsFinite(Request.Ratio) || Request.Ratio < 0.0f || Request.Ratio > 1.0f
		|| (Request.Kind != EHSRActionDistanceAdjustmentKind::Advance && Request.Kind != EHSRActionDistanceAdjustmentKind::Delay)) { UE_LOG(LogTemp, Warning, TEXT("ActionDistance Result=InvalidRequest Op=%s"), *Request.OperationId.ToString()); return MakeAdjustmentResult(EHSRActionDistanceAdjustmentResult::InvalidRequest); }
	if (ConsumedOperationIds.Contains(Request.OperationId)) return MakeAdjustmentResult(EHSRActionDistanceAdjustmentResult::DuplicateOperation);
	ConsumedOperationIds.Add(Request.OperationId);
	if (Request.BattleEpoch != BattleEpoch || BattleEpoch == 0) return MakeAdjustmentResult(EHSRActionDistanceAdjustmentResult::InvalidEpoch);
	if (State == EHSRTurnManagerState::Finished) return MakeAdjustmentResult(EHSRActionDistanceAdjustmentResult::Finished);
	const int32 Index = FindParticipantIndex(Request.TargetParticipantId);
	if (!OrderedParticipants.IsValidIndex(Index)) return MakeAdjustmentResult(EHSRActionDistanceAdjustmentResult::InvalidTarget);
	FHSRBattleParticipant& Target = OrderedParticipants[Index];
	if (!Target.IsValid()) return MakeAdjustmentResult(EHSRActionDistanceAdjustmentResult::InvalidTarget, Index);
	if (!IsParticipantTurnEligible(Target) && !bAllowAdmittedDeferredDefeat) return MakeAdjustmentResult(EHSRActionDistanceAdjustmentResult::DefeatedTarget, Index);
	const float Distance = Target.BaseActionDistance * Request.Ratio;
	if (!FMath::IsFinite(Distance)) return MakeAdjustmentResult(EHSRActionDistanceAdjustmentResult::ArithmeticFailure, Index);
	const FHSRActionDistanceResult Before = MakeAdjustmentResult(EHSRActionDistanceAdjustmentResult::Accepted, Index);
	if (Index == CurrentTurnIndex)
	{
		// A future speed callback can make recharge as large as 10000; prove the entire ordered queue first.
		float Preview = Target.RemainingActionDistance + MaximumBaseActionDistance;
		for (const FPendingPostActionOperation& Pending : PendingPostActionOperations) { Preview = Pending.Kind == EHSRActionDistanceAdjustmentKind::Advance ? FMath::Max(0.0f, Preview - Pending.Distance) : Preview + Pending.Distance; if (!FMath::IsFinite(Preview)) return MakeAdjustmentResult(EHSRActionDistanceAdjustmentResult::ArithmeticFailure, Index); }
		Preview = Request.Kind == EHSRActionDistanceAdjustmentKind::Advance ? FMath::Max(0.0f, Preview - Distance) : Preview + Distance;
		if (!FMath::IsFinite(Preview)) return MakeAdjustmentResult(EHSRActionDistanceAdjustmentResult::ArithmeticFailure, Index);
		PendingPostActionOperations.Add({ Request.Kind, Distance });
	}
	else
	{
		const float Value = Request.Kind == EHSRActionDistanceAdjustmentKind::Advance ? FMath::Max(0.0f, Target.RemainingActionDistance - Distance) : Target.RemainingActionDistance + Distance;
		if (!FMath::IsFinite(Value)) return MakeAdjustmentResult(EHSRActionDistanceAdjustmentResult::ArithmeticFailure, Index);
		Target.RemainingActionDistance = FMath::Abs(Value) <= DistanceEpsilon ? 0.0f : Value;
	}
	return MakeAdjustmentResult(EHSRActionDistanceAdjustmentResult::Accepted, Index, &Before);
}

bool UHSRTurnManager::ConsumeBreakDelay(const FHSRTurnDelayRequest& Request)
{
	FHSRActionDistanceRequest DistanceRequest; DistanceRequest.BattleEpoch = BattleEpoch; DistanceRequest.OperationId = Request.ActionId; DistanceRequest.TargetParticipantId = Request.TargetParticipantId; DistanceRequest.Ratio = 1.0f; DistanceRequest.Kind = EHSRActionDistanceAdjustmentKind::Delay;
	return RequestActionDistanceAdjustment(DistanceRequest).Result == EHSRActionDistanceAdjustmentResult::Accepted;
}

bool UHSRTurnManager::ConsumeAdmittedBreakDelay(const FHSRTurnDelayRequest& Request)
{
	FHSRActionDistanceRequest DistanceRequest; DistanceRequest.BattleEpoch = BattleEpoch; DistanceRequest.OperationId = Request.ActionId; DistanceRequest.TargetParticipantId = Request.TargetParticipantId; DistanceRequest.Ratio = 1.0f; DistanceRequest.Kind = EHSRActionDistanceAdjustmentKind::Delay;
	return RequestActionDistanceAdjustmentInternal(DistanceRequest, true).Result == EHSRActionDistanceAdjustmentResult::Accepted;
}

void UHSRTurnManager::FinishBattle() { PendingPostActionOperations.Empty(); UnbindSpeedDelegates(); State = EHSRTurnManagerState::Finished; CurrentTurnIndex = INDEX_NONE; }
void UHSRTurnManager::Reset() { UnbindSpeedDelegates(); OrderedParticipants.Empty(); PendingPostActionOperations.Empty(); ConsumedOperationIds.Empty(); CurrentTurnIndex = INDEX_NONE; BattleEpoch = 0; TurnSequence = 0; State = EHSRTurnManagerState::Waiting; bSelectingOrResolving = false; }
FName UHSRTurnManager::GetCurrentParticipantId() const { return OrderedParticipants.IsValidIndex(CurrentTurnIndex) ? OrderedParticipants[CurrentTurnIndex].ParticipantId : NAME_None; }
int32 UHSRTurnManager::FindParticipantIndex(FName ParticipantId) const { return OrderedParticipants.IndexOfByPredicate([ParticipantId](const FHSRBattleParticipant& P) { return P.ParticipantId == ParticipantId; }); }

bool UHSRTurnManager::AdvanceToNextValidTurn()
{
	TArray<int32> Eligible;
	float Minimum = TNumericLimits<float>::Max();
	for (int32 I = 0; I < OrderedParticipants.Num(); ++I) if (IsParticipantTurnEligible(OrderedParticipants[I])) { if (!FMath::IsFinite(OrderedParticipants[I].RemainingActionDistance)) { State = EHSRTurnManagerState::Finished; CurrentTurnIndex = INDEX_NONE; return false; } Eligible.Add(I); Minimum = FMath::Min(Minimum, OrderedParticipants[I].RemainingActionDistance); }
	if (Eligible.IsEmpty()) { State = EHSRTurnManagerState::Finished; CurrentTurnIndex = INDEX_NONE; return false; }
	TArray<int32> FrozenCandidates;
	for (int32 I : Eligible) if (FMath::Abs(OrderedParticipants[I].RemainingActionDistance - Minimum) <= DistanceEpsilon) FrozenCandidates.Add(I);
	for (int32 I : Eligible) { float& Value = OrderedParticipants[I].RemainingActionDistance; Value -= Minimum; if (FMath::Abs(Value) <= DistanceEpsilon) Value = 0.0f; }
	FrozenCandidates.Sort([this](int32 A, int32 B) { return OrderedParticipants[A].ParticipantId.LexicalLess(OrderedParticipants[B].ParticipantId); });
	CurrentTurnIndex = FrozenCandidates[0];
	State = OrderedParticipants[CurrentTurnIndex].Team == EHSRBattleParticipantTeam::Player ? EHSRTurnManagerState::PlayerTurn : EHSRTurnManagerState::EnemyTurn;
	++TurnSequence;
	BroadcastLifecycleEvent(EHSRTurnLifecycleEventType::TurnStarted, GetCurrentParticipantId());
	return true;
}

void UHSRTurnManager::BroadcastLifecycleEvent(EHSRTurnLifecycleEventType Type, FName Id)
{
	if (BattleEpoch == 0 || TurnSequence == 0 || Id.IsNone()) return;
	FHSRTurnLifecycleEvent Event; Event.BattleEpoch = BattleEpoch; Event.ParticipantId = Id; Event.TurnSequence = TurnSequence; Event.EventType = Type;
	if (Type == EHSRTurnLifecycleEventType::TurnStarted) TurnStarted.Broadcast(Event); else TurnEnded.Broadcast(Event);
}
bool UHSRTurnManager::IsCurrentParticipantValid() const { return OrderedParticipants.IsValidIndex(CurrentTurnIndex) && IsParticipantTurnEligible(OrderedParticipants[CurrentTurnIndex]); }
bool UHSRTurnManager::IsParticipantTurnEligible(const FHSRBattleParticipant& P) { return P.IsValid() && P.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) > 0.0f; }
FHSRActionDistanceResult UHSRTurnManager::MakeAdjustmentResult(EHSRActionDistanceAdjustmentResult Result, int32 Index, const FHSRActionDistanceResult* OldSnapshot) const { FHSRActionDistanceResult Out; Out.Result = Result; Out.BattleEpoch = BattleEpoch; Out.TurnSequence = TurnSequence; Out.CurrentParticipantId = GetCurrentParticipantId(); Out.NextParticipantId = GetCurrentParticipantId(); Out.PendingOperationCount = PendingPostActionOperations.Num(); Out.OldPendingOperationCount = Out.NewPendingOperationCount = Out.PendingOperationCount; if (OrderedParticipants.IsValidIndex(Index)) { const FHSRBattleParticipant& P = OrderedParticipants[Index]; Out.OldBase = P.BaseActionDistance; Out.NewBase = P.BaseActionDistance; Out.OldRemaining = P.RemainingActionDistance; Out.NewRemaining = P.RemainingActionDistance; } if (OldSnapshot) { Out.OldBase = OldSnapshot->OldBase; Out.OldRemaining = OldSnapshot->OldRemaining; Out.OldPendingOperationCount = OldSnapshot->PendingOperationCount; } return Out; }
#if WITH_EDITOR
bool UHSRTurnManager::InvalidateCurrentParticipantForDevelopmentTest() { if (!OrderedParticipants.IsValidIndex(CurrentTurnIndex)) return false; OrderedParticipants[CurrentTurnIndex].Actor.Reset(); return true; }
#endif
#if WITH_DEV_AUTOMATION_TESTS
bool UHSRTurnManager::GetActionDistanceForAutomation(FName ParticipantId, float& OutSpeed, float& OutBase, float& OutRemaining, int32& OutPending) const
{
	const int32 Index = FindParticipantIndex(ParticipantId); if (!OrderedParticipants.IsValidIndex(Index)) return false;
	const FHSRBattleParticipant& P = OrderedParticipants[Index]; OutSpeed = P.EffectiveSpeed; OutBase = P.BaseActionDistance; OutRemaining = P.RemainingActionDistance; OutPending = PendingPostActionOperations.Num(); return true;
}
#endif
