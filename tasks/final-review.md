# TASK-P17-PATCH-01B Pre-Implementation Task Gate

Status: `REVISE`

Role: `Prompt Reviewer + Safety Reviewer`
Evidence level: `STATIC REVIEW / PLANNING ONLY`
Scope: task contract, allowlist, existing P8/P9 Break ownership and test seams; no implementation was reviewed.

## Gate findings

- `PASS` — The task has one independently testable outcome: replace the participant-lifetime Break latch with repeatable authoritative `Before > 0 && After == 0` edge transactions.
- `PASS` — Ownership is correct: Coordinator owns ActionId replay and Break publication, Status consumes a request, and TurnManager remains the sole Delay consumer. PATCH-01C action-value work is explicitly excluded.
- `PASS` — The production allowlist covers the currently evidenced latch, result enum, Coordinator transaction, editor harness body, Automation, and execution report. `HSRBattleGameMode.h` is not presently required if the existing P8/P9 opt-in harness entry is reused; adding a new flag/function declaration would require a stop-and-authorize request.
- `REVISE` — “死亡不触发” is ambiguous and conflicts with the inherited P8 requirement that same-frame death/Break priority be frozen. Current Coordinator commits HP damage, then performs independent Toughness/Break resolution before `ResolveDefeat`. The task must explicitly distinguish an already-dead/invalid target (zero Break side effects) from a target that is alive at command admission but is lethally damaged while the same authoritative transaction also crosses Toughness to zero. Freeze whether that latter transaction publishes Break or suppresses it; do not let Implementation choose implicitly.
- `REVISE` — The matrix names Break/Status/Delay but does not require auditable per-request counts/results. Freeze assertions for first edge, replay, second edge, zero-to-zero, Finished and Reset/rebuild as deltas: Break presentation/result count, Break Status request/result count, Delay registration/consumption count, Toughness value, and turn advancement. Replay must return the cached Resolution while all new side-effect deltas remain zero.
- `REVISE` — “旧 Battle/旧 target 回调” is not mapped to an existing asynchronous callback in the inspected synchronous action path. Replace it with executable cases: after Reset/rebuild, an old BattleId command is rejected with zero mutation; reused ActionId under the new BattleId is treated according to the existing reset contract; stale participant/runtime references are not used. If an actual callback seam is intended, name it and include every required file in the allowlist.
- `REVISE` — Editor/Automation boundary needs one precise rule: Automation must own the full deterministic transaction matrix where transient fixtures suffice; PIE is only required for the production DataAsset/GE path and must report two independent ActionIds, two Break events, two Status requests/results, two accepted Delay registrations, replay zero-delta, and zero `FAIL/INCOMPLETE/SKIPPED`. Reuse of the existing P9 DoT/Break switch should be stated explicitly, or a header expansion must be authorized first.

## Required planning correction

Coordinator must revise `tasks/active-task.md` before Implementation restatement to freeze the same-frame lethal priority, replace non-existent callback wording with executable stale BattleId/reset cases, and add exact observable side-effect counters/deltas plus the Automation/PIE split. No production implementation may begin under the current wording.

## Verdict

`REVISE` — Implementation read-only restatement may not begin yet. After Coordinator updates the active task within planning ownership, this task gate must be rechecked; no user scope expansion is required unless the revision adds `HSRBattleGameMode.h` or another file outside the current allowlist.

---

## Pre-Implementation Task Gate Re-review

Status: `PASS`

Role: `Prompt Reviewer + Safety Reviewer`
Evidence level: `STATIC REVIEW / REVISED PLANNING ONLY`
Scope: Coordinator revision following gate commit `0ff5be7`; the historical `REVISE` above remains authoritative evidence of the first review.

### Closure audit

- `PASS` — Same-frame priority is now explicit and compatible with the inherited P8 pipeline: a target alive at command admission may publish exactly one Break/Status/Delay before `ResolveDefeat`; an already-dead target is rejected with zero Break side effects.
- `PASS` — Required deltas now cover first edge, cached replay, recovery-only, second independent edge, zero-to-zero, non-zero damage, weakness failure, Finished, death, Reset, stale BattleId and ActionId reuse in a fresh battle-local epoch. Break, Status, Delay, Toughness and turn effects are auditable rather than inferred from a final snapshot alone.
- `PASS` — The non-existent callback abstraction was removed. Reset behavior is expressed through executable old-BattleId rejection and new-BattleId ActionId reuse cases, consistent with the existing battle-local processed-resolution contract.
- `PASS` — Automation must exercise controlled runtime transactions, while PIE reuses `bRunP9DotBreakHarness` and the existing `P9-003 DotBreak Harness` entry. No `HSRBattleGameMode.h`, Content or Config expansion is authorized or required by the revised contract.
- `PASS` — Ownership and scope remain narrow: Coordinator detects the authoritative edge and owns replay; Status and TurnManager only consume requests; existing skip-once Delay semantics are preserved; PATCH-01C action-value, Speed, Advance, Delay/Slow redesign and TurnManager edits remain prohibited.

### Safety notes for implementation

- Any counters or development seams must remain editor/test-only where applicable and must not become a second production truth source.
- “Delay +1” means one newly accepted registration/request for the Break ActionId. Actual later skip consumption remains governed by the existing TurnManager contract and applicable P9 regression; PATCH-01B must not modify that algorithm.
- If the existing P9 harness cannot express the required runtime matrix without a new header declaration, or if a new production file becomes necessary, stop and request the smallest allowlist expansion.

### Verdict

`PASS` — The revised task is self-contained and safe to hand to Implementation. Implementation read-only restatement may begin; production work still requires the task-specific user confirmation mandated by Automatic Role Handoff.
