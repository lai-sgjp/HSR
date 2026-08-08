# TASK-P17-RELIC-EQUIPMENT-001 - Relic/Equipment Selection and Enhancement

Status: `C++/AUTOMATION COMPLETE / USER EDITOR ASSET GATE`

## Scope identity

This task implements the Phase 17 execution-plan meaning of P17-007. It must
not be labeled `TASK-P17-006`: the repository task with that number is the
archived Quest Frontend. It must not be labeled `TASK-P17-012`: the repository
task with that number is the archived five-module dynamic mount. This new
unambiguous task name is the only active identity for the implementation.

User authorization: 2026-08-07, explicit authorization for
`TASK-P17-RELIC-EQUIPMENT-001`.

## Sole observable outcome

From the accepted Character Shell Relics tab, the player can enter one
Relic/Equipment flow, select a relic slot, inspect bag candidates, compare a
candidate with the committed relic, equip or replace through one authoritative
movement request, open enhancement choices, and confirm one material-backed
enhancement transaction. Back returns one presentation level at a time; X and
the existing Frontend route retain their existing semantics.

The only committed domain result is an authoritative Equipment/Inventory state
with exactly-once revisions/publication. A rejected request keeps the previous
Inventory snapshot, Equipment registry/loadout, revisions, materials, and UI
selection state intact. Missing catalog or authority data is a typed
unavailable state, not a Blueprint-created success.

The Exploration Pause shortcut remains numeric `1`. Character and Inventory
remain dynamically mounted and must not use direct `AddToViewport`.

## Ownership and data-flow contract

1. `UHSREquipmentSubsystem` owns the complete equipment/relic instance payload,
   placement, owner mapping, Equipment revision, and enhancement transaction
   ledger.
2. `UHSRInventorySubsystem` owns bag membership, stackable material quantity,
   capacity, Inventory revision, and Inventory publication.
3. `UHSREquipmentEnhancementCatalog` owns static editor-authored target-level
   rules. It is not Save data and it does not perform a transaction.
4. The aggregate enhancement seam prepares an immutable Inventory candidate and
   Registry/loadout candidate, validates both expected revisions and the
   expected current enhancement level, installs both only after preflight,
   advances each revision once, then publishes once per domain. OperationId
   replay returns the cached result without a second side effect; a changed
   payload under the same OperationId is rejected.
5. `UHSRRelicEquipmentViewModel` owns only transient selected character/slot,
   selected candidate, stage, comparison values, and enhancement options. It
   reads committed snapshots and forwards UI intent; it is not domain or Save
   authority.
6. `UHSRRelicEquipmentWidget` owns delegate lifecycle and Blueprint event
   forwarding. Blueprint owns layout, styling, focus, entry visuals, and
   snapshot binding only.
7. Existing Character Shell, Frontend root/router/UIManager, Save schema,
   Party/Map/BattleReturn/Quest/Challenge/Inventory reward logic, and accepted
   009D/010A/010B/010C behavior are regression inputs, not implementation
   targets.

## Codex Source allowlist

New files:

- `Source/HSR/Data/Definitions/HSREquipmentEnhancementCatalog.h`
- `Source/HSR/Data/Definitions/HSREquipmentEnhancementCatalog.cpp`
- `Source/HSR/UI/Relic/HSRRelicEquipmentTypes.h`
- `Source/HSR/UI/Relic/HSRRelicEquipmentViewModel.h`
- `Source/HSR/UI/Relic/HSRRelicEquipmentViewModel.cpp`
- `Source/HSR/UI/Relic/HSRRelicEquipmentWidget.h`
- `Source/HSR/UI/Relic/HSRRelicEquipmentWidget.cpp`
- `Source/HSR/Tests/HSREquipmentEnhancementTests.cpp`
- `Source/HSR/Tests/HSRRelicEquipmentViewModelTests.cpp`
- `docs/testing/TASK-P17-RELIC-EQUIPMENT-001.tdd.md`

Existing files permitted only for the minimum authority/candidate seam,
enhancement transaction, and development fixture:

- `Source/HSR/Equipment/HSREquipmentTypes.h`
- `Source/HSR/Equipment/HSREquipmentSubsystem.h`
- `Source/HSR/Equipment/HSREquipmentSubsystem.cpp`
- `Source/HSR/Inventory/HSRItemTypes.h`
- `Source/HSR/Inventory/HSRInventorySubsystem.h`
- `Source/HSR/Inventory/HSRInventorySubsystem.cpp`
- `Source/HSR/Equipment/HSREquipmentDevelopmentHarness.h`
- `Source/HSR/Equipment/HSREquipmentDevelopmentHarness.cpp`

No other Source file is authorized. In particular, do not modify Character
Shell C++, Character/Equipment Detail ViewModel, UIManager, HUD, Frontend
Router/Root, Save/Migration/Schema, Battle, Party, Map, Quest, Challenge,
Reward, Gacha, Config, Build.cs, `.uproject`, plugins, or `.claude/**`.

## User Editor UAsset allowlist

User may create:

- `Content/UI/P17/Character/WBP_RelicEquipment_P17.uasset`
- `Content/UI/P17/Character/WBP_RelicSlotEntry_P17.uasset`
- `Content/UI/P17/Character/WBP_RelicCandidateEntry_P17.uasset`
- `Content/Data/Equipment/DA_EquipmentEnhancementCatalog_P17.uasset`

