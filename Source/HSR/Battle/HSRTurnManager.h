#pragma once

#include "CoreMinimal.h"
#include "HSRBattleParticipant.h"
#include "../Data/HSRBreakTypes.h"
#include "HSRTurnManager.generated.h"

class UAbilitySystemComponent;
class UHSRBattleCoordinator;

UENUM(BlueprintType)
enum class EHSRTurnManagerState : uint8 { Waiting, PlayerTurn, EnemyTurn, Finished };
UENUM()
enum class EHSRTurnLifecycleEventType : uint8 { TurnStarted, TurnEnded };

USTRUCT()
struct HSR_API FHSRTurnLifecycleEvent
{
	GENERATED_BODY()
	uint64 BattleEpoch = 0;
	FName ParticipantId;
	uint64 TurnSequence = 0;
	EHSRTurnLifecycleEventType EventType = EHSRTurnLifecycleEventType::TurnStarted;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FHSRActionResolvedDelegate, FName /* ParticipantId */);
DECLARE_MULTICAST_DELEGATE_OneParam(FHSRTurnLifecycleDelegate, const FHSRTurnLifecycleEvent&);

/** Event-driven, battle-local initiative manager. It owns no Actor lifetime and never ticks. */
UCLASS()
class HSR_API UHSRTurnManager : public UObject
{
	GENERATED_BODY()
public:
	bool Initialize(const TArray<FHSRBattleParticipant>& InParticipants);
	bool ResolveAction(FName ResolvingParticipantId);
	FHSRActionDistanceResult RequestActionDistanceAdjustment(const FHSRActionDistanceRequest& Request);
	/** P8 compatibility bridge. It keeps no second delay state. */
	bool ConsumeBreakDelay(const FHSRTurnDelayRequest& Request);
	void FinishBattle();
	void Reset();

	EHSRTurnManagerState GetState() const { return State; }
	FName GetCurrentParticipantId() const;
	uint64 GetBattleEpoch() const { return BattleEpoch; }
	uint64 GetTurnSequence() const { return TurnSequence; }
	/** Stable participant registry/diagnostic view, not a prediction of future turns. */
	const TArray<FHSRBattleParticipant>& GetOrderedParticipants() const { return OrderedParticipants; }
	/** Single definition of "can still act", shared with the Coordinator's team-wipe test so
	 * turn eligibility and defeat resolution can never disagree. */
	static bool IsParticipantTurnEligible(const FHSRBattleParticipant& Participant);
	FHSRTurnLifecycleDelegate& OnTurnStarted() { return TurnStarted; }
	FHSRTurnLifecycleDelegate& OnTurnEnded() { return TurnEnded; }
	FHSRActionResolvedDelegate& OnActionResolved() { return ActionResolved; }

#if WITH_EDITOR
	bool InvalidateCurrentParticipantForDevelopmentTest();
#endif
#if WITH_DEV_AUTOMATION_TESTS
	void SetSpeedDelegateBindFailureAfterForAutomation(int32 InBindCount) { SpeedDelegateBindFailureAfter = InBindCount; }
	int32 GetSpeedDelegateBindingCountForAutomation() const { return SpeedDelegateBindings.Num(); }
	float GetLastPostRechargeDistanceForAutomation() const { return LastPostRechargeDistanceForAutomation; }
	bool GetActionDistanceForAutomation(FName ParticipantId, float& OutSpeed, float& OutBase, float& OutRemaining, int32& OutPending) const;
	bool SetActionDistanceForAutomation(FName ParticipantId, float InSpeed, float InBase, float InRemaining);
	void InvokeSpeedChangedForAutomation(FName ParticipantId, UAbilitySystemComponent* SourceASC, uint64 BoundEpoch, float NewSpeed) { HandleSpeedChanged(ParticipantId, SourceASC, BoundEpoch, NewSpeed); }
#endif

private:
	friend class UHSRBattleCoordinator;
	/** The only defeated-target exception: Coordinator's same synchronous admitted-alive transaction. */
	bool ConsumeAdmittedBreakDelay(const FHSRTurnDelayRequest& Request);
	FHSRActionDistanceResult RequestActionDistanceAdjustmentInternal(const FHSRActionDistanceRequest& Request, bool bAllowAdmittedDeferredDefeat);
	struct FPendingPostActionOperation { EHSRActionDistanceAdjustmentKind Kind = EHSRActionDistanceAdjustmentKind::Advance; float Distance = 0.0f; };
	struct FSpeedDelegateBinding { FName ParticipantId; TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent; FDelegateHandle Handle; uint64 Epoch = 0; };

	static constexpr float DistanceEpsilon = 1.e-4f;
	static constexpr float MaximumBaseActionDistance = 10000.0f;
	bool AdvanceToNextValidTurn();
	bool IsCurrentParticipantValid() const;
	void BroadcastLifecycleEvent(EHSRTurnLifecycleEventType EventType, FName ParticipantId);
	static bool MakeBaseActionDistance(float Speed, float& OutBase);
	bool BindSpeedDelegates();
	void UnbindSpeedDelegates();
	void HandleSpeedChanged(FName ParticipantId, UAbilitySystemComponent* SourceASC, uint64 BoundEpoch, float NewSpeed);
	bool ApplyCurrentPendingAfterRecharge();
	int32 FindParticipantIndex(FName ParticipantId) const;
	FHSRActionDistanceResult MakeAdjustmentResult(EHSRActionDistanceAdjustmentResult Result, int32 ParticipantIndex = INDEX_NONE, const FHSRActionDistanceResult* OldSnapshot = nullptr) const;

	TArray<FHSRBattleParticipant> OrderedParticipants;
	TArray<FPendingPostActionOperation> PendingPostActionOperations;
	TArray<FSpeedDelegateBinding> SpeedDelegateBindings;
	TSet<FGuid> ConsumedOperationIds;
	int32 CurrentTurnIndex = INDEX_NONE;
	EHSRTurnManagerState State = EHSRTurnManagerState::Waiting;
	uint64 BattleEpochCounter = 0;
	uint64 BattleEpoch = 0;
	uint64 TurnSequence = 0;
	bool bSelectingOrResolving = false;
	FHSRTurnLifecycleDelegate TurnStarted;
	FHSRTurnLifecycleDelegate TurnEnded;
	FHSRActionResolvedDelegate ActionResolved;
#if WITH_DEV_AUTOMATION_TESTS
	int32 SpeedDelegateBindFailureAfter = INDEX_NONE;
	float LastPostRechargeDistanceForAutomation = 0.0f;
#endif
};
