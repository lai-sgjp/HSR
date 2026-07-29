#pragma once

#include "CoreMinimal.h"
#include "HSRBattleTypes.h"
#include "HSREncounterTypes.h"
#include "HSRBattleParticipant.h"
#include "../UI/HSRBattleCommandTypes.h"
#include "../GAS/Damage/HSRDamageTypes.h"
#include "../Status/HSRStatusTypes.h"
#include "GameplayEffectTypes.h"
#include "../Progression/HSRCharacterDerivedStats.h"
#include "../Equipment/HSREquipmentEffectBridge.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "HSRBattleCoordinator.generated.h"

class UWorld;
class AActor;
class APawn;
class UAbilitySystemComponent;
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
	FName GetCurrentRewardDefinitionId() const { return CurrentRewardDefinitionId; }
	int32 GetCurrentRewardSeed() const { return CurrentRewardSeed; }
	bool BuildVictoryRewardRequest(const struct FHSRBattleResult& Result, struct FHSRRewardRequest& OutRequest) const;

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
	void SetCharacterProgressionGameplayEffect(TSubclassOf<UGameplayEffect> InEffect) { CharacterProgressionGameplayEffect = InEffect; }
	void SetCharacterProgressionContext(FName ParticipantId, const FHSRCharacterProgressionContext& Context) { CharacterProgressionContexts.Add(ParticipantId, Context); }
	void SetPlayerCharacterDefinition(FName CharacterId, TSubclassOf<APawn> CharacterClass) { PlayerCharacterId = CharacterId; PlayerCharacterClass = CharacterClass; }
	void SetStatusDefinition(UHSRStatusDefinition* InDefinition) { StatusDefinition = InDefinition; }
	void SetDamageOverTimeStatusDefinition(UHSRStatusDefinition* InDefinition) { DamageOverTimeStatusDefinition = InDefinition; }
	void SetBreakStatusDefinition(UHSRStatusDefinition* InDefinition) { BreakStatusDefinition = InDefinition; }
	const UHSRStatusDefinition* GetStatusDefinition() const { return StatusDefinition; }
	UHSRStatusComponent* GetStatusComponent(FName ParticipantId) const;
	const FHSRTeamResourceState& GetTeamResourceState() const { return TeamResourceState; }
	bool WasLastBreakDelayRegistered() const { return bLastBreakDelayRegistered; }
	FGuid GetLastBreakDelayActionId() const { return LastBreakDelayActionId; }
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	/** Controlled-runtime counters for repeatable Break transaction audits. */
	int32 GetBreakStatusRequestCountForDevelopmentTest() const { return BreakStatusRequestCountForTest; }
	int32 GetBreakDelayRegistrationCountForDevelopmentTest() const { return BreakDelayRegistrationCountForTest; }
	EHSRStatusOperationResult GetLastBreakStatusResultForDevelopmentTest() const { return LastBreakStatusResultForTest; }
	bool WasLastBreakDelayAcceptedForDevelopmentTest() const { return bLastBreakDelayAcceptedForTest; }
