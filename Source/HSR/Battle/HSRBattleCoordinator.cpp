#include "HSRBattleCoordinator.h"
#include "../Data/Definitions/HSREnemyDefinition.h"
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
#include "../Status/HSRStatusComponent.h"
#include "../Data/Definitions/HSRStatusDefinition.h"

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
#if WITH_EDITOR
	LastSubmittedRequestForDevelopment = InRequest;
#endif

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
	if (!EnemyDefinition || EnemyDefinition->EnemyDefinitionId != CurrentEnemyDefinitionId)
	{
		CurrentState = EHSRBattleCoordinatorState::Failed;
		return FHSRBattleInitResult::MakeFailure(EHSRBattleInitFailureType::DefinitionNotFound, FText::FromString(TEXT("Configured EnemyDefinition does not match encounter EnemyDefinitionId.")), CurrentEnemyDefinitionId);
	}

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
		if (!ApplyParticipantInitializationGameplayEffect(Participant))
		{
			UE_LOG(LogTemp, Error, TEXT("P8-005 InitGE Participant=%s Result=FAILED"), *Participant.ParticipantId.ToString());
			SpawnedActor->Destroy(); for (FHSRBattleParticipant& Existing : Participants) { if (Existing.Actor.IsValid()) Existing.Actor->Destroy(); }
			Participants.Empty(); ParticipantDefinitions.Empty(); CurrentState = EHSRBattleCoordinatorState::Failed;
			return FHSRBattleInitResult::MakeFailure(EHSRBattleInitFailureType::InitFailed, FText::FromString(TEXT("Participant initialization GameplayEffect failed.")), Def.DefinitionId);
		}
		if (Def.Team == EHSRBattleParticipantTeam::Enemy)
		{
			Participant.WeaknessTags = EnemyDefinition->WeaknessTags;
			Participant.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxToughnessAttribute(), EnemyDefinition->InitialMaxToughness);
			Participant.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetToughnessAttribute(), EnemyDefinition->InitialToughness);
			UE_LOG(LogTemp, Log, TEXT("P8-005 EnemyToughness Participant=%s Toughness=%.2f MaxToughness=%.2f"), *Participant.ParticipantId.ToString(), Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetToughnessAttribute()), Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetMaxToughnessAttribute()));
		}
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
	if (!InitializeStatusComponents())
	{
		ClearStatusComponents();
		for (FHSRBattleParticipant& Participant : Participants) { if (Participant.Actor.IsValid()) Participant.Actor->Destroy(); }
		Participants.Empty();
		TurnManager = nullptr;
		CurrentState = EHSRBattleCoordinatorState::Failed;
		return FHSRBattleInitResult::MakeFailure(EHSRBattleInitFailureType::InitFailed, FText::FromString(TEXT("Failed to initialize StatusComponents.")));
	}

	// Atomically transition to Spawned
	CurrentState = EHSRBattleCoordinatorState::Spawned;
	DevelopmentDamageRandomStream.Initialize(DevelopmentDamageSeed);
	DevelopmentDamageConsumeCount = 0;
	DevelopmentDamageResults.Empty();
#if WITH_EDITOR
	ClearDamageTestInjection();
	LastDevelopmentFormalExecutionResult = FHSRFormalDamageExecutionResult();
