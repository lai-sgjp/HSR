# TASK-P17-DIALOGUE-004 — 生命周期、旅行恢复、Editor/PIE 收口
Status: COMPLETE / GATE 0 PASS / TDD RED CONFIRMED / GREEN PASS / FOCUSED AUTOMATION PASS / EDITOR-PIE PASS / USER ACCEPTED

This is the unambiguous implementation identity for the Phase 17 execution-plan
meaning of P17-012. It does not reuse the archived `TASK-P17-012` dynamic-mount
task. User authorization was supplied on 2026-08-09.

## Sole observable outcome

Pressing `F` on a valid Dialogue interactable opens one event-driven Dialogue
Overlay above the Exploration HUD. The Overlay is outside the Pause Hub
Router, receives UIOnly input and deterministic initial focus, submits stable
Dialogue choices through the existing Presentation ViewModel, and closes on
Escape/Gamepad Back/X, terminal choice or explicit close. A failed create,
attach, policy, focus, begin, choice or close operation preserves the last
complete owned state and does not leave a second/stale Overlay instance.

Authorized map travel tears down the Overlay exactly once. It is not restored
across a map boundary; the new HUD host starts at a clean Exploration Root with
GameOnly input and no stale Dialogue query.

## Gate 0 decisions

1. `UHSRInteractionComponent` remains the sole owner of the current Candidate
   and `F` interaction. `AHSRDialogueInteractable` returns a typed
   Dialogue-start payload in the existing interaction result; HUD does not
   infer DialogueId from an Actor or prompt text.
2. `UHSRUIManagerSubsystem` owns the Overlay session, input policy, focus,
   attach/close compensation and travel teardown. The Overlay never enters
   `UHSRScreenStack` or `UHSRFrontendRouter` and therefore does not pause the
   world through the Frontend session.
3. Overlay input uses the already-authorized controller routes: Escape /
   Gamepad Back and X close Dialogue first; while Dialogue is active, frontend
   module opens are rejected. No new InputAction, MappingContext or Config is
   introduced in this package.
4. The Overlay Widget is a pure projection/intent boundary. It exposes
   bounds-safe snapshot/choice accessors and delegates SubmitChoice/Close to
   the Presentation ViewModel/UIManager. It never calls Quest, Reward,
   Encounter, Inventory, SaveGame or AddToViewport itself.
5. A terminal choice closes the Overlay only after the Presentation ViewModel
   successfully publishes its closed snapshot. Invalid/stale/Authority-failed
   choices leave the complete previous snapshot visible.
6. Travel teardown releases the Overlay and its ViewModel without restoring the
   old query. A newly registered host must not receive a stale Dialogue widget.

## TDD gates

- Gate 0: complete; ownership, payload, route/input/focus, compensation and
  travel boundaries are frozen here.
- RED: confirmed; before production implementation the real UE5.6 UBT reached
  the intended missing `HSRDialogueOverlayWidget.h` include in the new lifecycle
  test.
- GREEN: confirmed; Development Editor Build succeeded and the focused Overlay,
  Dialogue Authority, Dialogue Presentation and QuestDialogue Automation suites
  all passed. Evidence is recorded in
  `docs/testing/TASK-P17-DIALOGUE-004.tdd.md`.
- Editor/PIE: complete by user. WBP/DataAsset/map actor/HUD binding persisted
  after Save All and Editor reopen; user confirmed normal visual/interaction
  behavior, including choice progression, Escape/Gamepad Back/X/CloseButton,
  frontend blocking and travel teardown without stale restoration.

## Code-gate evidence

- Build: `Build.bat HSREditor Win64 Development -Project=E:\\work\\unreal_projects\\HSR\\HSR.uproject -WaitMutex -NoHotReload` — `Result: Succeeded`.
- Automation: `HSR.Dialogue.Overlay` 4/4, `HSR.Dialogue.Authority` 5/5,
  `HSR.Dialogue.Presentation` 4/4 and `HSR.QuestDialogue` 1/1; every discovered
  test returned `Result={Success}` and each command exited 0.
- `git diff --check`: passed; only the repository's known LF/CRLF conversion
  warnings were printed.

## Editor/PIE handoff (completed by user)

1. Reopen `E:\\work\\unreal_projects\\HSR\\HSR.uproject` after the code build
   finishes. In `/Game/UI/P17/Dialogue`, create `WBP_DialogueOverlay_P17` as a
   User Widget and set its Parent Class to `UHSRDialogueOverlayWidget` in
   Class Settings. Save and compile the Blueprint.
2. Build a simple full-screen overlay: a panel with Speaker, Body and a
   vertical group of focusable choice Buttons, plus a Close Button. In the
   `OnDialogueSnapshotChanged` event, break the snapshot, copy SpeakerText and
   BodyText to the labels, use `GetChoiceCount` and bounds-safe `GetChoiceAt`
   for each button slot, set its display text, and hide slots that are absent.
   Each choice button calls `SubmitChoiceByIndex` with its fixed slot index;
   Close calls `RequestCloseDialogue`. Implement `GetPreferredFocusWidget` to
   return the always-visible Close Button (or the first choice Button when it
   is guaranteed to exist). Do not call `AddToViewport`, Router, Quest,
   Reward, Encounter or Inventory from the Widget graph.