#endif
	/** Event-driven, pure-value UI snapshot. Consumers never receive runtime Actors or ASCs. */
	FHSRBattleCommandStateReadyDelegate& OnCommandStateReady() { return CommandStateReady; }
	FHSRBattleCommandViewState GetCommandViewState() const;
	static bool ValidateCharacterProgressionEffectContract(const UGameplayEffect* Effect);
	static bool HasSameProgressionFingerprint(const FHSRCharacterProgressionContext& A, const FHSRCharacterProgressionContext& B);
	bool RefreshCharacterProgression(FName ParticipantId, const FHSRCharacterProgressionContext& Context);
	void SetEquipmentGameplayEffect(TSubclassOf<UGameplayEffect> InEffect) { EquipmentGameplayEffect=InEffect; }
	void SetRelicSetGameplayEffect(TSubclassOf<UGameplayEffect> InEffect) { RelicSetGameplayEffect=InEffect; }
	bool ApplyEquipmentSource(FName ParticipantId, const FGuid& InstanceId, const FHSREquipmentAggregate& Aggregate, int64 Revision);
	bool RemoveEquipmentSource(FName ParticipantId, const FGuid& InstanceId);
	bool ApplyEquipmentSetSource(FName ParticipantId, FName SetSourceId, const FHSREquipmentAggregate& Aggregate, int64 Revision);
	bool RemoveEquipmentSetSource(FName ParticipantId, FName SetSourceId);
	bool ProjectEquipmentRestore(const TMap<FGuid,FHSREquipmentRestoreState>& Candidate);
	void BindEquipmentMovementProjection(UHSREquipmentSubsystem& Equipment);
	bool SetEquipmentSource(FName ParticipantId,const FGuid& InstanceId,const FHSREquipmentAggregate& Aggregate,int64 Revision) { return ApplyEquipmentSource(ParticipantId,InstanceId,Aggregate,Revision); }
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	bool HasProgressionPrimaryHandleForDevelopmentTest(FName Id) const;
	FString GetProgressionPrimaryHandleForDevelopmentTest(FName Id) const;
	int32 GetProgressionSecondaryCountForDevelopmentTest(FName Id) const;
	int32 GetProgressionActiveHandleCountForDevelopmentTest(FName Id) const;
	FString GetProgressionFingerprintForDevelopmentTest(FName Id) const;
	int32 GetProgressionRefreshCountForDevelopmentTest() const { return ProgressionRefreshCountForTest; }
	bool GetLastProgressionRefreshResultForDevelopmentTest() const { return bLastProgressionRefreshResultForTest; }
	/** Editor-only fault injection for P11-006 transactional refresh audits. */
	void SetProgressionApplyFailureForDevelopmentTest(bool bForce) { bForceProgressionApplyFailureForTest=bForce; }
	void SetProgressionOldRemoveFailureForDevelopmentTest(bool bForce) { bForceProgressionOldRemoveFailureForTest=bForce; }
	void SetEquipmentRestoreProjectionFailureForDevelopmentTest(bool bForce) { bForceEquipmentRestoreProjectionFailure=bForce; }
	void SetEquipmentRestoreProjectionFailureAfterOperationsForDevelopmentTest(int32 Count) { EquipmentRestoreFailureAfterOperations=Count; }
	void SetParticipantsForEquipmentProjectionDevelopmentTest(const TArray<FHSRBattleParticipant>& InParticipants) { Participants=InParticipants; }
	int32 GetEquipmentProjectionSourceCountForDevelopmentTest() const { return EquipmentEffectBridge?EquipmentEffectBridge->GetActiveSourceCount():0; }
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
	/** Read-only terminal result inspection for preflight; it never consumes or changes authority. */
	bool GetBattleResultForPresentation(FHSRBattleResult& OutResult) const;
	FHSRBattleResultReadyDelegate& OnBattleResultReady() { return BattleResultReady; }

	/** Reset to Idle for a fresh battle session. */
	void Reset();

private:
	EHSRBattleCoordinatorState CurrentState = EHSRBattleCoordinatorState::Idle;
	FGuid CurrentRequestId;
	FName CurrentRewardDefinitionId;
	int32 CurrentRewardSeed = 0;
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
	TArray<FHSRBattlePresentationEvent> PresentationEvents;
	/** The manager currently allowed to enqueue deterministic enemy turns. */
	TWeakObjectPtr<UHSRTurnManager> BoundEnemyTurnManager;
	FDelegateHandle EnemyTurnStartedHandle;
	TOptional<FString> PendingEnemyTurnKey;
	TSet<FString> ConsumedEnemyTurnKeys;
	int32 RequestActionDispatchDepth = 0;
	bool bDrainingEnemyTurns = false;
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	int32 PublicRequestActionDepth = 0;
	int32 MaxPublicRequestActionDepth = 0;
	int32 CoreExecutionDepth = 0;
	int32 MaxCoreExecutionDepth = 0;
	int32 EnemyTurnQueueCount = 0;
	int32 EnemyTurnDispatchCount = 0;
	int32 EnemyTurnRejectedCount = 0;
