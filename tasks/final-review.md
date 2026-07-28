# TASK-P17-005 Independent Review

Current review status: `ASSET GATE READY — CODE GATE PASS / FINAL TASK PASS NOT YET AVAILABLE`

Review role: Independent Reviewer
Review date: 2026-07-28
Current review scope: the final X root-policy recovery fix, complete rollback call-site audit, updated RecoveryMatrix, actual Source diff, allowlist, and Reviewer-rerun eleven-test Frontend Automation.

## Current Asset Gate result

`ASSET GATE READY`

The P17-005 C++/Automation implementation may now proceed to the user-owned Editor Asset Gate. This is a code-gate approval only. It is **not** a final task `PASS`: Input assets, WBP assets, persistence, PIE behavior, target resolutions, and user-visible acceptance remain unverified.

### Reviewer dynamic evidence

- Reviewer inspected the final `CloseFrontendToRoot` root-policy primary-failure branch: old-policy restoration is captured and passed to `ResolveCompensation`.
- Reviewer inspected the matching RecoveryMatrix injection: primary root-policy failure plus failed old-policy recovery must return `CompensationFailed` and mark the manager inconsistent.
- Reviewer reran `HSR.UI.FrontendNavigation` against the current compiled module.
- 11/11 tests completed with `Result={Success}`:
  - `CrossTypeReplace`
  - `DirectAndBackFailure`
  - `FailureCompensation`
  - `InputBindingGuard`
  - `PlaceholderReplace`
  - `RecoveryMatrix`
  - `RouterFailures`
  - `RouterReplace`
  - `RouterSequence`
  - `SharedSession`
  - `TravelDiscard`
- Final log: `**** TEST COMPLETE. EXIT CODE: 0 ****`.
- Reviewer reran `git diff --check`: exit code 0; line-ending warnings only.
- Development Editor Build is report-level `Succeeded`; the dynamic Editor-Cmd run successfully loaded and exercised the current compiled module.

### Final code-gate findings

- The fixed public state shape is maintained across successful Hub, Character, Inventory, and placeholder transitions.
- Direct-root shortcuts compensate the just-created Hub on module-stage failure.
- Cross-type replacement preserves old module ownership until the candidate transaction succeeds.
- Router and ScreenStack snapshot restoration is coordinated by UIManager; Blueprint entry points cannot bypass the Frontend facade.
- Frontend pause ownership is acquired once, retained across module navigation, and released only on successful session close; external pause remains protected.
- Character/Inventory Back validates policy, focus, and Router before releasing Widget/ViewModel ownership; failure restores old route, Stack, policy, and module focus.
- Hub/module/placeholder/Back/X rollback paths now check fallible recovery and escalate failed compensation through `ResolveCompensation`.
- Travel tears Frontend down to exact root, discards Frontend route state, and does not restore it on the new host.
- PlayerController InputComponent binding is guarded per active component; Frontend IMC ownership remains separate from Exploration IMC.
- No blocking UE reflection, GC ownership, domain-authority, Config, dependency, or allowlist defect was found.

### User Asset Gate requirements

The user may now create and bind only the allowlisted assets from `tasks/active-task.md`:

- Six Digital Input Actions with `Trigger When Paused` enabled.
- `IMC_FrontendNavigation` with Esc/B/T/M/F4/X mappings and no custom Trigger/Modifier.
- `WBP_FrontendShell_P17` and `WBP_FrontendModuleRoot_P17` using the specified C++ parent classes.
- PlayerController and Exploration HUD Class Default references allowed by the task card.

The user must Save All, close/reopen the Editor, verify reference persistence, and provide PIE evidence for keyboard/mouse navigation, focus, Back/X, controlled missing-class behavior, Exploration input restoration, and 1920x1080 plus 1280x720 layout/readability.

### Still not verified

- All user Editor assets and bindings.
- Editor reopen/reference persistence.
- PIE keyboard/mouse/focus and visible pause/navigation behavior.
- Both target resolutions.
- Dialogue competition: `NOT VERIFIED / P17-012`.
- Physical controller, Standalone, Packaged, and Shipping: `NOT VERIFIED`.

### Scope and existing-change isolation

P17-005 Source changes remain inside the frozen allowlist. Pre-existing or user-owned Character/Enemy Blueprints, Enemy DataAsset, exploration map, `Content/AI/**`, `.claude/**`, and `learn/SaveSystem.md` remain excluded. This review does not authorize staging, committing, resetting, cleaning, deleting, or attributing them to P17-005.

### Next review

After the user completes the Asset Gate and supplies persistence/PIE evidence, Independent Reviewer must perform the final implementation-plus-user-evidence review. Only that later review may issue `PASS`, `PASS WITH FOLLOW-UP`, `REVISE`, or `BLOCKED` for task completion.

---

## Historical review — fourth post-REVISE re-review