#endif
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
			if (ExistingResolution->bHasToughnessResult)
			{
				const FHSRToughnessResult& CachedToughness = ExistingResolution->ToughnessResult;
				const FString ReplayTarget = Command.TargetParticipantIds.IsEmpty() ? TEXT("<omitted>") : Command.TargetParticipantIds[0].ToString();
				const UHSRSkillDefinition* CachedSkillDefinition = FindSkillDefinition(Command.SkillId);
				const FString ReplayElement = CachedSkillDefinition ? CachedSkillDefinition->ElementTag.ToString() : TEXT("<unavailable>");
				const FString ReplayExpectedWeakness = ReplayElement.StartsWith(TEXT("Element."))
					? FString::Printf(TEXT("Weakness.%s"), *ReplayElement.RightChop(FCString::Strlen(TEXT("Element."))))
					: TEXT("<invalid>");
				UE_LOG(LogTemp, Log, TEXT("P8-002 Toughness Replay ActionId=%s Actor=%s Target=%s Element=%s ExpectedWeakness=%s Matched=%d Before=%.2f Damage=%.2f After=%.2f ReachedZero=%d FailureReason=%d"),
					*Command.ActionId.ToString(), *ExistingResolution->ActorParticipantId.ToString(), *ReplayTarget, *ReplayElement, *ReplayExpectedWeakness, CachedToughness.bMatched ? 1 : 0,
					CachedToughness.Before, CachedToughness.Damage, CachedToughness.After, CachedToughness.bReachedZero ? 1 : 0, static_cast<int32>(CachedToughness.FailureReason));
			}
			if (ExistingResolution->bHasBreakResult)
			{
				const FHSRBreakResult& CachedBreak = ExistingResolution->BreakResult;
				UE_LOG(LogTemp, Log, TEXT("P8-003 Break ActionId=%s Target=%s Before=%.2f After=%.2f Triggered=%d Replay=1 FailureReason=%d"),
					*CachedBreak.ActionId.ToString(), *CachedBreak.TargetParticipantId.ToString(), CachedBreak.ToughnessBefore,
					CachedBreak.ToughnessAfter, CachedBreak.bTriggered ? 1 : 0, static_cast<int32>(CachedBreak.FailureReason));
			}
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
	FHSRBattleParticipant* Target = Participants.FindByPredicate([&Command](const FHSRBattleParticipant& Participant) { return Participant.ParticipantId == Command.TargetParticipantIds[0]; });
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
	// Basic and Skill use the formal prepared-damage seam.  Heal and Ultimate
	// intentionally retain their existing paths until their dedicated migration.
	if (ResolvedSkillDefinition->Category == EHSRSkillCategory::BasicAttack || ResolvedSkillDefinition->Category == EHSRSkillCategory::Skill || ResolvedSkillDefinition->Category == EHSRSkillCategory::Ultimate)
	{
		const EHSRAbilityFailureReason FormalPreActivationFailure = Ability->GetPreActivationFailureReason(AbilitySpec->Handle, Attacker->AbilitySystemComponent->AbilityActorInfo.Get());
		if (FormalPreActivationFailure != EHSRAbilityFailureReason::None)
		{
			UE_LOG(LogTemp, Warning, TEXT("P7-003 Formal Stage=PreActivation Result=REJECT ActionId=%s Skill=%s Reason=%d RNG=%d SP=%d"), *Command.ActionId.ToString(), *Command.SkillId.ToString(), static_cast<int32>(FormalPreActivationFailure), DevelopmentDamageConsumeCount, TeamResourceState.CurrentSkillPoints);
			Ability->ClearPendingTarget();
			return Reject(FormalPreActivationFailure);
		}
		const UHSRDamageRuleDefinition* Rule = ResolvedSkillDefinition->DamageRule.LoadSynchronous();
		const TSubclassOf<UGameplayEffect> DamageEffect = ResolvedSkillDefinition->EffectGameplayEffectClass.LoadSynchronous();
		const FGameplayTag AbilityMultiplierTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Damage.Data.AbilityMultiplier")), false);
		const FGameplayTag DefenseCoefficientTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Damage.Data.DefenseCoefficient")), false);
		const FGameplayTag MinDamageTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Damage.Data.MinDamage")), false);
		const FGameplayTag CritRollTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Damage.Data.CritRoll")), false);
		if (!Rule || !Rule->IsValidRuleDefinition() || !DamageEffect || !ResolvedSkillDefinition->DamageType.IsValid()
			|| !FMath::IsFinite(ResolvedSkillDefinition->AbilityMultiplier) || ResolvedSkillDefinition->AbilityMultiplier <= 0.0f
			|| !AbilityMultiplierTag.IsValid() || !DefenseCoefficientTag.IsValid() || !MinDamageTag.IsValid() || !CritRollTag.IsValid())
		{
			Ability->ClearPendingTarget();
			return Reject(EHSRAbilityFailureReason::DefinitionMissing);
		}
		FRandomStream PreviewStream = DevelopmentDamageRandomStream;
		FHSRFormalDamageRequest Request;
		Request.BattleId = CurrentRequestId; Request.ActionId = Command.ActionId; Request.SkillId = Command.SkillId;
		Request.SourceParticipantId = Attacker->ParticipantId; Request.TargetParticipantId = Target->ParticipantId;
		Request.DamageType = ResolvedSkillDefinition->DamageType; Request.AbilityMultiplier = ResolvedSkillDefinition->AbilityMultiplier;
		Request.DefenseCoefficient = Rule->DefenseCoefficient; Request.MinDamage = Rule->MinDamage; Request.CritRoll = PreviewStream.GetFraction();
		FGameplayEffectContextHandle ContextHandle = Attacker->AbilitySystemComponent->MakeEffectContext();
		FHSRDamageEffectContext* DamageContext = ContextHandle.IsValid() ? static_cast<FHSRDamageEffectContext*>(ContextHandle.Get()) : nullptr;
		if (!DamageContext || DamageContext->GetScriptStruct() != FHSRDamageEffectContext::StaticStruct()) { Ability->ClearPendingTarget(); return Reject(EHSRAbilityFailureReason::EffectFailed); }
		ContextHandle.AddSourceObject(Attacker->Actor.Get());
		DamageContext->DamageContext.ActionId = Request.ActionId; DamageContext->DamageContext.DamageType = Request.DamageType;
		DamageContext->DamageContext.AbilityMultiplier = Request.AbilityMultiplier; DamageContext->DamageContext.CritRoll = Request.CritRoll;
		DamageContext->DefenseCoefficient = Request.DefenseCoefficient; DamageContext->MinDamage = Request.MinDamage;
#if WITH_EDITOR
		DamageContext->TestInjection = DamageTestInjectionActionId == Command.ActionId ? NextDamageTestInjection : EHSRDamageTestInjection::None;
		UE_LOG(LogTemp, Log, TEXT("P7-004 InjectionBind ActionId=%s BoundActionId=%s Requested=%d Applied=%d"), *Command.ActionId.ToString(), *DamageTestInjectionActionId.ToString(), static_cast<int32>(NextDamageTestInjection), static_cast<int32>(DamageContext->TestInjection));
		if (DamageTestInjectionActionId == Command.ActionId) { ClearDamageTestInjection(); }
#endif
		FGameplayEffectSpecHandle DamageSpec = Attacker->AbilitySystemComponent->MakeOutgoingSpec(DamageEffect, 1.0f, ContextHandle);
		if (!DamageSpec.IsValid()) { UE_LOG(LogTemp, Warning, TEXT("P7-003 Formal Stage=MakeSpec Result=FAIL ActionId=%s Skill=%s"), *Command.ActionId.ToString(), *Command.SkillId.ToString()); Ability->ClearPendingTarget(); return Reject(EHSRAbilityFailureReason::EffectFailed); }
		if (FHSRDamageEffectContext* SpecContext = static_cast<FHSRDamageEffectContext*>(DamageSpec.Data->GetContext().Get()))
		{
			SpecContext->TestInjection = DamageContext->TestInjection;
			SpecContext->DamageContext.ActionId = Request.ActionId;
			UE_LOG(LogTemp, Log, TEXT("P7-004 SpecContext ActionId=%s Injection=%d"), *SpecContext->DamageContext.ActionId.ToString(), static_cast<int32>(SpecContext->TestInjection));
		}
		DamageSpec.Data->SetSetByCallerMagnitude(AbilityMultiplierTag, Request.AbilityMultiplier);
		DamageSpec.Data->SetSetByCallerMagnitude(DefenseCoefficientTag, Request.DefenseCoefficient);
		DamageSpec.Data->SetSetByCallerMagnitude(MinDamageTag, Request.MinDamage);
		DamageSpec.Data->SetSetByCallerMagnitude(CritRollTag, Request.CritRoll);
		FHSRFormalDamagePrepareResult PrepareResult;
		if (!Ability->PrepareFormalDamage(Request, DamageSpec, Target->AbilitySystemComponent.Get(), PrepareResult)) { UE_LOG(LogTemp, Warning, TEXT("P7-003 Formal Stage=Prepare Result=FAIL ActionId=%s Skill=%s DamageResult=%d"), *Command.ActionId.ToString(), *Command.SkillId.ToString(), static_cast<int32>(PrepareResult.Result)); Ability->ClearPendingTarget(); return Reject(EHSRAbilityFailureReason::EffectFailed); }
		const int32 SkillPointDelta = ResolvedSkillDefinition->Category == EHSRSkillCategory::BasicAttack ? 1 : (ResolvedSkillDefinition->Category == EHSRSkillCategory::Skill ? -1 : 0);
		if (!ReserveSkillPoints(Command.ActionId, SkillPointDelta)) { Ability->ClearPreparedFormalDamage(); Ability->ClearPendingTarget(); return Reject(EHSRAbilityFailureReason::InsufficientSkillPoint); }
		Ability->SetActionContext(Command.ActionId, Command.SkillId);
		bFormalDamageTransactionOpen = true;
		PendingDefeatedParticipantId = NAME_None;
		const bool bActivated = Attacker->AbilitySystemComponent->TryActivateAbility(AbilitySpec->Handle);
		const FHSRFormalDamageExecutionResult ExecutionResult = Ability->GetLastFormalDamageExecutionResult();
#if WITH_EDITOR
		LastDevelopmentFormalExecutionResult = ExecutionResult;
#endif
		bFormalDamageTransactionOpen = false;
		Ability->ClearPreparedFormalDamage(); Ability->ClearPendingTarget();
		if (!bActivated || !Ability->DidLastActivationSucceed() || !ExecutionResult.bSucceeded)
		{
			Resolution.bHasDamageResult = true;
			Resolution.DamageResult = ExecutionResult.DamageResult;
			UE_LOG(LogTemp, Warning, TEXT("P7-003 Formal Stage=Activate Result=FAIL ActionId=%s Skill=%s TryActivate=%d AbilitySuccess=%d ExecutionSuccess=%d DamageResult=%d CostCommitted=%d Refund=%d EnergyBefore=%.2f EnergyAfter=%.2f"), *Command.ActionId.ToString(), *Command.SkillId.ToString(), bActivated ? 1 : 0, Ability->DidLastActivationSucceed() ? 1 : 0, ExecutionResult.bSucceeded ? 1 : 0, static_cast<int32>(ExecutionResult.DamageResult.Result), ExecutionResult.bCostCommitted ? 1 : 0, ExecutionResult.bRefundApplied ? 1 : 0, ExecutionResult.EnergyBefore, ExecutionResult.EnergyAfter);
			PendingDefeatedParticipantId = NAME_None;
			RollbackSkillPoints(Command.ActionId);
			return Reject(EHSRAbilityFailureReason::EffectFailed);
		}
		DevelopmentDamageRandomStream = PreviewStream;
		++DevelopmentDamageConsumeCount;
		CommitSkillPoints(Command.ActionId);
		Resolution.Status = EHSRAbilityResolutionStatus::Succeeded;
		Resolution.FailureReason = EHSRAbilityFailureReason::None;
		Resolution.bHasDamageResult = true;
		Resolution.DamageResult = ExecutionResult.DamageResult;
		// Toughness is an independent post-HP result.  Its failure deliberately
		// never rewrites the already-committed formal damage transaction.
		Resolution.bHasToughnessResult = true;
		FHSRToughnessResult& ToughnessResult = Resolution.ToughnessResult;
		ToughnessResult.Before = Target->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetToughnessAttribute());
		ToughnessResult.After = ToughnessResult.Before;
		const FString ElementName = ResolvedSkillDefinition->ElementTag.ToString();
		const FString ElementPrefix(TEXT("Element."));
		if (!ResolvedSkillDefinition->ElementTag.IsValid() || !ElementName.StartsWith(ElementPrefix))
		{
			ToughnessResult.FailureReason = EHSRToughnessFailureReason::MissingElement;
		}
		else if (Target->WeaknessTags.IsEmpty())
		{
			ToughnessResult.FailureReason = EHSRToughnessFailureReason::EmptyWeaknesses;
		}
		else
		{
			const FGameplayTag MatchingWeakness = FGameplayTag::RequestGameplayTag(
				FName(*FString::Printf(TEXT("Weakness.%s"), *ElementName.RightChop(ElementPrefix.Len()))), false);
			if (!MatchingWeakness.IsValid() || !Target->WeaknessTags.HasTagExact(MatchingWeakness))
			{
				ToughnessResult.FailureReason = EHSRToughnessFailureReason::NoWeaknessMatch;
			}
			else if (!FMath::IsFinite(ResolvedSkillDefinition->ToughnessDamage) || ResolvedSkillDefinition->ToughnessDamage <= 0.0f)
			{
				ToughnessResult.FailureReason = EHSRToughnessFailureReason::InvalidDamage;
			}
			else
			{
				ToughnessResult.bMatched = true;
				ToughnessResult.Damage = ResolvedSkillDefinition->ToughnessDamage;
				const TSubclassOf<UGameplayEffect> ToughnessEffect = ResolvedSkillDefinition->ToughnessDamageGameplayEffectClass.LoadSynchronous();
				const FGameplayTag ToughnessDamageTag = FGameplayTag::RequestGameplayTag(TEXT("Damage.Data.ToughnessDamage"), false);
				if (!ToughnessEffect || !ToughnessDamageTag.IsValid())
				{
					ToughnessResult.FailureReason = EHSRToughnessFailureReason::MissingGameplayEffect;
				}
				else
				{
					FGameplayEffectSpecHandle ToughnessSpec = Attacker->AbilitySystemComponent->MakeOutgoingSpec(ToughnessEffect, 1.0f, Attacker->AbilitySystemComponent->MakeEffectContext());
					if (!ToughnessSpec.IsValid())
					{
						ToughnessResult.FailureReason = EHSRToughnessFailureReason::EffectApplicationFailed;
					}
					else
					{
						ToughnessSpec.Data->SetSetByCallerMagnitude(ToughnessDamageTag, ToughnessResult.Damage);
						// Instant GameplayEffects may apply immediately without retaining an
						// active-effect handle.  Audit the AttributeSet write instead of
						// interpreting that invalid handle as an application failure.
						Target->AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*ToughnessSpec.Data.Get());
						ToughnessResult.After = Target->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetToughnessAttribute());
						const float MaxToughness = Target->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetMaxToughnessAttribute());
						const float ExpectedAfter = FMath::Clamp(ToughnessResult.Before - ToughnessResult.Damage, 0.0f, MaxToughness);
						if (FMath::IsNearlyEqual(ToughnessResult.After, ExpectedAfter))
						{
							ToughnessResult.bReachedZero = ToughnessResult.Before > 0.0f && ToughnessResult.After <= 0.0f;
						}
						else
						{
							ToughnessResult.FailureReason = EHSRToughnessFailureReason::EffectApplicationFailed;
						}
					}
				}
			}
		}
		const FString ExpectedWeakness = ElementName.StartsWith(ElementPrefix)
			? FString::Printf(TEXT("Weakness.%s"), *ElementName.RightChop(ElementPrefix.Len()))
			: TEXT("<invalid>");
		UE_LOG(LogTemp, Log, TEXT("P8-002 Toughness ActionId=%s Actor=%s Target=%s Element=%s ExpectedWeakness=%s Matched=%d Before=%.2f Damage=%.2f After=%.2f ReachedZero=%d FailureReason=%d"),
			*Command.ActionId.ToString(), *Command.ActorParticipantId.ToString(), *Target->ParticipantId.ToString(), *ElementName,
			*ExpectedWeakness,
			ToughnessResult.bMatched ? 1 : 0, ToughnessResult.Before, ToughnessResult.Damage, ToughnessResult.After,
			ToughnessResult.bReachedZero ? 1 : 0, static_cast<int32>(ToughnessResult.FailureReason));
		// P8-003 only publishes a pure result.  The participant latch scopes
		// exactly-once publication to this spawned battle lifecycle.
		Resolution.bHasBreakResult = true;
		FHSRBreakResult& BreakResult = Resolution.BreakResult;
		BreakResult.ActionId = Command.ActionId;
		BreakResult.TargetParticipantId = Target->ParticipantId;
		BreakResult.ToughnessBefore = ToughnessResult.Before;
		BreakResult.ToughnessAfter = ToughnessResult.After;
		if (bBattleResultProduced || CurrentState != EHSRBattleCoordinatorState::Spawned)
		{
			BreakResult.FailureReason = EHSRBreakFailureReason::BattleFinished;
		}
		else if (!Target->IsValid())
		{
			BreakResult.FailureReason = EHSRBreakFailureReason::InvalidTarget;
		}
		else if (!ToughnessResult.bReachedZero || ToughnessResult.Before <= 0.0f || !FMath::IsNearlyZero(ToughnessResult.After))
		{
			BreakResult.FailureReason = EHSRBreakFailureReason::ToughnessNotDepleted;
		}
		else if (Target->bBreakResultPublished)
		{
			BreakResult.FailureReason = EHSRBreakFailureReason::AlreadyPublished;
		}
		else
		{
			Target->bBreakResultPublished = true;
			BreakResult.bTriggered = true;
		}
		UE_LOG(LogTemp, Log, TEXT("P8-003 Break ActionId=%s Target=%s Before=%.2f After=%.2f Triggered=%d Replay=0 FailureReason=%d"),
			*BreakResult.ActionId.ToString(), *BreakResult.TargetParticipantId.ToString(), BreakResult.ToughnessBefore,
			BreakResult.ToughnessAfter, BreakResult.bTriggered ? 1 : 0, static_cast<int32>(BreakResult.FailureReason));
		if (BreakResult.bTriggered)
		{
			const EHSRStatusOperationResult BreakStatusResult = RequestBreakStatus(Command.ActorParticipantId, BreakResult.TargetParticipantId, BreakResult.ActionId);
			UE_LOG(LogTemp, Log, TEXT("P9-003 BreakStatus ActionId=%s Target=%s Result=%d"), *BreakResult.ActionId.ToString(), *BreakResult.TargetParticipantId.ToString(), static_cast<int32>(BreakStatusResult));
			FHSRTurnDelayRequest DelayRequest;
			DelayRequest.ActionId = BreakResult.ActionId;
			DelayRequest.TargetParticipantId = BreakResult.TargetParticipantId;
			LastBreakDelayActionId = DelayRequest.ActionId;
			bLastBreakDelayRegistered = TurnManager->ConsumeBreakDelay(DelayRequest);
		}
		Finalize(Resolution);
		if (!PendingDefeatedParticipantId.IsNone()) { const FName Defeated = PendingDefeatedParticipantId; PendingDefeatedParticipantId = NAME_None; ResolveDefeat(Defeated); }
		else if (!bBattleResultProduced && !TurnManager->ResolveAction(Command.ActorParticipantId)) { UE_LOG(LogTemp, Error, TEXT("UHSRBattleCoordinator::RequestAction - turn resolve failed after formal commit ActionId=%s"), *Command.ActionId.ToString()); }
		return Resolution;
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
	State.LastStatusOperation = LastStatusOperation;
	for (const TPair<FName, TObjectPtr<UHSRStatusComponent>>& Pair : StatusComponents)
	{
		if (Pair.Value) State.Statuses.Append(Pair.Value->GetPublicSnapshots());
	}
	State.Statuses.Sort([](const FHSRStatusPublicSnapshot& A, const FHSRStatusPublicSnapshot& B)
	{
		if (A.TargetParticipantId != B.TargetParticipantId) return A.TargetParticipantId.LexicalLess(B.TargetParticipantId);
		return A.StatusId.LexicalLess(B.StatusId);
	});
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
#if WITH_EDITOR
	ClearDamageTestInjection();
	LastDevelopmentFormalExecutionResult = FHSRFormalDamageExecutionResult();
