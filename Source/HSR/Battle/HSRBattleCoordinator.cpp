#include "HSRBattleCoordinator.h"
#include "HSRTurnManager.h"
#include "HSREncounterTypes.h"
#include "HSRTargetingPolicy.h"
#include "../GAS/Ability/HSRGameplayAbilityBase.h"
#include "../GAS/HSRAbilitySystemComponent.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "GameplayEffectTypes.h"
#include "../Data/HSRSkillDefinition.h"
#include "../GAS/Damage/HSRDamageEffectContext.h"
#include "../GAS/Damage/HSRDamageRuleDefinition.h"
#include "GameplayEffect.h"

bool UHSRBattleCoordinator::SubmitBattleRequest(const FHSREncounterRequest& InRequest)
{
	if (CurrentState != EHSRBattleCoordinatorState::Idle)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::SubmitBattleRequest - REJECTED state=%d RequestId=%s (expected Idle)"),
			static_cast<int32>(CurrentState), *InRequest.RequestId.ToString());
		return false;
	}

	if (!InRequest.RequestId.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::SubmitBattleRequest - REJECTED invalid RequestId"));
		return false;
	}

	if (InRequest.EncounterId.IsNone())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::SubmitBattleRequest - REJECTED EncounterId=None RequestId=%s"),
			*InRequest.RequestId.ToString());
		return false;
	}

	if (InRequest.EnemyDefinitionId.IsNone())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::SubmitBattleRequest - REJECTED EnemyDefinitionId=None RequestId=%s"),
			*InRequest.RequestId.ToString());
		return false;
	}

	if (InRequest.BattleMapPath.IsNone())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::SubmitBattleRequest - REJECTED BattleMapPath=None RequestId=%s"),
			*InRequest.RequestId.ToString());
		return false;
	}

	// Build pure-value ReturnContext from the consumed request
	FHSRBattleReturnContext RetCtx;
	RetCtx.RequestId = InRequest.RequestId;
	RetCtx.ExplorationMapPath = InRequest.ExplorationMapPath;
	RetCtx.ReturnTransform = InRequest.ReturnTransform;

	// Capture state atomically
	CurrentRequestId = InRequest.RequestId;
	CurrentEncounterId = InRequest.EncounterId;
	CurrentEnemyDefinitionId = InRequest.EnemyDefinitionId;
	ReturnContext = RetCtx;
	CurrentState = EHSRBattleCoordinatorState::Consuming;

	UE_LOG(LogTemp, Log,
		TEXT("UHSRBattleCoordinator::SubmitBattleRequest - SUCCESS RequestId=%s EncounterId=%s EnemyDefId=%s BattleMap=%s ExplorationMap=%s"),
		*InRequest.RequestId.ToString(), *InRequest.EncounterId.ToString(),
		*InRequest.EnemyDefinitionId.ToString(), *InRequest.BattleMapPath.ToString(),
		*InRequest.ExplorationMapPath.ToString());

	return true;
}