User may edit only the following existing assets for wiring/data rows:

- `Content/UI/P17/Character/WBP_CharacterShell_P17.uasset`, only to place
  the Relic widget in the existing shell `ContentHost`/Relics content path
  and forward the selected Character ID.
- `Content/Data/Items/DA_ItemEquipmentMappingCatalog_P17.uasset`, only to add
  missing explicit ItemId -> DefinitionId/Kind/Slot rows; existing rows are
  not deleted or rewritten.

The formal Frontend route remains mounted through
`WBP_FrontendModuleRoot_P17.ModuleContentHost`. No direct viewport mount is
allowed for Character or Inventory. Codex does not binary-edit, delete,
replace, or overwrite any UAsset.

## C++ / Blueprint boundary

- C++ owns catalog validation, candidate filtering, comparison/delta values,
  expected revisions, OperationId creation/forwarding, authority result
  mapping, committed snapshot refresh, typed unavailable/failure states, and
  delegate bind/unbind.
- Blueprint owns visual layout, slot/candidate entry widgets, selected-state
  styling, focus order, button labels, and binding of pure-value snapshots.
- Blueprint may call `SelectSlot`, `SelectCandidate`, `OpenEnhancement`,
  `CommitSelectedMovement`, `CommitEnhancement`, and `Back`; it may not call
  `AddToViewport`, mutate Inventory/Equipment, remove materials directly,
  call `SetEnhancementLevel` for the P17 flow, apply GameplayEffects, save,
  travel, or infer success from a button click.

## Acceptance and failure matrix

- valid relic slot selection yields deterministic slot rows and bag candidates;
  invalid slot/candidate/catalog/authority data yields typed unavailable;
- candidate comparison is pure and does not mutate Inventory, Equipment,
  materials, revisions, or Save data;
- bag -> equip removes one matching unique item and places the same registry
  instance exactly once;
- occupied slot -> replace atomically returns the displaced instance to the
  bag and places the incoming instance, with net capacity validation;
- enhancement validates DefinitionId, Kind, target level, modifier snapshot,
  material definition/cost, expected Inventory/Equipment revisions, and
  expected current enhancement level;
- successful enhancement consumes material once, updates Registry payload once,
  advances Inventory and Equipment revisions once, and broadcasts once per
  domain; same OperationId replay is cached/no-op;
- stale revisions, wrong owner/slot, insufficient material, missing mapping,
  invalid catalog row, target above cap, duplicate OperationId payload, and
  injected preflight failure leave all old snapshots/revisions/selection intact;
- ViewModel refreshes only from committed callbacks and unbinds cleanly on
  shutdown/destruction; no duplicate or stale callback remains;
- Build target is `HSREditor Win64 Development`; focused Automation covers
  authority and ViewModel tests plus adjacent Equipment movement/detail tests;
- user performs Save All, Editor close/reopen, and one single-resolution PIE
  happy path. Failure PIE may be skipped after code/Automation wiring review
  per user direction; 1920x1080/1280x720 comparison is `NOT VERIFIED` and
  non-blocking in this task.

## Explicit non-goals

- Character level/ascension/trace/eidolon/outfit upgrades;
- generic Inventory classification, consumable use, disassembly, or economy;
- arbitrary Equipment/Relic stat generation, random rolls, set-bonus redesign,
  currency creation, or Save schema/migration changes;
- direct legacy `SetEnhancementLevel` UI use; the P17 flow uses the new
  material-backed request contract;
- Battle HUD, dialogue, reward, gacha, Party, Map, Quest, Challenge, or
  Frontend routing changes;
- Config, Build.cs, plugins, new modules, direct viewport mounting, or Git
  commit/push.

## TDD and evidence gates

1. Add `HSR.Equipment.Enhancement.*` and
   `HSR.UI.RelicEquipment.ViewModel` tests first and obtain a valid RED from
   the missing contract/implementation.
2. Implement the smallest authority API and rerun the same tests to GREEN.
3. Implement the pure-value ViewModel/Widget boundary and rerun focused tests.
4. Run `HSREditor Win64 Development` Build, focused Automation, adjacent
   regressions, `git diff --check`, and exact allowlist audit.
5. Write only factual RED/GREEN/build/Automation evidence; do not claim PIE or
   Editor persistence before user evidence is supplied.
6. Stop at the User Editor Asset Gate. Do not create or edit UAsset files in
   this implementation pass.

## Current checkpoint

- Authorization: complete.
- Task card: this file, `TASK-P17-RELIC-EQUIPMENT-001`.
- RED: confirmed by the intended missing `HSREquipmentEnhancementCatalog.h`
  compile error after the external UBT-cache permission issue was bypassed.
- GREEN: final `HSREditor Win64 Development` Build succeeded.
- Focused Automation: enhancement ExactlyOnce, enhancement FailureMatrix, and
  Relic ViewModel passed 3/3.
- Adjacent regression: final Equipment/Equipment Detail/Character Shell/
  Frontend Navigation plus Relic selection suite passed 31/31.
- `git diff --check`: no whitespace errors; existing line-ending warnings only.
- User Editor/PIE Asset Gate: pending. No User UAsset was created or modified.
- Failure PIE is user-accepted non-blocking if not run; different resolutions
  remain `NOT VERIFIED` and non-blocking.
- Automatic Git commits: prohibited by user instruction.
