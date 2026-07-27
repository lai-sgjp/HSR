# TASK-P17-PATCH-02 Radius Integration Revision Review

## Review metadata

- Reviewer: Independent Reviewer / Safety Reviewer
- Reviewed revision: `0522ee4`
- Prior finding: `9c3debd`
- Result: `REVISE`
- Date: 2026-07-28

## Test-realism improvements accepted

- Controller Automation now invokes the same `ApplyPerceptionConfig` helper used by `OnPossess` and reads the real `UAISenseConfig_Sight` values.
- Character Automation invokes the same `ApplyDefinitionEncounterConfig` helper used by BeginPlay and reads the real EncounterCollision sphere radius.
- Actual component/config assertions cover perception fallback `1000/1500`, Definition normalization `1200/1000 -> 1200/1200`, negative perception clamps, EncounterRadius `333`, and negative EncounterRadius clamp.
- Production changes remain limited to helper extraction and dev-only seams/getters. BT ordering, state transitions, movement ownership, Encounter admission, and user assets are unchanged.

## Remaining blocking assertions

Two requirements from `9c3debd` are still not actually covered:

1. Encounter no-Definition fallback `200`:
   - The test verifies the Definition object's default is `200`, but it never reads a fresh Character's actual EncounterCollision radius before assigning a Definition.
   - Therefore a broken constructor fallback sphere could pass.
2. Radius application zero mutation:
   - The Controller test does not snapshot state, epoch, target, active/Blackboard Encounter request, retry state, or Tick before/after applying null, normalized, and negative Definitions.
   - The Character test does not assert applying EncounterRadius leaves origin/lifecycle and Tick behavior unchanged.

Minimum test-only correction within the current allowlist:

- Immediately after spawning the Character and before assigning `EnemyDefinition`, assert the real sphere radius is `200` and that applying the helper with no Definition preserves `200`.
- Around each Controller apply call, capture/compare CurrentState, BehaviorTreeEpoch, current target, active Encounter RequestId, retry flag, submission-attempt count, and `PrimaryActorTick.bCanEverTick`; all must remain unchanged.
- Where no runtime Blackboard is bound, explicitly record that fact; where a Blackboard is used, compare all six keys before/after.
- Around Character apply calls, assert `PrimaryActorTick.bCanEverTick` remains false and SpawnOrigin/Actor location are unchanged.
- Rebuild and rerun `BehaviorTreeAdapter`. `MapContract` does not need rerun unless Encounter/Transition production changes.

## Scope and provenance

The revision remains inside the existing Controller/Character/test/result allowlist. Dirty user Blueprints, Map, Enemy DataAsset, `Content/AI/**`, learning, and `.claude/**` files remain isolated.

## Conclusion

`REVISE`: the tests now exercise real production helpers and real components, but actual Character fallback `200` and the required zero-mutation invariants are not yet proven.
