# TASK-P17-PATCH-02 — Behavior Tree AI Migration Planning Gate Review

## Review metadata

- Reviewer: Independent Task-Gate / Prompt-Safety Reviewer
- Date: 2026-07-27
- Result: `REVISE`

## Evidence inspected

- `tasks/active-task.md` (Patch 02 card and exact candidate allowlist)
- `docs/phase-roadmap-0-20.md` (cross-stage improvement 4 and regression requirements)
- `docs/phase-17-patch-01-execution-plan.md` (Patch 02 boundary/non-goals)
- `tasks/execution-result.md` (`NOT STARTED / TASK GATE REQUIRED`)
- Existing enemy implementation in `HSREnemyAIController`, `HSREnemyCharacter`, `HSREnemyTypes`, and encounter DTOs

## Findings

The card has the correct single outcome, frozen non-goals, no-Tick intent, and an allowlist that is sufficient for a planning review. Existing code establishes a useful migration baseline: perception is delegate-driven, the controller owns the hand-written state machine and movement callbacks, patrol is seeded from `SpawnOrigin`, and encounter submission is guarded by `HasPending()` plus the subsystem result. However, the gate is not yet safe to hand to implementation.

1. **Behavior Tree/Blackboard ownership is underspecified (Blocking).** The card must name the owner of the `UBehaviorTreeComponent`, Blackboard component, tree start/stop, and perception-to-Blackboard writes. It must explicitly prohibit Blackboard keys that retain actor references across map transitions; target identity should be an ephemeral weak/object value or a stable encounter/participant identifier with a clear invalidation event. Define which decisions remain authoritative in C++ (Encounter request, resolved rejection, state transition) and which nodes are presentation/orchestration only.

2. **The observable state mapping is incomplete (Blocking).** Freeze a one-to-one mapping for Idle, patrol wait/move, Alert, Chasing, LostTarget, MoveFailed, and EncounterPending. In particular, existing `HandleChaseTargetLost()` returns to a random patrol point after a delay; it does not return to `SpawnOrigin`. If “return to birthplace” is required, specify a dedicated return-to-origin state/task and its completion/failure behavior, or explicitly preserve the current random-patrol semantics and amend the acceptance criterion. Also define how `OnMoveCompleted` and perception loss produce BT events without polling/Tick.

3. **Encounter idempotency and resolved rejection need an explicit contract (Blocking).** `TryRequestEncounterFromCharacter()` can be reached from overlap and perception, while the subsystem currently only exposes pending/consumed result categories through the result DTO. Freeze the transaction key (enemy identity + player identity + encounter epoch/request id), the single authoritative submission owner, and the exact behavior for duplicate, already-pending/traveling, already-consumed/resolved, invalid definition, and travel-failure results. A failed or resolved request must not restart chase, enqueue another request, or leave a BT task permanently running. Add a same-frame duplicate and post-resolved rejection regression case to the allowlisted automation fixture.

4. **Asset Gate is missing (Blocking).** No Behavior Tree, Blackboard, service/task node, perception asset, or enemy Blueprint/DataAsset binding is present in the candidate evidence. Before implementation, the user must either provide/confirm exact asset paths and editable fields (tree, Blackboard keys, service interval/event mode, controller/character class bindings) or authorize a code-only adapter that does not create assets. Implementation must stop at Asset Gate and must not invent Content files, Build.cs dependencies, or Config entries.

5. **No-Tick and lifecycle semantics need measurable acceptance (Risk).** The controller currently has `PrimaryActorTick=false`, but a BT service with a nonzero interval would still be polling. Require delegate/event-driven perception and movement completion, zero `Tick`/polling services, and explicit BeginPlay/Possess/UnPossess/EndPlay tree start-stop and delegate unbinding. Include stale callback, re-possess, and teardown cases.

6. **Regression coverage is not frozen precisely (Risk).** Add concrete cases for: perception acquire/loss, target destruction, move success/failure/abort, return-to-origin (or the documented preserved behavior), duplicate overlap+perception submission, already-resolved submission, and no-Tick evidence. Require before/after observable state and Encounter result/request IDs, not only a final success log.

## Required card revisions before PASS

- Add the ownership/key/state mapping and authoritative C++ boundary described above.
- Resolve the return-to-origin versus current random-patrol behavior explicitly.
- Freeze Encounter transaction/idempotency/resolved-rejection semantics and add the missing automation cases.
- Add a user Asset Gate with exact paths/fields and a hard stop for missing assets.
- Define lifecycle/no-Tick proof and stale-callback acceptance evidence.

No production code, assets, active-task state, or execution result was modified by this review. Until these revisions are recorded and re-reviewed, Implementation may only provide a read-only restatement and must not implement `TASK-P17-PATCH-02`.

## Conclusion

`REVISE` — the migration direction is plausible and the existing state machine is discoverable, but ownership, semantic mapping, resolved Encounter behavior, and required user assets are insufficiently frozen for a safe implementation gate.
