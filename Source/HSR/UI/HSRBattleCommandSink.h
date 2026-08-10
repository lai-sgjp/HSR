#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "../Battle/HSRBattleTypes.h"
#include "../GAS/Ability/HSRAbilityTypes.h"
#include "HSRBattleCommandSink.generated.h"

/**
 * The whole authority surface a command widget needs: verify the battle it snapshotted is still the
 * live one, then submit a pure-value command. Previously the widget held a UHSRBattleCoordinator*
 * and could reach its entire API, which made the UI compile against battle internals it never used.
 *
 * Implemented by UHSRBattleCoordinator. A test double or a replay recorder can stand in without the
 * UI knowing, and the two methods below are the only coupling to honour.
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UHSRBattleCommandSink : public UInterface
{
	GENERATED_BODY()
};

class HSR_API IHSRBattleCommandSink
{
	GENERATED_BODY()

public:
	/**
	 * Id of the battle currently accepting commands. A widget compares this against the id it
	 * snapshotted so a command built before a battle reset is rejected rather than misrouted.
	 */
	virtual FGuid GetActiveBattleId() const = 0;

	/** Submits a command for authoritative resolution. The sink stays the single source of truth. */
	virtual FHSRAbilityResolution SubmitBattleCommand(const FHSRBattleActionCommand& Command) = 0;
};
