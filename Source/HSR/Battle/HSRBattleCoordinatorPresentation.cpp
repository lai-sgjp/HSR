#include "HSRBattleCoordinator.h"
#include "HSRCombatMotionComponent.h"
#include "HSRTurnManager.h"
#include "HSRBattleStage.h"
#include "EngineUtils.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Data/HSRSkillDefinition.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Animation/AnimSequenceBase.h"

FHSRAbilityResolution UHSRBattleCoordinator::SubmitBattleCommand(const FHSRBattleActionCommand& Command)
{
	return PresentationWorld.IsValid() ? QueuePresentedAction(Command) : RequestAction(Command);
}

FHSRAbilityResolution UHSRBattleCoordinator::QueuePresentedAction(const FHSRBattleActionCommand& Command)
{
	if (const auto* Previous = ProcessedActionResolutions.Find(Command.ActionId)) return *Previous;
	FHSRAbilityResolution Result;
	Result.ActionId = Command.ActionId;
	Result.ActorParticipantId = Command.ActorParticipantId;
	Result.SkillId = Command.SkillId;
	if (PresentedAction.IsSet())
	{
		if (PresentedAction->ActionId == Command.ActionId) Result.Status = EHSRAbilityResolutionStatus::Accepted;
		else Result.FailureReason = EHSRAbilityFailureReason::NotCurrentActor;
		return Result;
	}
	const FHSRBattleCommandViewState View = GetCommandViewState();
	const FHSRBattleCommandSkillView* Skill = View.FindSkill(Command.SkillId);
	if (!PresentationWorld.IsValid() || !Command.ActionId.IsValid() || Command.BattleId != CurrentRequestId
		|| CurrentState != EHSRBattleCoordinatorState::Spawned || Command.ActorParticipantId != View.CurrentActorId)
	{
		Result.FailureReason = EHSRAbilityFailureReason::InvalidBattle;
		return Result;
	}
	if (!Skill || !Skill->bAvailable || Command.TargetParticipantIds.Num() != 1
		|| !Skill->CandidateTargetIds.Contains(Command.TargetParticipantIds[0]))
	{
		Result.FailureReason = Skill && !Skill->bAvailable ? Skill->DisabledReason : EHSRAbilityFailureReason::InvalidTarget;
		return Result;
	}
	PresentedAction = Command;
	PresentedTurnToResolve = NAME_None;
	for (TActorIterator<AHSRBattleStage> It(PresentationWorld.Get()); It; ++It) It->ShowAction();
	PlayActionMotion(Command, false);
	PublishCommandViewState();
	// Rule settlement has a fixed bounded windup, independent of animation notifies.
	PresentationWorld->GetTimerManager().SetTimer(PresentationTimer, this, &ThisClass::ApplyPresentedAction, .4f, false);
	Result.Status = EHSRAbilityResolutionStatus::Accepted;
	return Result;
}

void UHSRBattleCoordinator::ApplyPresentedAction()
{
	if (!PresentedAction.IsSet() || !PresentationWorld.IsValid()) return;
	const FHSRBattleActionCommand Command = PresentedAction.GetValue();
	FHSRAbilityResolution Result;
	{
		TGuardValue<bool> Applying(bApplyingPresentedAction, true);
		Result = RequestAction(Command);
	}
	if (Result.Succeeded()) PlayActionMotion(Command, true);
	else if (const auto* Actor = FindParticipant(Command.ActorParticipantId))
	{
		// A target may become invalid during windup. An automated enemy must not
		// strand the consumed turn key; release it without applying another action.
		if (Actor->Team == EHSRBattleParticipantTeam::Enemy) PresentedTurnToResolve = Actor->ParticipantId;
	}
	if (!PresentedAction.IsSet()) return;
	PresentationWorld->GetTimerManager().SetTimer(PresentationTimer, this, &ThisClass::FinishPresentedAction, 1.f, false);
}

bool UHSRBattleCoordinator::ResolveActionOrWait(FName ActorId)
{
	if (bApplyingPresentedAction && PresentedAction.IsSet())
	{
		PresentedTurnToResolve = ActorId;
		return true;
	}
	return TurnManager && TurnManager->ResolveAction(ActorId);
}

void UHSRBattleCoordinator::FinishPresentedAction()
{
	if (!PresentedAction.IsSet()) return;
	PresentedAction.Reset();
	const FName ActorId = PresentedTurnToResolve;
	PresentedTurnToResolve = NAME_None;
	if (CurrentState == EHSRBattleCoordinatorState::Spawned && TurnManager)
	{
		if (!ActorId.IsNone() && TurnManager->GetCurrentParticipantId() == ActorId) TurnManager->ResolveAction(ActorId);
		PublishCommandViewState();
		DrainPendingEnemyTurns();
	}
	else if (bBattleResultProduced)
	{
		BattleResultReady.Broadcast(BattleResult);
	}
}

void UHSRBattleCoordinator::PlayActionMotion(const FHSRBattleActionCommand& Command, bool bImpact)
{
	const UHSRSkillDefinition* Skill = FindSkillDefinition(Command.SkillId);
	const bool bHealing = Skill && Skill->RestoresHealth();
	const FHSRBattleParticipant* Source = FindParticipant(Command.ActorParticipantId);
	const FHSRBattleParticipant* Target = Command.TargetParticipantIds.IsEmpty() ? nullptr : FindParticipant(Command.TargetParticipantIds[0]);
	const FHSRBattleParticipant* Subject = bImpact ? Target : Source;
	if (!Subject || !Subject->Actor.IsValid() || !Source || !Target || (bImpact && bHealing)) return;
	AActor* Actor = Subject->Actor.Get();
	auto* Motion = Actor->FindComponentByClass<UHSRCombatMotionComponent>();
	if (!Motion)
	{
		Motion = NewObject<UHSRCombatMotionComponent>(Actor);
		Actor->AddInstanceComponent(Motion);
		Motion->RegisterComponent();
	}
	const UHSRCharacterDefinition* Definition = nullptr;
	if (auto* GI = Actor->GetGameInstance())
		if (auto* Profiles = GI->GetSubsystem<UHSRCharacterProfileSubsystem>()) Profiles->GetDefinition(Subject->DefinitionId, Definition);
	UAnimSequenceBase* Animation = nullptr;
	if (Definition)
		Animation = bImpact ? (Subject->IsAlive() ? Definition->HitAnimation.LoadSynchronous() : Definition->DefeatAnimation.LoadSynchronous()) : Definition->AttackAnimation.LoadSynchronous();
	if (!bImpact && Definition && Skill && Skill->Category != EHSRSkillCategory::BasicAttack)
		if (auto* SkillAnimation = Definition->SkillAnimation.LoadSynchronous()) Animation = SkillAnimation;
	if (bImpact) Motion->React(Animation, !Subject->IsAlive());
	else Motion->Attack(Target->Actor.Get(), Animation, !bHealing && (!Definition || Definition->bMeleeBasicAttack) && (!Skill || Skill->Category == EHSRSkillCategory::BasicAttack));
}
