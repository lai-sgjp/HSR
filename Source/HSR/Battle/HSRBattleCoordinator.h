#pragma once

#include "CoreMinimal.h"
#include "HSRBattleTypes.h"
#include "HSREncounterTypes.h"
#include "HSRBattleParticipant.h"
#include "../UI/HSRBattleCommandTypes.h"
#include "../GAS/Damage/HSRDamageTypes.h"
#include "../Status/HSRStatusTypes.h"
#include "HSRBattleCoordinator.generated.h"

class UWorld;
class AActor;
class UHSRTurnManager;
class UHSRSkillDefinition;
class UHSRDamageRuleDefinition;
class UGameplayEffect;
class UHSREnemyDefinition;
class UHSRStatusDefinition;
class UHSRStatusComponent;
struct FOnAttributeChangeData;

DECLARE_MULTICAST_DELEGATE_OneParam(FHSRBattleResultReadyDelegate, const FHSRBattleResult&);
DECLARE_MULTICAST_DELEGATE_OneParam(FHSRBattleCommandStateReadyDelegate, const FHSRBattleCommandViewState&);

/**
 * State machine for battle initialization.
 * Owned by AHSRBattleGameMode; not persisted across worlds.
 *
 * States: Idle -> Consuming -> Spawned | Failed
 * Exactly-once consumption enforced by RequestId.
 */
UCLASS()
class HSR_API UHSRBattleCoordinator : public UObject
{
	GENERATED_BODY()

public:
	EHSRBattleCoordinatorState GetCurrentState() const { return CurrentState; }
	FGuid GetCurrentRequestId() const { return CurrentRequestId; }

	/** Submit a consumed encounter request exactly once. Rejects duplicate RequestId. */
	bool SubmitBattleRequest(const struct FHSREncounterRequest& InRequest);

	/** Spawn player + enemy actors and initialize their ASC in the Battle World. */
	FHSRBattleInitResult BuildParticipants(UWorld* BattleWorld);

	/** Pure-value return context for later travel back to exploration. */
	const FHSRBattleReturnContext& GetReturnContext() const { return ReturnContext; }

	/** Access spawned participants (valid only in current Battle World). */
	const TArray<FHSRBattleParticipant>& GetParticipants() const { return Participants; }
	UHSRTurnManager* GetTurnManager() const { return TurnManager; }