3. Create a `UHSRDialogueDefinition` Data Asset, for example
   `/Game/Data/Dialogue/DA_Dialogue_P17_Demo`. Set `DialogueId` to
   `Dialogue.P17.Demo`, `StartNodeId` to `Node.Start`, author non-empty
   SpeakerText/Body Text and DisplayText values, and add a second `Node.End`.
   Give Start one `None` branch choice targeting `Node.End`; give End one
   `None` branch choice with an empty TargetNodeId to exercise terminal close.
   Keep Quest/Reward/Encounter fields empty for this first lifecycle pass.
4. Place an `AHSRDialogueInteractable` in the exploration map. Set its
   `DialogueId` to the exact same stable name and assign the same
   `DialogueDefinition`; leave `QuestDefinition` empty for the no-authority
   branch demo. Save the level. The actor registers the definition at BeginPlay;
   do not add a second registration path.
5. Open `BP_HSRHUD` Class Defaults, set `HUD|P17 -> Dialogue Overlay Widget
   Class` to `WBP_DialogueOverlay_P17`, then Compile and Save. Use the existing
   `FrontendNavigationMappingContext`; do not create an InputAction or Mapping
   Context for Dialogue. Save All, close the Editor completely, and reopen it
   to verify the class/asset references persist.
6. PIE the exploration map and walk into the interactable. Confirm the `Talk`
   prompt, press `F`, verify one overlay and deterministic focus, select a
   choice to update the next node, and select the terminal choice. Also verify
   Escape, Gamepad Back and `X` close the overlay, and that `B`/frontend-module
   input is rejected while Dialogue is active.
7. With Dialogue open, perform one authorized map travel. Confirm the overlay
   is torn down once, the arrival map starts at a clean Exploration Root with
   GameOnly input, and the old query is not restored. Check Output Log for no
   Blueprint Runtime Error, Ensure or array-index error. Record the first
   concrete failure/log if any; otherwise report the visual and interaction
   result for final closeout.

## User acceptance evidence (2026-08-09)

- User created and saved `DA_Dialogue_P17_Demo` with the authored Start/End
  nodes and terminal `TargetNodeId=None` choice.
- User created and saved `WBP_DialogueOverlay_P17`, reparented it to
  `UHSRDialogueOverlayWidget`, authored the full-screen layout and persisted the
  four choice/close Blueprint event bindings.
- `BP_HSRHUD` persists `DialogueOverlayWidgetClass`; the existing Frontend
  Navigation mapping was reused without new InputAction, MappingContext or
  Config. `AHSRDialogueInteractable` is placed in
  `Map_Exploration_P15_A` at `(0,300,92)` with the matching Dialogue ID and
  definition.
- The interactable visibility/range issue was resolved with a visible sphere
  marker and interaction radius 260; PIE observed the Talk prompt at distance
  140, then successful typed payload and Overlay open (`FocusResult=1`, UI
  result 0). Frontend opening while active returned `AlreadyOpen` (result 12).
- User confirmed the final PIE interaction is normal: choice progression,
  Escape/Gamepad Back/X/CloseButton close, and authorized travel teardown with
  no stale Dialogue restoration. Output Log contained no Blueprint Runtime
  Error, Ensure or array-bounds error.

## Allowed files

Production files:

- `Source/HSR/Interaction/HSRInteractionTypes.h`
- `Source/HSR/Dialogue/HSRDialogueInteractable.h`
- `Source/HSR/Dialogue/HSRDialogueInteractable.cpp`
- `Source/HSR/UI/Dialogue/HSRDialogueOverlayWidget.h`
- `Source/HSR/UI/Dialogue/HSRDialogueOverlayWidget.cpp`
- `Source/HSR/UI/HSRHUD.h`
- `Source/HSR/UI/HSRHUD.cpp`
- `Source/HSR/UI/HSRUIManagerSubsystem.h`
- `Source/HSR/UI/HSRUIManagerSubsystem.cpp`
- `Source/HSR/Player/HSRPlayerController.cpp`

Test and evidence files:

- `Source/HSR/Tests/HSRDialogueOverlayLifecycleTests.cpp`
- `tasks/active-task.md`
- `tasks/execution-result.md`
- `docs/testing/TASK-P17-DIALOGUE-004.tdd.md`
- `PROJECT_STATE.md`
- `todo_plan.md`
- `worklog.md`

## Explicit prohibitions

- Do not add Dialogue to the Pause Hub Router or create a second viewport path.
- Do not modify Quest, Reward, Encounter/BattleTransition, Inventory,
  Interaction candidate selection, Save, Config, Build.cs or `.uproject`.
- Do not create or edit UAsset/UMG/Input assets; those are user Editor work.
- Do not restore a stale Dialogue query across travel and do not claim PIE before
  the user completes the Editor handoff.
- Do not stage, commit, push, reset, clean, delete or rewrite Git history.

