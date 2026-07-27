# TASK-P17-PATCH-02 Navigation-Projection Automation Review

## Review metadata

- Reviewer: Independent Reviewer / Safety Reviewer
- Reviewed revision: `2f96e84`
- Result: `PASS WITH FOLLOW-UP`
- Date: 2026-07-28

## Verified

- `EHSRPatrolIntentResult` explicitly distinguishes `Reachable`, `ProjectPointFailed`, and `RandomReachableFailed`; production maps the real projection/random-query booleans to these outcomes without changing the projection flow.
- The deterministic seam independently exercises reachable candidate publication, projection failure, and random-reachability failure. It asserts the successful candidate/state, the common bounded `PatrolWaiting`/SpawnOrigin fallback for both failures, and distinct failure result values.
- Production still projects with `(100,100,300)`, queries random reachability only after projection success, and retains separate failure logging. No direct movement, recurring polling, or retry expansion was introduced.
- Revision provenance is confined to the allowlisted Controller, Enemy Types, test, and execution-result files. User Map, Enemy DataAsset, and `Content/AI/**` assets remain outside the commit.
- Final `Saved/Logs/HSR.log` records `BehaviorTreeAdapter` as `Success` and `TEST COMPLETE. EXIT CODE: 0`. The execution report records the editor Build as 8 actions, exit `0`.

## Follow-up boundary

This closes the branch-exact Automation revision only. User PIE must still demonstrate the real Nav projection/candidate and stock-BT movement behavior; PATCH-02 remains incomplete until the broader runtime matrix is accepted.

## Conclusion

`PASS WITH FOLLOW-UP`: the seam now preserves and verifies distinct projection-versus-random failure classification while retaining the same bounded fallback and production navigation behavior.