	/** Requests one synchronous basic attack. Only a current participant may attack an opposing valid target. */
	bool RequestBasicAttack(FName AttackerParticipantId, FName TargetParticipantId);
	FHSRAbilityResolution RequestAction(const FHSRBattleActionCommand& Command);
	FHSRDamageResult ResolveStatusDamage(FName SourceParticipantId, FName TargetParticipantId, const FGuid& ActionId, const UHSRStatusDefinition* Definition);
	void FinalizeStatusDamage();
	void SetBasicAttackDefinition(UHSRSkillDefinition* InDefinition) { BasicAttackDefinition = InDefinition; }
	const UHSRSkillDefinition* GetBasicAttackDefinition() const { return BasicAttackDefinition; }
	void SetUltimateDefinition(UHSRSkillDefinition* InDefinition) { UltimateDefinition = InDefinition; }
	void SetSkillDefinition(UHSRSkillDefinition* InDefinition) { SkillDefinition = InDefinition; }
	void SetHealDefinition(UHSRSkillDefinition* InDefinition) { HealDefinition = InDefinition; }
	void SetEnemyDefinition(UHSREnemyDefinition* InDefinition) { EnemyDefinition = InDefinition; }
	void SetParticipantInitializationGameplayEffect(TSubclassOf<UGameplayEffect> InEffect) { ParticipantInitializationGameplayEffect = InEffect; }
	void SetStatusDefinition(UHSRStatusDefinition* InDefinition) { StatusDefinition = InDefinition; }
	void SetDamageOverTimeStatusDefinition(UHSRStatusDefinition* InDefinition) { DamageOverTimeStatusDefinition = InDefinition; }
	void SetBreakStatusDefinition(UHSRStatusDefinition* InDefinition) { BreakStatusDefinition = InDefinition; }
	const UHSRStatusDefinition* GetStatusDefinition() const { return StatusDefinition; }
	UHSRStatusComponent* GetStatusComponent(FName ParticipantId) const;
	const FHSRTeamResourceState& GetTeamResourceState() const { return TeamResourceState; }
	bool WasLastBreakDelayRegistered() const { return bLastBreakDelayRegistered; }
	FGuid GetLastBreakDelayActionId() const { return LastBreakDelayActionId; }
	/** Event-driven, pure-value UI snapshot. Consumers never receive runtime Actors or ASCs. */
	FHSRBattleCommandStateReadyDelegate& OnCommandStateReady() { return CommandStateReady; }
	FHSRBattleCommandViewState GetCommandViewState() const;
#if WITH_EDITOR
	EHSRStatusOperationResult AddStatusForDevelopmentTest(FName SourceParticipantId, FName TargetParticipantId);
	EHSRStatusOperationResult AddDamageOverTimeForDevelopmentTest(FName SourceParticipantId, FName TargetParticipantId, FGuid OperationId = FGuid());
	EHSRStatusOperationResult AddSpecificStatusForDevelopmentTest(const UHSRStatusDefinition* Definition, FName SourceParticipantId, FName TargetParticipantId, FGuid OperationId = FGuid());
	EHSRStatusOperationResult RequestBreakStatusForDevelopmentTest(FName SourceParticipantId, FName TargetParticipantId, FGuid OperationId) { return RequestBreakStatus(SourceParticipantId, TargetParticipantId, OperationId); }
	void SetStatusDamageApplyFailureForDevelopmentTest(bool bForce) { bForceStatusDamageApplyFailure = bForce; }
	int32 GetStatusDamageCommitCountForDevelopmentTest() const { return StatusDamageCommitCount; }
	int32 GetDefeatCountForDevelopmentTest() const { return DefeatCount; }
	int32 GetBattleResultBroadcastCountForDevelopmentTest() const { return BattleResultBroadcastCount; }
	FHSRStatusRuntimeSnapshot GetLastClearedStatusSnapshotForDevelopmentTest(FName ParticipantId) const { const FHSRStatusRuntimeSnapshot* Found = LastClearedStatusSnapshots.Find(ParticipantId); return Found ? *Found : FHSRStatusRuntimeSnapshot(); }
	EHSRStatusOperationResult DispelOneStatusForDevelopmentTest(FName TargetParticipantId);
	int32 RouteSourceInvalidForDevelopmentTest(FName SourceParticipantId) { return RouteSourceInvalid(SourceParticipantId); }
	int32 GetLastSourceInvalidRemovedCountForDevelopmentTest() const { return LastSourceInvalidRemovedCount; }
	void SetStatusApplyFailureForDevelopmentTest(bool bForce);
	void SetDispelRemoveFailureForDevelopmentTest(bool bForce);
	FHSRStatusRuntimeSnapshot GetStatusSnapshotForDevelopmentTest(FName ParticipantId, FName StatusId = NAME_None) const;
	TSubclassOf<UGameplayEffect> GetParticipantInitializationGameplayEffectForDevelopmentTest() const { return ParticipantInitializationGameplayEffect; }
	void ClearRuntimeDelegatesForDevelopmentTest() { ClearRuntimeDelegates(); }
	void ClearStatusComponentsForDevelopmentTest() { ClearStatusComponents(); }
	bool InitializeStatusComponentsForDevelopmentTest() { return InitializeStatusComponents(); }
	void SetDamageTestInjectionForAction(const FGuid& ActionId, EHSRDamageTestInjection InInjection) { DamageTestInjectionActionId = ActionId; NextDamageTestInjection = InInjection; }
	void ClearDamageTestInjection() { DamageTestInjectionActionId = FGuid(); NextDamageTestInjection = EHSRDamageTestInjection::None; }
	const FHSRFormalDamageExecutionResult& GetLastDevelopmentFormalExecutionResult() const { return LastDevelopmentFormalExecutionResult; }
	FHSRBattleInitResult ResetAndRebuildForDevelopmentTest(UWorld* BattleWorld);
	void SetTeamSkillPointsForDevelopmentTest(int32 Current, int32 Max) { TeamResourceState.MaxSkillPoints = FMath::Max(0, Max); TeamResourceState.CurrentSkillPoints = FMath::Clamp(Current, 0, TeamResourceState.MaxSkillPoints); }
	void InitializeDevelopmentDamageRng(int32 InSeed);
	int32 GetDevelopmentDamageConsumeCount() const { return DevelopmentDamageConsumeCount; }
	FHSRDamageResult ResolveDevelopmentExecutionDamage(FName SourceParticipantId, FName TargetParticipantId, const FGuid& ActionId, const FGameplayTag& DamageType, float AbilityMultiplier, const UHSRDamageRuleDefinition* Rule, TSubclassOf<UGameplayEffect> DamageEffectClass);
#endif

	/** Exactly-once read of the terminal pure-value result. */
	bool ConsumeBattleResult(FHSRBattleResult& OutResult);
	FHSRBattleResultReadyDelegate& OnBattleResultReady() { return BattleResultReady; }

