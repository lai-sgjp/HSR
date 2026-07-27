#include "HSREnemyAIController.h"
#include "HSREnemyCharacter.h"
#include "../Data/Definitions/HSREnemyDefinition.h"
#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "../Character/HSRExplorationCharacter.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
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
	ScheduleNavReadyPatrolIntent();
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
	ScheduleNavReadyPatrolIntent();
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
	bHasPublishedPatrolLocation = false;
	PublishedPatrolLocation = FVector::ZeroVector;
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
	else if (CurrentState == EHSREnemyExplorationState::MovingToPatrol)
	{
		AHSREnemyCharacter* Enemy = Cast<AHSREnemyCharacter>(GetPawn());
		UHSREnemyDefinition* Definition = Enemy ? Enemy->EnemyDefinition : nullptr;
		if (Enemy && Definition)
		{
			PublishNextPatrolIntent(Enemy->GetSpawnOrigin(), Definition->PatrolRadius);
		}
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
		BeginChasingTarget(Actor);
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

void AHSREnemyAIController::BeginChasingTarget(AActor* Actor)
{
	// Perception only publishes Alert/Chasing intent for the stock BT Move To.
	// Encounter admission remains exclusively owned by Character overlap contact.
	if (CurrentState == EHSREnemyExplorationState::EncounterPending)
	{
		return;
	}

	if (CurrentState == EHSREnemyExplorationState::Chasing && CurrentTarget.Get() == Actor)
	{
		UE_LOG(LogTemp, Log, TEXT("P4-002: repeat perception of same target, blocked (no storm)"));
		return;
	}

	SetState(EHSREnemyExplorationState::Alert);
	CurrentTarget = Actor;
	SetBlackboardTarget(Actor);
	StopMovement();
	SetState(EHSREnemyExplorationState::Chasing);

	UE_LOG(LogTemp, Log, TEXT("AHSREnemyAIController::BeginChasingTarget - %s sensed %s, chasing without encounter submission"), *GetName(), *GetNameSafe(Actor));
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
#if WITH_DEV_AUTOMATION_TESTS
	++EncounterSubmissionAttemptsForAutomation;
#endif
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
	if (!Tree || !BlackboardData || Tree->BlackboardAsset != BlackboardData || !UseBlackboard(BlackboardData, BlackboardComponent) || !BlackboardComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("P17-PATCH-02 AI init failed: %s has invalid BT/BB references"), *GetName());
		return false;
	}

	RuntimeBlackboard = BlackboardComponent;
	++BehaviorTreeEpoch;
	ActiveEncounterRequestId.Invalidate();
	PublishNextPatrolIntent(Enemy->GetSpawnOrigin(), Definition->PatrolRadius);
	if (!RunBehaviorTree(Tree))
	{
		ClearBlackboardRuntimeState();
		RuntimeBlackboard = nullptr;
		CurrentState = EHSREnemyExplorationState::Idle;
		UE_LOG(LogTemp, Error, TEXT("P17-PATCH-02 AI init failed: %s could not run the Behavior Tree"), *GetName());
		return false;
	}
	return true;
}

void AHSREnemyAIController::StopBehaviorTreeRuntime()
{
	const bool bHadRuntimeOwnership = RuntimeBlackboard != nullptr || bNavReadyRetryScheduled || ActiveEncounterRequestId.IsValid();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NavReadyRetryTimerHandle);
	}
	bNavReadyRetryScheduled = false;
	NavReadyRetryEpoch = INDEX_NONE;
	if (BrainComponent)
	{
		BrainComponent->StopLogic(TEXT("P17-PATCH-02 lifecycle teardown"));
	}
	ClearBlackboardRuntimeState();
	// ClearState/UnPossess/EndPlay follow this call. Detach before returning so
	// none of their cleanup writes can repopulate the just-cleared Blackboard.
	RuntimeBlackboard = nullptr;
	ActiveEncounterRequestId.Invalidate();
	if (bHadRuntimeOwnership)
	{
		++BehaviorTreeEpoch;
	}
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
#if WITH_DEV_AUTOMATION_TESTS
	BlackboardRuntimeKeyWriteMaskForAutomation = 0x3f;
