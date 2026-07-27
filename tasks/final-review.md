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

---

# TASK-P17-PATCH-01B Independent Implementation Review

Status: `REVISE`

Role: `Independent Reviewer`
Date: `2026-07-27`
Evidence level: `STATIC CODE/COMMIT REVIEW + RECORDED BUILD CLAIM; AUTOMATION/PIE NOT RUN`
Reviewed commits: Task Gate `0ff5be7`, Task Gate re-review `4e23bf0`, Coordinator `08cec4d`, Implementation `60ee700`

## Scope and implementation findings

- `PASS` — Implementation commit `60ee700` changes only the active-task allowlist: Coordinator, participant, existing P9 harness, and execution report. No TurnManager, action-value, Config, Content, Save, UI, network, or Tick change is present.
- `PASS` — The participant-lifetime `bBreakResultPublished` latch was removed without introducing a renamed lifetime latch. Break publication remains on the authoritative `Before > 0 && After == 0` Toughness edge, and the existing completed ActionId resolution cache remains the replay boundary.
- `PASS` — The existing same-transaction ordering remains Break/Status/Delay followed by deferred `ResolveDefeat`; no production ownership was moved into Status, Widget, GameplayCue, or TurnManager.
- `PASS` — Added counter state is scalar, editor-only, reset synchronously in `Reset`, and introduces no UObject/GC, reflection, delegate, asynchronous, replication, or Tick hazard. Existing weak ASC/Actor participant references are unchanged.
- `RISK` — `BreakStatusRequestCountForTest` records only attempted requests and does not retain/assert `EHSRStatusOperationResult`; `BreakDelayRegistrationCountForTest` increments even when `ConsumeBreakDelay` returns false. These names and the PIE assertions therefore cannot establish the frozen Status request/result and accepted Delay counts.

## Evidence findings

- `PASS` — `git diff --check 08cec4d..60ee700` succeeds. The worktree's unrelated `learn/SaveSystem.md` and `.claude/**` changes are not in the Implementation commit.
- `PASS (reported, not independently rerun)` — `tasks/execution-result.md` records a successful `HSREditor Win64 Development` build after one preserved sandbox write failure. No build artifact/log was committed as independent evidence, but the implementation report clearly separates the claim from unrun runtime checks.
- `BLOCKING` — The required `HSR.Battle.Patch.RepeatableBreak` Automation test does not exist. `Source/HSR/Tests/HSRCombatPatchTests.cpp` still contains only `HSR.Battle.Patch.StatusGeneric` and was not changed by `60ee700`.
- `BLOCKING` — No Automation or PIE runtime evidence was run. The task explicitly forbids inferring runtime correctness from harness source.
- `BLOCKING` — The added P9 case exercises only first edge, cached replay, fixture recovery, and second edge. It does not cover or log initial/continued `0 -> 0`, non-zero/no-break, weakness failure, already-dead admission, Finished, Reset plus stale BattleId, reused ActionId under the new BattleId, or same-frame lethal Break-before-Defeat behavior.
- `BLOCKING` — The P9 case does not assert cached Resolution equality, exact Toughness values, Status operation results, accepted Delay results, or exact turn advancement. It also reinitializes TurnManager before the second edge, which avoids demonstrating recovery/second-edge behavior across the same uninterrupted turn runtime.

## Minimal required revision (within the existing allowlist)

