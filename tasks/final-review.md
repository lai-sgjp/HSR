# TASK-P17-PATCH-02 Return-Completion Review

## Review metadata

- Reviewer: Independent Reviewer / Safety Reviewer
- Reviewed revision: `c1d33d9`
- Result: `REVISE` (implementation passes; final user PIE evidence pending)
- Date: 2026-07-28

## Implementation accepted

- A successful `OnMoveCompleted` while `ReturningToSpawnOrigin` obtains the same Character SpawnOrigin/definition used by recovery and emits `ReturnComplete` with Controller, Actor location, SpawnOrigin, distance, and current Encounter RequestId.
- Completion then calls `PublishNextPatrolIntent` exactly once in that handler. A reachable navigation candidate transitions to `MovingToPatrol`; projection/random failure uses the existing bounded `PatrolWaiting` plus `PatrolLocation=SpawnOrigin` fallback.
- No direct Move request, repeating timer, polling loop, Encounter request, or new recovery retry was added. Recovery state is exited by publishing the next patrol/fallback state.
- Adapter coverage verifies reachable next-candidate publication, bounded fallback, and no Encounter creation. It does not substitute for the physical Move To completion evidence.
- Raw logs confirm final `BehaviorTreeAdapter` and `MapContract` Success/exit `0`; the execution report records the Development Build as passing. Revision provenance is confined to the Controller/test/result allowlist, and user assets remain isolated.

## Final required USER PIE evidence

PATCH-02 has no remaining known C++/Automation correction. It remains open only until the real stock-BT return completes in PIE. The expected evidence is:

1. Before loss: one `Chasing` state record with tree epoch, valid target, Actor location, and SpawnOrigin.
2. Loss: `Chasing -> LostTarget -> ReturningToSpawnOrigin`, `TargetActor` cleared, and no Encounter request.
3. Completion: exactly one `P17-PATCH-02 ReturnComplete` line for that movement request, containing:
   - Actor `Location`;
   - `SpawnOrigin`;
   - `Distance` within the configured Move To acceptance radius;
   - invalid/empty Encounter `RequestId` for this no-overlap path.
4. Immediately after completion:
   - either `PatrolIntent ... Result=Reachable` followed by `MovingToPatrol` and a new candidate;
   - or a projection/random failure reason followed by bounded `PatrolWaiting` and `PatrolLocation=SpawnOrigin`.
5. No second `ReturnComplete` for the same completion, no direct Controller Move, no repeating retry, and no Encounter submission. Preserve the first failure/SKIPPED reason if the run cannot reach SpawnOrigin.

## Conclusion

`REVISE`: `c1d33d9` is accepted and closes the implementation side of return completion. The complete Gate now depends only on the final user-provided PIE log above; do not archive before it is reviewed.