public:
	int32 GetMaxPublicRequestActionDepthForDevelopmentTest() const { return MaxPublicRequestActionDepth; }
	int32 GetMaxCoreExecutionDepthForDevelopmentTest() const { return MaxCoreExecutionDepth; }
	int32 GetEnemyTurnQueueCountForDevelopmentTest() const { return EnemyTurnQueueCount; }
	int32 GetEnemyTurnDispatchCountForDevelopmentTest() const { return EnemyTurnDispatchCount; }
	int32 GetEnemyTurnRejectedCountForDevelopmentTest() const { return EnemyTurnRejectedCount; }
	int32 GetProcessedActionCountForDevelopmentTest() const { return ProcessedActionResolutions.Num(); }
	int32 GetSkillPointReservationCountForDevelopmentTest() const { return SkillPointReservations.Num(); }
	const FHSRAbilityResolution& GetLastActionResolutionForDevelopmentTest() const { return LastActionResolution; }
	bool BeginEnemyTurnAutomationAuditForDevelopmentTest(UHSRTurnManager* IsolatedManager);
	void EndEnemyTurnAutomationAuditForDevelopmentTest();
	void InjectEnemyTurnStartedForDevelopmentTest(UHSRTurnManager* SourceManager, const struct FHSRTurnLifecycleEvent& Event);
	bool IsEnemyTurnAutomationAuditActiveForDevelopmentTest() const { return bEnemyTurnAutomationAuditActive; }
	bool HasPendingEnemyTurnForDevelopmentTest() const { return PendingEnemyTurnKey.IsSet(); }
	int32 GetConsumedEnemyTurnCountForDevelopmentTest() const { return ConsumedEnemyTurnKeys.Num(); }
	UHSRTurnManager* GetBoundEnemyTurnManagerForDevelopmentTest() const { return BoundEnemyTurnManager.Get(); }
	bool HasEnemyTurnStartedBindingForDevelopmentTest() const { return EnemyTurnStartedHandle.IsValid(); }
private:
	TWeakObjectPtr<UHSRTurnManager> SavedEnemyTurnManagerForAudit;
	TOptional<FString> SavedPendingEnemyTurnKeyForAudit;
	TSet<FString> SavedConsumedEnemyTurnKeysForAudit;
	bool bEnemyTurnAutomationAuditActive = false;
#endif
	FHSRTeamResourceState TeamResourceState;
	bool bLastBreakDelayRegistered = false;
	FGuid LastBreakDelayActionId;
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	int32 BreakStatusRequestCountForTest = 0;
	int32 BreakDelayRegistrationCountForTest = 0;
	EHSRStatusOperationResult LastBreakStatusResultForTest = EHSRStatusOperationResult::UnknownStatus;
	bool bLastBreakDelayAcceptedForTest = false;
#endif
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
	TSubclassOf<UGameplayEffect> CharacterProgressionGameplayEffect;
	TMap<FName, FHSRCharacterProgressionContext> CharacterProgressionContexts;
	struct FHSRProgressionEffectState { TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent; TSubclassOf<UGameplayEffect> EffectClass; FName CharacterId; FHSRCharacterDerivedStats Bonuses; FActiveGameplayEffectHandle ActiveHandle; TArray<FActiveGameplayEffectHandle> SecondaryOwnedHandles; uint64 Epoch = 0; int64 Revision = 0; };
	TMap<FName, FHSRProgressionEffectState> ProgressionEffects;
	UPROPERTY() TSubclassOf<UGameplayEffect> EquipmentGameplayEffect;
	UPROPERTY() TSubclassOf<UGameplayEffect> RelicSetGameplayEffect;
	UPROPERTY() TObjectPtr<UHSREquipmentEffectBridge> EquipmentEffectBridge;
	TMap<FGuid,FHSREquipmentAggregate> EquipmentProjectionStates;
	TMap<FGuid,FName> EquipmentProjectionParticipants;
	TMap<FName,FHSREquipmentAggregate> EquipmentSetProjectionStates;
	TMap<FName,FName> EquipmentSetProjectionParticipants;
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	bool bForceEquipmentRestoreProjectionFailure=false;
	int32 EquipmentRestoreFailureAfterOperations=-1;
