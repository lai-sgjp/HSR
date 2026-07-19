#pragma once

#include "CoreMinimal.h"
#include "HSRBattleParticipant.h"
#include "HSRTurnManager.generated.h"

UENUM(BlueprintType)
enum class EHSRTurnManagerState : uint8
{
	Waiting,
	PlayerTurn,
	EnemyTurn,
	Finished
};

DECLARE_MULTICAST_DELEGATE_OneParam(FHSRActionResolvedDelegate, FName /* ParticipantId */);

/** Event-driven, battle-local initiative manager. It owns no Actor lifetime and never ticks. */
UCLASS()
class HSR_API UHSRTurnManager : public UObject
{
	GENERATED_BODY()

public:
	bool Initialize(const TArray<FHSRBattleParticipant>& InParticipants);
	bool ResolveAction(FName ResolvingParticipantId);
	void Reset();

	EHSRTurnManagerState GetState() const { return State; }
	FName GetCurrentParticipantId() const;
	const TArray<FHSRBattleParticipant>& GetOrderedParticipants() const { return OrderedParticipants; }
	FHSRActionResolvedDelegate& OnActionResolved() { return ActionResolved; }

#if WITH_EDITOR
	/** Development-only seam used by the Battle GameMode PIE verification harness. */
	bool InvalidateCurrentParticipantForDevelopmentTest();
#endif

private:
	bool AdvanceToNextValidTurn();
	bool IsCurrentParticipantValid() const;
	static float ReadInitiativeSpeed(const FHSRBattleParticipant& Participant);

	TArray<FHSRBattleParticipant> OrderedParticipants;
	int32 CurrentTurnIndex = INDEX_NONE;
	EHSRTurnManagerState State = EHSRTurnManagerState::Waiting;
	FHSRActionResolvedDelegate ActionResolved;
};
