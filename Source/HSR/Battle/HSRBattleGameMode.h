#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayTagContainer.h"
#include "HSRBattleGameMode.generated.h"

class UHSRBattleCoordinator;
class UHSRSkillDefinition;
class UHSRBattleCommandViewModel;
class UHSRBattleCommandWidget;
class UHSRDamageRuleDefinition;
class UGameplayEffect;
class UHSREnemyDefinition;
class UHSRStatusDefinition;
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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle|Definitions") TObjectPtr<UHSREnemyDefinition> EnemyDefinition;
	/** Required initialization GE, applied once to every spawned participant before abilities are granted. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle|Initialization") TSubclassOf<UGameplayEffect> ParticipantInitializationGameplayEffect;

	/** Editor-only test selector. Set on BP_HSRBattleGameMode; no runtime UI is added. */
	UPROPERTY(EditDefaultsOnly, Category = "Development", meta = (DisplayName = "P5 Terminal Test Scenario"))
	EHSRP5TerminalTestScenario TerminalTestScenario = EHSRP5TerminalTestScenario::None;

	/** User-owned /Game/UI/Battle/WBP_BattleCommandPanel class. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle|UI")
	TSubclassOf<UHSRBattleCommandWidget> BattleCommandWidgetClass;

#if WITH_EDITORONLY_DATA
	/** Opt-in legacy P5/P6 harnesses. Keep disabled for normal editor PIE. */
	UPROPERTY(EditDefaultsOnly, Category = "Development|Legacy")
	bool bRunLegacyBattleHarnesses = false;
	UPROPERTY(EditDefaultsOnly, Category = "Development|P7")
	TObjectPtr<UHSRDamageRuleDefinition> DevelopmentDamageRule;
	UPROPERTY(EditDefaultsOnly, Category = "Development|P7")
	TSubclassOf<UGameplayEffect> DevelopmentDamageExecutionGameplayEffect;
	UPROPERTY(EditDefaultsOnly, Category = "Development|P7")
	int32 DevelopmentDamageSeed = 1337;
	UPROPERTY(EditDefaultsOnly, Category = "Development|P7")
	bool bRunP7DamageHarness = false;
	UPROPERTY(EditDefaultsOnly, Category = "Development|P7")
	FGameplayTag DevelopmentDamageType;
	UPROPERTY(EditDefaultsOnly, Category = "Development|P7", meta = (ClampMin = "0.000001", ClampMax = "100.0"))
	float DevelopmentAbilityMultiplier = 1.0f;
	/** Opt-in pure-value P8-001 contract audit; it never invokes battle gameplay. */
	UPROPERTY(EditDefaultsOnly, Category = "Development|P8")
	bool bRunP8ContractHarness = false;
	/** Opt-in isolated P9-000 turn lifecycle contract harness. */
	UPROPERTY(EditDefaultsOnly, Category = "Development|P9")
	bool bRunP9TurnLifecycleHarness = false;
	UPROPERTY(EditDefaultsOnly, Category = "Development|P9")
	bool bRunP9StatusHarness = false;
	UPROPERTY(EditDefaultsOnly, Category = "Development|P9")
	bool bRunP9DotBreakHarness = false;
	UPROPERTY(EditDefaultsOnly, Category = "Development|P9")
	bool bRunP9ImmunityDispelHarness = false;
	UPROPERTY(EditDefaultsOnly, Category = "Development|P9")
	bool bRunP9StatusViewHarness = false;
	/** Opt-in P10-001A dispatcher audit. Disabled for normal PIE. */
	UPROPERTY(EditDefaultsOnly, Category = "Development|P10")
	bool bRunP10001AEnemyTurnHarness = false;
	UPROPERTY(EditDefaultsOnly, Category = "Development|P10")
	bool bRunP10001CommandHarness = false;
	/** Opt-in P10-002 pure-value turn/participant view harness. */
	UPROPERTY(EditDefaultsOnly, Category = "Development|P10")
	bool bRunP10002ViewHarness = false;
	/** Opt-in pure-value P10-004 result/confirm lifecycle harness. */
	UPROPERTY(EditDefaultsOnly, Category = "Development|P10")
	bool bRunP10004ResultHarness = false;
#endif
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle|Status")
	TObjectPtr<UHSRStatusDefinition> AttackUpStatusDefinition;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle|Status")
	TObjectPtr<UHSRStatusDefinition> AttackUpStackStatusDefinition;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle|Status")
	TObjectPtr<UHSRStatusDefinition> DamageOverTimeStatusDefinition;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle|Status")
	TObjectPtr<UHSRStatusDefinition> BreakStatusDefinition;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle|Status")
	TSubclassOf<UGameplayEffect> DebuffImmunityGameplayEffect;

	UPROPERTY()
	TObjectPtr<UHSRBattleCoordinator> Coordinator;
	UPROPERTY() TObjectPtr<UHSRBattleCommandViewModel> CommandViewModel;
	UPROPERTY() TObjectPtr<UHSRBattleCommandWidget> BattleCommandWidget;
	FDelegateHandle CommandStateReadyHandle;
	FDelegateHandle BattleResultReadyHandle;
	FDelegateHandle ResultConfirmRequestedHandle;

	void HandleBattleResultReady(const FHSRBattleResult& Result);
	void HandleBattleResultConfirmRequested(const FGuid& RequestId);
	void HandleCommandStateReady(const struct FHSRBattleCommandViewState& State);
	void RunTerminalScenarioForDevelopment();
	#if WITH_EDITOR
	void RunP10001AEnemyTurnHarness();
	void RunP10002ViewHarness();
	void RunP10004ResultHarness();
	#endif
};