1. Add `HSR.Battle.Patch.RepeatableBreak` to `Source/HSR/Tests/HSRCombatPatchTests.cpp`. It must use a real or controlled Coordinator runtime and deterministically cover first/replay/recovery/second, initial and continued zero, non-zero/no-break, weakness failure, already-dead admission, Finished, Reset, stale BattleId, reused ActionId in the new battle epoch, and same-frame lethal priority. Assert exact Break, Status request/result, accepted Delay, Toughness, and turn deltas; replay must return the cached Resolution with all new deltas zero.
2. Make the editor-only observability truthful: retain enough Status result and Delay acceptance evidence to distinguish a request attempt from a successful result/accepted registration. Increment any accepted-Delay counter only when `ConsumeBreakDelay` returns true. Do not change TurnManager behavior.
3. Tighten the existing P9 repeatable-Break case to verify both independent ActionIds, both successful Status results, both accepted Delay registrations, replay zero-delta/cached result, recovery zero-delta, second edge, Toughness, and turn effects. Add the missing runtime cases to Automation; PIE may remain the narrower production DataAsset/GE gate required by the task.
4. Rebuild, run `HSR.Battle.Patch.RepeatableBreak` plus applicable P8/P9/Battle Automation, and run the existing `bRunP9DotBreakHarness` PIE gate. Preserve the first real failure and attach exact logs. PIE must show `P9-003 DotBreak Harness=COMPLETE`, the repeatable case `Result=PASS`, two distinct ActionIds, exact successful Status/accepted Delay evidence, and zero related `FAIL`, `INCOMPLETE`, or `SKIPPED`.

## Verdict

`REVISE` — The production latch removal and edge/replay direction are plausible and stay in scope, but the task's mandatory runtime matrix and exact side-effect evidence are absent. Route automatically back to the original Implementation Agent; all required fixes fit the existing allowlist and authorization, so no user scope expansion is required. After Implementation commits the revision, return to an independent Reviewer before requesting the user PIE gate.

---

# TASK-P17-PATCH-01B Independent Follow-up Review

Status: `REVISE`

Role: `Independent Reviewer`
Date: `2026-07-27`
Evidence level: `STATIC FOLLOW-UP REVIEW + RECORDED BUILD CLAIM; AUTOMATION/PIE STILL NOT RUN`
Reviewed follow-up: Implementation `418b8a1` after Reviewer `968d4d8`

## Closed findings

- `PASS` — Status observability now retains the latest `EHSRStatusOperationResult` and increments its count only for `Success`.
- `PASS` — Delay observability now retains the actual `ConsumeBreakDelay` return and increments its count only for an accepted registration.
- `PASS` — The P9 repeatable case captures and asserts both first and second Status success plus both accepted Delay results. The changes remain editor-only, reset synchronously, and do not alter TurnManager or action-value semantics.
- `PASS (reported, not independently rerun)` — The follow-up execution report records another successful Development Editor build and preserves the existing external `AISystem.h` warning.

## Blocker assessment

- `REJECTED` — Expanding `HSRBattleGameMode.h` is not proven necessary. The internal `RunP9DotBreakHarness` is a PIE/log harness and exposing it to Automation would couple deterministic tests to GameMode asset/configuration and logging ownership.
- `AVAILABLE WITHIN CURRENT ALLOWLIST` — `UHSRBattleCoordinator` already owns the private battle state, actual `RequestAction` path, participant/TurnManager/status runtime, completed-resolution cache, and Reset/rebuild contract. Add one narrowly named `#if WITH_DEV_AUTOMATION_TESTS` fixture initializer in the already-allowlisted `HSRBattleCoordinator.h/.cpp`. It may create a transient two-participant world/runtime and a valid saved battle request solely for `HSR.Battle.Patch.RepeatableBreak`; the Automation then drives the existing public `RequestAction`, ASC attributes, Reset/rebuild, and read-only counters. The seam must not be `UFUNCTION`, Blueprint-visible, compiled in shipping/non-test builds, or become an alternate Break resolver.
- This Coordinator-owned seam is safer than a GameMode header API because it initializes test state adjacent to the private invariants it must satisfy, while the test still traverses the production RequestAction/GE/Status/Delay path. Do not expose the internal P9 namespace function, P9 switches, configured DataAssets, or a generic mutable-state API.

## Remaining required work