FHSRBattleInitResult UHSRBattleCoordinator::BuildParticipants(UWorld* BattleWorld)
{
	if (CurrentState != EHSRBattleCoordinatorState::Consuming)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::BuildParticipants - REJECTED state=%d RequestId=%s (expected Consuming)"),
			static_cast<int32>(CurrentState), *CurrentRequestId.ToString());
		return FHSRBattleInitResult::MakeFailure(
			EHSRBattleInitFailureType::DefinitionNotFound,
			FText::FromString(TEXT("Coordinator is not in Consuming state."))
		);
	}

	if (!BattleWorld)
	{
		UE_LOG(LogTemp, Error,
			TEXT("UHSRBattleCoordinator::BuildParticipants - FAILED BattleWorld=null RequestId=%s EncounterId=%s"),
			*CurrentRequestId.ToString(), *CurrentEncounterId.ToString());
		CurrentState = EHSRBattleCoordinatorState::Failed;
		return FHSRBattleInitResult::MakeFailure(
			EHSRBattleInitFailureType::SpawnFailed,
			FText::FromString(TEXT("Battle World is null."))
		);
	}

	// Build and validate participant definitions from request
	FHSRBattleInitResult DefResult = BuildAndValidateParticipantDefinitions();
	if (!DefResult.IsSuccess())
	{
		CurrentState = EHSRBattleCoordinatorState::Failed;
		return DefResult;
	}

	Participants.Empty();

	// Spawn each participant from its definition
	for (const auto& Def : ParticipantDefinitions)
	{
		AActor* SpawnedActor = SpawnParticipantActor(BattleWorld, Def);
		if (!SpawnedActor)
		{
			UE_LOG(LogTemp, Error,
				TEXT("UHSRBattleCoordinator::BuildParticipants - FAILED spawn ParticipantId=%s DefId=%s RequestId=%s"),
				*Def.ParticipantId.ToString(), *Def.DefinitionId.ToString(), *CurrentRequestId.ToString());
			// Cleanup previously spawned
			for (auto& P : Participants) { if (P.Actor.IsValid()) P.Actor->Destroy(); }
			Participants.Empty();
			ParticipantDefinitions.Empty();
			CurrentState = EHSRBattleCoordinatorState::Failed;
			return FHSRBattleInitResult::MakeFailure(
				EHSRBattleInitFailureType::SpawnFailed,
				FText::FromString(TEXT("Failed to spawn participant actor.")),
				Def.DefinitionId);
		}

		if (!InitParticipantASC(SpawnedActor))
		{
			UE_LOG(LogTemp, Error,
				TEXT("UHSRBattleCoordinator::BuildParticipants - FAILED InitASC ParticipantId=%s DefId=%s RequestId=%s"),
				*Def.ParticipantId.ToString(), *Def.DefinitionId.ToString(), *CurrentRequestId.ToString());
			SpawnedActor->Destroy();
			for (auto& P : Participants) { if (P.Actor.IsValid()) P.Actor->Destroy(); }
			Participants.Empty();
			ParticipantDefinitions.Empty();
			CurrentState = EHSRBattleCoordinatorState::Failed;
			return FHSRBattleInitResult::MakeFailure(
				EHSRBattleInitFailureType::InitFailed,
				FText::FromString(TEXT("Failed to initialize ASC on participant.")),
				Def.DefinitionId);
		}

		FHSRBattleParticipant Participant;
		Participant.ParticipantId = Def.ParticipantId;
		Participant.DefinitionId = Def.DefinitionId;
		Participant.Team = Def.Team;
		Participant.Actor = SpawnedActor;
		Participant.AbilitySystemComponent = SpawnedActor->FindComponentByClass<UAbilitySystemComponent>();
		if (!GrantBasicAttackAbility(Participant))
		{
			UE_LOG(LogTemp, Error, TEXT("UHSRBattleCoordinator::BuildParticipants - FAILED to grant BasicAttack ParticipantId=%s"), *Participant.ParticipantId.ToString());
			SpawnedActor->Destroy();
			for (FHSRBattleParticipant& ExistingParticipant : Participants) { if (ExistingParticipant.Actor.IsValid()) ExistingParticipant.Actor->Destroy(); }
			Participants.Empty();
			ParticipantDefinitions.Empty();
			CurrentState = EHSRBattleCoordinatorState::Failed;
			return FHSRBattleInitResult::MakeFailure(EHSRBattleInitFailureType::InitFailed, FText::FromString(TEXT("Failed to grant BasicAttack ability.")), Def.DefinitionId);
		}
		if (UltimateDefinition && !GrantUltimateAbility(Participant))
		{
			UE_LOG(LogTemp, Error, TEXT("UHSRBattleCoordinator::BuildParticipants - FAILED to grant Ultimate ParticipantId=%s"), *Participant.ParticipantId.ToString());
			SpawnedActor->Destroy();
			for (FHSRBattleParticipant& ExistingParticipant : Participants) { if (ExistingParticipant.Actor.IsValid()) ExistingParticipant.Actor->Destroy(); }
			Participants.Empty();
			ParticipantDefinitions.Empty();
			CurrentState = EHSRBattleCoordinatorState::Failed;
			return FHSRBattleInitResult::MakeFailure(EHSRBattleInitFailureType::InitFailed, FText::FromString(TEXT("Failed to grant Ultimate ability.")), Def.DefinitionId);
		}
		if (SkillDefinition && !GrantSkillAbility(Participant))
		{
			UE_LOG(LogTemp, Error, TEXT("UHSRBattleCoordinator::BuildParticipants - FAILED to grant Skill ParticipantId=%s"), *Participant.ParticipantId.ToString());
			SpawnedActor->Destroy();
			for (FHSRBattleParticipant& ExistingParticipant : Participants) { if (ExistingParticipant.Actor.IsValid()) ExistingParticipant.Actor->Destroy(); }
			Participants.Empty(); ParticipantDefinitions.Empty(); CurrentState = EHSRBattleCoordinatorState::Failed;
			return FHSRBattleInitResult::MakeFailure(EHSRBattleInitFailureType::InitFailed, FText::FromString(TEXT("Failed to grant Skill ability.")), Def.DefinitionId);
		}
		if (HealDefinition && !GrantHealAbility(Participant)) { SpawnedActor->Destroy(); CurrentState=EHSRBattleCoordinatorState::Failed; return FHSRBattleInitResult::MakeFailure(EHSRBattleInitFailureType::InitFailed,FText::FromString(TEXT("Failed to grant Heal ability.")),Def.DefinitionId); }
		Participants.Add(Participant);
		BindHealthObserver(Participant);

		UE_LOG(LogTemp, Log,
			TEXT("UHSRBattleCoordinator::BuildParticipants - Spawned ParticipantId=%s DefId=%s Team=%d Actor=%s ASC=%s"),
			*Def.ParticipantId.ToString(), *Def.DefinitionId.ToString(),
			static_cast<int32>(Def.Team),
			*SpawnedActor->GetName(),
			Participant.AbilitySystemComponent.IsValid() ? *Participant.AbilitySystemComponent->GetName() : TEXT("null"));
	}

	TurnManager = NewObject<UHSRTurnManager>(this);
	if (!TurnManager || !TurnManager->Initialize(Participants))
	{
		UE_LOG(LogTemp, Error, TEXT("UHSRBattleCoordinator::BuildParticipants - FAILED to initialize TurnManager RequestId=%s"), *CurrentRequestId.ToString());
		for (FHSRBattleParticipant& Participant : Participants) { if (Participant.Actor.IsValid()) Participant.Actor->Destroy(); }
		Participants.Empty();
		TurnManager = nullptr;
		CurrentState = EHSRBattleCoordinatorState::Failed;
		return FHSRBattleInitResult::MakeFailure(EHSRBattleInitFailureType::InitFailed, FText::FromString(TEXT("Failed to initialize TurnManager.")));
	}

	// Atomically transition to Spawned
	CurrentState = EHSRBattleCoordinatorState::Spawned;
	DevelopmentDamageRandomStream.Initialize(DevelopmentDamageSeed);
	DevelopmentDamageConsumeCount = 0;
	DevelopmentDamageResults.Empty();
	PublishCommandViewState();

	UE_LOG(LogTemp, Log,
		TEXT("UHSRBattleCoordinator::BuildParticipants - SUCCESS RequestId=%s Participants=%d Definitions=%d"),
		*CurrentRequestId.ToString(), Participants.Num(), ParticipantDefinitions.Num());

	return FHSRBattleInitResult::MakeSuccess();
}

bool UHSRBattleCoordinator::RequestBasicAttack(FName AttackerParticipantId, FName TargetParticipantId)
{
	FHSRBattleActionCommand Command;
	Command.ActionId = FGuid::NewGuid();
	Command.BattleId = CurrentRequestId;
	Command.ActorParticipantId = AttackerParticipantId;
	Command.SkillId = FName(TEXT("BasicAttack"));
	Command.TargetParticipantIds.Add(TargetParticipantId);
	return RequestAction(Command).Succeeded();
}