Historical fourth re-review status: `REVISE — FOURTH RE-REVIEW / ASSET GATE NOT READY`

## Historical fourth re-review result

`REVISE`

The rollback audit is nearly complete and the new RecoveryMatrix is valuable, but one fallible restoration remains unchecked in the X close transaction. Because the task contract requires every failed recovery to escalate to `CompensationFailed` plus `Inconsistent`, the implementation is still **not ready for the user Asset Gate**.

### Reviewer dynamic evidence

- Reviewer reran `HSR.UI.FrontendNavigation` against the fourth-round compiled module.
- 11/11 tests completed with `Result={Success}`, including `RecoveryMatrix`.
- Final log: `**** TEST COMPLETE. EXIT CODE: 0 ****`.
- Reviewer reran `git diff --check`: exit code 0; line-ending warnings only.
- Development Editor Build remains report-level `Succeeded`; the Reviewer did not rerun UBT.

### Fixed since the prior review

- Hub focus and Router rollback now aggregate unpause and old-policy restoration through `ResolveCompensation`.
- Character/Inventory open focus and Router rollback now check old-policy restoration.
- Placeholder policy/focus/Router rollback and placeholder Back now check recovery.
- X focus and Router late rollback aggregate pause and old-policy restoration.
- Back Router rollback restores old module focus and escalates failed focus recovery.
- RecoveryMatrix independently passes for Hub focus plus failed unpause recovery, placeholder focus plus failed policy recovery, and X unpause plus failed policy recovery.

### Remaining blocking finding

**X primary policy-failure recovery is still unchecked.** In `CloseFrontendToRoot`, when applying the Exploration/root policy fails, the implementation restores the old Stack and calls `ApplyPolicyBackend(PC, OldPolicy, UIOnly)`, but ignores that restoration result and immediately returns `PolicyApplyFailed`. If the primary policy application partially mutates input state and the old-policy restore also fails, the manager can remain corrupted without setting `bInconsistent` or returning `CompensationFailed`.

This is distinct from the tested RecoveryMatrix X case. The current test injects primary unpause failure followed by failed old-policy recovery, which exercises the next branch. It does not inject primary root-policy failure followed by failed old-policy recovery.

### Required next step

1. Capture the old-policy restoration result in the X primary policy-failure branch and return `ResolveCompensation(bPolicyRestored, PolicyApplyFailed)`.
2. Add one Automation assertion for primary X policy failure plus failed old-policy recovery, expecting `CompensationFailed` and `IsInconsistent()==true` while old module ownership remains retained.
3. Rerun Build, eleven-or-more Frontend tests, and diff-check; request the next Asset Gate review. User Editor work remains stopped.

### Scope and evidence boundary

No other unchecked fallible rollback was found in the enumerated Hub/module/placeholder/Back/X transaction paths. No reflection/GC, allowlist, domain-authority, Config, dependency, or Git-operation violation was found. User-owned changes remain isolated. Editor assets, persistence, PIE, resolutions, physical controller, Standalone, Packaged, Shipping, and Dialogue competition remain `NOT VERIFIED`.

---

## Historical review — third post-REVISE re-review

Historical third re-review status: `REVISE — THIRD RE-REVIEW / ASSET GATE NOT READY`

## Historical third re-review result

`REVISE`

Character/Inventory Back now restores the old module focus on Router failure, and the new late-recovery tests are meaningful. Nevertheless, `ResolveCompensation` has not been applied to all compensation branches. Directly inspected paths can still return an original failure after recovery itself fails, contrary to the frozen failure matrix. The implementation remains **not ready for the user Asset Gate**.

### Reviewer dynamic evidence

- Reviewer reran `HSR.UI.FrontendNavigation` against the third-round compiled module outside the sandbox.
- 10/10 tests completed with `Result={Success}`.
- `DirectAndBackFailure`, `FailureCompensation`, `CrossTypeReplace`, `SharedSession`, and `TravelDiscard` all completed `Success`.
- Final log: `**** TEST COMPLETE. EXIT CODE: 0 ****`.
- Reviewer reran `git diff --check`: exit code 0; line-ending warnings only.
- Development Editor Build remains report-level `Succeeded`; successful Editor-Cmd module load and Automation provide dynamic compiled-code evidence, but the Reviewer did not rerun UBT.

### Previous blockers — disposition

1. **Back old-module focus restoration: fixed.** Character and Inventory Router-failure rollback now restores Stack, Router, old policy, and preferred/fallback focus to the still-owned module. Automation asserts preserved Character route and restored Character focus. Failed old-module focus restoration escalates to `CompensationFailed` plus `Inconsistent`.
2. **Compensation aggregation: partially fixed.** `ResolveCompensation` exists and is used for Back Router recovery and X late pause/policy recovery. Late pause recovery and old-module focus recovery tests pass. It is not yet consistently used across the full rollback matrix.