1. Add the narrowly guarded Coordinator fixture initializer described above and implement `HSR.Battle.Patch.RepeatableBreak` in the already-allowlisted test file.
2. Cover and assert the full frozen matrix from the previous review, including cached Resolution equality, exact Toughness and turn deltas, Reset/stale BattleId/new-epoch ActionId reuse, and same-frame lethal ordering/counts.
3. Run the new Automation and applicable regressions. Only after Automation passes should the existing P9 PIE gate be handed to the user.

## Verdict

`REVISE` — The accepted Status/Delay counter defect is fixed, but required Automation remains absent. No allowlist expansion or new user authorization is required: route back to the original Implementation Agent with the Coordinator-owned, `WITH_DEV_AUTOMATION_TESTS`-only fixture direction. Return to independent review after its role commit.

---

# TASK-P17-PATCH-01B Fixture Feasibility Reassessment

Status: `BLOCKED`

Role: `Independent Reviewer`
Date: `2026-07-27`
Evidence level: `STATIC INITIALIZATION-PATH REVIEW`
Inputs: Implementation blocker after `7cc3a5e`; Coordinator `SubmitBattleRequest`, `BuildParticipants`, `BuildAndValidateParticipantDefinitions`, ASC/ability/status initialization; GameMode configuration injection.

## Correction to the previous review

- The Coordinator-only fixture direction in `7cc3a5e` was not concrete enough and is withdrawn.
- A helper that directly writes `CurrentState`, `Participants`, `TurnManager`, granted abilities, or status components would bypass the production `SubmitBattleRequest -> BuildParticipants -> RequestAction` initialization contract and cannot satisfy the required runtime evidence.
- A fully transient production build is theoretically possible but not a safe minimal seam here: `BuildParticipants` requires a valid BasicAttack definition/ability, formal damage and Toughness GameplayEffects, participant classes, initialization GE, enemy definition/weakness, and Break Status definition. Constructing or globally mutating these contracts inside the test would duplicate GameMode configuration and could contaminate other Automation.

## Exact viable route: minimal GameMode header expansion

Authorize `Source/HSR/Battle/HSRBattleGameMode.h` and add exactly one non-reflected factory under `#if WITH_DEV_AUTOMATION_TESTS`:

```cpp
static UHSRBattleCoordinator* CreateRepeatableBreakAutomationFixture(
    UObject* Outer,
    UWorld* BattleWorld,
    TSubclassOf<AHSRBattleGameMode> ConfiguredGameModeClass,
    FText& OutFailure);
```

Implementation ownership and restrictions:

- Implement only in the already-allowlisted `HSRBattleGameMode.cpp`, also guarded by `#if WITH_DEV_AUTOMATION_TESTS`.
- Resolve the supplied class CDO and read its existing BasicAttack, enemy, participant-initialization GE, Break Status, character catalog/class and related production configuration. Do not mutate the CDO or any Content asset.
- Create a transient Coordinator owned by `Outer`, inject the CDO configuration through existing Coordinator setters, submit a deterministic fresh encounter request, and call the real `BuildParticipants(BattleWorld)`. Return only a fully `Spawned` Coordinator; otherwise return `nullptr` with structured `OutFailure`.
- The Automation test may reference only the existing configured `BP_HSRBattleGameMode` class path, not individual GE/DataAsset paths, then drive the public `RequestAction`, ASC values, Reset/rebuild and read-only test counters.
- The factory must not be a `UFUNCTION`, Blueprint-visible, editor property, shipping/runtime API, alternate action resolver, or generic private-state mutator. It must not add a harness switch or change normal `BeginPlay`.
- Created Coordinator/Actors belong to the supplied transient test World/Outer and must be destroyed by the Automation fixture. No raw UObject cache or global state is permitted.

## Evidence consequence

This is a genuine allowlist stop condition. Revising the Automation requirement to a pure helper plus PIE would weaken the already-frozen full runtime matrix and is not justified while the exact factory above can execute the production initialization/action path.

## Verdict