FHSRAbilityResolution UHSRBattleCoordinator::RequestAction(const FHSRBattleActionCommand& Command)
{
	FHSRAbilityResolution Resolution;
	Resolution.ActionId = Command.ActionId;
	Resolution.ActorParticipantId = Command.ActorParticipantId;
	Resolution.SkillId = Command.SkillId;
	const auto Finalize = [this, &Command](FHSRAbilityResolution InResolution)
	{
		if (Command.ActionId.IsValid())
		{
			ProcessedActionResolutions.Add(Command.ActionId, InResolution);
		}
		LastActionResolution = InResolution;
		PublishCommandViewState();
		return InResolution;
	};
	const auto Reject = [&Resolution, &Finalize](EHSRAbilityFailureReason Reason)
	{
		Resolution.Status = EHSRAbilityResolutionStatus::Rejected;
		Resolution.FailureReason = Reason;
		return Finalize(Resolution);
	};
	if (Command.ActionId.IsValid())
	{
		if (const FHSRAbilityResolution* ExistingResolution = ProcessedActionResolutions.Find(Command.ActionId))
		{
			return *ExistingResolution;
		}
	}

	if (CurrentState != EHSRBattleCoordinatorState::Spawned || !TurnManager || Command.BattleId != CurrentRequestId)
	{
		return Reject(EHSRAbilityFailureReason::InvalidBattle);
	}
	if (!Command.ActionId.IsValid())
	{
		return Reject(EHSRAbilityFailureReason::DuplicateAction);
	}
	if (TurnManager->GetCurrentParticipantId() != Command.ActorParticipantId)
	{
		return Reject(EHSRAbilityFailureReason::NotCurrentActor);
	}
	const UHSRSkillDefinition* ResolvedSkillDefinition = FindSkillDefinition(Command.SkillId);
	if (!ResolvedSkillDefinition || !ResolvedSkillDefinition->IsValidDefinition())
	{
		return Reject(EHSRAbilityFailureReason::DefinitionMissing);
	}

	const FHSRBattleParticipant* Attacker = Participants.FindByPredicate([&Command](const FHSRBattleParticipant& Participant) { return Participant.ParticipantId == Command.ActorParticipantId; });
	if (!Attacker || !Attacker->IsValid() || Attacker->bDefeated
		|| !FHSRTargetingPolicy::ValidateTargetIds(*ResolvedSkillDefinition, *Attacker, Participants, Command.TargetParticipantIds))
	{
		return Reject(EHSRAbilityFailureReason::InvalidTarget);
	}
	const FHSRBattleParticipant* Target = Participants.FindByPredicate([&Command](const FHSRBattleParticipant& Participant) { return Participant.ParticipantId == Command.TargetParticipantIds[0]; });
	if (!Target || !Target->AbilitySystemComponent.IsValid())
	{
		return Reject(EHSRAbilityFailureReason::InvalidTarget);
	}
	if (ResolvedSkillDefinition->Category == EHSRSkillCategory::Heal && Target->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) >= Target->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetMaxHealthAttribute())) return Reject(EHSRAbilityFailureReason::AlreadyAtFullHealth);

	// This is the synchronous preflight for the only mutating P6-001 ability.
	// ResolveAction can reject only if its current participant is absent/invalid or
	// changes. The current id and the TurnManager's own weak participant copy are
	// both checked before GE activation; no Tick, delegate, or asynchronous work
	// runs between this check and the immediate ResolveAction below.
	const FHSRBattleParticipant* TurnParticipant = TurnManager->GetOrderedParticipants().FindByPredicate([&Command](const FHSRBattleParticipant& Participant) { return Participant.ParticipantId == Command.ActorParticipantId; });
	if (!TurnParticipant || !TurnParticipant->IsValid())
	{
		return Reject(EHSRAbilityFailureReason::NotCurrentActor);
	}

	FGameplayAbilitySpec* AbilitySpec = Attacker->AbilitySystemComponent->FindAbilitySpecFromClass(ResolvedSkillDefinition->AbilityClass);
	UHSRGameplayAbilityBase* Ability = AbilitySpec ? Cast<UHSRGameplayAbilityBase>(AbilitySpec->GetPrimaryInstance()) : nullptr;
	if (!Ability || !Ability->SetPendingTarget(Target->AbilitySystemComponent.Get()))
	{
		return Reject(EHSRAbilityFailureReason::AbilityUnavailable);
	}
	const EHSRAbilityFailureReason PreActivationFailure = Ability->GetPreActivationFailureReason(AbilitySpec->Handle, Attacker->AbilitySystemComponent->AbilityActorInfo.Get());
	if (PreActivationFailure != EHSRAbilityFailureReason::None)
	{
		Ability->ClearPendingTarget();
		return Reject(PreActivationFailure);
	}
	const int32 SkillPointDelta = ResolvedSkillDefinition->Category == EHSRSkillCategory::BasicAttack ? 1 : (ResolvedSkillDefinition->Category == EHSRSkillCategory::Skill ? -1 : 0);
	if (!ReserveSkillPoints(Command.ActionId, SkillPointDelta))
	{
		return Reject(EHSRAbilityFailureReason::InsufficientSkillPoint);
	}
	Ability->SetActionContext(Command.ActionId, Command.SkillId);
	if (!Attacker->AbilitySystemComponent->TryActivateAbility(AbilitySpec->Handle) || !Ability->DidLastActivationSucceed())
	{
		Ability->ClearPendingTarget();
		RollbackSkillPoints(Command.ActionId);
		return Reject(Ability->GetLastFailureReason());
	}

	if (!bBattleResultProduced && !TurnManager->ResolveAction(Command.ActorParticipantId))
	{
		ensureMsgf(false, TEXT("UHSRBattleCoordinator::RequestAction post-GE ResolveAction failure violates the synchronous preflight invariant. ActionId=%s"), *Command.ActionId.ToString());
		UE_LOG(LogTemp, Error, TEXT("UHSRBattleCoordinator::RequestAction - FAILED post-GE turn resolve ActionId=%s; GE side effect already occurred and this branch is contractually unreachable after preflight"), *Command.ActionId.ToString());
		RollbackSkillPoints(Command.ActionId);
		return Reject(EHSRAbilityFailureReason::EffectFailed);
	}
	CommitSkillPoints(Command.ActionId);
	Resolution.Status = EHSRAbilityResolutionStatus::Succeeded;
	Resolution.FailureReason = EHSRAbilityFailureReason::None;
	Finalize(Resolution);
	UE_LOG(LogTemp, Log, TEXT("UHSRBattleCoordinator::RequestAction - SUCCESS ActionId=%s Skill=%s Actor=%s Target=%s"), *Command.ActionId.ToString(), *Command.SkillId.ToString(), *Command.ActorParticipantId.ToString(), *Command.TargetParticipantIds[0].ToString());
	return Resolution;
}