### Remaining blocking finding

**Several recovery branches still ignore compensation failure.** Concrete examples in `UHSRUIManagerSubsystem.cpp` include:

- Hub focus failure rollback calls `ApplyPauseBackend(World, false)` and `ApplyPolicyBackend(...OldPolicy...)` without checking either result before returning `FocusApplyFailed`.
- Hub Router failure rollback has the same unchecked unpause and policy restoration before returning `StackRejected`.
- Character/Inventory open focus and Router rollback restore policy without checking the result.
- Placeholder policy/focus/Router rollback restores the old policy without checking the result.
- X primary unpause failure restores Stack and old policy but does not check whether policy restoration succeeded before returning `PauseApplyFailed`.

These are reachable late-stage failures. A recovery failure can therefore leave the world paused state, semantic control mode, cursor/focus, or resolved policy inconsistent while reporting only the original controlled error. The task contract explicitly requires failed recovery to set `bInconsistent` and return `CompensationFailed`. A shared helper existing in the file is insufficient unless every rollback branch aggregates its recovery results.

### Test-truthfulness boundary

The 10/10 result is genuine and independently reproduced. The added tests cover old-module focus restoration failure and X pause-restoration failure. They do not inject recovery failure into Hub focus/Router rollback, Character/Inventory open focus/Router rollback, placeholder rollback, or X unpause-primary-failure policy restoration. Therefore the green suite does not contradict the source finding.

### Required next step

1. Enumerate every rollback return in Hub, Character, Inventory, placeholder, Back, and X flows.
2. Route every fallible recovery operation through `ResolveCompensation`; no `ApplyPauseBackend`, `ApplyPolicyBackend`, or focus restore in a rollback may have an ignored result.
3. Add table-driven or focused Automation for at least Hub focus + failed unpause recovery, module open Router/focus + failed old-policy recovery, placeholder rollback + failed old-policy recovery, and X unpause-primary-failure + failed old-policy recovery.
4. Rerun Build, Frontend Automation, and diff-check; request another Asset Gate review. User Editor work remains stopped.

### Scope and evidence boundary

No new reflection/GC, allowlist, domain-authority, Config, dependency, or Git-operation violation was found. User-owned Content/Map/AI/.claude/learning changes remain isolated. Editor assets, persistence, PIE, both resolutions, physical controller, Standalone, Packaged, Shipping, and Dialogue competition remain `NOT VERIFIED`.

---

## Historical review — second post-REVISE re-review

Historical second re-review status: `REVISE — SECOND RE-REVIEW / ASSET GATE NOT READY`

## Historical second re-review result

`REVISE`

The direct-root outer rollback is now present and the reported ten tests were independently reproduced by the Reviewer. However, Back rollback still does not restore the prior module focus after a Router failure, and multiple compensation branches still ignore compensation-stage failure. The implementation remains **not ready for the user Asset Gate**.

### Reviewer dynamic evidence

- First sandboxed Automation launch failed before tests because Zen utility `CreateProc` returned `Access denied`, followed by the UE Zen assertion. This first real infrastructure error is retained and is not classified as a test failure.
- The identical narrow command was rerun outside the sandbox with approval.
- `HSR.UI.FrontendNavigation`: all 10 tests completed with `Result={Success}`.
- `DirectAndBackFailure` completed `Success`.
- Final log: `**** TEST COMPLETE. EXIT CODE: 0 ****`.
- Reviewer-rerun `git diff --check`: exit code 0; line-ending warnings only.
- The Build remains report-level evidence: Development Editor Build reported `Succeeded`; the Reviewer did not rerun Build because the dynamic Automation loaded the compiled module successfully and direct source blockers remained.

### Prior three blockers — disposition

1. **Direct-root module shortcut outer transaction: fixed for primary module-stage failures.** `CompleteModuleAttempt` closes the newly opened Hub when Character/Inventory/placeholder opening fails, and escalates failed outer cleanup to `CompensationFailed` plus `Inconsistent`. `DirectAndBackFailure` verifies missing Character class restores exact root, unpauses, and closes Router history.
2. **Character/Inventory Back transaction: partially fixed.** Stack and Router are snapshotted, focus and route results are now checked, and Widget/VM ownership is released only after success. One focus-restoration defect remains below.
3. **Compensation result aggregation: not fully fixed.** A single recovery-failure test exists, but several production rollback branches still ignore recovery return values.

### Remaining blocking findings

1. **Back Router-failure rollback does not restore old module focus.** Character and Inventory Back apply Hub focus before submitting Router Back. If Router submission fails, they restore Stack, Router, and policy but never apply focus back to the still-owned Character/Inventory Widget. The old module remains visible and logically active, yet keyboard/gamepad focus may remain on the Shell underneath. `DirectAndBackFailure` asserts only Stack depth after the injected Router failure, not restored active route, Widget/VM ownership, or old-module focus. Rollback must explicitly focus the old module and treat failure to restore focus as `CompensationFailed`/`Inconsistent`.