#endif
	LastActionResolution = FHSRAbilityResolution();
	SkillPointReservations.Empty();
	TeamResourceState = FHSRTeamResourceState();
	bLastBreakDelayRegistered = false;
	LastBreakDelayActionId.Invalidate();
	if (TurnManager)
	{
		TurnManager->Reset();
		TurnManager = nullptr;
	}
	PublishCommandViewState();
}

FHSRDamageResult UHSRBattleCoordinator::ResolveStatusDamage(FName SourceParticipantId, FName TargetParticipantId, const FGuid& ActionId, const UHSRStatusDefinition* Definition)
{
	FHSRDamageResult Failure; Failure.ActionId = ActionId; Failure.DamageType = Definition ? Definition->DamageType : FGameplayTag();
	if (!Definition || Definition->EffectKind != EHSRStatusEffectKind::DamageOverTime) { Failure.Result = EHSRDamageResultType::MissingDamageRule; return Failure; }
	const FHSRBattleParticipant* Source = Participants.FindByPredicate([SourceParticipantId](const FHSRBattleParticipant& P){ return P.ParticipantId == SourceParticipantId; });
	const FHSRBattleParticipant* Target = Participants.FindByPredicate([TargetParticipantId](const FHSRBattleParticipant& P){ return P.ParticipantId == TargetParticipantId; });
	const UHSRDamageRuleDefinition* Rule = Definition->DamageRule.LoadSynchronous();
	if (CurrentState != EHSRBattleCoordinatorState::Spawned) { Failure.Result = EHSRDamageResultType::BattleTerminal; return Failure; }
	if (!ActionId.IsValid()) { Failure.Result = EHSRDamageResultType::DuplicateAction; return Failure; }
	if (!Source || !Source->AbilitySystemComponent.IsValid()) { Failure.Result = EHSRDamageResultType::InvalidSource; return Failure; }
	if (!Target || !Target->AbilitySystemComponent.IsValid()) { Failure.Result = EHSRDamageResultType::InvalidTarget; return Failure; }
	if (!Rule || !Rule->IsValidRuleDefinition()) { Failure.Result = EHSRDamageResultType::MissingDamageRule; return Failure; }
	const FGameplayTag AbilityMultiplierTag = FGameplayTag::RequestGameplayTag(TEXT("Damage.Data.AbilityMultiplier"), false);
	const FGameplayTag DefenseCoefficientTag = FGameplayTag::RequestGameplayTag(TEXT("Damage.Data.DefenseCoefficient"), false);
	const FGameplayTag MinDamageTag = FGameplayTag::RequestGameplayTag(TEXT("Damage.Data.MinDamage"), false);
	const FGameplayTag CritRollTag = FGameplayTag::RequestGameplayTag(TEXT("Damage.Data.CritRoll"), false);
	if (!Definition->DamageType.IsValid() || !AbilityMultiplierTag.IsValid() || !DefenseCoefficientTag.IsValid() || !MinDamageTag.IsValid() || !CritRollTag.IsValid()) { Failure.Result = EHSRDamageResultType::InvalidDamageType; return Failure; }
	FGameplayEffectContextHandle Context = Source->AbilitySystemComponent->MakeEffectContext();
	FHSRDamageEffectContext* DamageContext = static_cast<FHSRDamageEffectContext*>(Context.Get());
	if (!DamageContext || DamageContext->GetScriptStruct() != FHSRDamageEffectContext::StaticStruct()) { Failure.Result = EHSRDamageResultType::SpecCreationFailed; return Failure; }
	DamageContext->DamageContext.ActionId = ActionId; DamageContext->DamageContext.DamageType = Definition->DamageType; DamageContext->DamageContext.AbilityMultiplier = Definition->DamageAbilityMultiplier; DamageContext->DamageContext.CritRoll = 0.0f; DamageContext->DefenseCoefficient = Rule->DefenseCoefficient; DamageContext->MinDamage = Rule->MinDamage;
	const TSubclassOf<UGameplayEffect> DamageClass = Definition->DamageGameplayEffectClass.LoadSynchronous();
	if (!DamageClass) { Failure.Result = EHSRDamageResultType::SpecCreationFailed; return Failure; }
	FGameplayEffectSpecHandle Spec = Source->AbilitySystemComponent->MakeOutgoingSpec(DamageClass, 1.0f, Context);
	if (!Spec.IsValid()) { Failure.Result = EHSRDamageResultType::SpecCreationFailed; return Failure; }
	const FRandomStream RandomStreamBeforeApply = DevelopmentDamageRandomStream;
	DamageContext->DamageContext.CritRoll = DevelopmentDamageRandomStream.GetFraction();
	Spec.Data->SetSetByCallerMagnitude(AbilityMultiplierTag, Definition->DamageAbilityMultiplier); Spec.Data->SetSetByCallerMagnitude(DefenseCoefficientTag, Rule->DefenseCoefficient); Spec.Data->SetSetByCallerMagnitude(MinDamageTag, Rule->MinDamage); Spec.Data->SetSetByCallerMagnitude(CritRollTag, DamageContext->DamageContext.CritRoll);
	const float HealthBefore = Target->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());
	bFormalDamageTransactionOpen = true; PendingDefeatedParticipantId = NAME_None;