## Verification boundary

The required code result is a real RED/GREEN lifecycle gate plus a detailed
Editor/PIE handoff. Visual layout, WBP bindings, Editor Save All/reopen and
manual PIE remain user-owned acceptance boundaries. Dialogue-004 is not complete
until the user confirms those steps.

---

# TASK-P17-DIALOGUE-003 — Authority 分支 exactly-once 转发
Status: COMPLETE / GATE 0 PASS / TDD RED CONFIRMED / GREEN PASS / FOCUSED AUTOMATION PASS

This is the unambiguous implementation identity for the Phase 17 execution-plan
meaning of P17-012. It does not reuse the archived `TASK-P17-012` dynamic-mount
task. User authorization was supplied on 2026-08-09.

## Sole observable outcome

Dialogue choice selection submits Quest, Reward and Encounter branches exactly
once through their existing Authorities. A stable `BranchOperationId` is the
idempotency identity for every explicit Authority branch. Repeated submission
of the same branch returns `NoOp`; reusing the same operation ID for a different
Dialogue/Node/Choice returns `OperationIdConflict`; failed Authority calls do
not enter the Dialogue branch ledger. Presentation failures preserve the last
complete snapshot and retain the raw branch result.

## Gate 0 decisions

1. The branch contract explicitly supports `None`, `Quest`, `Encounter` and
   `Reward`; a non-`None` branch must carry a valid stable `BranchOperationId`.
2. Quest submits its authored event through Quest Authority. Reward submits an
   `FHSRRewardRequest` through Reward Authority with the operation ID as
   `ClaimId`. Encounter submits the complete authored `FHSREncounterRequest`
   through BattleTransition Authority with the operation ID as `RequestId`.
   The Dialogue layer never infers enemy, map or return context from an ID.
3. The Presentation ViewModel calls the Dialogue Authority seam. Widgets do not
   call Quest/Reward/Encounter/Inventory authorities directly and do not fake a
   branch success with presentation state.
4. Authority-unavailable, invalid-definition, conflict and Authority rejection
   results remain typed; original subsystem result values are retained and
   rejected Reward/Encounter branches are logged with their raw result.
5. The legacy three-`FName` `SelectChoice` overload remains available for
   existing callers and delegates explicit branches through the new contract.

## TDD gates

- Gate 0: complete; authority ownership, stable operation identity, full
  Encounter request, ledger semantics and snapshot-preservation rules are
  frozen.
- RED: complete; `HSRDialogueAuthorityTests.cpp` reached the intended missing
  branch contract/API compiler failures before production implementation.
- GREEN: complete; the allowlisted Dialogue Authority and Presentation files
  were implemented and the focused Automation suites passed.
- Refactor/verify: complete for the code gate. Development Editor Build,
  focused Dialogue Automation and `git diff --check` passed. Overlay, Editor
  asset wiring, PIE, travel teardown and Dialogue-004 are not claimed.

## Allowed files

Production files:

- `Source/HSR/Dialogue/HSRDialogueTypes.h`
- `Source/HSR/Dialogue/HSRDialogueSubsystem.h`
- `Source/HSR/Dialogue/HSRDialogueSubsystem.cpp`
- `Source/HSR/UI/Dialogue/HSRDialoguePresentationViewModel.h`
- `Source/HSR/UI/Dialogue/HSRDialoguePresentationViewModel.cpp`

Test and evidence files:

- `Source/HSR/Tests/HSRDialogueAuthorityTests.cpp`
- `tasks/active-task.md`
- `tasks/execution-result.md`
- `docs/testing/TASK-P17-DIALOGUE-003.tdd.md`
- `PROJECT_STATE.md`
- `todo_plan.md`
- `worklog.md`

## Explicit prohibitions

- Do not modify Quest, Reward, Inventory, Encounter/BattleTransition Authority
  implementations, InteractionComponent, HUD, Router, UAsset, Config,
  Build.cs, `.uproject`, generated files or `.claude/**`.
- Do not infer an Encounter request, use a UI-side transaction, or collapse raw
  Authority result codes into an indistinguishable success/failure value.
- Do not implement Dialogue Overlay input/focus/travel lifecycle or start
  Dialogue-004 without separate authorization.
- Do not stage, commit, push, reset, clean, delete or rewrite Git history.

## Verification boundary

Dialogue-003 is complete at the code/Automation gate. The next separately
authorized package is `TASK-P17-DIALOGUE-004`, covering Overlay/HUD/input,
focus, travel teardown/arrival restore, Editor asset integration and PIE.

---

# TASK-P17-DIALOGUE-001 — Gate 0、Interaction/Dialogue 合同与 TDD RED
Status: COMPLETE / GATE 0 PASS / TDD RED CONFIRMED / GREEN NOT STARTED

This is the current task record. It maps to the Phase 17 execution-plan meaning
of P17-012 and does not reuse the archived dynamic-mount `TASK-P17-012`.
Inventory-001～004 remain historical completed work below. User authorization
was supplied on 2026-08-09.

