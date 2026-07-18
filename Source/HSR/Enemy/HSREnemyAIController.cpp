#include "HSREnemyAIController.h"
#include "HSREnemyCharacter.h"
#include "../Data/Definitions/HSREnemyDefinition.h"
#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "../Character/HSRExplorationCharacter.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/Character.h"

AHSREnemyAIController::AHSREnemyAIController()
{
	PrimaryActorTick.bCanEverTick = false;

	PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComponent"));

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 1000.0f;
	SightConfig->LoseSightRadius = 1500.0f;
	SightConfig->PeripheralVisionAngleDegrees = 90.0f;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->SetMaxAge(5.0f);

	PerceptionComponent->ConfigureSense(*SightConfig);
	PerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());

	CurrentState = EHSREnemyExplorationState::Idle;
}

void AHSREnemyAIController::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("AHSREnemyAIController::BeginPlay - %s"), *GetName());

	// Bind perception delegate
	if (PerceptionComponent)
	{
		PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &AHSREnemyAIController::OnTargetPerceptionUpdated);
	}

	// Start patrol after a short frame delay to allow NavMesh to initialize
	FTimerHandle InitTimer;
	GetWorld()->GetTimerManager().SetTimer(InitTimer, this, &AHSREnemyAIController::StartPatrol, 0.2f, false);
}

void AHSREnemyAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearState();

	if (PerceptionComponent)
	{
		PerceptionComponent->OnTargetPerceptionUpdated.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AHSREnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	UE_LOG(LogTemp, Log, TEXT("AHSREnemyAIController::OnPossess - Controller=%s Pawn=%s"), *GetName(), InPawn ? *InPawn->GetName() : TEXT("null"));
}

void AHSREnemyAIController::OnUnPossess()
{
	ClearState();
	Super::OnUnPossess();
}

void AHSREnemyAIController::SetState(EHSREnemyExplorationState NewState)
{
	if (CurrentState == NewState)
		return;

	EHSREnemyExplorationState OldState = CurrentState;
	CurrentState = NewState;

	UE_LOG(LogTemp, Log, TEXT("AHSREnemyAIController::SetState - %s: %d -> %d"),
		*GetName(), static_cast<int32>(OldState), static_cast<int32>(NewState));
}

void AHSREnemyAIController::ClearState()
{
	// Clear patrol timer
	GetWorld()->GetTimerManager().ClearTimer(PatrolWaitTimerHandle);

	// Stop movement
	if (GetPathFollowingComponent())
	{
		GetPathFollowingComponent()->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished);
	}

	CurrentTarget.Reset();
	CurrentState = EHSREnemyExplorationState::Idle;
}

void AHSREnemyAIController::StartPatrol()
{
	AHSREnemyCharacter* EnemyChar = Cast<AHSREnemyCharacter>(GetPawn());
	if (!EnemyChar)
	{
		ClearState();
		return;
	}

	UHSREnemyDefinition* Def = EnemyChar->EnemyDefinition;
	if (!Def)
	{
		SetState(EHSREnemyExplorationState::Idle);
		UE_LOG(LogTemp, Log, TEXT("AHSREnemyAIController::StartPatrol - %s: no Definition, idle"), *GetName());
		return;
	}

	SetState(EHSREnemyExplorationState::MovingToPatrol);

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSREnemyAIController::StartPatrol - %s: no NavSystem"), *GetName());
		HandleMoveFailedOrAborted();
		return;
	}

	FNavLocation NavLoc;
	if (NavSys->GetRandomReachablePointInRadius(EnemyChar->GetSpawnOrigin(), Def->PatrolRadius, NavLoc))
	{
		EPathFollowingRequestResult::Type MoveResult = MoveToLocation(NavLoc.Location, -1.f, false);
		if (MoveResult == EPathFollowingRequestResult::AlreadyAtGoal)
		{
			UE_LOG(LogTemp, Log, TEXT("AHSREnemyAIController::StartPatrol - %s: already at patrol goal"), *GetName());
			SetState(EHSREnemyExplorationState::PatrolWaiting);
			GetWorld()->GetTimerManager().SetTimer(PatrolWaitTimerHandle, this, &AHSREnemyAIController::StartPatrol, Def->PatrolWaitTime, false);
		}
		else if (MoveResult == EPathFollowingRequestResult::Failed)
		{
			UE_LOG(LogTemp, Warning, TEXT("AHSREnemyAIController::StartPatrol - %s: MoveTo Failed"), *GetName());
			HandleMoveFailedOrAborted();
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSREnemyAIController::StartPatrol - %s: no reachable point"), *GetName());
		SetState(EHSREnemyExplorationState::PatrolWaiting);
		GetWorld()->GetTimerManager().SetTimer(PatrolWaitTimerHandle, this, &AHSREnemyAIController::StartPatrol, Def->PatrolWaitTime, false);
	}
}

void AHSREnemyAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	// Don't process moves after EncounterPending or during cleanup
	if (CurrentState == EHSREnemyExplorationState::EncounterPending ||
		CurrentState == EHSREnemyExplorationState::Idle)
	{
		return;
	}

	if (CurrentState == EHSREnemyExplorationState::MovingToPatrol)
	{
		// Patrol move completed
		if (Result.IsSuccess())
		{
			AHSREnemyCharacter* EnemyChar = Cast<AHSREnemyCharacter>(GetPawn());
			if (!EnemyChar) return;

			UHSREnemyDefinition* Def = EnemyChar->EnemyDefinition;
			float WaitTime = Def ? Def->PatrolWaitTime : 3.0f;

			SetState(EHSREnemyExplorationState::PatrolWaiting);
			GetWorld()->GetTimerManager().SetTimer(PatrolWaitTimerHandle, this, &AHSREnemyAIController::StartPatrol, WaitTime, false);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("AHSREnemyAIController::OnMoveCompleted - %s: patrol move %s"),
				*GetName(), *Result.ToString());
			HandleMoveFailedOrAborted();
		}
	}
	else if (CurrentState == EHSREnemyExplorationState::Chasing)
	{
		// Chase move completed - re-evaluate target
		AActor* Target = CurrentTarget.Get();
		if (Target && IsValid(Target))
		{
			// Continue chasing
			MoveToActor(Target);
		}
		else
		{
			HandleChaseTargetLost();
		}
	}
}

void AHSREnemyAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	// Only interested in ExplorationCharacter
	AHSRExplorationCharacter* PlayerChar = Cast<AHSRExplorationCharacter>(Actor);
	if (!PlayerChar)
		return;

	if (Stimulus.WasSuccessfullySensed())
	{
		// Target sensed - start chasing if not already EncounterPending
		if (CurrentState == EHSREnemyExplorationState::EncounterPending)
		{
			return;
		}

		// If already chasing the same target, don't restart
		if (CurrentState == EHSREnemyExplorationState::Chasing && CurrentTarget.Get() == Actor)
		{
			return;
		}

		// Stop patrol timer and set new target
		SetState(EHSREnemyExplorationState::Alert);
		CurrentTarget = Actor;

		GetWorld()->GetTimerManager().ClearTimer(PatrolWaitTimerHandle);
		StopMovement();

		// Start chase
		SetState(EHSREnemyExplorationState::Chasing);
		MoveToActor(Actor);

		UE_LOG(LogTemp, Log, TEXT("AHSREnemyAIController::OnTargetPerceptionUpdated - %s sensed %s, chasing"), *GetName(), *Actor->GetName());
	}
	else
	{
		// Target lost - only react if we are chasing THIS target
		if (CurrentTarget.Get() == Actor && 
			(CurrentState == EHSREnemyExplorationState::Chasing || CurrentState == EHSREnemyExplorationState::Alert))
		{
			UE_LOG(LogTemp, Log, TEXT("AHSREnemyAIController::OnTargetPerceptionUpdated - %s lost sight of %s"), *GetName(), *Actor->GetName());
			HandleChaseTargetLost();
		}
	}
}

void AHSREnemyAIController::HandleChaseTargetLost()
{
	CurrentTarget.Reset();
	StopMovement();

	SetState(EHSREnemyExplorationState::LostTarget);

	// Return to patrol after a short delay
	float WaitTime = 2.0f;
	GetWorld()->GetTimerManager().SetTimer(PatrolWaitTimerHandle, this, &AHSREnemyAIController::StartPatrol, WaitTime, false);
}

void AHSREnemyAIController::HandleMoveFailedOrAborted()
{
	SetState(EHSREnemyExplorationState::MoveFailed);
	StopMovement();

	AHSREnemyCharacter* EnemyChar = Cast<AHSREnemyCharacter>(GetPawn());
	UHSREnemyDefinition* Def = EnemyChar ? EnemyChar->EnemyDefinition : nullptr;
	float WaitTime = Def ? Def->PatrolWaitTime : 3.0f;

	// Retry patrol after wait
	GetWorld()->GetTimerManager().SetTimer(PatrolWaitTimerHandle, this, &AHSREnemyAIController::StartPatrol, WaitTime, false);
}

void AHSREnemyAIController::TryRequestEncounterFromCharacter()
{
	// Only process Encounter requests while in Chasing state
	if (CurrentState != EHSREnemyExplorationState::Chasing)
	{
		UE_LOG(LogTemp, Log, TEXT("AHSREnemyAIController::TryRequestEncounter - %s FAILED (state=%d, not Chasing)"),
			*GetName(), static_cast<int32>(CurrentState));
		return;
	}

	AHSREnemyCharacter* EnemyChar = Cast<AHSREnemyCharacter>(GetPawn());
	if (!EnemyChar)
		return;

	UHSREnemyDefinition* Def = EnemyChar->EnemyDefinition;
	if (!Def)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSREnemyAIController::TryRequestEncounter - %s FAILED (EnemyDefinition null)"), *GetName());
		return;
	}

	UHSREncounterDefinition* EncounterDef = Def->EncounterDefinition;
	if (!EncounterDef)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSREnemyAIController::TryRequestEncounter - %s FAILED (EncounterDefinition null, EnemyId=%s)"),
			*GetName(), *Def->EnemyDefinitionId.ToString());
		return;
	}

	UHSRBattleTransitionSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UHSRBattleTransitionSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSREnemyAIController::TryRequestEncounter - %s FAILED (no Subsystem)"), *GetName());
		return;
	}

	if (Subsystem->HasPending())
	{
		UE_LOG(LogTemp, Log, TEXT("AHSREnemyAIController::TryRequestEncounter - %s: subsystem already pending, skipping"), *GetName());
		return;
	}

	FHSREncounterResult EncResult = Subsystem->RequestEncounter(EncounterDef);
	if (EncResult.ResultType == EHSREncounterResultType::Success)
	{
		SetState(EHSREnemyExplorationState::EncounterPending);
		StopMovement();
		GetWorld()->GetTimerManager().ClearTimer(PatrolWaitTimerHandle);

		UE_LOG(LogTemp, Log, TEXT("AHSREnemyAIController::TryRequestEncounter - %s SUCCESS (RequestId=%s, EnemyId=%s)"),
			*GetName(), *EncResult.RequestId.ToString(), *Def->EnemyDefinitionId.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSREnemyAIController::TryRequestEncounter - %s FAILED (type=%d, msg=%s)"),
			*GetName(), static_cast<int32>(EncResult.ResultType), *EncResult.Message.ToString());
		// Stay in Chasing state - Encounter overlap may be retriggered
	}
}