FHSRBattleCommandViewState UHSRBattleCoordinator::GetCommandViewState() const
{
	FHSRBattleCommandViewState State;
	State.BattleId = CurrentRequestId;
	State.SkillPoints = TeamResourceState.CurrentSkillPoints;
	State.MaxSkillPoints = TeamResourceState.MaxSkillPoints;
	State.LastResolution = LastActionResolution;
	if (!TurnManager || CurrentState != EHSRBattleCoordinatorState::Spawned)
	{
		return State;
	}

	State.CurrentActorId = TurnManager->GetCurrentParticipantId();
	const FHSRBattleParticipant* Actor = Participants.FindByPredicate([&State](const FHSRBattleParticipant& P) { return P.ParticipantId == State.CurrentActorId; });
	if (!Actor || !Actor->AbilitySystemComponent.IsValid())
	{
		return State;
	}
	State.Energy = Actor->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute());
	State.MaxEnergy = Actor->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetMaxEnergyAttribute());
	const UHSRSkillDefinition* Definitions[] = { BasicAttackDefinition, SkillDefinition, UltimateDefinition, HealDefinition };
	for (const UHSRSkillDefinition* Definition : Definitions)
	{
		if (!Definition)
		{
			continue;
		}
		FHSRBattleCommandSkillView View;
		View.SkillId = Definition->SkillId;
		View.Category = Definition->Category;
		View.TargetType = Definition->TargetType;
		View.CandidateTargetIds = FHSRTargetingPolicy::BuildCandidateTargetIds(*Definition, *Actor, Participants);
		View.bAvailable = Definition->IsValidDefinition() && View.CandidateTargetIds.Num() > 0;
		if (!Definition->IsValidDefinition()) View.DisabledReason = EHSRAbilityFailureReason::DefinitionMissing;
		else if (View.CandidateTargetIds.Num() == 0) View.DisabledReason = EHSRAbilityFailureReason::InvalidTarget;
		else if (Definition->Category == EHSRSkillCategory::Skill && TeamResourceState.CurrentSkillPoints <= 0) View.DisabledReason = EHSRAbilityFailureReason::InsufficientSkillPoint;
		else
		{
			FGameplayAbilitySpec* Spec = Actor->AbilitySystemComponent->FindAbilitySpecFromClass(Definition->AbilityClass);
			UHSRGameplayAbilityBase* Ability = Spec ? Cast<UHSRGameplayAbilityBase>(Spec->GetPrimaryInstance()) : nullptr;
			if (!Ability || !Ability->SetPendingTarget(Participants.FindByPredicate([&View](const FHSRBattleParticipant& P) { return P.ParticipantId == View.CandidateTargetIds[0]; })->AbilitySystemComponent.Get()))
			{
				View.bAvailable = false; View.DisabledReason = EHSRAbilityFailureReason::AbilityUnavailable;
			}
			else
			{
				View.DisabledReason = Ability->GetPreActivationFailureReason(Spec->Handle, Actor->AbilitySystemComponent->AbilityActorInfo.Get());
				View.bAvailable = View.DisabledReason == EHSRAbilityFailureReason::None;
				Ability->ClearPendingTarget();
			}
		}
		State.Skills.Add(View);
	}
	return State;
}

void UHSRBattleCoordinator::PublishCommandViewState()
{
	CommandStateReady.Broadcast(GetCommandViewState());
}

bool UHSRBattleCoordinator::ConsumeBattleResult(FHSRBattleResult& OutResult)
{
	if (!bBattleResultProduced || bBattleResultConsumed)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleCoordinator::ConsumeBattleResult - REJECTED produced=%d consumed=%d"), bBattleResultProduced ? 1 : 0, bBattleResultConsumed ? 1 : 0);
		return false;
	}

	OutResult = BattleResult;
	bBattleResultConsumed = true;
	UE_LOG(LogTemp, Log, TEXT("UHSRBattleCoordinator::ConsumeBattleResult - SUCCESS RequestId=%s Outcome=%d"), *OutResult.RequestId.ToString(), static_cast<int32>(OutResult.Outcome));
	return true;
}