The Gate 0 audit freezes InteractionComponent ownership of `F` and the current
candidate, DialogueSubsystem ownership of definition/choice progression, an
Exploration Overlay outside the Pause Hub Router, pure-value active-query and
stable Node/Choice IDs, and failure preservation. The missing typed start
payload and missing speaker/choice display data are explicit future API/data
decisions, not UI inference points.

The RED test is `Source/HSR/Tests/HSRDialoguePresentationTests.cpp`. No
production Dialogue/UI/Authority/UAsset/Config file is modified. The first
sandboxed UBT attempt was blocked by the known UnrealBuildTool cache permission;
the controlled retry while the Editor was open was blocked by Live Coding. After
the user fully exited the Editor, the real compile reached the intended RED at
the missing `HSRDialoguePresentationTypes.h` include. The same build also
reported an unrelated `NameLess` duplicate-definition failure between
`HSRSaveVersion.cpp` and `HSRChallengeProgressionSubsystem.cpp`; that dirty-
worktree issue is outside this package and was not changed. No Git
stage/commit/push was performed.

---

# TASK-P17-INVENTORY-004 — Editor 集成、PIE 与收口
Status: COMPLETE / CODE + EDITOR + CORE PIE COMPLETE / USER ACCEPTED

This is the current task record. The detailed 004 contract is recorded in the
004 section below; the preceding Inventory-003 block is historical context only.
The user supplied Editor integration and core PIE evidence, then confirmed the
manual visual/interaction experience is normal. This task is closed; no next
task is created automatically.

---

# TASK-P17-INVENTORY-003 — 合法命令与失败保持

Status: `COMPLETE / GATE 0 PASS / TDD RED CONFIRMED / GREEN PASS / FOCUSED AUTOMATION PASS`

## Scope identity

This is the unambiguous implementation identity for the Phase 17 execution
plan meaning of P17-008. It is not `TASK-P17-008`, and it must not reuse the
archived meanings of `TASK-P17-006` (Quest Frontend) or `TASK-P17-012` (five-
module dynamic mounting).

User authorization: the user's `授权` response to the immediately preceding
Inventory-002 handoff is interpreted as authorization for the next explicitly
named package, `TASK-P17-INVENTORY-003`, on 2026-08-09.

## Sole observable outcome

The Inventory ViewModel can submit Equip and Enhance only through the existing
Equipment Authority, carrying the selected stable unique `InstanceId`, the real
Inventory/Equipment revisions, the selected character, and a fresh operation
ID. Use and Disassemble remain typed unavailable because no corresponding
Authority exists. Authority rejection, revision conflict, missing mapping or
definition, empty candidate/option data, and duplicate-operation rejection do
not mutate Inventory/Equipment and do not replace the last complete UI
snapshot with an empty failure stage.

This package does not wire B/Back/X, UIManager, Frontend route mounting,
`ModuleContentHost`, UAsset assets, travel teardown, or PIE. Those remain
Inventory-004 and user Editor work.

## Frozen contracts inherited from Inventory-002

1. `UHSRInventorySubsystem` remains the authority for stack/unique possession,
   capacity, and Inventory revision/publication. `UHSREquipmentSubsystem`
   remains the authority for equipment ownership, placement, and enhancement.
2. Inventory rows use `ItemId + invalid InstanceId` for stacks and
   `ItemId + committed InstanceId` for unique items. Array indices never become
   command identity.
3. The ViewModel reads committed snapshots and delegates; the Widget forwards
   intent and reads pure values. Neither may call `RemoveStack`, mutate
   SaveGame, apply GameplayEffects, or own viewport state.
4. Every failed command preserves the complete committed snapshot and emits no
   synthetic empty stage. A successful Authority publication is the only source
   of the subsequent rebuilt snapshot.

## Implementation contract

- Extend the Inventory command seam with an explicit Equipment command context:
  `UHSREquipmentSubsystem`, `UHSRItemEquipmentMappingCatalog`,
  `UHSREquipmentEnhancementCatalog`, and a valid `CharacterId`.
- Equip resolves the selected unique row through the mapping catalog, chooses
  the Authority-supported Equip/Replace intent, and submits a
  `FHSREquipmentMovementRequest` with the snapshot's real Inventory revision,
  current Equipment revision, stable `InstanceId`, and a new `OperationId`.
- Enhance accepts an explicit target level and submits a
  `FHSREquipmentEnhancementRequest` only when the selected stable instance and
  command context are valid. It never removes material directly.
- Use and Disassemble return `AuthorityUnavailable` without calling Inventory.
  Missing command context is also typed unavailable; mapping/catalog failures,
  Authority rejection, stale revisions, and invalid target levels remain
  distinguishable at the ViewModel boundary. Raw subsystem result codes must be
  retained in logs when mapped.
- Subscribe to the Equipment loadout delegate for the configured character and
  include its revision in the pure snapshot. Keep all Blueprint accessors
  bounds-safe.

## TDD gates

1. Gate 0: complete. The Authority ownership, command context, stable IDs,
   revision contract, failure matrix, and allowlist are frozen here.
2. RED: complete. New command tests were compiled and reached the intended
   missing command seam/compiler failures before production changes.