#endif
	FName PlayerCharacterId;
	TSubclassOf<APawn> PlayerCharacterClass;
	uint64 ProgressionEpoch = 0;
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	int32 ProgressionRefreshCountForTest=0;
	bool bLastProgressionRefreshResultForTest=false;
	bool bForceProgressionApplyFailureForTest=false;
	bool bForceProgressionOldRemoveFailureForTest=false;
#endif
	FHSRBattleResultReadyDelegate BattleResultReady;
	FHSRBattleCommandStateReadyDelegate CommandStateReady;
	FRandomStream DevelopmentDamageRandomStream;
	int32 DevelopmentDamageSeed = 1337;
	int32 DevelopmentDamageConsumeCount = 0;
	TMap<FGuid, FHSRDamageResult> DevelopmentDamageResults;
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
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
	bool CanProjectEquipmentMovement(const FHSREquipmentMovementRequest& Request, const FHSREquipmentLoadout& Candidate) const;
	void CommitEquipmentMovementProjection(const FHSREquipmentMovementRequest& Request, const FHSREquipmentLoadout& Candidate);
	bool ApplyCharacterProgressionGameplayEffect(const FHSRBattleParticipant& Participant);
	bool ClearProgressionGameplayEffects();
	bool GrantBasicAttackAbility(const FHSRBattleParticipant& Participant);
	bool GrantUltimateAbility(const FHSRBattleParticipant& Participant);
	bool GrantSkillAbility(const FHSRBattleParticipant& Participant);
	bool GrantHealAbility(const FHSRBattleParticipant& Participant);
	const UHSRSkillDefinition* FindSkillDefinition(FName SkillId) const;
	FHSRAbilityResolution RequestActionCore(const FHSRBattleActionCommand& Command);
	void BindEnemyTurnManager(UHSRTurnManager* InManager);
	void ClearEnemyTurnAutomation();
	void RecordEnemyTurnIfCurrent(UHSRTurnManager* SourceManager, const struct FHSRTurnLifecycleEvent& Event);
	void RecordCurrentEnemyTurnIfNeeded();
	void DrainPendingEnemyTurns();
	FString MakeEnemyTurnKey(const UHSRTurnManager* Manager, uint64 BattleEpoch, uint64 TurnSequence, FName ParticipantId) const;
	bool ReserveSkillPoints(const FGuid& ActionId, int32 Delta);
	void RollbackSkillPoints(const FGuid& ActionId);
	void CommitSkillPoints(const FGuid& ActionId);
	void CommitActionEnergyGain(const FGuid& ActionId, const UHSRSkillDefinition& ActionSkillDefinition, UAbilitySystemComponent& SourceASC);
	FHSRBattleInitResult BuildAndValidateParticipantDefinitions();
	void BindHealthObserver(const FHSRBattleParticipant& Participant);
	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData, FName ParticipantId);
	void ResolveDefeat(FName DefeatedParticipantId);
	void ClearRuntimeDelegates();
	bool InitializeStatusComponents();
	void ClearStatusComponents();
	EHSRStatusOperationResult RequestBreakStatus(FName SourceParticipantId, FName TargetParticipantId, const FGuid& OperationId, bool bAllowPendingDeferredDefeat = false);
	int32 RouteSourceInvalid(FName SourceParticipantId);
	void HandleStatusChanged(FName ParticipantId);
	FHSRStatusPublicOperationEvent LastStatusOperation;
	void PublishCommandViewState();
};