	/** Reset to Idle for a fresh battle session. */
	void Reset();

private:
	EHSRBattleCoordinatorState CurrentState = EHSRBattleCoordinatorState::Idle;
	FGuid CurrentRequestId;
	FName CurrentEncounterId;
	FName CurrentEnemyDefinitionId;
	FHSRBattleReturnContext ReturnContext;
	TArray<FHSRBattleParticipant> Participants;
	TArray<FHSRBattleParticipantDefinition> ParticipantDefinitions;
	TMap<FName, FDelegateHandle> HealthChangedHandles;
	FHSRBattleResult BattleResult;
	bool bBattleResultProduced = false;
	bool bBattleResultConsumed = false;
	/** Final result for every valid ActionId seen in this battle. Replays return
	 * this immutable Resolution and never re-enter Ability, GE, or Turn logic. */
	TMap<FGuid, FHSRAbilityResolution> ProcessedActionResolutions;
	FHSRAbilityResolution LastActionResolution;
	FHSRTeamResourceState TeamResourceState;
	bool bLastBreakDelayRegistered = false;
	FGuid LastBreakDelayActionId;
	TMap<FGuid, FHSRSkillPointReservation> SkillPointReservations;
	UPROPERTY()
	TObjectPtr<UHSRSkillDefinition> BasicAttackDefinition;
	UPROPERTY()
	TObjectPtr<UHSRSkillDefinition> UltimateDefinition;
	UPROPERTY()
	TObjectPtr<UHSRSkillDefinition> SkillDefinition;
	UPROPERTY() TObjectPtr<UHSRSkillDefinition> HealDefinition;
	UPROPERTY() TObjectPtr<UHSREnemyDefinition> EnemyDefinition;
	UPROPERTY() TObjectPtr<UHSRStatusDefinition> StatusDefinition;
	UPROPERTY() TObjectPtr<UHSRStatusDefinition> DamageOverTimeStatusDefinition;
	UPROPERTY() TObjectPtr<UHSRStatusDefinition> BreakStatusDefinition;
	UPROPERTY() TMap<FName, TObjectPtr<UHSRStatusComponent>> StatusComponents;
	TMap<FName, FDelegateHandle> StatusChangedHandles;
	TSubclassOf<UGameplayEffect> ParticipantInitializationGameplayEffect;
	FHSRBattleResultReadyDelegate BattleResultReady;
	FHSRBattleCommandStateReadyDelegate CommandStateReady;
	FRandomStream DevelopmentDamageRandomStream;
	int32 DevelopmentDamageSeed = 1337;
	int32 DevelopmentDamageConsumeCount = 0;
	TMap<FGuid, FHSRDamageResult> DevelopmentDamageResults;
#if WITH_EDITOR
	EHSRDamageTestInjection NextDamageTestInjection = EHSRDamageTestInjection::None;
	FGuid DamageTestInjectionActionId;
	FHSRFormalDamageExecutionResult LastDevelopmentFormalExecutionResult;
	TOptional<FHSREncounterRequest> LastSubmittedRequestForDevelopment;
	bool bForceStatusDamageApplyFailure = false;
	int32 StatusDamageCommitCount = 0;
	int32 DefeatCount = 0;
	int32 BattleResultBroadcastCount = 0;
	TMap<FName, FHSRStatusRuntimeSnapshot> LastClearedStatusSnapshots;
	int32 LastSourceInvalidRemovedCount = 0;
#endif
	/** Health observers defer terminal publication while the one synchronous
	 * formal-damage application is still transactional. */
	bool bFormalDamageTransactionOpen = false;
	FName PendingDefeatedParticipantId;

	UPROPERTY()
	TObjectPtr<UHSRTurnManager> TurnManager;

	AActor* SpawnParticipantActor(UWorld* World, const FHSRBattleParticipantDefinition& Definition);
	bool InitParticipantASC(AActor* TargetActor);
	bool ApplyParticipantInitializationGameplayEffect(const FHSRBattleParticipant& Participant);
	bool GrantBasicAttackAbility(const FHSRBattleParticipant& Participant);
	bool GrantUltimateAbility(const FHSRBattleParticipant& Participant);
	bool GrantSkillAbility(const FHSRBattleParticipant& Participant);
	bool GrantHealAbility(const FHSRBattleParticipant& Participant);
	const UHSRSkillDefinition* FindSkillDefinition(FName SkillId) const;
	bool ReserveSkillPoints(const FGuid& ActionId, int32 Delta);
	void RollbackSkillPoints(const FGuid& ActionId);
	void CommitSkillPoints(const FGuid& ActionId);
	FHSRBattleInitResult BuildAndValidateParticipantDefinitions();
	void BindHealthObserver(const FHSRBattleParticipant& Participant);
	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData, FName ParticipantId);
	void ResolveDefeat(FName DefeatedParticipantId);
	void ClearRuntimeDelegates();
	bool InitializeStatusComponents();
	void ClearStatusComponents();
	EHSRStatusOperationResult RequestBreakStatus(FName SourceParticipantId, FName TargetParticipantId, const FGuid& OperationId);
	int32 RouteSourceInvalid(FName SourceParticipantId);
	void HandleStatusChanged(FName ParticipantId);
	FHSRStatusPublicOperationEvent LastStatusOperation;
	void PublishCommandViewState();
};
