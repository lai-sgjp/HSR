#include "HSRBattleCoordinator.h"
#include "HSRTurnManager.h"
#include "HSRTargetingPolicy.h"
#include "../Data/HSRSkillDefinition.h"
#include "../GAS/Ability/HSRGameplayAbilityBase.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"
#include "../GAS/HSRCombatEnergyEffect.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogHSRUltimate, Log, All);

const UHSRSkillDefinition* UHSRBattleCoordinator::FindParticipantSkill(FName ActorId, FName SkillId) const
{
	for (const auto& Entry : GetSkillLoadoutFor(ActorId))
		if (Entry && Entry->SkillId == SkillId) return Entry;
	return nullptr;
}

EHSRAbilityFailureReason UHSRBattleCoordinator::ValidateUltimate(const FHSRBattleActionCommand& Command) const
{
	if (!IsInGameThread() || !Command.ActionId.IsValid() || Command.BattleId != CurrentRequestId
		|| CurrentState != EHSRBattleCoordinatorState::Spawned || !TurnManager || bBattleResultProduced)
		return EHSRAbilityFailureReason::InvalidBattle;
	const auto* Actor = FindParticipant(Command.ActorParticipantId);
	if (!Actor || !Actor->IsAlive()) return EHSRAbilityFailureReason::AbilityUnavailable;
	const auto* Definition = FindParticipantSkill(Command.ActorParticipantId, Command.SkillId);
	if (!Definition || Definition->Category != EHSRSkillCategory::Ultimate || !Definition->IsValidForCategory())
		return EHSRAbilityFailureReason::DefinitionMissing;
	if (!FHSRTargetingPolicy::ValidateTargetIds(*Definition, *Actor, Participants, Command.TargetParticipantIds))
		return EHSRAbilityFailureReason::InvalidTarget;
	const auto* Target = FindParticipant(Command.TargetParticipantIds[0]);
	auto* ASC = Actor->AbilitySystemComponent.Get();
	auto* Spec = ASC->FindAbilitySpecFromClass(Definition->AbilityClass);
	auto* Ability = Spec ? Cast<UHSRGameplayAbilityBase>(Spec->GetPrimaryInstance()) : nullptr;
	if (!Ability || Spec->IsActive() || !Target || !Target->AbilitySystemComponent.IsValid())
		return EHSRAbilityFailureReason::AbilityUnavailable;
	const auto Failure = Ability->GetAvailabilityFailureReason(Spec->Handle, ASC->AbilityActorInfo.Get(), Target->AbilitySystemComponent.Get());
	if (Failure != EHSRAbilityFailureReason::None) return Failure;
	if (!Ability->CanActivateAbility(Spec->Handle, ASC->AbilityActorInfo.Get())) return EHSRAbilityFailureReason::AbilityUnavailable;
	if (Definition->RestoresHealth())
	{
		bool bWounded = false;
		for (FName Id : Command.TargetParticipantIds)
		{
			const auto* P = FindParticipant(Id);
			if (P && P->AbilitySystemComponent.IsValid())
				bWounded |= P->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) < P->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetMaxHealthAttribute());
		}
		if (!bWounded) return EHSRAbilityFailureReason::AlreadyAtFullHealth;
	}
	return EHSRAbilityFailureReason::None;
}

void UHSRBattleCoordinator::TraceUltimate(const TCHAR* Event, const FHSRUltimateRequestRecord& Request) const
{
	FString AV;
	if (TurnManager)
		for (const auto& P : TurnManager->GetOrderedParticipants())
			AV += FString::Printf(TEXT("%s:%.4f "), *P.ParticipantId.ToString(), P.RemainingActionDistance);
	const FString Target = Request.Command.TargetParticipantIds.IsEmpty() ? TEXT("None") : Request.Command.TargetParticipantIds[0].ToString();
	UE_LOG(LogHSRUltimate, Verbose, TEXT("Event=%s Generation=%llu Battle=%s Request=%s Sequence=%llu Actor=%s Target=%s State=%d Reason=%d NormalActor=%s AV=[%s]"),
		Event, Request.BattleGeneration, *Request.Command.BattleId.ToString(), *Request.Command.ActionId.ToString(), Request.AcceptedSequence,
		*Request.Command.ActorParticipantId.ToString(), *Target, int32(Request.State), int32(Request.FailureReason),
		TurnManager ? *TurnManager->GetCurrentParticipantId().ToString() : TEXT("None"), *AV);
}

