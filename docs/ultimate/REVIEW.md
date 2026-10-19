# Ultimate integration review

Reviewer: Codex Agent D (same agent previously owned the read-only reference audit). Date: 2026-10-19. Scope: current Coordinator, ordinary TurnManager boundary, Ultimate / prepared GAS execution, command DTO / ViewModel / Widget target intent. This is a source review; it does not declare a current build, packaged runtime or human acceptance PASS.

Update: Root has implemented the R1–R5 fixes below. The original findings are retained as an audit trail. Independent regression coverage now totals 20 Ultimate groups, including mixed public input, conflicting request identity, actor destruction during selection and event-driven resource/tag UI snapshots. The final integrated build and run remain Root-owned; no future result is prefilled here.

## Actionable findings

### R1 — P1: Reset from an atomic GAS delegate must be deferred

`UHSRBattleCoordinator::RequestActionCore` holds participant references across `TryActivateAbility`, Cost GE, damage GE and their synchronous Energy / Health delegates. An external listener can call public `Reset()` during those callbacks. Immediate clearing of `Participants`, `TurnManager` and dispatcher counters invalidates the active transaction, and a returning old action could populate the freshly reset result cache.

Root accepted this finding and is implementing deferred Reset while `RequestActionDispatchDepth`, `bFormalDamageTransactionOpen` or `bPumpingBattleFlow` is active, rejecting later intent and performing cleanup after the outer atomic scope returns. `ResetAndRebuildForDevelopmentTest` must reject inside that scope rather than rebuilding over a deferred Reset.

Independent regression added: `HSR.Battle.Ultimate.AtomicDelegateReset.U15_U17` in `Source/HSR/Tests/HSRUltimateArbitrationTests.cpp`. A real Health GE delegate requests Reset, then submits another Ultimate. Assertions cover immediate rejection, eventual generation change and registry/history cleanup, old timer inactivity and a successful action after rebuilding. Current-source execution evidence is pending Root's next integrated run.

### R2 — P1: ordinary presentation admission must use the same atomic busy gate

At review time, `SubmitBattleCommand` routed ordinary actions directly to `QueuePresentedAction`, whose initial busy check tested `PresentedAction` only. `RequestAction` separately checked dispatch depth. With presentation enabled, a caller may execute an ordinary action through the public synchronous `RequestAction`; its Health delegate can then submit another ordinary action through `SubmitBattleCommand`. No presented action exists yet, so the latter path can accept an ordinary command while the first is still resolving.

Recommended fix: ordinary admission must reject when dispatch / atomic settlement / flow pumping is active, while legal Ultimate intent must retain its distinct accept-and-wait behavior. Existing reentrant-settlement automation covers the usual presented-action case; this mixed-public-entry case needs a focused regression if fixed.

### R3 — P2: same request ID with different parameters receives a misleading acknowledgement

`RequestUltimate` looks up `ProcessedActionResolutions` and accepted history by ActionId before comparing the command's BattleId, actor, skill or targets. A mismatched command receives old success or an Accepted result even though it does not enter the queue. It does **not** replace the original command or cause duplicate Cost / GE, so this is an identity/feedback contract issue rather than duplicate execution.

Recommended fix: bind accepted ActionId to its immutable command values; exact replay is idempotent, conflicting replay rejects `DuplicateAction` / invalid battle. Compare group target sets according to the declared target-order policy.

### R4 — P2: AOE additional targets must share the positive-damage energy gate

The primary-target branch grants hit Energy only when `AppliedDamage > 0` and the target survives. The additional-target loop originally granted hit Energy to every surviving successfully applied GE. `UHSRDamageRuleDefinition` explicitly permits `MinDamage == 0`, so a zero-damage AOE can otherwise grant Energy to secondary targets but not its primary target.

Recommended fix: apply the same positive authoritative HP-delta predicate for all targets. Keep actual kill Energy and configured source EnergyGain independent.

