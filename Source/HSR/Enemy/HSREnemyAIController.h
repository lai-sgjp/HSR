#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
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

protected:
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);
	void HandleChaseTargetLost();
	void HandleMoveFailedOrAborted();
	bool StartBehaviorTreeRuntime();
	void StopBehaviorTreeRuntime();
	void WriteBlackboardRuntimeState();
	void ClearBlackboardRuntimeState();
	void SetBlackboardTarget(AActor* Target);
	void BeginSpawnOriginRecovery(EHSREnemyExplorationState RecoveryState);
	void ClearState();
	void SetState(EHSREnemyExplorationState NewState);

	UPROPERTY()
	TWeakObjectPtr<AActor> CurrentTarget;

	EHSREnemyExplorationState CurrentState;
	int32 BehaviorTreeEpoch = 0;
	FGuid ActiveEncounterRequestId;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY(Transient)
	TObjectPtr<UBlackboardComponent> RuntimeBlackboard;
};