FHSRAbilityResolution UHSRBattleCoordinator::RequestUltimate(const FHSRBattleActionCommand& Command)
{
	if (bResetRequested || !IsInGameThread())
	{
		FHSRAbilityResolution Result; Result.ActionId = Command.ActionId;
		Result.FailureReason = EHSRAbilityFailureReason::InvalidBattle;
		return Result;
	}
	FHSRAbilityResolution Result;
	Result.ActionId = Command.ActionId; Result.ActorParticipantId = Command.ActorParticipantId; Result.SkillId = Command.SkillId;
	for (const auto& Record : UltimateRequestHistory)
		if (Record.Command.ActionId == Command.ActionId)
		{
			if (Record.Command.BattleId != Command.BattleId || Record.Command.ActorParticipantId != Command.ActorParticipantId
				|| Record.Command.SkillId != Command.SkillId || Record.Command.TargetParticipantIds != Command.TargetParticipantIds)
			{
				Result.FailureReason = EHSRAbilityFailureReason::DuplicateAction;
				return Result;
			}
			if (const auto* Final = ProcessedActionResolutions.Find(Command.ActionId)) return *Final;
			Result.Status = EHSRAbilityResolutionStatus::Accepted;
			return Result;
		}
	if (ProcessedActionResolutions.Contains(Command.ActionId))
	{
		Result.FailureReason = EHSRAbilityFailureReason::DuplicateAction;
		return Result;
	}
	const bool bAlreadyPending = (ActiveUltimate.IsSet() && ActiveUltimate->Command.ActorParticipantId == Command.ActorParticipantId)
		|| PendingUltimates.ContainsByPredicate([&](const auto& R) { return R.Command.ActorParticipantId == Command.ActorParticipantId; });
	Result.FailureReason = bAlreadyPending ? EHSRAbilityFailureReason::DuplicateAction : ValidateUltimate(Command);
	if (Result.FailureReason != EHSRAbilityFailureReason::None)
	{
		FHSRUltimateRequestRecord Rejected;
		Rejected.Command = Command; Rejected.BattleGeneration = BattleGeneration; Rejected.FailureReason = Result.FailureReason;
		TraceUltimate(TEXT("UltimateRequestRejected"), Rejected);
		LastActionResolution = Result;
		PublishCommandViewState();
		return Result;
	}
	FHSRUltimateRequestRecord Record;
	Record.Command = Command; Record.BattleGeneration = BattleGeneration; Record.AcceptedSequence = ++NextUltimateSequence;
	PendingUltimates.Add(Record);
	UltimateRequestHistory.Add(Record);
	bExtraActionChain = true;
	TraceUltimate(TEXT("UltimateRequestAccepted"), Record);
	Result.Status = EHSRAbilityResolutionStatus::Accepted;
	PublishCommandViewState();
	PumpBattleFlow();
	if (const auto* Final = ProcessedActionResolutions.Find(Command.ActionId)) return *Final;
	return Result;
}

void UHSRBattleCoordinator::CompleteActiveUltimate(const FHSRAbilityResolution& Result)
{
	if (!ActiveUltimate.IsSet() || ActiveUltimate->Command.ActionId != Result.ActionId) return;
	FHSRUltimateRequestRecord Record = ActiveUltimate.GetValue();
	ActiveUltimate.Reset();
	Record.State = Result.Succeeded() ? EHSRUltimateRequestState::Completed : EHSRUltimateRequestState::Cancelled;
	Record.FailureReason = Result.FailureReason;
	for (auto& History : UltimateRequestHistory)
		if (History.Command.ActionId == Record.Command.ActionId) { History = Record; break; }
	FHSRAbilityResolution Final = Result;
	if (!Result.Succeeded()) Final.Status = EHSRAbilityResolutionStatus::Cancelled;
	ProcessedActionResolutions.Add(Final.ActionId, Final);
	LastActionResolution = Final;
	TraceUltimate(Result.Succeeded() ? TEXT("UltimateExecutionCompleted") : TEXT("UltimateExecutionCancelled"), Record);
}

void UHSRBattleCoordinator::CancelUltimateRequests(FName ActorId)
{
	for (int32 Index = PendingUltimates.Num() - 1; Index >= 0; --Index)
	{
		if (!ActorId.IsNone() && PendingUltimates[Index].Command.ActorParticipantId != ActorId) continue;
		auto Record = PendingUltimates[Index];
		PendingUltimates.RemoveAt(Index);
		Record.State = EHSRUltimateRequestState::Cancelled;
		Record.FailureReason = ActorId.IsNone() ? EHSRAbilityFailureReason::InvalidBattle : EHSRAbilityFailureReason::AbilityUnavailable;
		for (auto& H : UltimateRequestHistory) if (H.Command.ActionId == Record.Command.ActionId) H = Record;
		FHSRAbilityResolution Result;
		Result.ActionId = Record.Command.ActionId; Result.ActorParticipantId = Record.Command.ActorParticipantId; Result.SkillId = Record.Command.SkillId;
		Result.Status = EHSRAbilityResolutionStatus::Cancelled; Result.FailureReason = Record.FailureReason;
		ProcessedActionResolutions.Add(Result.ActionId, Result);
		LastActionResolution = Result;
		TraceUltimate(TEXT("UltimateExecutionCancelled"), Record);
	}
	// A committed successful transaction is retained; cancellation cannot undo settled damage.
	if (ActiveUltimate.IsSet() && (ActorId.IsNone() || ActiveUltimate->Command.ActorParticipantId == ActorId)
		&& !bExecutingUltimateCore)
	{
		FHSRAbilityResolution Result;
		if (const auto* Existing = ProcessedActionResolutions.Find(ActiveUltimate->Command.ActionId)) Result = *Existing;
		else { Result.ActionId = ActiveUltimate->Command.ActionId; Result.FailureReason = EHSRAbilityFailureReason::InvalidBattle; }
		CompleteActiveUltimate(Result);
	}
}

