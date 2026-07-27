#include "HSREnemyAIController.h"
#include "HSREnemyCharacter.h"
#include "../Data/Definitions/HSREnemyDefinition.h"
#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "../Character/HSRExplorationCharacter.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/Character.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"

namespace HSREnemyBlackboardKeys
{
	static const FName TargetActor(TEXT("TargetActor"));
	static const FName SpawnOrigin(TEXT("SpawnOrigin"));
	static const FName PatrolLocation(TEXT("PatrolLocation"));
	static const FName AIState(TEXT("AIState"));
	static const FName TreeEpoch(TEXT("TreeEpoch"));
	static const FName EncounterRequestId(TEXT("EncounterRequestId"));
}

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

	// Stage A deliberately does not start the legacy patrol timer. Stage B stock
	// Move To/Wait nodes are the sole movement driver after the BT is populated.
}

void AHSREnemyAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopBehaviorTreeRuntime();
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

	// P4-002: Re-Possess -> fresh binding, single observation chain
	UE_LOG(LogTemp, Log, TEXT("P4-002: %s - OnPossess, fresh delegate binding (single observation chain)"), *GetName());

	// Bind perception delegate when possessing a pawn
	if (PerceptionComponent)
	{
		PerceptionComponent->OnTargetPerceptionUpdated.RemoveAll(this);
		PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &AHSREnemyAIController::OnTargetPerceptionUpdated);
	}

	StartBehaviorTreeRuntime();
}

void AHSREnemyAIController::OnUnPossess()
{
	// P4-002: UnPossess -> clear timers/state, remove delegates (zero stale callbacks)
	UE_LOG(LogTemp, Log, TEXT("P4-002: %s - OnUnPossess, clearing state and delegates"), *GetName());
	StopBehaviorTreeRuntime();
	ClearState();

	if (PerceptionComponent)
	{
		PerceptionComponent->OnTargetPerceptionUpdated.RemoveAll(this);
	}

	Super::OnUnPossess();
}

void AHSREnemyAIController::SetState(EHSREnemyExplorationState NewState)
{
	if (CurrentState == NewState)
		return;

	EHSREnemyExplorationState OldState = CurrentState;
	CurrentState = NewState;
	WriteBlackboardRuntimeState();

	UE_LOG(LogTemp, Log, TEXT("AHSREnemyAIController::SetState - %s: %d -> %d"),
		*GetName(), static_cast<int32>(OldState), static_cast<int32>(NewState));
}

void AHSREnemyAIController::ClearState()
{
	// Stop movement
	if (GetPathFollowingComponent())
	{
		GetPathFollowingComponent()->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished);
	}

	CurrentTarget.Reset();
	SetBlackboardTarget(nullptr);
	CurrentState = EHSREnemyExplorationState::Idle;
	WriteBlackboardRuntimeState();
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

	if (!Result.IsSuccess())
	{
		HandleMoveFailedOrAborted();
	}
	else if (CurrentState == EHSREnemyExplorationState::Chasing)
	{
		AActor* Target = CurrentTarget.Get();
		if (!Target || !IsValid(Target))
		{
				// P4-002: Target destroyed/expired during chase, safe cleanup via weak reference
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
			UE_LOG(LogTemp, Log, TEXT("P4-002: repeat perception of same target, blocked (no storm)"));
			return;
		}

		// Stop patrol timer and set new target
		SetState(EHSREnemyExplorationState::Alert);
		CurrentTarget = Actor;
		SetBlackboardTarget(Actor);

		StopMovement();

	// Stage B's stock Move To node consumes TargetActor. C++ owns the state and
	// encounter admission, but never issues a competing movement request.
	SetState(EHSREnemyExplorationState::Chasing);

	// A4c: Try request encounter now that we're in Chasing state.
	// The physical overlap (NotifyActorBeginOverlap) may have already fired while the AI
	// was in PatrolWaiting/MovingToPatrol and was rejected by the Chasing guard.
	// Retrying here ensures the encounter proceeds once the AI is properly chasing.
	TryRequestEncounterFromCharacter();

	UE_LOG(LogTemp, Log, TEXT("AHSREnemyAIController::OnTargetPerceptionUpdated - %s sensed %s, chasing"), *GetName(), *Actor->GetName());
	}
	else
	{
		// Target lost - only react if we are chasing THIS target
		if (CurrentTarget.Get() == Actor && 
			(CurrentState == EHSREnemyExplorationState::Chasing || CurrentState == EHSREnemyExplorationState::Alert))
		{
			UE_LOG(LogTemp, Log, TEXT("AHSREnemyAIController::OnTargetPerceptionUpdated - %s lost sight of %s"), *GetName(), *Actor->GetName());
				// P4-002: Target destroyed/expired during chase, safe cleanup via weak reference
				HandleChaseTargetLost();
		}
	}
}

void AHSREnemyAIController::HandleChaseTargetLost()
{
	CurrentTarget.Reset();
	SetBlackboardTarget(nullptr);
	BeginSpawnOriginRecovery(EHSREnemyExplorationState::LostTarget);
}