3. GREEN: complete. The allowlisted Inventory UI/types/test files were
   implemented and the same focused Automation target passed all discovered
   tests.
4. Refactor and verify: complete for the code gate. Development Build,
   focused Inventory UI Automation, `git diff --check`, and exact allowlist
   audit passed. Do not claim PIE.

## Allowed files

Production files:

- `Source/HSR/UI/Inventory/HSRInventoryTypes.h`
- `Source/HSR/UI/Inventory/HSRInventoryViewModel.h`
- `Source/HSR/UI/Inventory/HSRInventoryViewModel.cpp`
- `Source/HSR/UI/Inventory/HSRInventoryModuleWidget.h`
- `Source/HSR/UI/Inventory/HSRInventoryModuleWidget.cpp`

Test and evidence files:

- `Source/HSR/Tests/HSRInventoryViewModelTests.cpp`
- `tasks/active-task.md`
- `tasks/execution-result.md`
- `docs/testing/TASK-P17-INVENTORY-003.tdd.md`
- `PROJECT_STATE.md`
- `worklog.md`
- `todo_plan.md` (only for real progress)
- `tasks/archive/TASK-P17-INVENTORY-002-active-task.md`
- `tasks/archive/TASK-P17-INVENTORY-002-execution-result.md`

Existing Authority, catalog, mapping, Save, Router, UIManager, HUD, Content,
Config, Build.cs, `.uproject`, plugin, generated, and `.claude/**` files are
outside this package.

## Explicit prohibitions

- Do not modify Inventory or Equipment Authority implementation.
- Do not call `RemoveStack` as a substitute for Use/Disassemble or Enhance.
- Do not create or edit UAsset, Config, Build.cs, `.uproject`, plugins, or
  generated/build outputs.
- Do not wire B/Back/X, UIManager, Router, `ModuleContentHost`, travel, or PIE.
- Do not stage, commit, push, reset, clean, delete, or rewrite Git history.

## User Editor work

None in 003. Mapping/enhancement catalog asset assignment and Inventory UMG
integration remain deferred to 004 after this code/Automation gate.

## Verification evidence

- RED: the real UBT run failed at the intended missing command seam with
  `SetCommandContext is not a member`, `SubmitAction does not take 2
  arguments`, and `AuthorityRejected undeclared`.
- GREEN: `HSREditor Win64 Development` returned `Result: Succeeded`.
- Focused Automation found 7 tests under `HSR.UI.Inventory`; all 7 completed
  with `Result={Success}`, including the Inventory-002 and Reward lifecycle
  regressions.
- `git diff --check` passed. No UAsset, PIE, Git, or `.claude/**` operation was
  performed.

The existing Equipment Authority only enhances instances it owns. Inventory
bag rows that are not Authority-owned therefore return the typed
`AuthorityRejected` result without inventing ownership or consuming material;
this boundary is covered by the enhancement failure test and is not a claim
of a successful enhancement PIE path.

## Historical records

Inventory-002 is preserved in `tasks/archive/` and its TDD evidence. Earlier
Inventory-001 and Relic/Equipment history remain in their existing archives.
# TASK-P17-INVENTORY-004 — Editor 集成、PIE 与收口
Status: `COMPLETE / GATE 0 PASS / TDD RED CONFIRMED / GREEN PASS / FOCUSED AUTOMATION PASS / PIE PASS / USER ACCEPTED`

## Scope identity

This is the unambiguous implementation identity for the Phase 17 execution-plan
meaning of P17-008. It is not `TASK-P17-008`, and it does not reuse the archived
meanings of `TASK-P17-006` (Quest Frontend) or `TASK-P17-012` (five-module dynamic
mounting).

User authorization: the user's explicit authorization for
`TASK-P17-INVENTORY-004` on 2026-08-09 covers this code gate and the handoff for
user-owned Editor/UAsset and PIE work.

## Sole observable outcome

Pressing `B` opens the categorized Inventory through the existing Frontend Shell
and `WBP_FrontendModuleRoot_P17.ModuleContentHost`; filtering, sorting, and
selection remain stable; Consumable Use and Equip/Enhance/Disassemble are
submitted only when their corresponding Authority supports them; Back/X and
travel teardown/restore preserve a coherent route, focus, and input policy; any
failure preserves the last complete snapshot and consistent UI.

## Gate 0 decisions

1. The shared Frontend Shell, Router, UIManager, and `ModuleContentHost` remain
   the only route/mount path. The old P13 Inventory widget remains a fallback
   when the P17 module class is not assigned.
2. `UHSRInventorySubsystem` remains the Inventory authority and
   `UHSREquipmentSubsystem` remains the Equip/Enhance authority. This package
   does not add Use or Disassemble authority and never calls `RemoveStack` as a
   substitute for either operation.
3. Inventory rows keep stable identity: `ItemId` plus an invalid `InstanceId`
   for stacks, or `ItemId` plus the committed unique `InstanceId` for unique
   items. The UI reads pure-value snapshots and uses bounds-safe accessors.