void UHSRBattleCoordinator::PumpBattleFlow()
{
	if (bResetRequested && RequestActionDispatchDepth == 0 && !bPumpingBattleFlow && !bFormalDamageTransactionOpen) { Reset(); return; }
	if (bPumpingBattleFlow || RequestActionDispatchDepth != 0 || PresentedAction.IsSet() || bFormalDamageTransactionOpen) return;
	{
	TGuardValue<bool> Pumping(bPumpingBattleFlow, true);
	while (CurrentState == EHSRBattleCoordinatorState::Spawned && !PresentedAction.IsSet())
	{
		for (const auto& P : Participants)
			if (!P.bDefeated && !P.IsAlive()) ResolveDefeat(P.ParticipantId);
		if (CurrentState != EHSRBattleCoordinatorState::Spawned) break;
		if (!PendingUltimates.IsEmpty())
		{
			ActiveUltimate = PendingUltimates[0];
			PendingUltimates.RemoveAt(0);
			const auto Failure = ActiveUltimate->BattleGeneration == BattleGeneration ? ValidateUltimate(ActiveUltimate->Command) : EHSRAbilityFailureReason::InvalidBattle;
			if (Failure != EHSRAbilityFailureReason::None)
			{
				FHSRAbilityResolution Result;
				Result.ActionId = ActiveUltimate->Command.ActionId; Result.ActorParticipantId = ActiveUltimate->Command.ActorParticipantId;
				Result.SkillId = ActiveUltimate->Command.SkillId; Result.FailureReason = Failure;
				CompleteActiveUltimate(Result);
				continue;
			}
			ActiveUltimate->State = EHSRUltimateRequestState::Executing;
			for (auto& H : UltimateRequestHistory) if (H.Command.ActionId == ActiveUltimate->Command.ActionId) H = ActiveUltimate.GetValue();
			TraceUltimate(TEXT("UltimateExecutionStarted.SafePoint"), ActiveUltimate.GetValue());
			const auto Command = ActiveUltimate->Command;
			if (PresentationWorld.IsValid()) { QueuePresentedAction(Command); break; }
			FHSRAbilityResolution Result;
			{
				TGuardValue<bool> Executing(bExecutingUltimateCore, true);
				TGuardValue<int32> Dispatching(RequestActionDispatchDepth, RequestActionDispatchDepth + 1);
				Result = RequestActionCore(Command);
			}
			CompleteActiveUltimate(Result);
			continue;
		}
		const FName FinalizeActor = SuspendedNormalFinalization;
		SuspendedNormalFinalization = NAME_None;
		if (TurnManager && !FinalizeActor.IsNone() && TurnManager->GetCurrentParticipantId() == FinalizeActor)
		{
			const auto* Actor = FindParticipant(FinalizeActor);
			if (Actor && Actor->IsAlive()) TurnManager->ResolveAction(FinalizeActor);
			else TurnManager->AdvancePastInvalidCurrentTurn();
			continue;
		}
		if (TurnManager)
		{
			const auto* Current = FindParticipant(TurnManager->GetCurrentParticipantId());
			if (!Current || !Current->IsAlive()) { if (TurnManager->AdvancePastInvalidCurrentTurn()) continue; }
		}
		if (bExtraActionChain && !UltimateRequestHistory.IsEmpty())
		{
			bExtraActionChain = false;
			TraceUltimate(TEXT("NormalTurnResumed"), UltimateRequestHistory.Last());
		}
		break;
	}
	if (CurrentState != EHSRBattleCoordinatorState::Spawned) CancelUltimateRequests();
	PublishCommandViewState();
	}
	if (bResetRequested) { Reset(); return; }
	if (!PresentedAction.IsSet()) DrainPendingEnemyTurns();
}