#endif
}

void AHSREnemyAIController::PublishNextPatrolIntent(const FVector& InSpawnOrigin, float PatrolRadius)
{
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const ANavigationData* NavData = NavSystem ? NavSystem->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
	FNavLocation ProjectedCenter;
	const FVector ProjectionExtent(100.0f, 100.0f, 300.0f);
	const bool bProjected = NavData
		&& NavSystem->ProjectPointToNavigation(InSpawnOrigin, ProjectedCenter, ProjectionExtent, NavData);
	FNavLocation Candidate;
	const bool bHasReachableCandidate = bProjected
		&& NavSystem->GetRandomReachablePointInRadius(ProjectedCenter.Location, FMath::Max(0.0f, PatrolRadius), Candidate);
	UE_LOG(LogTemp, Log, TEXT("P17-PATCH-02 PatrolIntent Controller=%s Center=%s Radius=%.2f NavSystem=%s NavData=%s Result=%s Candidate=%s"),
		*GetName(), *InSpawnOrigin.ToString(), PatrolRadius, *GetNameSafe(NavSystem), *GetNameSafe(NavData),
		bHasReachableCandidate ? TEXT("Reachable") : TEXT("Fallback"), *Candidate.Location.ToString());
	UE_LOG(LogTemp, Log, TEXT("P17-PATCH-02 PatrolProjection Controller=%s Input=%s Extent=%s Projected=%s Result=%s Random=%s"),
		*GetName(), *InSpawnOrigin.ToString(), *ProjectionExtent.ToString(), *ProjectedCenter.Location.ToString(),
		bProjected ? TEXT("Success") : TEXT("Failed"), bHasReachableCandidate ? TEXT("Success") : TEXT("SkippedOrFailed"));
	if (!bHasReachableCandidate)
	{
		UE_LOG(LogTemp, Warning, TEXT("P17-PATCH-02 patrol intent fallback: %s Reason=%s"), *GetName(),
			bProjected ? TEXT("RandomReachableFailed") : TEXT("ProjectPointFailed"));
	}
	PublishPatrolIntent(InSpawnOrigin, Candidate.Location, bHasReachableCandidate
		? EHSRPatrolIntentResult::Reachable
		: (bProjected ? EHSRPatrolIntentResult::RandomReachableFailed : EHSRPatrolIntentResult::ProjectPointFailed));
}

void AHSREnemyAIController::ScheduleNavReadyPatrolIntent()
{
	if (bNavReadyRetryScheduled || !RuntimeBlackboard || !GetWorld())
	{
		return;
	}

	AHSREnemyCharacter* Enemy = Cast<AHSREnemyCharacter>(GetPawn());
	UHSREnemyDefinition* Definition = Enemy ? Enemy->EnemyDefinition : nullptr;
	if (!Enemy || !Definition)
	{
		return;
	}

	const int32 ScheduledEpoch = BehaviorTreeEpoch;
	bNavReadyRetryScheduled = true;
	NavReadyRetryEpoch = ScheduledEpoch;
	UE_LOG(LogTemp, Log, TEXT("P17-PATCH-02 NavReadyRetry Scheduled Controller=%s Epoch=%d Center=%s Radius=%.2f"),
		*GetName(), ScheduledEpoch, *Enemy->GetSpawnOrigin().ToString(), Definition->PatrolRadius);
	GetWorld()->GetTimerManager().SetTimer(NavReadyRetryTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this, ScheduledEpoch]() { RunNavReadyPatrolIntent(ScheduledEpoch); }), 0.2f, false);
}

void AHSREnemyAIController::RunNavReadyPatrolIntent(int32 ScheduledEpoch)
{
	if (!bNavReadyRetryScheduled || NavReadyRetryEpoch != ScheduledEpoch)
	{
		return;
	}

	bNavReadyRetryScheduled = false;
	NavReadyRetryEpoch = INDEX_NONE;
	if (BehaviorTreeEpoch != ScheduledEpoch)
	{
		UE_LOG(LogTemp, Log, TEXT("P17-PATCH-02 NavReadyRetry Stale Controller=%s ScheduledEpoch=%d CurrentEpoch=%d"), *GetName(), ScheduledEpoch, BehaviorTreeEpoch);
		return;
	}

	AHSREnemyCharacter* Enemy = Cast<AHSREnemyCharacter>(GetPawn());
	UHSREnemyDefinition* Definition = Enemy ? Enemy->EnemyDefinition : nullptr;
	if (Enemy && Definition)
	{
		PublishNextPatrolIntent(Enemy->GetSpawnOrigin(), Definition->PatrolRadius);
	}
}