#if WITH_EDITOR
	if (bForceStatusDamageApplyFailure)
	{
		DevelopmentDamageRandomStream = RandomStreamBeforeApply; bFormalDamageTransactionOpen = false; Failure.Result = EHSRDamageResultType::EffectApplicationFailed; return Failure;
	}
#endif
	const FActiveGameplayEffectHandle Applied = Source->AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), Target->AbilitySystemComponent.Get());
	if (!Applied.WasSuccessfullyApplied()) { DevelopmentDamageRandomStream = RandomStreamBeforeApply; bFormalDamageTransactionOpen = false; PendingDefeatedParticipantId = NAME_None; Failure.Result = EHSRDamageResultType::EffectApplicationFailed; return Failure; }
	FHSRDamageResult Result = DamageContext->DamageResult; Result.ActionId = ActionId; Result.DamageType = Definition->DamageType; Result.Breakdown.AppliedDamage = FMath::Max(0.0f, HealthBefore - Target->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()));
	if (Result.Result != EHSRDamageResultType::DamageResolved)
	{
		DevelopmentDamageRandomStream = RandomStreamBeforeApply;
		bFormalDamageTransactionOpen = false;
		PendingDefeatedParticipantId = NAME_None;
	}
#if WITH_EDITOR
	else { ++StatusDamageCommitCount; }
