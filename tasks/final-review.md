# TASK-P17-PATCH-02 Intentional-Abort Review

## Review metadata

- Reviewer: Independent Reviewer / Safety Reviewer
- Reviewed revision: `2ed4786`
- Result: `REVISE`
- Date: 2026-07-28

## Production behavior verified

- Failed/aborted movement is converted to recovery only while the current state owns patrol/recovery movement: `MovingToPatrol` or `ReturningToSpawnOrigin`.
- Abort callbacks arriving after a branch switch to Alert, Chasing, EncounterPending, or Idle are ignored with structured `MoveAbortIgnored ... Reason=BranchSwitch` logging; the ignored branch contains no state or Blackboard write.
- True handled failure still uses the existing `MoveFailed -> ReturningToSpawnOrigin` publisher and does not introduce direct movement or a retry loop.
- Build, `BehaviorTreeAdapter`, and `MapContract` are reported/logged as Success with exit `0`. Revision provenance remains in the Controller/test/result allowlist; dirty user Blueprints, Map, DataAsset, and AI assets remain isolated.

## Blocking Automation gaps

The report claims both paths are verified, but the new predicate is exercised only for the ignored Chasing path:

- The earlier “true patrol failure” assertions call `PublishMoveFailureRecoveryForAutomation` directly, bypassing `ShouldHandleMoveFailureOrAbort` and the new `HandleMoveFailureForAutomation` decision path. A regression that rejects `MovingToPatrol` would still pass.
- The ignored path asserts only that `CurrentState` remains Chasing. It does not snapshot Blackboard values, active Encounter request, Nav-ready retry state, target identity, or recovery marker, so the claimed state/Blackboard zero mutation is incomplete.
- ReturningToSpawnOrigin, Alert, EncounterPending, and Idle predicate classifications are not asserted despite being explicitly named by the contract.

Minimum correction inside the existing allowlist:

1. Publish a reachable patrol intent to establish `MovingToPatrol`, call `HandleMoveFailureForAutomation`, require `true`, and assert `MoveFailed` was recorded before final `ReturningToSpawnOrigin` with `PatrolLocation=SpawnOrigin` and no retry/Encounter.
2. Establish `ReturningToSpawnOrigin` and assert the same handled result.
3. For Alert, Chasing, EncounterPending, and Idle, capture a full relevant snapshot before/after (AIState, TargetActor, PatrolLocation, SpawnOrigin, TreeEpoch, EncounterRequestId/active request, retry flag, and last recovery marker), call the decision seam, require `false`, and assert exact zero mutation.
4. Rebuild and rerun both Automation tests.

## Remaining user gate

After those deterministic assertions pass, the only remaining task-level evidence is still the real full-return PIE: `Chasing -> LostTarget -> ReturningToSpawnOrigin`, one `ReturnComplete` within acceptance radius, cleared target/no Encounter, followed by the next patrol candidate or bounded fallback, with no duplicate completion or C++ Move/retry.

## Conclusion

`REVISE`: production filtering is appropriately scoped, but Automation does not yet prove the handled predicate path or the claimed zero-mutation matrix. Full-return user PIE remains pending afterward.
