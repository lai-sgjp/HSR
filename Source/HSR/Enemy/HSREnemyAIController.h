#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "Engine/TimerHandle.h"
#include "HSREnemyTypes.h"
#include "HSREnemyAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UBlackboardComponent;

UCLASS()
class HSR_API AHSREnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AHSREnemyAIController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UFUNCTION(BlueprintPure, Category = "Enemy|AI")
	EHSREnemyExplorationState GetCurrentState() const { return CurrentState; }

	UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
	void TryRequestEncounterFromCharacter();

	UFUNCTION(BlueprintPure, Category = "Enemy|AI")
	int32 GetBehaviorTreeEpoch() const { return BehaviorTreeEpoch; }

#if WITH_DEV_AUTOMATION_TESTS
	bool GetPatrolLocationForAutomation(FVector& OutPatrolLocation) const;
	void PublishPatrolIntentForAutomation(UBlackboardComponent* InBlackboard, const FVector& InSpawnOrigin, const FVector& InCandidate, EHSRPatrolIntentResult Result);
	bool ArmNavReadyRetryForAutomation(int32 InEpoch);
	bool ConsumeNavReadyRetryForAutomation(int32 InEpoch);
	void ApplySuccessfulPerceptionForAutomation(AActor* Target);
	bool HasActiveEncounterRequestForAutomation() const { return ActiveEncounterRequestId.IsValid(); }
	int32 GetEncounterSubmissionAttemptsForAutomation() const { return EncounterSubmissionAttemptsForAutomation; }
	void BindRuntimeBlackboardForAutomation(UBlackboardComponent* InBlackboard);
	void StopBehaviorTreeRuntimeForAutomation();
	void ClearStateForAutomation();
	bool HasRuntimeBlackboardForAutomation() const { return RuntimeBlackboard != nullptr; }
	void SetActiveEncounterRequestForAutomation(const FGuid& InRequestId);
	void PublishMoveFailureRecoveryForAutomation(UBlackboardComponent* InBlackboard, const FVector& InSpawnOrigin);
	void PublishLostTargetRecoveryForAutomation(UBlackboardComponent* InBlackboard, const FVector& InSpawnOrigin);
	EHSREnemyExplorationState GetLastRecoveryStateForAutomation() const { return LastRecoveryStateForAutomation; }
	bool IsNavReadyRetryScheduledForAutomation() const { return bNavReadyRetryScheduled; }
	void CompleteReturnToPatrolForAutomation(UBlackboardComponent* InBlackboard, const FVector& InSpawnOrigin, const FVector& InCandidate, EHSRPatrolIntentResult Result);
	bool HandleMoveFailureForAutomation(UBlackboardComponent* InBlackboard, const FVector& InSpawnOrigin);
#endif

protected:
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);
	void BeginChasingTarget(AActor* Actor);
	void HandleChaseTargetLost();
	void HandleMoveFailedOrAborted();
	bool ShouldHandleMoveFailureOrAbort() const;
	bool StartBehaviorTreeRuntime();
	void StopBehaviorTreeRuntime();
	void WriteBlackboardRuntimeState();
	void PublishNextPatrolIntent(const FVector& InSpawnOrigin, float PatrolRadius);
	void PublishPatrolIntent(const FVector& InSpawnOrigin, const FVector& InCandidate, EHSRPatrolIntentResult Result);
	void ScheduleNavReadyPatrolIntent();
	void RunNavReadyPatrolIntent(int32 ScheduledEpoch);
	void ClearBlackboardRuntimeState();
	void SetBlackboardTarget(AActor* Target);
	void BeginSpawnOriginRecovery(EHSREnemyExplorationState RecoveryState);
	void PublishSpawnOriginRecoveryIntent(const FVector& InSpawnOrigin, EHSREnemyExplorationState RecoveryState);
	void ResumePatrolAfterReturn(const FVector& InSpawnOrigin, float PatrolRadius);
	void ClearState();
	void SetState(EHSREnemyExplorationState NewState);

	UPROPERTY()
	TWeakObjectPtr<AActor> CurrentTarget;

	EHSREnemyExplorationState CurrentState;
	int32 BehaviorTreeEpoch = 0;
	FGuid ActiveEncounterRequestId;
	FVector PublishedPatrolLocation = FVector::ZeroVector;
	bool bHasPublishedPatrolLocation = false;
	FTimerHandle NavReadyRetryTimerHandle;
	int32 NavReadyRetryEpoch = INDEX_NONE;
	bool bNavReadyRetryScheduled = false;

#if WITH_DEV_AUTOMATION_TESTS
	int32 EncounterSubmissionAttemptsForAutomation = 0;
	EHSREnemyExplorationState LastRecoveryStateForAutomation = EHSREnemyExplorationState::Idle;
#endif

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY(Transient)
	TObjectPtr<UBlackboardComponent> RuntimeBlackboard;
};
