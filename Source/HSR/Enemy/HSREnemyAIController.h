#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "Engine/TimerHandle.h"
#include "HSREnemyTypes.h"
#include "HSREnemyAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;

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

protected:
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	void StartPatrol();
	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);
	void HandleChaseTargetLost();
	void HandleMoveFailedOrAborted();
	void ClearState();
	void SetState(EHSREnemyExplorationState NewState);

	UPROPERTY()
	TWeakObjectPtr<AActor> CurrentTarget;

	EHSREnemyExplorationState CurrentState;

	FTimerHandle PatrolWaitTimerHandle;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;
};
