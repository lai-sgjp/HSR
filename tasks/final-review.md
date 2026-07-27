# TASK-P17-PATCH-02 Radius Zero-Mutation Review

## Review metadata

- Reviewer: Independent Reviewer / Safety Reviewer
- Reviewed revision: `cc700ca`
- Prior finding: `f81b80f`
- Result: `REVISE`
- Date: 2026-07-28

## Closed findings

- A fresh Character now calls the real encounter-config helper with `EnemyDefinition=nullptr` and reads the actual sphere radius as `200`.
- That null-Definition Character call verifies Tick remains disabled and ActorLocation/SpawnOrigin remain unchanged.
- The null-Definition Controller call verifies defaults `1000/1500` and checks controller state, epoch, target, active request, retry, attempt count, and Tick remain unchanged.
- Revision is test/result-only; there is no production, BT, state, Encounter-admission, or user-asset scope creep. Existing Build/Adapter evidence remains applicable.

## Remaining blocking coverage

The requested complete zero-mutation matrix is still incomplete:

- Controller runtime-state comparison occurs only after the null-Definition apply. The normalized `1200/1000` apply and negative-value apply are not followed by the same snapshot comparison.
- Blackboard/runtime binding state is not asserted. The requirement explicitly includes BB state; the existing `HasRuntimeBlackboardForAutomation()` can prove the fresh Controller remains unbound before and after every apply.
- Character Tick/SpawnOrigin/ActorLocation comparison occurs only after the null-Definition apply. The `333` and negative-radius production-helper calls are not followed by the same zero-mutation assertions.

Minimum test-only correction:

1. Create one test-local Controller invariant predicate/snapshot containing state, epoch, target, active request, retry, attempts, Tick, and `HasRuntimeBlackboardForAutomation`; compare it after all three calls: null, normalized Definition, and negative Definition.
2. Create one Character invariant snapshot containing Tick, ActorLocation, and SpawnOrigin; compare it after null, `333`, and negative EncounterRadius calls.
3. Keep the existing actual radius assertions alongside each invariant comparison.
4. Rebuild and rerun `BehaviorTreeAdapter`; no fresh `MapContract` is required because no Transition/Encounter production code changed.

## Conclusion

`REVISE`: actual fallback `200` is now proven, but zero-mutation is checked only for the first apply in each object. The normalized and negative production-helper calls plus Blackboard binding state still need the same assertions.