#endif
	return Result;
}

void UHSRBattleCoordinator::FinalizeStatusDamage()
{
	if (!PendingDefeatedParticipantId.IsNone()) { const FName Defeated = PendingDefeatedParticipantId; PendingDefeatedParticipantId = NAME_None; bFormalDamageTransactionOpen = false; ResolveDefeat(Defeated); }
	else { bFormalDamageTransactionOpen = false; }
}
#if WITH_EDITOR
FHSRBattleInitResult UHSRBattleCoordinator::ResetAndRebuildForDevelopmentTest(UWorld* BattleWorld)
{
	if (!BattleWorld || !LastSubmittedRequestForDevelopment.IsSet())
	{
		return FHSRBattleInitResult::MakeFailure(EHSRBattleInitFailureType::InitFailed, FText::FromString(TEXT("Missing saved request or BattleWorld.")));
	}
	FHSREncounterRequest SavedRequest = LastSubmittedRequestForDevelopment.GetValue();
	SavedRequest.RequestId = FGuid::NewGuid();
	Reset();
	if (!SubmitBattleRequest(SavedRequest))
	{
		return FHSRBattleInitResult::MakeFailure(EHSRBattleInitFailureType::InitFailed, FText::FromString(TEXT("Formal resubmit failed after Reset.")));
	}
	return BuildParticipants(BattleWorld);
}

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
		if (bFormalDamageTransactionOpen)
		{
			PendingDefeatedParticipantId = ParticipantId;
			return;
		}
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
#if WITH_EDITOR
	LastSourceInvalidRemovedCount = RouteSourceInvalid(DefeatedParticipantId);