2. **Compensation-stage failures are still ignored in multiple paths.** Examples include Hub focus/Router rollback (`ApplyPauseBackend(false)`, policy restore, and snapshot restores), Character/Inventory open focus/Router rollback, placeholder policy/focus/Router rollback, and X pause/policy restoration. These calls are made without aggregating success, so a failed recovery can return the original `FocusApplyFailed`, `PolicyApplyFailed`, or `StackRejected` while leaving corrupted pause/input/focus state. The frozen contract requires any failed recovery to set `bInconsistent` and return `CompensationFailed`.

3. **The new recovery-failure test is too narrow to prove blocker 2.** It configures a persistent primary policy failure during Hub open and observes `CompensationFailed`; it does not inject a failure that occurs specifically during pause restoration, policy restoration after a later stage, or old-module focus restoration. Thus 10/10 is genuine but does not cover the remaining production branches.

### Required next step

1. Add a shared compensation helper/result aggregator for pause, policy, focus, Router snapshot, and ScreenStack snapshot restoration. Every rollback call site must either prove recovery or return `CompensationFailed` and mark inconsistent.
2. On Character/Inventory Back failure after Hub focus, restore focus to the still-owned old module before returning. Assert old active route, Stack depth, Widget/VM ownership, pause ownership, policy, and focus target.
3. Add targeted Automation for Back Router failure plus old-module focus restoration and at least two late-stage recovery failures (for example failed unpause recovery and failed old-policy/focus recovery).
4. Rerun Build/Automation/diff-check and request another Asset Gate review. User asset work remains stopped.

### Evidence and scope boundary

No new reflection/GC or allowlist violation was found. User-owned Content/Map/AI/.claude/learning changes remain isolated. Editor assets, persistence, PIE, both resolutions, physical controller, Standalone, Packaged, Shipping, and Dialogue competition remain `NOT VERIFIED`.

---

## Historical review — first post-REVISE re-review

Historical re-review status: `REVISE — RE-REVIEW / ASSET GATE NOT READY`

## Historical first re-review result

`REVISE`

The implementation has materially improved and closes most of the prior eight findings, but it is still **not ready for the user Asset Gate**. Three transaction defects remain directly visible in production code, and the nine reported passing tests do not cover them.

### Evidence level

#### Directly inspected

- Updated UIManager Hub/module open, cross-type replace, Back, X, travel teardown, failure injection, and snapshot restoration code.
- Updated PlayerController input-binding guard and reflected ownership.
- All nine `HSR.UI.FrontendNavigation` test bodies.
- Updated execution report, current diff, allowlist, and user-change isolation.

#### Report-level evidence only

- Development Editor Build reported `Succeeded` after UHT/compile/link/metadata.
- `HSR.UI.FrontendNavigation` reported 9/9 `Success`, process exit code 0.
- `git diff --check` reported exit code 0 with line-ending warnings only.

These commands were not rerun by this Reviewer because direct source inspection found blocking uncovered paths. A green rerun would not change the result until those paths are implemented and tested.

### Previous eight findings — disposition

1. Cross-type Character/Inventory/placeholder replacement: **substantially fixed** for successful transitions; old module ownership is released after candidate success.
2. Router/ScreenStack transaction support: **partially fixed** through snapshot restoration, but direct root shortcut remains split into two separately committed transactions.
3. Failure compensation: **partially fixed** for tested stages; direct module failure and compensation-failure handling remain incomplete.
4. Focus as a transaction stage: **fixed for open/replace and X**, but not for Character/Inventory Back.
5. X dedicated close-session: **fixed**; module ownership is retained until close stages succeed.
6. Legacy Character/Inventory bypass: **fixed**; public facades route through `OpenFrontendModule`, while internal helpers are private.
7. Input binding guard: **fixed at the active InputComponent identity level**; recreated components receive one new binding set.
8. Travel/context: **substantially fixed** for exact-root discard and production pending queries; the travel test covers route discard/new host/old host rejection, though pending-query automation remains absent.

### Remaining blocking findings

1. **Direct shortcut open is not one transaction.** `OpenFrontendModule` first commits `OpenPauseScreen()` and then separately creates the requested Character/Inventory/placeholder module. If the second stage returns `MissingWidgetClass`, `WidgetCreationFailed`, ViewModel initialization failure, attach/policy/focus failure, or Router rejection, the method returns while the newly opened Shell remains attached, the world remains paused, the Frontend token remains owned, and Router/ScreenStack remain at Hub. From ExplorationRoot, B/T/M/F4 is specified as one direct route attempt; a failed module candidate must restore exact root with no pause/input/focus leak. None of the nine tests exercises direct-root module-stage failure.