4. Route failure, missing/invalid snapshot, focus failure, travel teardown, and
   restore failure must not publish an empty success stage or silently replace a
   complete committed snapshot.

## Implemented code contract

- `AHSRHUD` accepts `InventoryModuleWidgetClass` under `HUD|P17` and passes it
  to `UHSRUIManagerSubsystem` while retaining the legacy P13 class.
- Inventory opens through the shared dynamic module-root path and attaches the
  P17 widget to `ModuleContentHost`; no second viewport path is introduced.
- `UHSRInventoryModuleWidget` derives from `UHSRScreenWidget`, exposes snapshot,
  row, action-state, intent, close-to-root, and preferred-focus seams for UMG,
  and forwards command context from the party slot-0 character.
- Dynamic Inventory ownership participates in HasOpen, Back/X, focus restore,
  travel teardown, arrival restore, and host-generation consistency checks.
- Existing Inventory-001/002/003 projection and command contracts remain in the
  same focused regression target.

## TDD gates

1. Gate 0: complete. Authority ownership, shared mount path, legacy fallback,
   lifecycle boundaries, failure matrix, and Editor allowlist are frozen here.
2. RED: complete. The new dynamic-route test reached the intended missing
   `ConfigureAutomationInventoryModuleBackend` compiler seam before production
   integration was present.
3. GREEN: complete. The seam, dynamic route, focus fallback, Back/X lifecycle,
   travel restore, and production snapshot guard compile and pass focused
   Automation.
4. Editor/PIE closeout: complete. The user supplied saved UAssets and core PIE
   evidence, then confirmed the manual visual/interaction experience is normal.
   The task is `COMPLETE / USER ACCEPTED`; known ScreenLifecycle fixture
   failures remain outside this product claim.

## Allowed files

Production files:

- `Source/HSR/UI/HSRHUD.h`
- `Source/HSR/UI/HSRHUD.cpp`
- `Source/HSR/UI/HSRUIManagerSubsystem.h`
- `Source/HSR/UI/HSRUIManagerSubsystem.cpp`
- `Source/HSR/UI/Inventory/HSRInventoryModuleWidget.h`
- `Source/HSR/UI/Inventory/HSRInventoryModuleWidget.cpp`

Test and evidence files:

- `Source/HSR/Tests/HSRInventoryViewModelTests.cpp`
- `tasks/active-task.md`
- `tasks/execution-result.md`
- `docs/testing/TASK-P17-INVENTORY-004.tdd.md`
- `PROJECT_STATE.md`
- `worklog.md`
- `todo_plan.md`

User-owned Editor files are limited to the requested P17 Inventory Blueprint,
UMG layout, catalog asset assignments, and the existing HUD/root Blueprint
Class Defaults. The agent does not create or edit those UAssets.

## Explicit prohibitions

- Do not modify Inventory, Equipment, Party, Save, Travel, or Reward Authority
  implementations for this UI integration.
- Do not call `RemoveStack` as a substitute for Use, Disassemble, or Enhance.
- Do not create or edit UAsset, Config, Build.cs, `.uproject`, plugin, generated,
  or build-output files.
- Do not add a second viewport/mount path or call `AddToViewport` from Inventory.
- During implementation, do not stage, commit, push, reset, clean, delete, or
  rewrite Git history. A final closeout commit requires explicit user
  authorization; push remains a separate user decision.
- Do not claim PIE, Editor reopen, or UAsset provenance evidence before the user
  performs and reports those steps.

## User Editor work

The user must create `/Game/UI/P17/Inventory/WBP_Inventory_P17` with parent
`UHSRInventoryModuleWidget`, assign the Inventory Catalog, Item Equipment Mapping
Catalog, and Equipment Enhancement Catalog, build the category/list/detail/action
UMG, set a focusable preferred widget, and assign the class to
`BP_HSRHUD.InventoryModuleWidgetClass`. Detailed steps are in the 004 handoff
and TDD evidence report.

## Verification boundary

The code gate is complete: Development Editor Build succeeded, Inventory
Automation is 9/9, FrontendNavigation Automation is 11/11, and `git diff --check`
passed. The user supplied and accepted the UAsset, Editor, and PIE evidence.
Existing `HSR.UI.ScreenLifecycle` CharacterDetail/Inventory/TravelRestore
fixture failures remain a known pre-task baseline and were not reclassified as
product failures.

---
## Latest user-provided closeout update — 2026-08-09

The earlier code-gate-only Editor/PIE boundary in this file is superseded by
the user's latest delivery. DA_InventoryCatalog_P17 and WBP_Inventory_P17 now
exist and are saved; the Widget is parented to UHSRInventoryModuleWidget,
catalog references are assigned, BP_HSRHUD and frontend input are configured,
and the allowlisted C++ Blueprint seams/dynamic row projection are present.

Core PIE evidence from Map_Exploration_P15_A confirms Inventory open, Back to
Pause Hub, X to exploration, and no Blueprint Runtime Error, Ensure, or invalid
snapshot log. The user then confirmed the manual visual/interaction experience
is normal. Final status: `COMPLETE / USER ACCEPTED`.
# TASK-P17-DIALOGUE-001 — Gate 0、Interaction/Dialogue 合同与 TDD RED
Status: COMPLETE / GATE 0 PASS / TDD RED CONFIRMED / GREEN NOT STARTED