#else
	RouteSourceInvalid(DefeatedParticipantId);
#endif
#if WITH_EDITOR
	++DefeatCount;
#endif
	if (UHSRStatusComponent* StatusComponent = GetStatusComponent(DefeatedParticipantId))
	{
		StatusComponent->ClearStatus();
	}
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
	ClearStatusComponents();

	UE_LOG(LogTemp, Log, TEXT("UHSRBattleCoordinator::ResolveDefeat - SUCCESS RequestId=%s Defeated=%s Outcome=%d"), *BattleResult.RequestId.ToString(), *DefeatedParticipantId.ToString(), static_cast<int32>(BattleResult.Outcome));
#if WITH_EDITOR
	++BattleResultBroadcastCount;
#endif
	BattleResultReady.Broadcast(BattleResult);
}

void UHSRBattleCoordinator::ClearRuntimeDelegates()
{
	ClearStatusComponents();
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

bool UHSRBattleCoordinator::InitializeStatusComponents()
{
	ClearStatusComponents();
	if (!TurnManager) return false;
	for (const FHSRBattleParticipant& Participant : Participants)
	{
		if (!Participant.Actor.IsValid() || !Participant.AbilitySystemComponent.IsValid()) return false;
		UHSRStatusComponent* Component = NewObject<UHSRStatusComponent>(Participant.Actor.Get());
		if (!Component || !Component->InitializeStatusRuntime(Participant.ParticipantId, Participant.AbilitySystemComponent.Get())
			|| !Component->BindTurnManager(TurnManager))
		{
			return false;
		}
		Component->BindBattleCoordinator(this);
		Component->RegisterComponent();
		StatusComponents.Add(Participant.ParticipantId, Component);
		StatusChangedHandles.Add(Participant.ParticipantId, Component->OnStatusChanged().AddUObject(this, &UHSRBattleCoordinator::HandleStatusChanged, Participant.ParticipantId));
	}
	return true;
}

void UHSRBattleCoordinator::ClearStatusComponents()
{
	for (TPair<FName, TObjectPtr<UHSRStatusComponent>>& Pair : StatusComponents)
	{
		if (Pair.Value)
		{
			if (const FDelegateHandle* Handle = StatusChangedHandles.Find(Pair.Key)) Pair.Value->OnStatusChanged().Remove(*Handle);
			Pair.Value->ClearStatus();
#if WITH_EDITOR
			LastClearedStatusSnapshots.Add(Pair.Key, Pair.Value->GetSnapshot());
#endif
			Pair.Value->UnbindTurnManager();
			Pair.Value->DestroyComponent();
		}
	}
	StatusComponents.Empty();
	StatusChangedHandles.Empty();
	PublishCommandViewState();
}

void UHSRBattleCoordinator::HandleStatusChanged(FName ParticipantId)
{
	if (const UHSRStatusComponent* Component = GetStatusComponent(ParticipantId)) LastStatusOperation = Component->GetLastPublicOperation();
	PublishCommandViewState();
}

UHSRStatusComponent* UHSRBattleCoordinator::GetStatusComponent(FName ParticipantId) const
{
	const TObjectPtr<UHSRStatusComponent>* Found = StatusComponents.Find(ParticipantId);
	return Found ? Found->Get() : nullptr;
}

EHSRStatusOperationResult UHSRBattleCoordinator::RequestBreakStatus(FName SourceParticipantId, FName TargetParticipantId, const FGuid& OperationId)
{
	if (!BreakStatusDefinition) return EHSRStatusOperationResult::InvalidDefinition;
	UHSRStatusComponent* Component = GetStatusComponent(TargetParticipantId);
	return Component ? Component->AddOrRefreshStatus(BreakStatusDefinition, SourceParticipantId, TargetParticipantId, OperationId)
		: EHSRStatusOperationResult::InvalidTarget;
}

int32 UHSRBattleCoordinator::RouteSourceInvalid(FName SourceParticipantId)
{
	int32 Removed = 0;
	for (TPair<FName, TObjectPtr<UHSRStatusComponent>>& Pair : StatusComponents)
	{
		if (Pair.Value) Removed += Pair.Value->HandleSourceInvalid(SourceParticipantId);
	}
	return Removed;
}

#if WITH_EDITOR
EHSRStatusOperationResult UHSRBattleCoordinator::AddStatusForDevelopmentTest(FName SourceParticipantId, FName TargetParticipantId)
{
	const FHSRBattleParticipant* Source = Participants.FindByPredicate([SourceParticipantId](const FHSRBattleParticipant& Participant)
	{
		return Participant.ParticipantId == SourceParticipantId;
	});
	if (!Source || !Source->IsValid()) return EHSRStatusOperationResult::InvalidSource;
	if (Source->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) <= 0.0f)
	{
		return EHSRStatusOperationResult::InvalidSource;
	}
	UHSRStatusComponent* Component = GetStatusComponent(TargetParticipantId);
	if (!StatusDefinition) return EHSRStatusOperationResult::InvalidDefinition;
	return Component ? Component->AddOrRefreshStatus(StatusDefinition, SourceParticipantId, TargetParticipantId) : EHSRStatusOperationResult::InvalidTarget;
}