`BLOCKED` — Status/Delay counter corrections remain accepted, but runtime Automation cannot safely proceed under the current allowlist. Coordinator must request user authorization for the single file `Source/HSR/Battle/HSRBattleGameMode.h`, limited to the exact `WITH_DEV_AUTOMATION_TESTS` factory contract above. After authorization, route to the original Implementation Agent, then rebuild/run Automation and return to independent review before the user PIE gate.

---

# TASK-P17-PATCH-01B GameInstance Lifecycle Reassessment

Status: `REVISE`

Role: `Independent Reviewer`
Date: `2026-07-27`
Evidence level: `UE5.6 ENGINE SOURCE + STATIC UNCOMMITTED-DIFF REVIEW`

## Concrete lifecycle route

The reported null `BattleWorld->GetGameInstance()` is caused by creating a standalone `UGameInstance` and an unrelated `UWorld::CreateWorld`. UE5.6 already provides the correct public lifecycle:

1. Require `GEngine != nullptr`.
2. Create and strongly retain `UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine)`.
3. Call `GameInstance->InitializeStandalone(UniqueWorldPackageName)`. Do **not** call `CreateNewWorldContext` or `UWorld::CreateWorld` separately.
4. UE5.6 `InitializeStandalone` itself calls `GEngine->CreateNewWorldContext(EWorldType::Game)`, sets `WorldContext.OwningGameInstance`, creates the World, calls `World->SetGameInstance(GameInstance)`, installs it as the current context World, and finally calls `GameInstance->Init()`. Therefore `GetSubsystem<UHSRCharacterProfileSubsystem>()` is obtained through the real `UGameInstanceSubsystem` lifecycle rather than `NewObject` fabrication.
5. Obtain `UWorld* BattleWorld = GameInstance->GetWorld()` and assert `BattleWorld`, `BattleWorld->GetGameInstance() == GameInstance`, and the profile subsystem are non-null before invoking the fixture factory.
6. Preserve the World pointer before shutdown. On every exit path, first destroy/reset Coordinator-owned runtime while the subsystem collection is alive, then call `GameInstance->Shutdown()` to deinitialize subsystems, then `BattleWorld->DestroyWorld(false)`, then `GEngine->DestroyWorldContext(BattleWorld)`. Use scope-exit/RAII so assertion failures cannot leak a global WorldContext. Never call `Shutdown` twice.

Required test-file includes are `Engine/Engine.h`, `Engine/GameInstance.h`, and `Misc/ScopeExit.h`; all changes remain within `Source/HSR/Tests/HSRCombatPatchTests.cpp`. Risks are global WorldContext leakage and subsystem delegates surviving a failed test; the mandatory cleanup ordering and scope guard address them. The test must run serially on the game thread, as existing Editor Automation does.

## Factory ownership correction

- The current uncommitted diff places `CreateRepeatableBreakAutomationFixture` on `UHSRBattleCoordinator` and adds unrestricted `friend class UHSRBattleCoordinator` to `AHSRBattleGameMode`. That is broader than the previously approved exact surface and gives the whole Coordinator class access to every protected GameMode member.
- Keep the factory as the one `AHSRBattleGameMode` static method authorized in the prior review, implemented in `HSRBattleGameMode.cpp` under `#if WITH_DEV_AUTOMATION_TESTS`. A class member already has access to its own CDO configuration, so no friend declaration is required.
- The factory may create a transient Coordinator with the supplied Outer, copy configuration through existing public Coordinator setters, register the configured catalog using the properly initialized GameInstance subsystem, submit the deterministic request, and call real `BuildParticipants`. It must not manually construct the subsystem or write Coordinator private state.

## Verdict

`REVISE` — A safe executable route exists with the currently authorized files; no additional allowlist expansion is needed. Route to Implementation with the exact `InitializeStandalone`/cleanup sequence above and remove the broad GameMode friend plus Coordinator-owned factory surface. After Build, run the Automation in isolation first to detect leaked WorldContexts or subsystem delegates, then run it with applicable regressions before independent review.