This is the current task record. It is the unambiguous Dialogue package for the
Phase 17 execution-plan meaning of P17-012; it must not reuse the archived
`TASK-P17-012`, which records five-module dynamic mounting. Inventory-001～004
are historical completed work below.

User authorization: explicit user authorization for `TASK-P17-DIALOGUE-001` on
2026-08-09.

## Sole observable outcome

Freeze the Interaction/Dialogue contract and prove the missing presentation seam
with a real RED test. The contract must keep `F` and the current candidate owned
by `UHSRInteractionComponent`, keep dialogue progression owned by
`UHSRDialogueSubsystem`, keep Quest/Encounter/Reward as their own authorities,
and define a pure-value active-query/presentation snapshot for the next
Dialogue-002 package. This task does not implement the Overlay or any UAsset.

## Gate 0 audit and frozen boundaries

1. `UHSRInteractionComponent` owns the current interactable candidate and the
   `F` interaction attempt. The Dialogue Overlay does not enter the Pause Hub
   Frontend Router and does not replace candidate ownership.
2. `AHSRDialogueInteractable` currently validates the configured DialogueId and
   logs the start node, but the generic `FHSRInteractionResult` does not carry a
   typed DialogueId/NodeId presentation payload. A typed start/presentation
   seam is therefore a documented missing API, not something the UI may infer
   from log text or an Actor pointer.
3. `UHSRDialogueSubsystem` remains the authority for definition registration,
   node lookup, choice progression, and Quest event submission. It returns raw
   `EHSRQuestOperationResult` values; presentation mapping must retain that
   source result.
4. The active query is a stable pure-value identity (`FGuid QueryId` plus
   `DialogueId` and current `NodeId`) created once for an interaction. Repeating
   `F` while the same query is active is a no-op; stale or invalid queries do
   not replace the last complete snapshot.
5. The future presentation snapshot contains no UObject/AActor pointer and
   exposes stable `ChoiceId` values. A failed begin/choice/branch preserves the
   last complete snapshot and reports a typed result separately.
6. Existing `FHSRDialogueNodeDefinition` has body text but no authoritative
   speaker display field or choice display text field. Dialogue-002 must either
   extend that data contract through a separately authorized allowlist or use an
   explicitly approved mapping; it must not silently display IDs as authored
   prose or invent speaker data in the Widget.
7. Dialogue does not directly mutate Quest, Reward, Inventory, Encounter, or
   BattleTransition state. Dialogue-003 will handle stable branch forwarding and
   exactly-once behavior after the presentation contract is accepted.

## RED contract test

`Source/HSR/Tests/HSRDialoguePresentationTests.cpp` defines the intended next
seam: `FHSRDialoguePresentationRequest`,
`FHSRDialoguePresentationSnapshot`,
`EHSRDialoguePresentationResult`, and
`UHSRDialoguePresentationViewModel`. The tests cover active-query identity,
duplicate-open no-op behavior, stable Node/Choice projection, and failure
preservation. These presentation types are intentionally absent until the
separately authorized Dialogue-002 implementation. The post-Editor-exit UBT
run reached the intended missing-header compile failure, confirming the RED
gate. It also surfaced the unrelated `NameLess` duplicate-definition failure
described above; no attempt was made to repair that out-of-scope issue.

## Read-only audit paths

- `Source/HSR/Dialogue/HSRDialogueSubsystem.{h,cpp}`
- `Source/HSR/Dialogue/HSRDialogueInteractable.{h,cpp}`
- `Source/HSR/Dialogue/HSRDialogueTypes.h`
- `Source/HSR/Interaction/HSRInteractionComponent.{h,cpp}`
- `Source/HSR/Interaction/HSRInteractionTypes.h`
- `Source/HSR/Interaction/HSRInteractableInterface.h`
- `Source/HSR/Quest/HSRQuestSubsystem.{h,cpp}`
- `Source/HSR/Reward/HSRRewardSubsystem.{h,cpp}`
- `Source/HSR/Data/Definitions/HSRDialogueDefinition.h`
- `Source/HSR/Tests/HSRQuestDialogueTests.cpp`

## Allowed files for this package

Writable files:

- `Source/HSR/Tests/HSRDialoguePresentationTests.cpp`
- `tasks/active-task.md`
- `tasks/execution-result.md`
- `docs/testing/TASK-P17-DIALOGUE-001.tdd.md`
- `PROJECT_STATE.md`
- `todo_plan.md`
- `worklog.md`

No production Dialogue, Interaction, Quest, Reward, UI, Content, Config, or
UAsset file is writable in Dialogue-001. Those paths are audit-only above.

## Explicit prohibitions

- Do not implement `UHSRDialoguePresentationViewModel`, Dialogue Overlay,
  speaker/choice display data, or any GREEN production seam in this package.