EHSRStatusOperationResult UHSRBattleCoordinator::AddDamageOverTimeForDevelopmentTest(FName SourceParticipantId, FName TargetParticipantId, FGuid OperationId)
{
	const FHSRBattleParticipant* Source = Participants.FindByPredicate([SourceParticipantId](const FHSRBattleParticipant& Participant)
	{
		return Participant.ParticipantId == SourceParticipantId;
	});
	if (!Source || !Source->IsValid() || Source->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) <= 0.0f)
	{
		return EHSRStatusOperationResult::InvalidSource;
	}
	UHSRStatusComponent* Component = GetStatusComponent(TargetParticipantId);
	if (!DamageOverTimeStatusDefinition) return EHSRStatusOperationResult::InvalidDefinition;
	return Component ? Component->AddOrRefreshStatus(DamageOverTimeStatusDefinition, SourceParticipantId, TargetParticipantId, OperationId) : EHSRStatusOperationResult::InvalidTarget;
}

EHSRStatusOperationResult UHSRBattleCoordinator::AddSpecificStatusForDevelopmentTest(const UHSRStatusDefinition* Definition, FName SourceParticipantId, FName TargetParticipantId, FGuid OperationId)
{
	UHSRStatusComponent* Component = GetStatusComponent(TargetParticipantId);
	return Component ? Component->AddOrRefreshStatus(Definition, SourceParticipantId, TargetParticipantId, OperationId) : EHSRStatusOperationResult::InvalidTarget;
}

