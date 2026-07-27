# TASK-P17-PATCH-02 Stage A Revision Review

## Review metadata

- Reviewer: Independent Reviewer / Safety Reviewer
- Reviewed revision: `38d868e`
- Result: `REVISE`
- Date: 2026-07-27

## Verified evidence

- Revision provenance is limited to the allowlisted controller `.h/.cpp` and `tasks/execution-result.md`; no user `.uasset` is part of `38d868e`.
- `Saved/Logs/HSR.log` independently confirms one discovered `HSR.Exploration.Patch.BehaviorTreeAdapter` test, `Test Completed. Result={Success}`, `TEST COMPLETE. EXIT CODE: 0`, and normal engine exit.
- The execution report records the post-fix editor build as six actions with exit `0`. Stage B assets and PIE behavior remain unverified and must not be marked complete.
- The revision correctly removes the BeginPlay patrol timer, perception `MoveToActor`, and SpawnOrigin `MoveToLocation` call added by the first Stage-A implementation.

## Blocking finding

`Source/HSR/Enemy/HSREnemyAIController.cpp` still contains the legacy `StartPatrol()` implementation. It calls `MoveToLocation` directly and schedules itself through `PatrolWaitTimerHandle`. The `OnMoveCompleted` override also still interprets movement completion by exploration state and, for `MovingToPatrol`, schedules `StartPatrol` again. Consequently a Stage-B stock Behavior Tree `Move To` completion can enter this legacy timer path and later issue a second C++ movement request. The claim that stock `Move To`/`Wait` is the sole movement driver is therefore not yet true.

## Minimum required correction

- Remove the `StartPatrol` declaration and implementation, including all timer callbacks to it and its direct `MoveToLocation` call.
- Make `OnMoveCompleted` incapable of scheduling or issuing movement. If it is retained as the event adapter, it may only publish state/Blackboard recovery intent; Stage-B stock nodes must remain the only owner of Move To and Wait execution.
- Remove any now-dead legacy timer/handler storage only where it is no longer used, staying inside the existing controller allowlist.
- Rebuild and rerun `HSR.Exploration.Patch.BehaviorTreeAdapter`. Add a static/automation assertion that the Stage-A controller cannot reintroduce legacy movement ownership if feasible within the existing test allowlist.

## Scope guard

Do not edit or submit `Content/AI/**`, the Enemy Blueprint, or any other user asset during this correction. Stage B has not begun and PATCH-02 as a whole is not complete.

## Conclusion

`REVISE`: the explicit Stage-A single-movement-owner contract is still violated by reachable legacy patrol code. The correction is confined to the existing allowlist and needs no new user authorization.