FHSRBattleInitResult UHSRBattleCoordinator::BuildAndValidateParticipantDefinitions()
{
	ParticipantDefinitions.Empty();

	// Player Definition: uses a well-known constant, not EncounterId
	FHSRBattleParticipantDefinition PlayerDef;
	PlayerDef.ParticipantId = FName(TEXT("Player"));
	PlayerDef.DefinitionId = FName(TEXT("PlayerCharacter"));
	PlayerDef.Team = EHSRBattleParticipantTeam::Player;
	PlayerDef.PawnClass = nullptr;
	if (PlayerDef.DefinitionId != FName(TEXT("PlayerCharacter")))
	{
		return FHSRBattleInitResult::MakeFailure(EHSRBattleInitFailureType::DefinitionNotFound,
			FText::FromString(TEXT("Player definition is not registered.")), PlayerDef.DefinitionId);
	}

	// Enemy Definition: uses CurrentEnemyDefinitionId from the encounter request
	FHSRBattleParticipantDefinition EnemyDef;
	EnemyDef.ParticipantId = FName(TEXT("Enemy"));
	EnemyDef.DefinitionId = CurrentEnemyDefinitionId;
	EnemyDef.Team = EHSRBattleParticipantTeam::Enemy;
	EnemyDef.PawnClass = nullptr;
	// The phase-5 runtime currently exposes one intentionally small, explicit
	// definition registry. Unknown non-empty IDs must fail deterministically.
	if (EnemyDef.DefinitionId != FName(TEXT("Enemy_TestGoblin")))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::BuildAndValidateParticipantDefinitions - DefinitionNotFound DefId=%s RequestId=%s"),
			*EnemyDef.DefinitionId.ToString(), *CurrentRequestId.ToString());
		return FHSRBattleInitResult::MakeFailure(EHSRBattleInitFailureType::DefinitionNotFound,
			FText::FromString(TEXT("Enemy definition is not registered.")), EnemyDef.DefinitionId);
	}
	// Resolve the registered definitions to their controlled runtime class.
	// The prototype intentionally uses the native APawn shell for both entries;
	// SpawnParticipantActor never falls back for a validated definition.
	PlayerDef.PawnClass = APawn::StaticClass();
	EnemyDef.PawnClass = APawn::StaticClass();
	ParticipantDefinitions.Add(PlayerDef);
	ParticipantDefinitions.Add(EnemyDef);

	// Validate each definition
	for (const auto& Def : ParticipantDefinitions)
	{
		if (Def.DefinitionId.IsNone())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("UHSRBattleCoordinator::BuildAndValidateParticipantDefinitions - DefinitionNotFound ParticipantId=%s"),
				*Def.ParticipantId.ToString());
			ParticipantDefinitions.Empty();
			return FHSRBattleInitResult::MakeFailure(
				EHSRBattleInitFailureType::DefinitionNotFound,
				FText::FromString(TEXT("Definition ID is empty.")),
				Def.ParticipantId);
		}

		// If a specific PawnClass is set, validate it is a valid APawn subclass
		if (Def.PawnClass != nullptr)
		{
			UClass* ResolvedClass = Def.PawnClass.Get();
			if (!ResolvedClass || !ResolvedClass->IsChildOf<APawn>())
			{
				UE_LOG(LogTemp, Warning,
					TEXT("UHSRBattleCoordinator::BuildAndValidateParticipantDefinitions - ClassLoadFailed ParticipantId=%s DefId=%s"),
					*Def.ParticipantId.ToString(), *Def.DefinitionId.ToString());
				ParticipantDefinitions.Empty();
				return FHSRBattleInitResult::MakeFailure(
					EHSRBattleInitFailureType::ClassLoadFailed,
					FText::FromString(TEXT("PawnClass is not a valid APawn subclass.")),
					Def.DefinitionId);
			}
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("UHSRBattleCoordinator::BuildAndValidateParticipantDefinitions - SUCCESS Definitions=%d PlayerDefId=%s EnemyDefId=%s"),
		ParticipantDefinitions.Num(),
		*ParticipantDefinitions[0].DefinitionId.ToString(),
		*ParticipantDefinitions[1].DefinitionId.ToString());

	return FHSRBattleInitResult::MakeSuccess();
}

void UHSRBattleCoordinator::Reset()
{
	UE_LOG(LogTemp, Log,
		TEXT("UHSRBattleCoordinator::Reset - Clearing state. Previous state=%d RequestId=%s"),
		static_cast<int32>(CurrentState), *CurrentRequestId.ToString());

	ClearRuntimeDelegates();
	CurrentState = EHSRBattleCoordinatorState::Idle;
	CurrentRequestId = FGuid();
	CurrentEncounterId = NAME_None;
	CurrentEnemyDefinitionId = NAME_None;
	ReturnContext = FHSRBattleReturnContext();
	Participants.Empty();
	ParticipantDefinitions.Empty();
	BattleResult = FHSRBattleResult();
	bBattleResultProduced = false;
	bBattleResultConsumed = false;
	ProcessedActionResolutions.Empty();
	DevelopmentDamageResults.Empty();
	DevelopmentDamageRandomStream.Initialize(DevelopmentDamageSeed);
	DevelopmentDamageConsumeCount = 0;
	LastActionResolution = FHSRAbilityResolution();
	SkillPointReservations.Empty();
	TeamResourceState = FHSRTeamResourceState();
	if (TurnManager)
	{
		TurnManager->Reset();
		TurnManager = nullptr;
	}
	PublishCommandViewState();
}

#if WITH_EDITOR
void UHSRBattleCoordinator::InitializeDevelopmentDamageRng(int32 InSeed)
{
	DevelopmentDamageSeed = InSeed;
	DevelopmentDamageRandomStream.Initialize(DevelopmentDamageSeed);
	DevelopmentDamageConsumeCount = 0;
	DevelopmentDamageResults.Empty();
}