### R5 — P2: external actor destruction while selecting an ordinary action

The Coordinator observes Health changes but no actor-destruction delegate was found in the reviewed binding path. `PumpBattleFlow` sweeps invalid actors and `AdvancePastInvalidCurrentTurn` skips them correctly when another safe point runs. Destruction while the system is idle in ordinary selection therefore waits for another intent or safe point; if normal buttons are disabled and nobody has a ready Ultimate, there may be no input that drives the sweep.

Ultimate-induced death of the suspended ordinary actor is handled by the post-Ultimate pump. The remaining boundary is independent external actor destruction / direct health changes during selection. Either bind a lifecycle event to defer one safe pump, or explicitly limit the supported destruction route to battle teardown and cover that contract in runtime evidence.

## Reviewed invariants and positive evidence

- Ordinary AV remains in the existing TurnManager. Ultimate completion bypasses `ResolveAction`; deferred normal finalization is retained until the extra-action FIFO empties. Existing speed and explicit action-distance updates are not replaced by an old snapshot.
- `AdvancePastInvalidCurrentTurn` offers a distinct non-action path for a dead suspended ordinary actor. It does not invent an ordinary action or recharge an Ultimate owner.
- FIFO uses a monotonic sequence allocated on the GameThread; per-owner pending admission blocks repeated keypresses. The Ability consumes prepared specs before external delegates, and completion removes the active request before exposing the final state.
- `FinalizeDeferredDefeats` registers each deferred casualty before allowing the terminal result. The multiple-target GAS path holds independent contexts and per-target results; partial successful effects retain their cost rather than refunding an already damaging action.
- Cost and compensation now use `WasSuccessfullyApplied()` for Instant GE application. Net Energy remains diagnostic because legitimate delegate-triggered gains can change the observed delta. The independent real-GE test covers success with +10 recovery and failure compensation preserving that recovery.
- Group-target UI confirmation is a pure DTO projection: `FHSRBattleCommandViewState::BuildSelectedCommand` copies the displayed candidate IDs; `BeginCommandSubmit` understands group confirmation; execution revalidates the exact target set. Loss of a queued target cancels instead of silently retargeting.
- Presentation timer callbacks capture battle generation and ActionId and use a weak UObject binding. Rule settlement and bounded presentation completion jointly gate advancement. A real paused UWorld stops the timer and resumes it correctly in the independently authored pause test.
- Existing statuses consume their owner's ordinary `TurnEnded` with epoch / sequence deduplication. Ultimate scheduling does not publish an extra ordinary end.

## Evidence boundaries

`Saved/Ultimate/automation-02.log` records 15 of the then-16 Ultimate groups passing, including 20 actual stress rounds with 20 successes and zero failures. The Headless ordinary regression failed because its test attempted Skill with initial shared SP zero; the fixture was corrected to Basic first, then Skill. The original failure remains in the log. Subsequent review changes and the new atomic-reset test require a new integrated run; prior passing results are not claimed as validation of later edits.

Two older regression failures were also diagnosed without changing production behavior: the casualty test assumed random targeting must pick one specific actor within two samples, and the enemy-catalog test still assumed two catalog members despite an existing third authored patrol enemy. The scoped fixtures now express wounded targeting with a private duplicated definition and validate the three stable catalog IDs. Root owns final reruns and the delivery verdict.

## Root integration disposition (2026-10-19)

R1–R5 were fixed by their exclusive writers and covered in Automation03: all 45 battle tests passed, including 20 Ultimate groups, actor destruction, conflicting replay, positive-damage gate and reentrant settlement. UIAutomation01 passed 81/81. DemoAutomation02 passed 4/4 after correcting historical catalog expectations. These are final reruns after the reviewed gameplay edits; earlier failed logs remain available. Final packaging/graphical scope is in HANDOFF.md. This review is engineering evidence, not human acceptance.
