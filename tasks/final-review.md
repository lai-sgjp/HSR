# TASK-P17-PATCH-02 Stage B Initial-Patrol Revision Review

## Review metadata

- Reviewer: Independent Reviewer / Safety Reviewer
- Reviewed revision: `dd56861`
- Result: `PASS WITH FOLLOW-UP`
- Date: 2026-07-28

## Verified

- Root cause and fix align: after `UseBlackboard` succeeds and before `RunBehaviorTree`, the shared publisher sets `MovingToPatrol`, `SpawnOrigin`, and `PatrolLocation=SpawnOrigin`, so the initial selector no longer evaluates an Idle/empty patrol intent.
- The production path and automation seam share `PublishInitialPatrolIntent`; the test checks the non-Idle state and exact location value.
- Static inspection finds zero `StartPatrol`, `MoveToLocation`, `MoveToActor`, `SetTimer`, `TimerHandle`, or `NavigationSystem` matches in the Controller header/implementation. `OnMoveCompleted` still only publishes failure/recovery state or checks weak-target validity and does not regain movement ownership.
- Revision provenance is limited to the allowlisted Controller, test, and execution-result files. No user `.uasset` is included.
- The failed bare-`UWorld` fixture evidence remains preserved in `Saved/Logs/HSR-backup-2026.07.27-16.20.49.log`: invalid BT/BB initialization is the first relevant failure, followed by the expected state/location assertion failures and test exit `-1`. The final seam avoids claiming that such a world can initialize `UseBlackboard`/`RunBehaviorTree`.
- Final `Saved/Logs/HSR.log` independently records one `BehaviorTreeAdapter` test as `Success` and `TEST COMPLETE. EXIT CODE: 0`. The execution report records the final editor Build as 4 actions, exit `0`.

## Follow-up boundary

This accepts only the C++ initial-intent revision. User PIE is still required to prove that the saved Stage-B stock graph consumes the patrol intent before target perception. The five branches and the broader acquire/loss, movement failure/recovery, Encounter, stale-epoch, and no-polling runtime matrix remain pending; PATCH-02 is not complete.

## Conclusion

`PASS WITH FOLLOW-UP`: the initial patrol intent is published at the correct startup boundary without reintroducing a C++ movement owner or modifying user assets. Continue at the user PIE gate.
