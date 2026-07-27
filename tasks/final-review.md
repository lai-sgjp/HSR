# TASK-P17-PATCH-02 Spawn-Origin Lifecycle Review

## Review metadata

- Reviewer: Independent Reviewer / Safety Reviewer
- Reviewed revision: `eb0bd14`
- Result: `PASS WITH FOLLOW-UP`
- Date: 2026-07-28

## Verified

- Before `BeginPlay`, `GetSpawnOrigin()` returns the current Actor location; `BeginPlay` captures that location once, after which the getter returns the frozen origin even if the Actor moves.
- Controller startup, navigation candidate generation, Blackboard `SpawnOrigin`, patrol completion, and lost/failure recovery all consume `AHSREnemyCharacter::GetSpawnOrigin`; no parallel raw-origin source was introduced.
- The automation fixture asserts a non-zero pre-BeginPlay fallback and a stable post-capture origin after relocation.
- Final `Saved/Logs/HSR.log` records `BehaviorTreeAdapter` as `Success` and `TEST COMPLETE. EXIT CODE: 0`. The execution report records the editor Build as 7 actions, exit `0`, with only the existing engine warning.
- Revision provenance is limited to the allowlisted Enemy Character, test, and execution-result files. It contains none of the dirty user assets.
- Current dirty `Content/Data/Enemies/DA_Enemy_Phase5Test1.uasset`, `Content/Maps/Map_Phase1_Exploration.umap`, `Content/AI/**`, `learn/SaveSystem.md`, and `.claude/**` remain outside the Reviewer/Implementation commits.

## Follow-up boundary

This accepts the lifecycle origin fix only. User PIE must still verify the placed enemy's real pre-/post-BeginPlay ordering, generated patrol destination, return-to-origin behavior, and the remaining Stage-B runtime matrix. PATCH-02 is not complete.

## Conclusion

`PASS WITH FOLLOW-UP`: the getter now provides a valid pre-BeginPlay origin and a stable captured origin thereafter, consistently across Controller/Nav/Blackboard/recovery consumers, without absorbing user-owned changes.
