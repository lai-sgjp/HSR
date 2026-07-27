# TASK-P17-PATCH-02 Final Code-Gate Review

## Review metadata

- Reviewer: Independent Reviewer / Safety Reviewer
- Reviewed revision: `0c9aa0a`
- Result: `PASS WITH FOLLOW-UP`
- Date: 2026-07-28

## Code Gate accepted

- MovingToPatrol and ReturningToSpawnOrigin fixtures independently reset the recovery marker and execute the production move-failure decision seam.
- Each handled case verifies its own `MoveFailed` marker, final controller/Blackboard `ReturningToSpawnOrigin`, null controller and Blackboard targets, expected Blackboard SpawnOrigin/PatrolLocation, no active or Blackboard Encounter request, no retry arm, unchanged controller/Blackboard epoch, and unchanged Encounter attempt/admission count.
- Alert, Chasing, EncounterPending, and Idle retain the previously accepted full controller-plus-six-key Blackboard exact zero-mutation snapshots.
- Final raw logs record `BehaviorTreeAdapter` and `MapContract` as Success with exit `0`; Build is reported successful. Revision provenance is confined to the allowlisted header/test/result files, and user-owned Blueprint/Map/DataAsset/AI changes remain isolated.

## Sole remaining task-level follow-up — USER PIE

The C++/Automation Code Gate now has no known remaining defect. PATCH-02 itself must remain open until the real stock-BT return is observed:

1. Acquire the player, then leave sight without reacquiring or overlapping.
2. Capture `Chasing -> LostTarget -> ReturningToSpawnOrigin`, epoch, cleared TargetActor, Actor start location, and SpawnOrigin.
3. Capture exactly one `P17-PATCH-02 ReturnComplete` line with Actor Location, SpawnOrigin, Distance within the configured Move To acceptance radius, and empty Encounter RequestId.
4. Capture the next reachable patrol candidate/`MovingToPatrol`, or the bounded failure fallback to `PatrolWaiting`/SpawnOrigin.
5. Confirm no duplicate ReturnComplete, Controller Move request, repeating retry, or Encounter submission. Preserve the first failure/SKIPPED reason if the run cannot complete.

## Conclusion

`PASS WITH FOLLOW-UP`: the final Code Gate passes. The only remaining requirement is user-provided full-return PIE evidence; this review must not be interpreted as authorization to archive PATCH-02 before that evidence is reviewed.
