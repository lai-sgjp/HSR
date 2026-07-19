#include "HSRBattleCoordinator.h"
#include "HSRTurnManager.h"
#include "HSREncounterTypes.h"
#include "../GAS/Ability/HSRBasicAttackAbility.h"
#include "../GAS/HSRAbilitySystemComponent.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "GameplayEffectTypes.h"

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

	UE_LOG(LogTemp, Log,
		TEXT("UHSRBattleCoordinator::BuildParticipants - SUCCESS RequestId=%s Participants=%d Definitions=%d"),
		*CurrentRequestId.ToString(), Participants.Num(), ParticipantDefinitions.Num());

	return FHSRBattleInitResult::MakeSuccess();
}

bool UHSRBattleCoordinator::RequestBasicAttack(FName AttackerParticipantId, FName TargetParticipantId)
{
	if (CurrentState != EHSRBattleCoordinatorState::Spawned || !TurnManager || TurnManager->GetCurrentParticipantId() != AttackerParticipantId)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleCoordinator::RequestBasicAttack - REJECTED non-current or inactive Attacker=%s Current=%s"), *AttackerParticipantId.ToString(), TurnManager ? *TurnManager->GetCurrentParticipantId().ToString() : TEXT("None"));
		return false;
	}

	const FHSRBattleParticipant* Attacker = Participants.FindByPredicate([AttackerParticipantId](const FHSRBattleParticipant& Participant) { return Participant.ParticipantId == AttackerParticipantId; });
	const FHSRBattleParticipant* Target = Participants.FindByPredicate([TargetParticipantId](const FHSRBattleParticipant& Participant) { return Participant.ParticipantId == TargetParticipantId; });
	if (!Attacker || !Target || !Attacker->IsValid() || !Target->IsValid() || Attacker->Team == Target->Team)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleCoordinator::RequestBasicAttack - REJECTED invalid target Attacker=%s Target=%s"), *AttackerParticipantId.ToString(), *TargetParticipantId.ToString());
		return false;
	}

	FGameplayAbilitySpec* AbilitySpec = Attacker->AbilitySystemComponent->FindAbilitySpecFromClass(UHSRBasicAttackAbility::StaticClass());
	UHSRBasicAttackAbility* Ability = AbilitySpec ? Cast<UHSRBasicAttackAbility>(AbilitySpec->GetPrimaryInstance()) : nullptr;
	if (!Ability || !Ability->SetPendingTarget(Target->AbilitySystemComponent.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleCoordinator::RequestBasicAttack - REJECTED missing BasicAttack ability Attacker=%s"), *AttackerParticipantId.ToString());
		return false;
	}

	if (!Attacker->AbilitySystemComponent->TryActivateAbility(AbilitySpec->Handle) || !Ability->DidLastActivationSucceed())
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleCoordinator::RequestBasicAttack - REJECTED activation failed Attacker=%s Target=%s"), *AttackerParticipantId.ToString(), *TargetParticipantId.ToString());
		return false;
	}

	// The instant effect may synchronously produce a terminal result through the
	// health observer. A completed battle must not advance another turn.
	if (bBattleResultProduced)
	{
		UE_LOG(LogTemp, Log, TEXT("UHSRBattleCoordinator::RequestBasicAttack - SUCCESS terminal attack Attacker=%s Target=%s"), *AttackerParticipantId.ToString(), *TargetParticipantId.ToString());
		return true;
	}

	const bool bResolved = TurnManager->ResolveAction(AttackerParticipantId);
	if (bResolved)
	{
		UE_LOG(LogTemp, Log, TEXT("UHSRBattleCoordinator::RequestBasicAttack - SUCCESS Attacker=%s Target=%s"), *AttackerParticipantId.ToString(), *TargetParticipantId.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UHSRBattleCoordinator::RequestBasicAttack - FAILED ResolveAction Attacker=%s Target=%s"), *AttackerParticipantId.ToString(), *TargetParticipantId.ToString());
	}
	return bResolved;
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
	if (TurnManager)
	{
		TurnManager->Reset();
		TurnManager = nullptr;
	}
}

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
	if (!Participant.AbilitySystemComponent.IsValid())
	{
		return false;
	}

	const FGameplayAbilitySpecHandle AbilityHandle = Participant.AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(UHSRBasicAttackAbility::StaticClass(), 1, INDEX_NONE, this));
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