2. **Character/Inventory Back still ignores focus and Router failure after destructive mutation.** `CloseCharacterDetailScreen` and `CloseInventoryScreen` pop the Stack, apply policy, destroy Widget/VM, submit Router Back without checking its result, then log `FocusResult` and return `Success` even when focus is unavailable. This can leave Hub active with no valid focus, or Router/ScreenStack divergent with the old module already destroyed. Back needs the same old Stack/Route/policy/instance ownership transaction and compensation guarantees as X. The current failure test covers close-session failures, not module Back failures.

3. **Several compensation branches do not verify compensation success.** Hub focus/Router rollback, Character/Inventory focus/Router rollback, placeholder policy/focus/Router rollback, and X unpause/focus/Router rollback invoke restore operations such as pause restore, policy restore, or snapshot restore but ignore their return values. The frozen contract requires any failed compensation to set `Inconsistent` and return `CompensationFailed`; silently returning the original failure can certify a corrupted state. Existing one-shot failure tests verify primary failure recovery only and do not inject recovery failure.

### Required fixes and tests

1. Wrap root shortcut Shell + optional module in one outer transaction, or pre-create/validate the optional module before publishing Shell. On any module-stage failure restore exact ExplorationRoot, GameOnly/Exploration IMC/focus, no Frontend pause token, no Shell/module/VM, and closed Router history.
2. Rework Character and Inventory Back into reversible transactions. Check Router Back and focus results before releasing old module ownership; on failure restore old Stack, Route, policy/focus, Widget/VM, and pause session.
3. Centralize compensation result aggregation. Any failed pause/policy/focus/snapshot restoration must set `bInconsistent` and return `CompensationFailed`.
4. Add Automation for direct-root missing module class/create/VM/attach/policy/focus failure, Character and Inventory Back focus/Router failure, and at least one compensation-failure injection that asserts `Inconsistent`.
5. Rerun Build, all Frontend Automation, and diff-check, then request another independent Asset Gate review.

### Reflection, GC, scope, and evidence boundary

No new reflection or primary GC blocker was found. Frontend Router/Widgets, InputComponent binding guard, Widget classes, and module instances use reflected ownership appropriate to their lifetimes. Source changes remain within the frozen allowlist. User-owned Character/Enemy assets, map, `Content/AI/**`, `.claude/**`, and `learn/SaveSystem.md` remain excluded.

Editor assets, persistence, PIE, both resolutions, physical controller, Standalone, Packaged, Shipping, and Dialogue competition remain `NOT VERIFIED`; the user must not begin Asset Gate work yet.

---

## Historical review — first code-gate REVISE

Historical code-gate status: `REVISE — CODE GATE / ASSET GATE NOT READY`

## Historical first code-gate result

`REVISE`

The implementation is **not ready to enter the user Asset Gate**. The reported Development Editor Build success, five passing `HSR.UI.FrontendNavigation` tests, and `git diff --check` success establish only a report-level checkpoint. They do not override the directly inspected state-consistency and compensation defects below.

### Current evidence level

#### Directly inspected

- `tasks/active-task.md`, `tasks/execution-result.md`, and `tasks/review-template.md`.
- Actual changed P17-005 Source files, including Frontend DTO/Router/Widgets, UIManager, HUD, PlayerController, and the five Frontend Automation tests.
- Current `git status --short`, diff summary, allowlist alignment, production references, and relevant UIManager transaction/travel code.

#### Report-level evidence only

- Development Editor Build reported `Succeeded`, including UHT, compile, and link.
- `HSR.UI.FrontendNavigation` reported 5/5 tests `Success` and process exit code 0.
- `git diff --check` reported exit code 0 with line-ending warnings only.

The Reviewer did not independently rerun these commands. More importantly, the current five tests do not exercise the blocking production transitions and controlled failures identified below.

#### Not verified

- User Input Action, Mapping Context, Shell WBP, and ModuleRoot WBP creation or persistence.
- Editor reopen, PIE keyboard/mouse/focus, Back/X, input restoration, both target resolutions, physical controller, Standalone, Packaged, and Shipping.
- Dialogue Overlay competition remains `NOT VERIFIED / P17-012`.

### Blocking findings

1. **Cross-type module changes are not a unified atomic replace transaction.** The placeholder path decides Push versus Replace only from `FrontendModuleRootInstance`. If Character or Inventory is active, opening Party/Map/Challenge/Save pushes another Stack entry while leaving the old Widget/ViewModel alive, then changes the Router to the placeholder. The reverse placeholder-to-Character/Inventory path fails the old one/two-entry preflight and marks the manager inconsistent. This breaks the fixed three-level state shape, Router/ScreenStack correspondence, and old-module preservation on candidate failure.

