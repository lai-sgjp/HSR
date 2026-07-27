# TASK-P17-PATCH-02 Move-Ownership Matrix Review

## Review metadata

- Reviewer: Independent Reviewer / Safety Reviewer
- Reviewed revision: `21996d1`
- Result: `REVISE`
- Date: 2026-07-28

## Verified

- The test now reaches the production decision seam for both handled states, `MovingToPatrol` and `ReturningToSpawnOrigin`, rather than relying only on the direct recovery publisher.
- Alert, Chasing, EncounterPending, and Idle are each passed through the same seam and return `false`.
- Production classification remains correct, and no production `.cpp` behavior changed in this assertion revision.
- Build, `BehaviorTreeAdapter`, and `MapContract` remain reported/logged as successful. Revision provenance is restricted to the allowlisted header/test/result files; user Blueprints, Map, DataAsset, and AI assets remain isolated.

## Blocking assertion gaps

The new ignored-state assertion is still not the “full relevant snapshot” required by the preceding review. It checks only CurrentState, PatrolLocation, controller epoch, Encounter attempt count, and retry flag. It does not check:

- Blackboard `AIState`, `TargetActor`, `SpawnOrigin`, `TreeEpoch`, or `EncounterRequestId`;
- controller active Encounter request identity;
- the last recovery marker;
- target identity/validity.

Consequently, mutations to most Blackboard runtime state or the active request would remain undetected. The handled-state checks are also incomplete: the new Moving/Returning decision calls do not each assert final `PatrolLocation=SpawnOrigin`, no retry, no Encounter creation/identity change, and bounded final `ReturningToSpawnOrigin` state.

Minimum correction inside the current allowlist:

1. Add a test-local snapshot containing CurrentState, all six Blackboard values, tree epoch, active Encounter request validity/identity, Encounter attempt count, retry flag, last recovery marker, and target object/validity.
2. Seed non-default TargetActor, SpawnOrigin, PatrolLocation, TreeEpoch, EncounterRequestId/active request, and recovery marker where necessary.
3. For each of Alert, Chasing, EncounterPending, and Idle, capture before/after snapshots around `HandleMoveFailureForAutomation`, require `false`, and compare every snapshot field exactly.
4. For MovingToPatrol and ReturningToSpawnOrigin, require `true` and independently assert recorded `MoveFailed`, final `ReturningToSpawnOrigin`, `PatrolLocation=SpawnOrigin`, unchanged/no Encounter admission, no retry arm, and unchanged epoch.
5. Rebuild and rerun `BehaviorTreeAdapter`; `MapContract` only needs a fresh rerun if production/Transition code changes.

## Remaining task gate

After this exact snapshot matrix passes, the code Gate should have no known remaining issue. The sole task-level requirement will be full-return user PIE: one real `ReturnComplete` within acceptance radius, cleared target/no Encounter, then a new patrol candidate or bounded fallback, with no duplicate completion or Controller Move/retry.

## Conclusion

`REVISE`: the decision seam is now invoked for all six states, but the required handled invariants and full ignored-state zero-mutation snapshot are not yet proven.
