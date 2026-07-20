#pragma once

#include "CoreMinimal.h"
#include "HSRBattleTypes.h"
#include "HSRBattleParticipant.h"
#include "../UI/HSRBattleCommandTypes.h"
#include "HSRBattleCoordinator.generated.h"

class UWorld;
class AActor;
class UHSRTurnManager;
class UHSRSkillDefinition;
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
	void SetBasicAttackDefinition(UHSRSkillDefinition* InDefinition) { BasicAttackDefinition = InDefinition; }
	void SetUltimateDefinition(UHSRSkillDefinition* InDefinition) { UltimateDefinition = InDefinition; }
	void SetSkillDefinition(UHSRSkillDefinition* InDefinition) { SkillDefinition = InDefinition; }
	void SetHealDefinition(UHSRSkillDefinition* InDefinition) { HealDefinition = InDefinition; }
	const FHSRTeamResourceState& GetTeamResourceState() const { return TeamResourceState; }
	/** Event-driven, pure-value UI snapshot. Consumers never receive runtime Actors or ASCs. */
	FHSRBattleCommandStateReadyDelegate& OnCommandStateReady() { return CommandStateReady; }
	FHSRBattleCommandViewState GetCommandViewState() const;
#if WITH_EDITOR
	void SetTeamSkillPointsForDevelopmentTest(int32 Current, int32 Max) { TeamResourceState.MaxSkillPoints = FMath::Max(0, Max); TeamResourceState.CurrentSkillPoints = FMath::Clamp(Current, 0, TeamResourceState.MaxSkillPoints); }
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
	TMap<FGuid, FHSRSkillPointReservation> SkillPointReservations;
	UPROPERTY()
	TObjectPtr<UHSRSkillDefinition> BasicAttackDefinition;
	UPROPERTY()
	TObjectPtr<UHSRSkillDefinition> UltimateDefinition;
	UPROPERTY()
	TObjectPtr<UHSRSkillDefinition> SkillDefinition;
	UPROPERTY() TObjectPtr<UHSRSkillDefinition> HealDefinition;
	FHSRBattleResultReadyDelegate BattleResultReady;
	FHSRBattleCommandStateReadyDelegate CommandStateReady;

	UPROPERTY()
	TObjectPtr<UHSRTurnManager> TurnManager;

	AActor* SpawnParticipantActor(UWorld* World, const FHSRBattleParticipantDefinition& Definition);
	bool InitParticipantASC(AActor* TargetActor);
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
	void PublishCommandViewState();
};
