# TASK-P17-PATCH-02 Handled-Recovery Matrix Review

## Review metadata

- Reviewer: Independent Reviewer / Safety Reviewer
- Reviewed revision: `9f3bdec`
- Result: `REVISE`
- Date: 2026-07-28

## Verified

- The shared recovery publisher now explicitly writes both Blackboard `SpawnOrigin` and `PatrolLocation` after final Returning state publication, preventing pawnless fixture state refresh from obscuring the intended origin.
- Both MovingToPatrol and ReturningToSpawnOrigin decision calls assert final controller/Blackboard Returning state, null Blackboard target, expected origin values, no active Encounter request, and no retry arm.
- Production behavior remains bounded; Build, `BehaviorTreeAdapter`, and `MapContract` are reported/logged as passing. Changes stay inside the Controller/test/result allowlist, with user assets isolated.

## Remaining blocking assertions

The explicit matrix requested by the preceding review is still only partially implemented:

- Neither handled case captures and proves controller epoch and Blackboard TreeEpoch unchanged.
- Neither captures and proves Encounter attempt/admission count unchanged.
- Neither asserts Blackboard `EncounterRequestId` remains empty/no admission payload.
- Neither asserts the controller `CurrentTarget` is clear; only the Blackboard object key is checked.
- ReturningToSpawnOrigin does not independently assert the `MoveFailed` recovery marker after its own seam call. Its marker could be inherited from the prior MovingToPatrol case.

Minimum correction, test-only within the current allowlist:

1. Before each handled call independently capture controller epoch, Blackboard TreeEpoch, Encounter attempt count, active RequestId, Blackboard EncounterRequestId, target pointer, and retry flag.
2. After each call assert: final controller/BB Returning state; controller and Blackboard targets null; SpawnOrigin and PatrolLocation equal ExpectedSpawnOrigin; recovery marker `MoveFailed`; controller/BB epochs unchanged; attempt count unchanged; active and Blackboard request IDs empty/unchanged; retry remains false.
3. Reset or seed the recovery marker independently before the second case so its assertion cannot pass by inheritance.
4. Rebuild and rerun `BehaviorTreeAdapter`. A fresh `MapContract` is unnecessary unless Transition production changes.

## Remaining task gate

After these final handled-state assertions pass, the code Gate should be closed. Only full-return user PIE will remain: one real `ReturnComplete` within acceptance radius, no target/Encounter, then a new patrol candidate or bounded fallback without duplicate completion or Controller Move/retry.

## Conclusion

`REVISE`: explicit origin publication is fixed, but epoch, admission, controller-target, request-name, and independent second-marker invariants are still not proven for both handled paths.