void AHSREnemyAIController::PublishPatrolIntent(const FVector& InSpawnOrigin, const FVector& InCandidate, EHSRPatrolIntentResult Result)
{
	const bool bHasReachableCandidate = Result == EHSRPatrolIntentResult::Reachable;
	const FVector PublishedLocation = bHasReachableCandidate ? InCandidate : InSpawnOrigin;
	bHasPublishedPatrolLocation = true;
	PublishedPatrolLocation = PublishedLocation;
	SetState(bHasReachableCandidate ? EHSREnemyExplorationState::MovingToPatrol : EHSREnemyExplorationState::PatrolWaiting);

	if (!RuntimeBlackboard)
	{
		return;
	}

	RuntimeBlackboard->SetValueAsVector(HSREnemyBlackboardKeys::SpawnOrigin, InSpawnOrigin);
	RuntimeBlackboard->SetValueAsVector(HSREnemyBlackboardKeys::PatrolLocation, PublishedLocation);
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
#if WITH_DEV_AUTOMATION_TESTS
	BlackboardRuntimeKeyWriteMaskForAutomation = 0;
#endif
}

void AHSREnemyAIController::SetBlackboardTarget(AActor* Target)
{
	if (RuntimeBlackboard)
	{
		RuntimeBlackboard->SetValueAsObject(HSREnemyBlackboardKeys::TargetActor, Target);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
bool AHSREnemyAIController::GetPatrolLocationForAutomation(FVector& OutPatrolLocation) const
{
	if (!RuntimeBlackboard)
	{
		OutPatrolLocation = PublishedPatrolLocation;
		return bHasPublishedPatrolLocation;
	}

	OutPatrolLocation = RuntimeBlackboard->GetValueAsVector(HSREnemyBlackboardKeys::PatrolLocation);
	return true;
}

void AHSREnemyAIController::PublishPatrolIntentForAutomation(UBlackboardComponent* InBlackboard, const FVector& InSpawnOrigin, const FVector& InCandidate, EHSRPatrolIntentResult Result)
{
	RuntimeBlackboard = InBlackboard;
	PublishPatrolIntent(InSpawnOrigin, InCandidate, Result);
}

bool AHSREnemyAIController::ArmNavReadyRetryForAutomation(int32 InEpoch)
{
	if (bNavReadyRetryScheduled)
	{
		return false;
	}
	bNavReadyRetryScheduled = true;
	NavReadyRetryEpoch = InEpoch;
	return true;
}

bool AHSREnemyAIController::ConsumeNavReadyRetryForAutomation(int32 InEpoch)
{
	if (!bNavReadyRetryScheduled || NavReadyRetryEpoch != InEpoch)
	{
		return false;
	}
	bNavReadyRetryScheduled = false;
	NavReadyRetryEpoch = INDEX_NONE;
	return true;
}

void AHSREnemyAIController::ApplySuccessfulPerceptionForAutomation(AActor* Target)
{
	BeginChasingTarget(Target);
}

void AHSREnemyAIController::BindRuntimeBlackboardForAutomation(UBlackboardComponent* InBlackboard)
{
	RuntimeBlackboard = InBlackboard;
	++BehaviorTreeEpoch;
}

void AHSREnemyAIController::StopBehaviorTreeRuntimeForAutomation()
{
	StopBehaviorTreeRuntime();
}

void AHSREnemyAIController::ClearStateForAutomation()
{
	ClearState();
}

bool AHSREnemyAIController::AreBlackboardRuntimeKeysClearForAutomation(const UBlackboardComponent* InBlackboard) const
{
	return InBlackboard && BlackboardRuntimeKeyWriteMaskForAutomation == 0;
}
#endif
