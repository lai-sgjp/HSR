# TASK-P17-PATCH-02 Encounter-Assertion Review

## Review metadata

- Reviewer: Independent Reviewer / Safety Reviewer
- Reviewed revision: `a8bf783`
- Result: `REVISE` (code/Automation contract passes; only user full-return PIE remains)
- Date: 2026-07-28

## Encounter contract accepted

- The test-local `SameEncounterRequest` compares every current `FHSREncounterRequest` field: RequestId, EncounterId, EnemyDefinitionId, Initiative, BattleMapPath, ReturnTransform, ExplorationMapPath, RewardDefinitionId, and RewardSeed.
- The snapshot comparison additionally preserves encounter state, TravelKind, TravelRequestId, resolved membership, and admission mutation count.
- Both production rejection branches assert their exact result (`AlreadyPending` / `AlreadyConsumed`), invalid returned RequestId, nonempty failure message, and full snapshot/request zero mutation.
- Revision `a8bf783` changes only Automation assertions and the execution report; no production or fixture behavior changed.
- Final Build is reported as 4 actions, exit `0`, and final `BehaviorTreeAdapter` is logged as Success/exit `0`.

## MapContract freshness decision

A fresh `MapContract` rerun is not required for this assertion-only revision. The same production Transition fixture from `b181534` already passed `MapContract` with exit `0`, and `a8bf783` modifies neither `HSRBattleTransitionSubsystem` nor `MapContract`. The existing result remains valid provenance; rerunning would be optional redundancy rather than a gate requirement.

## Sole remaining acceptance item — USER PIE

Only full stock `Move To SpawnOrigin` completion remains:

1. Acquire the player, then leave sight without reacquiring or entering overlap.
2. Capture `Chasing -> LostTarget -> ReturningToSpawnOrigin`, tree epoch, target validity/cleared `TargetActor`, start location, and SpawnOrigin.
3. Wait for stock Move To completion; capture final Actor location and distance to SpawnOrigin within the configured acceptance radius, plus final AIState and Blackboard values.
4. Confirm no Controller Move request, repeating timer, or retry loop occurred. Preserve the first failure/SKIPPED reason if the run cannot complete.

No additional C++ or Automation correction is currently required. User Map, DataAsset, BT/BB, learning, and `.claude/**` changes remain separately owned.

## Conclusion

`REVISE`: the duplicate/post-resolved Encounter contract is now closed. PATCH-02 remains open solely for the required user-provided full-return completion evidence; do not archive until that PIE evidence is recorded and reviewed.
