# TASK-P17-PATCH-02 Full Move-Ownership Snapshot Review

## Review metadata

- Reviewer: Independent Reviewer / Safety Reviewer
- Reviewed revision: `555200c`
- Result: `REVISE`
- Date: 2026-07-28

## Ignored-state matrix accepted

- The test-local snapshot now covers controller state and target identity, all six Blackboard runtime values, active Encounter RequestId, controller epoch, retry flag, Encounter attempt count, and last recovery marker.
- Alert, Chasing, EncounterPending, and Idle each execute the production failure decision seam and compare the complete snapshot exactly before/after.
- Final `BehaviorTreeAdapter` and `MapContract` runs are logged as Success/exit `0`; Build is reported successful. Revision provenance remains inside the header/test/result allowlist, and user assets remain isolated.

## Remaining blocking handled-state assertions

The two handled paths still do not satisfy the explicitly requested complete invariant matrix:

- `MovingToPatrol` asserts only `true` and the recovery marker; it does not assert final Returning state, cleared target, both SpawnOrigin/PatrolLocation Blackboard values, retry state, active request identity, attempt count, or unchanged epoch.
- `ReturningToSpawnOrigin` asserts only `true` and final state; it omits the same remaining invariants and does not independently assert the `MoveFailed` recovery marker.
- `PublishSpawnOriginRecoveryIntent` currently writes only `PatrolLocation=InSpawnOrigin`. Its `SetState` calls refresh Blackboard `SpawnOrigin` from the pawn; in the pawnless deterministic seam this becomes zero. Therefore the requested explicit Blackboard SpawnOrigin invariant cannot currently be proven from the supplied `InSpawnOrigin`.

Minimum correction inside the existing allowlist:

1. In `PublishSpawnOriginRecoveryIntent`, publish both `SpawnOrigin=InSpawnOrigin` and `PatrolLocation=InSpawnOrigin` when a runtime Blackboard is present. This is redundant-but-consistent in production and makes the shared recovery payload explicit.
2. Before each handled case, seed/confirm no target and no active Encounter request; capture epoch, attempt count, and retry state.
3. For both MovingToPatrol and ReturningToSpawnOrigin, independently require:
   - seam returns `true`;
   - recovery marker is `MoveFailed`;
   - final state and Blackboard AIState are `ReturningToSpawnOrigin`;
   - TargetActor/controller target are clear;
   - Blackboard SpawnOrigin and PatrolLocation equal ExpectedSpawnOrigin;
   - no retry, no active/request-name Encounter, no attempt increase, and unchanged epoch.
4. Rebuild and rerun `BehaviorTreeAdapter`; rerun `MapContract` only if Transition production changes.

## Remaining task gate

After these two handled snapshots pass, no known code Gate issue remains. The sole task-level item will be final full-return user PIE with one `ReturnComplete` within acceptance radius, no Encounter, and transition to the next patrol candidate or bounded fallback.

## Conclusion

`REVISE`: the four ignored states now have exact full zero-mutation coverage, but the two handled states still lack their required complete bounded-recovery assertions and explicit Blackboard SpawnOrigin publication.