EHSRStatusOperationResult UHSRBattleCoordinator::DispelOneStatusForDevelopmentTest(FName TargetParticipantId)
{
	UHSRStatusComponent* Component = GetStatusComponent(TargetParticipantId);
	return Component ? Component->DispelOneStatus() : EHSRStatusOperationResult::InvalidTarget;
}

void UHSRBattleCoordinator::SetStatusApplyFailureForDevelopmentTest(bool bForce)
{
	for (TPair<FName, TObjectPtr<UHSRStatusComponent>>& Pair : StatusComponents)
	{
		if (Pair.Value) Pair.Value->SetForceApplyFailureForDevelopmentTest(bForce);
	}
}

void UHSRBattleCoordinator::SetDispelRemoveFailureForDevelopmentTest(bool bForce)
{
	for (TPair<FName, TObjectPtr<UHSRStatusComponent>>& Pair : StatusComponents)
	{
		if (Pair.Value) Pair.Value->SetForceDispelRemoveFailureForDevelopmentTest(bForce);
	}
}

FHSRStatusRuntimeSnapshot UHSRBattleCoordinator::GetStatusSnapshotForDevelopmentTest(FName ParticipantId, FName StatusId) const
{
	if (const UHSRStatusComponent* Component = GetStatusComponent(ParticipantId)) return Component->GetSnapshot(StatusId);
	FHSRStatusRuntimeSnapshot Snapshot;
	Snapshot.Result = EHSRStatusOperationResult::InvalidTarget;
	return Snapshot;
}
#endif

bool UHSRBattleCoordinator::ApplyParticipantInitializationGameplayEffect(const FHSRBattleParticipant& Participant)
{
	if (!Participant.AbilitySystemComponent.IsValid() || !ParticipantInitializationGameplayEffect)
	{
		UE_LOG(LogTemp, Error, TEXT("P8-005 InitGE Participant=%s Result=FAILED Reason=MissingASCOrEffect"), *Participant.ParticipantId.ToString());
		return false;
	}
	const FGameplayEffectSpecHandle Spec = Participant.AbilitySystemComponent->MakeOutgoingSpec(ParticipantInitializationGameplayEffect, 1.0f, Participant.AbilitySystemComponent->MakeEffectContext());
	if (!Spec.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("P8-005 InitGE Participant=%s Result=FAILED Reason=MakeSpec"), *Participant.ParticipantId.ToString());
		return false;
	}
	const FActiveGameplayEffectHandle Applied = Participant.AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	if (!Applied.WasSuccessfullyApplied())
	{
		UE_LOG(LogTemp, Error, TEXT("P8-005 InitGE Participant=%s Result=FAILED Reason=Apply"), *Participant.ParticipantId.ToString());
		return false;
	}
	const float Health = Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());
	const float MaxHealth = Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetMaxHealthAttribute());
	const float Energy = Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute());
	const float MaxEnergy = Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetMaxEnergyAttribute());
	const float Speed = Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetSpeedAttribute());
	const float Toughness = Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetToughnessAttribute());
	const float MaxToughness = Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetMaxToughnessAttribute());
	const bool bValid = FMath::IsFinite(Health) && FMath::IsFinite(MaxHealth) && FMath::IsFinite(Energy) && FMath::IsFinite(MaxEnergy) && FMath::IsFinite(Speed) && FMath::IsFinite(Toughness) && FMath::IsFinite(MaxToughness)
		&& Health > 0.0f && MaxHealth > 0.0f && Health <= MaxHealth && Speed > 0.0f && Energy >= 0.0f && MaxEnergy >= 0.0f && Energy <= MaxEnergy && Toughness >= 0.0f && MaxToughness >= 0.0f && Toughness <= MaxToughness;
	if (bValid)
	{
		UE_LOG(LogTemp, Log, TEXT("P8-005 InitGE Participant=%s Result=SUCCESS Health=%.2f MaxHealth=%.2f Energy=%.2f MaxEnergy=%.2f Speed=%.2f Toughness=%.2f MaxToughness=%.2f"), *Participant.ParticipantId.ToString(), Health, MaxHealth, Energy, MaxEnergy, Speed, Toughness, MaxToughness);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("P8-005 InitGE Participant=%s Result=FAILED Health=%.2f MaxHealth=%.2f Energy=%.2f MaxEnergy=%.2f Speed=%.2f Toughness=%.2f MaxToughness=%.2f"), *Participant.ParticipantId.ToString(), Health, MaxHealth, Energy, MaxEnergy, Speed, Toughness, MaxToughness);
	}
	if (!bValid)
	{
		return false;
	}
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