FHSRDamageResult UHSRBattleCoordinator::ResolveDevelopmentExecutionDamage(FName SourceParticipantId, FName TargetParticipantId, const FGuid& ActionId, const FGameplayTag& DamageType, float AbilityMultiplier, const UHSRDamageRuleDefinition* Rule, TSubclassOf<UGameplayEffect> DamageEffectClass)
{
	FHSRDamageResult Failure;
	Failure.ActionId = ActionId;
	Failure.DamageType = DamageType;
	if (const FHSRDamageResult* Existing = DevelopmentDamageResults.Find(ActionId)) { return *Existing; }
	const auto CacheFailure = [this, &ActionId](FHSRDamageResult Result) { if (ActionId.IsValid()) { DevelopmentDamageResults.Add(ActionId, Result); } return Result; };
	if (CurrentState != EHSRBattleCoordinatorState::Spawned) { Failure.Result = EHSRDamageResultType::BattleTerminal; return CacheFailure(Failure); }
	if (!ActionId.IsValid()) { Failure.Result = EHSRDamageResultType::DuplicateAction; return Failure; }
	const FHSRBattleParticipant* Source = Participants.FindByPredicate([SourceParticipantId](const FHSRBattleParticipant& P) { return P.ParticipantId == SourceParticipantId; });
	if (!Source || !Source->AbilitySystemComponent.IsValid()) { Failure.Result = EHSRDamageResultType::InvalidSource; return CacheFailure(Failure); }
	const FHSRBattleParticipant* Target = Participants.FindByPredicate([TargetParticipantId](const FHSRBattleParticipant& P) { return P.ParticipantId == TargetParticipantId; });
	if (!Target || !Target->AbilitySystemComponent.IsValid()) { Failure.Result = EHSRDamageResultType::InvalidTarget; return CacheFailure(Failure); }
	if (!Rule || !Rule->IsValidRuleDefinition()) { Failure.Result = EHSRDamageResultType::MissingDamageRule; return CacheFailure(Failure); }
	if (!DamageType.IsValid() || !FMath::IsFinite(AbilityMultiplier) || AbilityMultiplier <= 0.0f || AbilityMultiplier > 100.0f) { Failure.Result = EHSRDamageResultType::InvalidDamageType; return CacheFailure(Failure); }
	if (!DamageEffectClass) { Failure.Result = EHSRDamageResultType::SpecCreationFailed; return CacheFailure(Failure); }
	const FGameplayTag AbilityMultiplierTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Damage.Data.AbilityMultiplier")), false);
	const FGameplayTag DefenseCoefficientTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Damage.Data.DefenseCoefficient")), false);
	const FGameplayTag MinDamageTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Damage.Data.MinDamage")), false);
	const FGameplayTag CritRollTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Damage.Data.CritRoll")), false);
	if (!AbilityMultiplierTag.IsValid() || !DefenseCoefficientTag.IsValid() || !MinDamageTag.IsValid() || !CritRollTag.IsValid()) { Failure.Result = EHSRDamageResultType::SpecCreationFailed; return CacheFailure(Failure); }
	FGameplayEffectContextHandle ContextHandle = Source->AbilitySystemComponent->MakeEffectContext();
	FHSRDamageEffectContext* DamageContext = static_cast<FHSRDamageEffectContext*>(ContextHandle.Get());
	if (!DamageContext || DamageContext->GetScriptStruct() != FHSRDamageEffectContext::StaticStruct()) { Failure.Result = EHSRDamageResultType::SpecCreationFailed; return CacheFailure(Failure); }
	DamageContext->DamageContext.ActionId = ActionId;
	DamageContext->DamageContext.DamageType = DamageType;
	DamageContext->DamageContext.AbilityMultiplier = AbilityMultiplier;
	// Build the Spec before consuming RNG. All failures above this point are side-effect free.
	DamageContext->DamageContext.CritRoll = 0.0f;
	DamageContext->DefenseCoefficient = Rule->DefenseCoefficient;
	DamageContext->MinDamage = Rule->MinDamage;
	FGameplayEffectSpecHandle Spec = Source->AbilitySystemComponent->MakeOutgoingSpec(DamageEffectClass, 1.0f, ContextHandle);
	if (!Spec.IsValid()) { Failure.Result = EHSRDamageResultType::SpecCreationFailed; return CacheFailure(Failure); }
	// The unique commit point: Spec is valid and Apply is immediately next.
	DamageContext->DamageContext.CritRoll = DevelopmentDamageRandomStream.GetFraction();
	++DevelopmentDamageConsumeCount;
	Spec.Data->SetSetByCallerMagnitude(AbilityMultiplierTag, AbilityMultiplier);
	Spec.Data->SetSetByCallerMagnitude(DefenseCoefficientTag, Rule->DefenseCoefficient);
	Spec.Data->SetSetByCallerMagnitude(MinDamageTag, Rule->MinDamage);
	Spec.Data->SetSetByCallerMagnitude(CritRollTag, DamageContext->DamageContext.CritRoll);
	const float HealthBefore = Target->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());
	const FActiveGameplayEffectHandle Applied = Source->AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), Target->AbilitySystemComponent.Get());
	if (!Applied.WasSuccessfullyApplied()) { Failure.Result = EHSRDamageResultType::EffectApplicationFailed; return CacheFailure(Failure); }
	FHSRDamageResult Result = DamageContext->DamageResult;
	const float HealthAfter = Target->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());
	Result.Breakdown.AppliedDamage = FMath::Max(0.0f, HealthBefore - HealthAfter);
	DevelopmentDamageResults.Add(ActionId, Result);
	UE_LOG(LogTemp, Log, TEXT("P7-002 Damage Result=%d ActionId=%s Seed=%d ConsumeIndex=%d Raw=%f Final=%f Applied=%f"), static_cast<int32>(Result.Result), *ActionId.ToString(), DevelopmentDamageSeed, DevelopmentDamageConsumeCount, Result.Breakdown.RawDamage, Result.Breakdown.FinalDamage, Result.Breakdown.AppliedDamage);
	return Result;
}
#endif

void UHSRBattleCoordinator::BindHealthObserver(const FHSRBattleParticipant& Participant)
{
	if (!Participant.AbilitySystemComponent.IsValid())
	{
		return;
	}

	FDelegateHandle& Handle = HealthChangedHandles.FindOrAdd(Participant.ParticipantId);
	Handle = Participant.AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetHealthAttribute()).AddUObject(
		this, &UHSRBattleCoordinator::HandleHealthChanged, Participant.ParticipantId);
}

void UHSRBattleCoordinator::HandleHealthChanged(const FOnAttributeChangeData& ChangeData, FName ParticipantId)
{
	if (ChangeData.NewValue <= 0.0f)
	{
		ResolveDefeat(ParticipantId);
	}
}

