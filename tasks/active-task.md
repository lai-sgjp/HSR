# TASK-P17-INVENTORY-004 — Editor 集成、PIE 与收口
Status: IN PROGRESS / CODE + EDITOR + CORE PIE COMPLETE / USER VISUAL ACCEPTANCE PENDING

This is the current active task. The detailed 004 contract is recorded in the
004 section below; the preceding Inventory-003 block is historical context only.
The user has supplied Editor integration and core PIE evidence. The only
remaining work is manual visual/interaction experience and final acceptance.

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
Status: `IN PROGRESS / GATE 0 PASS / TDD RED CONFIRMED / GREEN PASS / FOCUSED AUTOMATION PASS / PIE PENDING`

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
4. Editor/PIE closeout: pending user action. UAsset creation, Blueprint wiring,
   Editor reopen, and happy/failure PIE evidence must be supplied by the user;
   this task card does not claim those results.

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
- Do not stage, commit, push, reset, clean, delete, or rewrite Git history.
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
passed. Existing `HSR.UI.ScreenLifecycle` CharacterDetail/Inventory/
TravelRestore fixture failures remain a known pre-task baseline and were not
reclassified as product failures. UAsset creation, Editor reopen, and PIE are
`NOT VERIFIED` until user evidence is supplied.

---
## Latest user-provided closeout update — 2026-08-09

The earlier code-gate-only Editor/PIE boundary in this file is superseded by
the user's latest delivery. DA_InventoryCatalog_P17 and WBP_Inventory_P17 now
exist and are saved; the Widget is parented to UHSRInventoryModuleWidget,
catalog references are assigned, BP_HSRHUD and frontend input are configured,
and the allowlisted C++ Blueprint seams/dynamic row projection are present.

Core PIE evidence from Map_Exploration_P15_A confirms Inventory open, Back to
Pause Hub, X to exploration, and no Blueprint Runtime Error, Ensure, or invalid
snapshot log. The only remaining item is manual visual/gamepad experience and
final USER ACCEPTED confirmation.
