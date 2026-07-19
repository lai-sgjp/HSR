#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HSRBattleGameMode.generated.h"

class UHSRBattleCoordinator;
struct FHSRBattleResult;

UENUM(BlueprintType)
enum class EHSRP5TerminalTestScenario : uint8
{
	None UMETA(DisplayName = "None"),
	PlayerVictory UMETA(DisplayName = "Player Victory"),
	PlayerDefeat UMETA(DisplayName = "Player Defeat")
};

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
	/** Editor-only test selector. Set on BP_HSRBattleGameMode; no runtime UI is added. */
	UPROPERTY(EditDefaultsOnly, Category = "Development", meta = (DisplayName = "P5 Terminal Test Scenario"))
	EHSRP5TerminalTestScenario TerminalTestScenario = EHSRP5TerminalTestScenario::None;

	UPROPERTY()
	TObjectPtr<UHSRBattleCoordinator> Coordinator;

	void HandleBattleResultReady(const FHSRBattleResult& Result);
	void RunTerminalScenarioForDevelopment();
};
