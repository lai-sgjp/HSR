#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HSRBattleGameMode.generated.h"

class UHSRBattleCoordinator;
class UHSRSkillDefinition;
class UHSRBattleCommandViewModel;
class UHSRBattleCommandWidget;
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
	UFUNCTION(BlueprintPure, Category = "Battle") UHSRBattleCommandViewModel* GetCommandViewModel() const { return CommandViewModel; }

protected:
	/** User-created SkillDefinition used by the Battle runtime; this is not owned by the exploration character. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle|Skills")
	TObjectPtr<UHSRSkillDefinition> BasicAttackSkillDefinition;

	/** Optional until the user creates the P6-002 DataAsset. Once set, it must
	 * be a valid Ultimate definition and is granted to battle participants. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle|Skills")
	TObjectPtr<UHSRSkillDefinition> UltimateSkillDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle|Skills")
	TObjectPtr<UHSRSkillDefinition> SkillSkillDefinition;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle|Skills") TObjectPtr<UHSRSkillDefinition> HealSkillDefinition;

	/** Editor-only test selector. Set on BP_HSRBattleGameMode; no runtime UI is added. */
	UPROPERTY(EditDefaultsOnly, Category = "Development", meta = (DisplayName = "P5 Terminal Test Scenario"))
	EHSRP5TerminalTestScenario TerminalTestScenario = EHSRP5TerminalTestScenario::None;

	/** User-owned /Game/UI/Battle/WBP_BattleCommandPanel class. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle|UI")
	TSubclassOf<UHSRBattleCommandWidget> BattleCommandWidgetClass;

	UPROPERTY()
	TObjectPtr<UHSRBattleCoordinator> Coordinator;
	UPROPERTY() TObjectPtr<UHSRBattleCommandViewModel> CommandViewModel;
	UPROPERTY() TObjectPtr<UHSRBattleCommandWidget> BattleCommandWidget;
	FDelegateHandle CommandStateReadyHandle;

	void HandleBattleResultReady(const FHSRBattleResult& Result);
	void HandleCommandStateReady(const struct FHSRBattleCommandViewState& State);
	void RunTerminalScenarioForDevelopment();
};