void UHSRBattleCoordinator::ResolveDefeat(FName DefeatedParticipantId)
{
	if (bBattleResultProduced || CurrentState != EHSRBattleCoordinatorState::Spawned)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleCoordinator::ResolveDefeat - REJECTED duplicate/inactive Defeated=%s"), *DefeatedParticipantId.ToString());
		return;
	}

	FHSRBattleParticipant* Defeated = Participants.FindByPredicate([DefeatedParticipantId](const FHSRBattleParticipant& Participant)
	{
		return Participant.ParticipantId == DefeatedParticipantId;
	});
	if (!Defeated)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleCoordinator::ResolveDefeat - REJECTED unknown participant=%s"), *DefeatedParticipantId.ToString());
		return;
	}

	Defeated->bDefeated = true;
	BattleResult.RequestId = CurrentRequestId;
	BattleResult.DefeatedParticipantId = DefeatedParticipantId;
	BattleResult.EncounterId = CurrentEncounterId;
	BattleResult.ReturnContext = ReturnContext;
	BattleResult.Outcome = Defeated->Team == EHSRBattleParticipantTeam::Enemy ? EHSRBattleOutcome::PlayerVictory : EHSRBattleOutcome::PlayerDefeat;
	bBattleResultProduced = true;
	CurrentState = EHSRBattleCoordinatorState::Finished;
	if (TurnManager)
	{
		TurnManager->FinishBattle();
	}

	UE_LOG(LogTemp, Log, TEXT("UHSRBattleCoordinator::ResolveDefeat - SUCCESS RequestId=%s Defeated=%s Outcome=%d"), *BattleResult.RequestId.ToString(), *DefeatedParticipantId.ToString(), static_cast<int32>(BattleResult.Outcome));
	BattleResultReady.Broadcast(BattleResult);
}

void UHSRBattleCoordinator::ClearRuntimeDelegates()
{
	for (const FHSRBattleParticipant& Participant : Participants)
	{
		if (Participant.AbilitySystemComponent.IsValid())
		{
			if (const FDelegateHandle* Handle = HealthChangedHandles.Find(Participant.ParticipantId))
			{
				Participant.AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetHealthAttribute()).Remove(*Handle);
			}
		}
	}
	HealthChangedHandles.Empty();
}

AActor* UHSRBattleCoordinator::SpawnParticipantActor(UWorld* World, const FHSRBattleParticipantDefinition& Definition)
{
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleCoordinator::SpawnParticipantActor - World is null"));
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	APawn* Pawn = World->SpawnActor<APawn>(Definition.PawnClass ? Definition.PawnClass.Get() : APawn::StaticClass(), FTransform::Identity, Params);
	if (!Pawn)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::SpawnParticipantActor - FAILED to spawn APawn for team=%d"),
			static_cast<int32>(Definition.Team));
		return nullptr;
	}

	Pawn->SetActorLabel(Definition.Team == EHSRBattleParticipantTeam::Player ? TEXT("BattlePlayerPawn") : TEXT("BattleEnemyPawn"));

	UE_LOG(LogTemp, Log,
		TEXT("UHSRBattleCoordinator::SpawnParticipantActor - SUCCESS Actor=%s Team=%d DefId=%s"),
		*Pawn->GetName(), static_cast<int32>(Definition.Team), *Definition.DefinitionId.ToString());

	return Pawn;
}

bool UHSRBattleCoordinator::InitParticipantASC(AActor* TargetActor)
{
	if (!TargetActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleCoordinator::InitParticipantASC - TargetActor is null"));
		return false;
	}

	// Add UHSRAbilitySystemComponent at runtime (UE5.0+ AddComponentByClass)
	UHSRAbilitySystemComponent* ASC = Cast<UHSRAbilitySystemComponent>(
		TargetActor->AddComponentByClass(UHSRAbilitySystemComponent::StaticClass(), false, FTransform::Identity, false));

	if (!ASC)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::InitParticipantASC - Failed to add ASC component to Actor=%s"),
			*TargetActor->GetName());
		return false;
	}

	// Configure ASC: single-player, no tick
	ASC->SetIsReplicated(false);
	ASC->SetComponentTickEnabled(false);
	// ASC->RegisterComponent() ?? auto-registered by AddComponentByClass

	// Register CoreAttributeSet via InitStats
	const UAttributeSet* AttrSet = ASC->InitStats(UHSRCoreAttributeSet::StaticClass(), nullptr);
	if (!AttrSet)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::InitParticipantASC - InitStats failed for Actor=%s"),
			*TargetActor->GetName());
		ASC->DestroyComponent(true);
		return false;
	}

	// InitAbilityActorInfo with Owner=Avatar=self
	ASC->InitAbilityActorInfo(TargetActor, TargetActor);

	if (!ASC->AbilityActorInfo.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::InitParticipantASC - ActorInfo invalid after Init for Actor=%s"),
			*TargetActor->GetName());
		ASC->DestroyComponent(true);
		return false;
	}

	UE_LOG(LogTemp, Log,
		TEXT("UHSRBattleCoordinator::InitParticipantASC - SUCCESS Actor=%s ASC=%s ActorInfo valid, Owner=Avatar=self"),
		*TargetActor->GetName(), *ASC->GetName());

	return true;
}

bool UHSRBattleCoordinator::GrantBasicAttackAbility(const FHSRBattleParticipant& Participant)
{
	if (!Participant.AbilitySystemComponent.IsValid() || !BasicAttackDefinition || !BasicAttackDefinition->IsValidDefinition() || BasicAttackDefinition->Category != EHSRSkillCategory::BasicAttack || BasicAttackDefinition->TargetType != EHSRTargetType::SingleEnemy)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleCoordinator::GrantBasicAttackAbility - REJECTED missing/invalid BasicAttack SkillDefinition Participant=%s"), *Participant.ParticipantId.ToString());
		return false;
	}

	const FGameplayAbilitySpecHandle AbilityHandle = Participant.AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(BasicAttackDefinition->AbilityClass, 1, INDEX_NONE, this));
	const bool bGranted = AbilityHandle.IsValid();
	if (bGranted)
	{
		UE_LOG(LogTemp, Log, TEXT("UHSRBattleCoordinator::GrantBasicAttackAbility - SUCCESS Participant=%s"), *Participant.ParticipantId.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleCoordinator::GrantBasicAttackAbility - FAILED Participant=%s"), *Participant.ParticipantId.ToString());
	}
	return bGranted;
}

