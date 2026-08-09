# TASK-P17-INVENTORY-001 TDD Evidence

Date: 2026-08-09
Source intent: `docs/phase-17-execution-plan.md`, P17-008 and section 7.1
Authorization: explicit user instruction to begin
`TASK-P17-INVENTORY-001`

## User journeys converted to RED guarantees

- As a player, I want the inventory category and display metadata to come from
  authored data so that Weapon/Relic/Consumable/Material rows are not inferred
  from unstable string prefixes.
- As a player, I want filtering, sorting, and selection to use stable keys so
  that an Inventory refresh does not select a different item by array index.
- As a player, I want unsupported actions to report unavailable without
  changing my inventory or the visible snapshot.

## Gate 0 decisions

The authoritative contract is recorded in `tasks/active-task.md`. The chosen
classification source is the explicit UI `UHSRInventoryCatalog`, while
`UHSRInventorySubsystem` remains the sole owner of quantities, unique bag
membership, capacity, and revision. Existing Reward Summary code remains
untouched. The eventual module route must mount through
`WBP_FrontendModuleRoot_P17.ModuleContentHost`.

## RED evidence

The first sandbox build stopped before compilation because UBT could not write
its external cache. The same command was rerun with the required external
permission and reached the new test compilation:

```text
[Adaptive Build] Excluded from HSR unity file: HSRInventoryViewModelTests.cpp
HSRInventoryViewModelTests.cpp(5,1): fatal error C1083:
Cannot open include file: '../Data/Definitions/HSRInventoryCatalog.h':
No such file or directory
```

This is the intended compile-time RED for the missing Inventory contract. No
production implementation was edited after RED.

Command:

```text
E:\programs\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat HSREditor Win64 Development -Project="E:/work/unreal_projects/HSR/HSR.uproject" -NoHotReload -WaitMutex
```

## Test specification

| # | Guarantee | Test | Result |
|---|---|---|---|
| 1 | Catalog-backed category projection, deterministic display-name ordering, stable key selection, and refresh-preserved selection | `HSR.UI.Inventory.ViewModel.ClassificationSortSelection` in `Source/HSR/Tests/HSRInventoryViewModelTests.cpp` | RED at missing contract |
| 2 | Use/Disassemble/Equip/Enhance with no supporting Authority return typed unavailable and preserve the complete prior snapshot | `HSR.UI.Inventory.ViewModel.UnsupportedActionsPreserveSnapshot` in `Source/HSR/Tests/HSRInventoryViewModelTests.cpp` | RED at missing contract |

## Coverage and gaps

UE Automation is the project test runner; no source-coverage percentage is
claimed for this Gate 0/RED-only package. GREEN implementation, focused
Automation, build, Editor wiring, Save All/reopen, input, travel teardown, and
PIE remain intentionally unverified until the separately authorized
Inventory-002/003/004 packages.