2. **Router and ScreenStack are not atomically published.** Hub, Character, Inventory, and placeholder flows mutate the live ScreenStack, viewport, policy/pause, or instance ownership before submitting the Router request. Router failure is handled by returning or marking `Inconsistent`, not by restoring the prior complete state. No joint candidate snapshots or single publish point exist in the inspected implementation.

3. **Several failure paths leave externally visible half-state.** Placeholder attach failure occurs after live ScreenStack Push/Replace and has no Stack compensation. `CloseFrontendToRoot` mutates the Stack before applying close policy/unpause, then returns directly if either step fails. Hub Router failure can retain an attached, paused Shell. These paths violate the frozen reverse-compensation matrix.

4. **Focus failure is treated as success.** Hub, Character, Inventory, placeholder, Back, and close flows log or ignore `FocusResult` and still return `Success`. This contradicts the contract requiring focus failure to produce a controlled failure and compensation. The Automation focus-failure switch therefore does not validate the required behavior.

5. **X is not a dedicated atomic close-session transaction.** With Character or Inventory active, `CloseFrontendToRoot` first calls `RequestBack()` and destroys the module, then performs the session close. A later policy/unpause/close failure cannot restore the old module and session, recreating the explicitly forbidden half-close sequence.

6. **Legacy Character/Inventory entry points can bypass the Frontend session.** `OpenCharacterDetailScreen()` and `OpenInventoryScreen()` remain public Blueprint-callable entry points and still accept exact-root opening without a Shell or Frontend pause token. They must become compatibility facades to `OpenFrontendModule`, with non-Blueprint internal transaction helpers for actual candidate work. Travel must not use these helpers to restore a Frontend route.

7. **Enhanced Input binding is not guaranteed exactly once.** `SetupInputComponent()` binds all six actions whenever it runs. The IMC has an idempotent-added flag, but bindings have no corresponding guard or cleanup for InputComponent recreation/repeated setup. Missing-action warnings do not prove the no-duplicate-callback contract.

8. **Travel/context integration and evidence are incomplete.** Frontend open preflight does not demonstrate the frozen Map/Battle transition-pending rejection. Travel teardown can still fall through forced iterative cleanup after a Frontend close failure, and none of the five reported tests covers Frontend exact-root route discard, new-host recovery, old host/route tokens, or stale callbacks.

### Test-truthfulness finding

Three of the five tests exercise only the pure-value Router. `SharedSession` covers direct Inventory, Back, and X; `PlaceholderReplace` covers only placeholder-to-placeholder replacement. There is no Character/Inventory/placeholder cross-type replacement, missing/create/attach/policy/pause/focus/close compensation, external pause, travel/stale callback, or repeated-input-binding coverage. Because production focus failures are ignored, the existing test suite can report green while required transaction behavior is incorrect.

### Reflection, GC, scope, and existing-change isolation

The newly owned UObject, Widget, and Widget-class references inspected in UIManager/HUD/PlayerController use reflected `UPROPERTY`, `TObjectPtr`, or `TSubclassOf` storage; no primary reflection or GC blocker was found. P17-005 Source edits remain within the frozen Source allowlist.

Pre-existing or user-owned changes outside P17-005—including Character/Enemy Blueprints, Enemy DataAsset, the exploration map, `Content/AI/**`, `.claude/**`, and `learn/SaveSystem.md`—remain excluded and must not be modified, staged, committed, reset, cleaned, deleted, or attributed to this implementation.

### Required next step

1. Return the task to the authorized Implementation role; do not ask the user to create assets yet.
2. Implement one UIManager-owned transaction core for all Hub/module open, cross-type replace, Back, and X operations, including candidate/old-state ownership and reversible Router/ScreenStack publication.
3. Convert public Character/Inventory entry points to Frontend facades and move their actual candidate lifecycle into non-Blueprint internal helpers.
4. Treat focus as a real transaction stage and compensate every attach/policy/pause/focus/Router/ScreenStack/close failure.
5. Make InputComponent binding exactly once per active component or explicitly unbind/rebind on recreation.
6. Complete travel-pending rejection and exact-root/stale-host/stale-route tests.
7. Add truthful Automation for all cross-type transitions and controlled failures, rerun Build/Automation/diff checks, and request a new independent code-gate review. Only a later `ASSET GATE READY` review may hand work to the user.

---

## Historical review — corrected contract gate

Historical contract review status: `PASS — CONTRACT GATE ONLY`

## Historical contract-gate result

`PASS`

This PASS authorizes the frozen P17-005 contract to return to the Implementation role. It is not an implementation, Build, Automation, PIE, Editor-asset, or task-completion PASS.

The corrected task card now freezes:

- The only public state shape as `ExplorationRoot -> FrontendShell(PauseHub) -> optional Module`, with Router history and Frontend ScreenStack entries required to remain one-to-one.
- UIManager as the sole owner/coordinator of Router, ScreenStack, Shell/Module/Widget/ViewModel lifecycle, pause token, input mode, and focus.
- Candidate-first state construction, joint Router/ScreenStack validation, a single publish point, reverse compensation, and `Inconsistent` only when compensation fails.
- A dedicated close-session transaction for Hub Back and X; iterative `RequestBack()` is explicitly forbidden.
- One Frontend-owned pause token per session, no second pause on module navigation, and preservation of external pause ownership.
- Frontend travel teardown to exact root with route discard, no Frontend descriptor restoration, and rejection of stale host/route tokens and callbacks.
- Six Digital Input Actions, no IA/IMC Trigger or Modifier, `bTriggerWhenPaused=true`, and one C++ `ETriggerEvent::Started` binding per action.
- Session module changes as one atomic replace operation. History remains `[Hub, ActiveModule]`; candidate failure preserves the prior module; repeating the active module is a token-consuming NoOp.
- Router attempt-token allocation only after UIManager host/context/external-pause/travel prechecks. A constructed, structurally valid request consumes its token for Success, NoOp, or later business rejection; zero/negative or structurally invalid requests do not consume; reuse of the same positive token with any payload is `AlreadyProcessed`.
- Dialogue Overlay competition as explicit `NOT VERIFIED / P17-012`, outside the P17-005 mandatory acceptance contract.
- Legacy `OpenPauseScreen()` and PlayerController pause calls as compatibility facades that only forward to the Frontend Hub. The old Pause widget class/instance path must be removed rather than retained as a second implementation.
- Exact Source and user Editor-asset allowlists, controlled missing-asset behavior, required Automation failure coverage, and truthful separation of Build, Automation, PIE, reopen, resolution, controller, Standalone, Packaged, and Shipping evidence.

## Historical contract-gate evidence and boundaries

### Directly inspected for the contract PASS

- The final corrected `tasks/active-task.md` contract.
- Closure of the three prior remaining contract findings: Dialogue acceptance conflict, ambiguous module-switch strategy, and attempt-token allocation/consumption boundary.
- The added legacy Pause facade migration and the single `Started` input-binding contract.

### Not established by this PASS

- No corrected implementation was reviewed.
- No build, UHT, compile, link, Automation, PIE, Editor reopen, asset persistence, resolution, or physical-controller verification was performed in the final contract review.
- Pause ownership, Router/ScreenStack atomicity, compensation, travel teardown, stale callback rejection, input binding, Character/Inventory integration, and placeholder module behavior remain implementation obligations to be proven by the task evidence matrix.

## Contract-gate blockers at that time

There are no blockers at the contract gate. Implementation must stop and request revision or expanded authorization if the frozen transaction cannot be implemented within the listed Source allowlist, if Config/Gameplay authority/new dependencies become necessary, or if user Editor assets are required before the code/Automation gate can safely continue.

## Historical contract-gate next step

1. Coordinator records the contract-gate PASS and returns the same authorized task to the Implementation role.
2. Implementation follows the frozen order and writes only `tasks/execution-result.md` as its execution report.
3. Input and WBP creation stops at the user Asset Gate; the user supplies Save All, reopen, PIE, and resolution evidence.
4. Independent Reviewer performs a new implementation/evidence review after the code, Build, Automation, scope check, and available user evidence are complete. That later review must issue and persist its own result; this contract PASS cannot be reused as task-completion approval.

## Historical contract-gate existing-change isolation

Pre-existing or user-owned changes outside P17-005—including Character/Enemy Blueprints, Enemy DataAsset, the exploration map, `Content/AI/**`, `.claude/**`, and `learn/SaveSystem.md`—remain excluded. This PASS does not authorize modifying, staging, committing, deleting, cleaning, resetting, or attributing them to P17-005.

---

## Historical review — initial Router-core checkpoint

Historical review status: `REVISE`

Historical review scope: the initial `tasks/active-task.md`, `docs/phase-17-execution-plan.md`, existing P17 UI implementation, current working-tree diff, and the reported Router-core verification at that time.

## Evidence level

### Directly inspected

- The frozen Source/Content allowlist and task acceptance criteria in `tasks/active-task.md`.
- The Phase 17 ownership, navigation, failure-matrix, travel, evidence, and Editor handoff requirements in `docs/phase-17-execution-plan.md`.
- Current Frontend Router/DTO/Shell source, UIManager and ScreenWidget diff, and `HSRFrontendNavigationTests.cpp`.
- Current `git status --short`, `git diff --name-status`, targeted source diff, and production-reference search.

### Report-level evidence only

- Development Editor build reported `Succeeded` with UHT/compile/link completion.
- `HSR.UI.FrontendNavigation` reported two tests found and both `Success`.
- `git diff --check` reported exit code 0.

These results were not independently rerun by this Reviewer. The two Automation tests exercise only the pure-value Router and do not constitute UIManager, ScreenStack, pause, input, travel, or PIE integration evidence.