void UHSRBattleCoordinator::ApplyCombatEnergy(UAbilitySystemComponent& ASC, float Delta)
{
	if (!FMath::IsFinite(Delta) || Delta <= 0.f) return;
	auto Spec = ASC.MakeOutgoingSpec(UHSRCombatEnergyEffect::StaticClass(), 1.f, ASC.MakeEffectContext());
	if (!Spec.IsValid()) return;
	Spec.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(TEXT("Resource.EnergyDelta")), Delta);
	ASC.ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

void UHSRBattleCoordinator::FinalizeDeferredDefeats()
{
	const auto Defeats = MoveTemp(PendingDefeatedParticipantIds);
	PendingDefeatedParticipantIds.Reset();
	for (int32 Index = 0; Index < Defeats.Num(); ++Index)
	{
		// Register every casualty before publishing the single terminal result.
		TGuardValue<bool> Defer(bDeferringTerminalDefeat, Index + 1 < Defeats.Num());
		ResolveDefeat(Defeats[Index]);
	}
}

void UHSRBattleCoordinator::AppendUltimateCommandViews(FHSRBattleCommandViewState& State) const
{
	if (ActiveUltimate.IsSet()) State.ExecutingUltimateActorId = ActiveUltimate->Command.ActorParticipantId;
	for (const auto& Actor : Participants)
	{
		if (Actor.Team != EHSRBattleParticipantTeam::Player) continue;
		for (const auto& Entry : GetSkillLoadoutFor(Actor.ParticipantId))
		{
			if (!Entry || Entry->Category != EHSRSkillCategory::Ultimate) continue;
			FHSRBattleCommandSkillView View;
			View.ActorParticipantId = Actor.ParticipantId;
			View.SkillId = Entry->SkillId; View.Category = Entry->Category; View.TargetType = Entry->TargetType;
			View.DisplayName = Entry->DisplayName.IsEmpty() ? FText::FromName(Entry->SkillId) : Entry->DisplayName;
			View.Description = Entry->Description; View.bDescriptionIsPlaceholder = Entry->Description.IsEmpty();
			View.bEnergyCostIsKnown = Entry->TryGetDisplayEnergyCost(View.EnergyCost);
			View.CandidateTargetIds = FHSRTargetingPolicy::BuildCandidateTargetIds(*Entry, Actor, Participants);
			if (Entry->RestoresHealth() && Entry->TargetType != EHSRTargetType::AllAllies)
				View.CandidateTargetIds.RemoveAll([this](FName Id)
				{
					const auto* P = FindParticipant(Id);
					return !P || !P->AbilitySystemComponent.IsValid() || P->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) >= P->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetMaxHealthAttribute());
				});
			const auto* Pending = PendingUltimates.FindByPredicate([&](const auto& R) { return R.Command.ActorParticipantId == Actor.ParticipantId; });
			View.bExecuting = ActiveUltimate.IsSet() && ActiveUltimate->Command.ActorParticipantId == Actor.ParticipantId;
			View.bPending = Pending != nullptr;
			View.AcceptedSequence = Pending ? int64(Pending->AcceptedSequence) : View.bExecuting ? int64(ActiveUltimate->AcceptedSequence) : 0;
			FHSRBattleActionCommand Preview;
			Preview.ActionId = FGuid::NewGuid(); Preview.BattleId = CurrentRequestId;
			Preview.ActorParticipantId = Actor.ParticipantId; Preview.SkillId = Entry->SkillId;
			if (Entry->TargetType == EHSRTargetType::AllAllies || Entry->TargetType == EHSRTargetType::AllEnemies) Preview.TargetParticipantIds = View.CandidateTargetIds;
			else if (!View.CandidateTargetIds.IsEmpty()) Preview.TargetParticipantIds.Add(View.CandidateTargetIds[0]);
			View.DisabledReason = (View.bPending || View.bExecuting) ? EHSRAbilityFailureReason::DuplicateAction : ValidateUltimate(Preview);
			View.bAvailable = View.DisabledReason == EHSRAbilityFailureReason::None;
			State.UltimateSkills.Add(View);
		}
	}
}

void UHSRBattleCoordinator::SchedulePresentationPhase(bool bImpact, float Delay)
{
	if (!PresentedAction.IsSet() || !PresentationWorld.IsValid()) return;
	const uint64 Generation = BattleGeneration;
	const FGuid Id = PresentedAction->ActionId;
	FTimerDelegate Callback;
	Callback.BindWeakLambda(this, [this, Generation, Id, bImpact]()
	{
		if (Generation != BattleGeneration || !PresentedAction.IsSet() || PresentedAction->ActionId != Id) return;
		if (bImpact) ApplyPresentedAction(); else FinishPresentedAction();
	});
	PresentationWorld->GetTimerManager().SetTimer(PresentationTimer, Callback, Delay, false);
}
