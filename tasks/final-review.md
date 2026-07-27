# TASK-P17-PATCH-02 Navigation-Projection Review

## Review metadata

- Reviewer: Independent Reviewer / Safety Reviewer
- Reviewed revision: `292deb0`
- Result: `REVISE`
- Date: 2026-07-28

## Verified production behavior

- After obtaining valid NavData, production code projects `SpawnOrigin` with extent `(100,100,300)` and calls `GetRandomReachablePointInRadius` only when projection succeeds, using the projected center.
- Projection failure and post-projection random failure have distinct warning reasons (`ProjectPointFailed` and `RandomReachableFailed`) and both use the bounded SpawnOrigin/`PatrolWaiting` fallback.
- No direct movement, repeating timer, polling loop, or user asset edit was added. Revision provenance is limited to the allowlisted Controller, test, and execution-result files.
- The execution report records Build as 5 actions, exit `0`; final `Saved/Logs/HSR.log` records `BehaviorTreeAdapter` success and test exit `0`.

## Blocking finding

The claimed Automation coverage does not actually distinguish the four requested outcomes. The test calls `PublishPatrolIntentForAutomation(..., true)` once for a candidate and `PublishPatrolIntentForAutomation(..., false)` once for fallback. It never supplies or observes separate projection-success/random-failure versus projection-failure inputs. The assertion named “Projection failure and random failure...” merely rechecks the same single fallback result, so it cannot detect a regression that conflates or reverses the two production branches or their reason classification.

## Minimum correction

- Add an allowlisted deterministic seam/helper that accepts projection success and random success independently (or returns an explicit failure reason), while keeping real NavSystem calls in production.
- Exercise all four requested cases separately: projection+random success, projected random candidate publication, projection failure, and random failure after successful projection. Assert the distinct failure classification and the common bounded fallback.
- Rebuild and rerun `HSR.Exploration.Patch.BehaviorTreeAdapter`; keep user Map/DataAsset/AI assets isolated.

## Scope boundary

PIE remains user-pending and PATCH-02 is not complete. This revision request stays inside the existing Controller/test/result allowlist and needs no new authorization.

## Conclusion

`REVISE`: production projection flow is bounded and plausible, but the required branch-exact Automation evidence is not present.