### Not verified

- Frontend pause ownership and exactly-once release.
- Router + ScreenStack + Widget + input/focus atomicity and compensation.
- Enhanced Input asset persistence, one-time binding, UIOnly Back/X, and context rejection.
- Travel teardown/rebuild, host generation, stale callback, and route restoration.
- Missing class/create/attach/policy/pause/focus controlled failures.
- PIE happy path and failure path, 1920x1080, 1280x720, Editor reopen, physical controller, Standalone, Packaged, or Shipping behavior.

## Blocking findings

1. **Reviewer gate occurred after implementation began.** The Router core and tests existed before this Independent Review. This does not satisfy the required pre-implementation review chain. The current result must remain recorded as `REVISE`; the Coordinator must revise/freeze the contract and route it through review before further integration work.

2. **Router and ScreenStack are not integrated or atomic.** Repository references show `UHSRFrontendRouter` is used only by its two Automation tests. `UHSRUIManagerSubsystem::OpenFrontendModule` directly calls the legacy Pause/Character/Inventory open functions, while the Shell has no production path that publishes Router snapshots. No transaction currently keeps Router history, ScreenStack entries, widget ownership, input policy, focus, and pause ownership consistent.

3. **`CloseFrontendToRoot` does not implement the frozen X contract.** It only calls `RequestBack()` while the logical ScreenStack count is greater than one. It does not reset/commit Router history, explicitly close all Frontend candidates and ViewModels as one operation, or prove exactly-once pause release and Exploration input restoration. Its result also reflects the last pop rather than a unified close transaction.

4. **Unified Frontend pause ownership is absent.** Existing Pause, Character, and Inventory paths retain their older independent open/close behavior. There is no demonstrated single Frontend pause session spanning Hub-to-module navigation, no proof that the final close releases only the Frontend-owned token, and no integration coverage for external pause preservation or repeated/competing input.

5. **The task card does not freeze a sufficient failure/compensation matrix.** “Failure complete compensation” is stated but the card does not specify the candidate-first commit order, Router/ScreenStack joint commit point, reverse compensation order for create/attach/stack/policy/pause/focus failures, or the condition that marks UIManager inconsistent. The travel decision—restore a stable Frontend route descriptor or return to root—is also not frozen precisely.

6. **Input lifecycle is not implemented or evidenced.** No production reference currently connects the proposed Frontend Input Action/Mapping Context to PlayerController or UIManager. Constant IMC ownership, single binding, Back/X while paused, Battle/Dialogue/Travel/no-host rejection, repeated actions, and same-frame contention remain unverified.

7. **Travel and stale-state handling are not integrated.** Frontend Router state is not part of the existing P17-004 teardown/rebuild contract. No Automation covers host generation, stale route tokens/callbacks, arrival ordering, or safe restoration/root fallback.

8. **The evidence matrix is incomplete for the claimed integration direction.** Build, two pure Router tests, and diff-check support only a Router-core checkpoint. They do not support an integration-ready or task-complete conclusion.

## Scope and existing-change isolation

The inspected P17-005 Source additions/edits are inside the frozen Source allowlist. The working tree also contains pre-existing or user-owned changes outside this task, including Character/Enemy Blueprints, Enemy DataAsset, exploration map, `Content/AI/**`, `.claude/**`, and `learn/SaveSystem.md`. This review does not attribute those changes to P17-005. They must remain unstaged, unmodified, and excluded from this task unless the user separately classifies and authorizes them. No reset, clean, overwrite, deletion, stage, commit, or push is authorized by this review.

## Required next step

1. Coordinator records this `REVISE` in the active workflow and revises `tasks/active-task.md` before further implementation.
2. Freeze an explicit Frontend transaction matrix: validation/candidate creation, Router and ScreenStack candidate state, attach, input policy, pause, focus, publish point, reverse compensation, and inconsistent-state behavior.
3. Freeze the travel contract and stable descriptor: teardown behavior, route restore versus root fallback, host generation, stale token/callback rejection, and arrival ordering.
4. Make UIManager the sole coordinator of Router, ScreenStack, Widget/ViewModel, pause, input mode, and focus ownership. Do not expose independent Blueprint paths that can bypass the transaction.
5. Add integration Automation for pause ownership, Router/ScreenStack consistency, each controlled failure stage, Back/X, duplicate/same-frame input, teardown/rebuild, and stale callbacks before claiming an integration gate.
6. Stop at the user Asset Gate for Input and WBP work. The user must create/save/reopen the allowlisted assets and provide PIE evidence at both required resolutions; unavailable controller/Standalone/Packaged evidence must remain `NOT VERIFIED`.

The task may automatically return to the authorized Implementation role after the Coordinator freezes the revised contract and the pre-implementation review gate is satisfied. No new Gameplay authority, Config, external dependency, module, or out-of-allowlist file is authorized.
