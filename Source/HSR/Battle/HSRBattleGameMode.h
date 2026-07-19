#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HSRBattleGameMode.generated.h"

class UHSRBattleCoordinator;

/**
 * GameMode for the standalone Battle World.
 * Entry point: reads the consumed encounter request from the TransitionSubsystem
 * and delegates participant rebuild to the Coordinator.
 * Does NOT own the Coordinator's authority over battle rules.
 */
UCLASS()
class HSR_API AHSRBattleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHSRBattleGameMode();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Battle")
	UHSRBattleCoordinator* GetCoordinator() const { return Coordinator; }

protected:
	UPROPERTY()
	TObjectPtr<UHSRBattleCoordinator> Coordinator;
};