bool UHSRBattleCoordinator::GrantUltimateAbility(const FHSRBattleParticipant& Participant)
{
	if (!Participant.AbilitySystemComponent.IsValid() || !UltimateDefinition || !UltimateDefinition->IsValidUltimateDefinition())
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleCoordinator::GrantUltimateAbility - REJECTED missing/invalid Ultimate SkillDefinition Participant=%s"), *Participant.ParticipantId.ToString());
		return false;
	}

	const FGameplayAbilitySpecHandle AbilityHandle = Participant.AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(UltimateDefinition->AbilityClass, 1, INDEX_NONE, this));
	FGameplayAbilitySpec* AbilitySpec = AbilityHandle.IsValid() ? Participant.AbilitySystemComponent->FindAbilitySpecFromClass(UltimateDefinition->AbilityClass) : nullptr;
	UHSRGameplayAbilityBase* Ability = AbilitySpec ? Cast<UHSRGameplayAbilityBase>(AbilitySpec->GetPrimaryInstance()) : nullptr;
	if (!Ability || !Ability->ConfigureFromSkillDefinition(*UltimateDefinition))
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleCoordinator::GrantUltimateAbility - FAILED configuration Participant=%s"), *Participant.ParticipantId.ToString());
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("UHSRBattleCoordinator::GrantUltimateAbility - SUCCESS Participant=%s"), *Participant.ParticipantId.ToString());
	return true;
}

bool UHSRBattleCoordinator::GrantSkillAbility(const FHSRBattleParticipant& Participant)
{
	if (!Participant.AbilitySystemComponent.IsValid() || !SkillDefinition || !SkillDefinition->IsValidSkillDefinition()) return false;
	const FGameplayAbilitySpecHandle Handle = Participant.AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(SkillDefinition->AbilityClass, 1, INDEX_NONE, this));
	FGameplayAbilitySpec* Spec = Handle.IsValid() ? Participant.AbilitySystemComponent->FindAbilitySpecFromClass(SkillDefinition->AbilityClass) : nullptr;
	UHSRGameplayAbilityBase* Ability = Spec ? Cast<UHSRGameplayAbilityBase>(Spec->GetPrimaryInstance()) : nullptr;
	const bool bSuccess = Ability && Ability->ConfigureFromSkillDefinition(*SkillDefinition);
	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("UHSRBattleCoordinator::GrantSkillAbility - SUCCESS Participant=%s"), *Participant.ParticipantId.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleCoordinator::GrantSkillAbility - FAILED Participant=%s"), *Participant.ParticipantId.ToString());
	}
	return bSuccess;
}
bool UHSRBattleCoordinator::GrantHealAbility(const FHSRBattleParticipant& Participant)
{
	if(!Participant.AbilitySystemComponent.IsValid()||!HealDefinition||!HealDefinition->IsValidHealDefinition())return false;
	auto H=Participant.AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(HealDefinition->AbilityClass,1,INDEX_NONE,this)); auto* S=H.IsValid()?Participant.AbilitySystemComponent->FindAbilitySpecFromClass(HealDefinition->AbilityClass):nullptr; auto* A=S?Cast<UHSRGameplayAbilityBase>(S->GetPrimaryInstance()):nullptr; return A&&A->ConfigureFromSkillDefinition(*HealDefinition);
}

const UHSRSkillDefinition* UHSRBattleCoordinator::FindSkillDefinition(FName SkillId) const
{
	if (BasicAttackDefinition && BasicAttackDefinition->SkillId == SkillId)
	{
		return BasicAttackDefinition;
	}
	if (UltimateDefinition && UltimateDefinition->SkillId == SkillId)
	{
		return UltimateDefinition;
	}
	if (SkillDefinition && SkillDefinition->SkillId == SkillId)
	{
		return SkillDefinition;
	}
	if (HealDefinition && HealDefinition->SkillId == SkillId) return HealDefinition;
	return nullptr;
}

bool UHSRBattleCoordinator::ReserveSkillPoints(const FGuid& ActionId, int32 Delta)
{
	if (SkillPointReservations.Contains(ActionId)) return false;
	if (Delta < 0 && TeamResourceState.CurrentSkillPoints < -Delta) return false;
	FHSRSkillPointReservation Reservation; Reservation.ActionId = ActionId;
	// A capped BasicAttack is still a valid action; it commits a zero delta.
	Reservation.Delta = Delta > 0 ? FMath::Min(Delta, TeamResourceState.MaxSkillPoints - TeamResourceState.CurrentSkillPoints) : Delta;
	SkillPointReservations.Add(ActionId, Reservation);
	UE_LOG(LogTemp, Log, TEXT("UHSRBattleCoordinator::ReserveSkillPoints - ActionId=%s Delta=%d Current=%d Max=%d"), *ActionId.ToString(), Reservation.Delta, TeamResourceState.CurrentSkillPoints, TeamResourceState.MaxSkillPoints);
	return true;
}

void UHSRBattleCoordinator::RollbackSkillPoints(const FGuid& ActionId)
{
	if (SkillPointReservations.Remove(ActionId) > 0) UE_LOG(LogTemp, Log, TEXT("UHSRBattleCoordinator::RollbackSkillPoints - ActionId=%s"), *ActionId.ToString());
}

void UHSRBattleCoordinator::CommitSkillPoints(const FGuid& ActionId)
{
	if (FHSRSkillPointReservation* Reservation = SkillPointReservations.Find(ActionId))
	{
		TeamResourceState.CurrentSkillPoints = FMath::Clamp(TeamResourceState.CurrentSkillPoints + Reservation->Delta, 0, TeamResourceState.MaxSkillPoints);
		UE_LOG(LogTemp, Log, TEXT("UHSRBattleCoordinator::CommitSkillPoints - ActionId=%s Delta=%d Current=%d"), *ActionId.ToString(), Reservation->Delta, TeamResourceState.CurrentSkillPoints);
		SkillPointReservations.Remove(ActionId);
	}
}
