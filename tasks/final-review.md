# TASK-P17-PATCH-02 Real-Blackboard Teardown Review

## Review metadata

- Reviewer: Independent Reviewer / Safety Reviewer
- Reviewed revision: `4620704`
- Result: `REVISE` (teardown revision passes; overall acceptance matrix remains incomplete)
- Date: 2026-07-28

## Teardown revision accepted

- The synthetic write mask and its pass oracle were removed.
- The Automation fixture creates an AI World-backed initialized Blackboard with the six real key types, seeds each key with a non-default value, and directly reads every key.
- It verifies real defaults after Stop, Stop then ClearState, repeated Stop, fresh rebind plus stale retry, and EndPlay-equivalent Stop then ClearState. Cleared vectors are correctly compared with `FAISystem::InvalidLocation`.
- The preserved first failed run (`HSR-backup-2026.07.27-17.52.24.log`) shows the vector-clear assertions failing before the InvalidLocation correction; the final adapter log records Success/exit `0`. Final `MapContract` also records Success/exit `0`, and the report records Build as 4 actions, exit `0`.
- Revision provenance is confined to the Controller/test/result allowlist. User Map, Enemy DataAsset, `Content/AI/**`, learning, and `.claude/**` changes remain isolated.

## Remaining blocking acceptance groups

The code revision closes the Blackboard teardown defect, but four required runtime groups remain unverified, so PATCH-02 cannot yet pass its complete gate:

1. Full return completion:
   - Enter chase, leave sight, do not reacquire.
   - Record state/epoch/target validity and distance to SpawnOrigin at `Chasing -> LostTarget -> ReturningToSpawnOrigin`.
   - Wait for stock Move To completion and record final location/state, proving arrival rather than only transition intent.
2. Move failure/abort:
   - During patrol or return, temporarily select an unreachable destination or interrupt the active stock Move To.
   - Capture the result as failure/abort, `MoveFailed -> ReturningToSpawnOrigin`, Blackboard target/location, and proof that no C++ movement/timer retry loop starts.
3. Target destruction:
   - While Chasing and before overlap, destroy the target Actor.
   - Capture target validity before/after, cleared `TargetActor`, state/epoch, and recovery toward SpawnOrigin.
4. Encounter duplicate/resolved matrix:
   - Trigger two overlap entry calls for the same transaction in one frame; capture both structured result reasons, identical/new request IDs as applicable, state/epoch, and subsystem counts before/after.
   - After the admitted request is consumed/resolved, repeat the same transaction and capture `AlreadyResolved` (or the frozen equivalent) with zero mutation.
   - If physical PIE cannot reliably generate same-frame duplicates, add a deterministic allowlisted transaction fixture; do not infer duplicate handling from one successful overlap.

Each run must retain before/after state, tree epoch, target validity, Encounter request/result IDs, and the first failure/SKIPPED reason. These are explicit active-task acceptance requirements rather than optional follow-up.

## Conclusion

`REVISE`: `4620704` correctly supplies real six-key teardown evidence and needs no further teardown correction. The task remains blocked only on the four runtime acceptance groups above; do not archive PATCH-02 yet.