void AHSREnemyAIController::HandleMoveFailedOrAborted()
{
	BeginSpawnOriginRecovery(EHSREnemyExplorationState::MoveFailed);
}

void AHSREnemyAIController::BeginSpawnOriginRecovery(EHSREnemyExplorationState RecoveryState)
{
	AHSREnemyCharacter* Enemy = Cast<AHSREnemyCharacter>(GetPawn());
	if (!Enemy)
		return;

	SetState(RecoveryState);
	if (RuntimeBlackboard)
	{
		RuntimeBlackboard->SetValueAsVector(HSREnemyBlackboardKeys::PatrolLocation, Enemy->GetSpawnOrigin());
	}
	SetState(EHSREnemyExplorationState::ReturningToSpawnOrigin);
	// Stage B's stock Move To consumes PatrolLocation=SpawnOrigin.
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
	if (ActiveEncounterRequestId.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("P17-PATCH-02 Encounter duplicate rejected: %s RequestId=%s"), *GetName(), *ActiveEncounterRequestId.ToString());
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

	FHSREncounterResult EncResult = Subsystem->RequestEncounter(EncounterDef, EHSREncounterInitiative::Enemy);
	if (EncResult.ResultType == EHSREncounterResultType::Success)
	{
		ActiveEncounterRequestId = EncResult.RequestId;
		SetState(EHSREnemyExplorationState::EncounterPending);
		StopMovement();

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

bool AHSREnemyAIController::StartBehaviorTreeRuntime()
{
	AHSREnemyCharacter* Enemy = Cast<AHSREnemyCharacter>(GetPawn());
	UHSREnemyDefinition* Definition = Enemy ? Enemy->EnemyDefinition : nullptr;
	UBehaviorTree* Tree = Definition ? Definition->BehaviorTreeAsset.LoadSynchronous() : nullptr;
	UBlackboardData* BlackboardData = Definition ? Definition->BlackboardAsset.LoadSynchronous() : nullptr;
	UBlackboardComponent* BlackboardComponent = nullptr;
	if (!Tree || !BlackboardData || Tree->BlackboardAsset != BlackboardData || !UseBlackboard(BlackboardData, BlackboardComponent) || !BlackboardComponent || !RunBehaviorTree(Tree))
	{
		UE_LOG(LogTemp, Error, TEXT("P17-PATCH-02 AI init failed: %s has invalid BT/BB references"), *GetName());
		return false;
	}

	RuntimeBlackboard = BlackboardComponent;
	++BehaviorTreeEpoch;
	ActiveEncounterRequestId.Invalidate();
	WriteBlackboardRuntimeState();
	return true;
}

void AHSREnemyAIController::StopBehaviorTreeRuntime()
{
	ClearBlackboardRuntimeState();
	if (BrainComponent)
	{
		BrainComponent->StopLogic(TEXT("P17-PATCH-02 lifecycle teardown"));
	}
	ActiveEncounterRequestId.Invalidate();
	++BehaviorTreeEpoch;
}

void AHSREnemyAIController::WriteBlackboardRuntimeState()
{
	if (!RuntimeBlackboard)
		return;

	const AHSREnemyCharacter* Enemy = Cast<AHSREnemyCharacter>(GetPawn());
	RuntimeBlackboard->SetValueAsVector(HSREnemyBlackboardKeys::SpawnOrigin, Enemy ? Enemy->GetSpawnOrigin() : FVector::ZeroVector);
	RuntimeBlackboard->SetValueAsEnum(HSREnemyBlackboardKeys::AIState, static_cast<uint8>(CurrentState));
	RuntimeBlackboard->SetValueAsInt(HSREnemyBlackboardKeys::TreeEpoch, BehaviorTreeEpoch);
	RuntimeBlackboard->SetValueAsName(HSREnemyBlackboardKeys::EncounterRequestId, ActiveEncounterRequestId.IsValid() ? FName(*ActiveEncounterRequestId.ToString()) : NAME_None);
	SetBlackboardTarget(CurrentTarget.Get());
}

void AHSREnemyAIController::ClearBlackboardRuntimeState()
{
	if (!RuntimeBlackboard)
		return;

	RuntimeBlackboard->ClearValue(HSREnemyBlackboardKeys::TargetActor);
	RuntimeBlackboard->ClearValue(HSREnemyBlackboardKeys::PatrolLocation);
	RuntimeBlackboard->ClearValue(HSREnemyBlackboardKeys::EncounterRequestId);
	RuntimeBlackboard->ClearValue(HSREnemyBlackboardKeys::TreeEpoch);
	RuntimeBlackboard->ClearValue(HSREnemyBlackboardKeys::AIState);
	RuntimeBlackboard->ClearValue(HSREnemyBlackboardKeys::SpawnOrigin);
}

void AHSREnemyAIController::SetBlackboardTarget(AActor* Target)
{
	if (RuntimeBlackboard)
	{
		RuntimeBlackboard->SetValueAsObject(HSREnemyBlackboardKeys::TargetActor, Target);
	}
}