- Do not add a second viewport or Frontend Router path; Dialogue remains an
  Exploration Overlay and `F` remains owned by InteractionComponent.
- Do not call Quest/Reward/Inventory/Encounter authority directly from a Widget
  or fake branch completion with UI state.
- Do not modify UAsset, Config, Build.cs, `.uproject`, generated/build output,
  `.claude/**`, or unrelated dirty-worktree files.
- Do not stage, commit, push, reset, clean, delete, or rewrite Git history
  without a separate explicit user instruction. The current authorization is
  for this task's Gate 0/RED work, not an automatic Git checkpoint.

## Verification boundary

The required result is a real compile-time RED caused by the intentionally
missing Dialogue presentation contract, and that result is now confirmed. No
GREEN, PIE, Editor asset work, or Phase 18 work is claimed. The unrelated
`NameLess` compile failure remains a separate dirty-worktree follow-up. This
package is closed at RED; the next separately authorized package is
Dialogue-002.

---

# TASK-P17-DIALOGUE-002 - Event-driven Dialogue Presentation
Status: COMPLETE / GATE 0 PASS / TDD RED CONFIRMED / GREEN PASS / FOCUSED AUTOMATION PASS

This is the unambiguous implementation task for the Phase 17 execution-plan
meaning of P17-012. It follows Dialogue-001 and does not reuse the archived
`TASK-P17-012` dynamic-mount package. User authorization was supplied on
2026-08-09.

## Sole observable outcome

The Dialogue presentation layer projects authored speaker text, body text and
stable ChoiceId/display text into a pure-value snapshot. A stable QueryId plus
DialogueId/NodeId gates begin, selection, repeat-open, stale and exit requests.
Selection advances through a read-only Dialogue Authority preview; Quest,
Encounter and Reward branch submission remains deferred to Dialogue-003. Any
invalid, unavailable, stale or Authority failure preserves the last complete
snapshot and exposes the typed result plus raw Authority result separately.

## Gate 0 decisions

1. `UHSRInteractionComponent` still owns `F` and the current Candidate. This
   task does not add an Overlay to the Pause Hub Frontend Router or create a
   second viewport path.
2. `FHSRDialogueNodeDefinition` gains authored `SpeakerText`; choices gain
   authored `DisplayText`. Empty authored text is projected as empty; the UI
   does not invent prose from IDs.
3. `UHSRDialogueSubsystem::GetNode` and `PreviewChoice` are read-only
   presentation seams. `UHSRDialoguePresentationViewModel` must not call
   `SelectChoice`, `SubmitEvent`, Quest, Reward, Inventory, Encounter, or
   BattleTransition authorities in this package.
4. A successful choice with a target NodeId publishes the next complete
   snapshot under the same QueryId. A terminal choice or explicit matching exit
   closes the snapshot. Failed requests do not publish a replacement snapshot.
5. Raw `EHSRQuestOperationResult` is retained through
   `GetLastAuthorityResult`; presentation mapping never hides the source code.

## TDD and verification state

- RED: the real UBT reached the intentional missing
  `HSRDialoguePresentationTypes.h` compile failure before production code.
- GREEN build: `HSREditor Win64 Development` returned `Result: Succeeded`.
- Focused Automation discovered 4 tests under `HSR.Dialogue.Presentation` and
  all 4 completed with `Result={Success}`. The corrected fixture uses a valid
  `UGameInstance` Outer. Existing `HSR.QuestDialogue` discovered 1 test and
  completed with `Result={Success}`.

## Allowed files

- `Source/HSR/Dialogue/HSRDialogueTypes.h`
- `Source/HSR/Dialogue/HSRDialogueSubsystem.h`
- `Source/HSR/Dialogue/HSRDialogueSubsystem.cpp`
- `Source/HSR/UI/Dialogue/HSRDialoguePresentationTypes.h`
- `Source/HSR/UI/Dialogue/HSRDialoguePresentationViewModel.h`
- `Source/HSR/UI/Dialogue/HSRDialoguePresentationViewModel.cpp`
- `Source/HSR/Tests/HSRDialoguePresentationTests.cpp`
- `tasks/active-task.md`
- `tasks/execution-result.md`
- `docs/testing/TASK-P17-DIALOGUE-002.tdd.md`
- `PROJECT_STATE.md`
- `todo_plan.md`
- `worklog.md`

## Explicit prohibitions

- Do not modify InteractionComponent, DialogueInteractable, Quest, Reward,
  Inventory, Encounter, BattleTransition, Frontend Router, HUD, UAsset, Config,
  Build.cs, `.uproject`, generated files, or `.claude/**`.
- Do not stage, commit, push, reset, clean, delete, or rewrite Git history.
- Do not claim focused Automation GREEN, Overlay PIE, Editor asset work, or
  Dialogue-003 exactly-once branch behavior from this package.

## Verification boundary

Dialogue-002 is complete at the code/Automation gate. It does not claim a
Dialogue Overlay, UAsset, Editor, PIE, Dialogue-003 exactly-once branch flow or
Git checkpoint. The next separately authorized package is Dialogue-003.

---
