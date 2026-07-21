#pragma once

#include "CoreMinimal.h"
#include "HSRBattleParticipant.h"
#include "../Data/HSRBreakTypes.h"
#include "HSRTurnManager.generated.h"

UENUM(BlueprintType)
enum class EHSRTurnManagerState : uint8
{
	Waiting,
	PlayerTurn,
	EnemyTurn,
	Finished
};

UENUM()
enum class EHSRTurnLifecycleEventType : uint8
{
	TurnStarted,
	TurnEnded
};

/** Pure-value turn boundary notification. Runtime object references are intentionally excluded. */
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
	/** Registers one pure break-delay request. The next candidate turn for its living target is skipped once. */
	bool ConsumeBreakDelay(const FHSRTurnDelayRequest& Request);
	/** Stops turn progression after a terminal battle result. */
	void FinishBattle();
	void Reset();

	EHSRTurnManagerState GetState() const { return State; }
	FName GetCurrentParticipantId() const;
	uint64 GetBattleEpoch() const { return BattleEpoch; }
	uint64 GetTurnSequence() const { return TurnSequence; }
	const TArray<FHSRBattleParticipant>& GetOrderedParticipants() const { return OrderedParticipants; }
	FHSRTurnLifecycleDelegate& OnTurnStarted() { return TurnStarted; }
	FHSRTurnLifecycleDelegate& OnTurnEnded() { return TurnEnded; }
	/** Compatibility notification. Status lifecycles must use the explicit turn-boundary delegates. */
	FHSRActionResolvedDelegate& OnActionResolved() { return ActionResolved; }

#if WITH_EDITOR
	/** Development-only seam used by the Battle GameMode PIE verification harness. */
	bool InvalidateCurrentParticipantForDevelopmentTest();
#endif

private:
	bool AdvanceToNextValidTurn();
	bool IsCurrentParticipantValid() const;
	static bool IsParticipantTurnEligible(const FHSRBattleParticipant& Participant);
	void BroadcastLifecycleEvent(EHSRTurnLifecycleEventType EventType, FName ParticipantId);
	static float ReadInitiativeSpeed(const FHSRBattleParticipant& Participant);

	TArray<FHSRBattleParticipant> OrderedParticipants;
	TSet<FGuid> ConsumedBreakDelayActionIds;
	TMap<FName, FGuid> PendingBreakDelayActionIds;
	int32 CurrentTurnIndex = INDEX_NONE;
	EHSRTurnManagerState State = EHSRTurnManagerState::Waiting;
	uint64 BattleEpochCounter = 0;
	uint64 BattleEpoch = 0;
	uint64 TurnSequence = 0;
	FHSRTurnLifecycleDelegate TurnStarted;
	FHSRTurnLifecycleDelegate TurnEnded;
	FHSRActionResolvedDelegate ActionResolved;
};
