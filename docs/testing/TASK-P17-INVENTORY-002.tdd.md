# TASK-P17-INVENTORY-002 TDD Evidence

Date: 2026-08-09
Source intent: `docs/phase-17-execution-plan.md`, section 6.4 and P17-008
Authorization: explicit user instruction to execute
`TASK-P17-INVENTORY-002`

## User journeys converted to guarantees

- As a player, I want an authored category and display projection so that
  Inventory rows do not infer gameplay categories from ItemId strings.
- As a player, I want filtering and deterministic sorting so that the visible
  list is predictable and presentation-only.
- As a player, I want selection to use a stable stack/unique key so that an
  Inventory revision refresh does not select a different row by array index.
- As a player, I want unsupported commands to report typed unavailable and
  preserve the complete visible snapshot.

## Gate 0 decisions carried forward

`UHSRInventorySubsystem` remains the sole authority for quantities, unique bag
membership, capacity, and committed revision publication. The presentation
classification source is the explicit `UHSRInventoryCatalog`. Stack keys are
`ItemId + invalid InstanceId`; unique keys are `ItemId + committed InstanceId`.
The Widget reads and forwards pure snapshots and does not own Inventory,
SaveGame, viewport, or command authority state.

## RED evidence

The serial 001 RED gate compiled the expanded test contract before the 002
production files existed. The authorized UBT run failed at the intended
missing catalog include:

```text
HSRInventoryViewModelTests.cpp(5,1): fatal error C1083:
Cannot open include file: '../Data/Definitions/HSRInventoryCatalog.h'
```

This is recorded in
`tasks/archive/TASK-P17-INVENTORY-001-execution-result.md`. No production
implementation was present at that RED point.

## GREEN evidence

The minimal implementation added the catalog, pure-value types, ViewModel, and
read-only Widget in the 002 allowlist. A focused run initially revealed that
the fixture query `beta` was ambiguous: it matched a display name and another
row's internal ItemId. The test was narrowed to `beta relic`, a
case-insensitive display-name phrase, and the same production implementation
then passed without an authority or snapshot change.

Development Editor Build:

```text
E:\programs\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat HSREditor Win64 Development -Project="E:/work/unreal_projects/HSR/HSR.uproject" -NoHotReload -WaitMutex
```

Observed result: `Result: Succeeded`.

Focused Automation:

```text
UnrealEditor-Cmd.exe HSR.uproject -ExecCmds="Automation RunTests HSR.UI.Inventory; Quit" -TestExit="Automation Test Queue Empty" -unattended -nop4 -nosplash -NullRHI -NoSound -log
```

Observed log result from `Saved/Logs/HSR.log`:

```text
Found 4 automation tests based on 'HSR.UI.Inventory'
4 tests completed with Result={Success}
**** TEST COMPLETE. EXIT CODE: 0 ****
```

## Test specification

| # | What is guaranteed | Test | Type | Result |
|---|---|---|---|---|
| 1 | Catalog-backed category projection, deterministic display-name ordering, display filtering, stable key selection, unique details, and refresh-preserved selection | `HSR.UI.Inventory.ViewModel.ClassificationSortSelection` in `Source/HSR/Tests/HSRInventoryViewModelTests.cpp` | UE Automation | PASS |
| 2 | Use/Disassemble/Equip/Enhance without supporting Authority return typed unavailable and preserve the complete prior snapshot | `HSR.UI.Inventory.ViewModel.UnsupportedActionsPreserveSnapshot` in `Source/HSR/Tests/HSRInventoryViewModelTests.cpp` | UE Automation | PASS |
| 3 | Widget binds/unbinds correctly, forwards presentation intent, reads a pure snapshot, and stops receiving detached updates | `HSR.UI.Inventory.Widget.ReadOnlyLifecycle` in `Source/HSR/Tests/HSRInventoryViewModelTests.cpp` | UE Automation | PASS |
| 4 | Existing Reward Summary lifecycle remains green under the same Inventory selector | `HSR.UI.InventoryReward.Lifecycle` in `Source/HSR/Tests/HSRInventoryRewardViewModelTests.cpp` | UE Automation regression | PASS |

## Coverage and known gaps

The repository has no configured UE source-coverage percentage runner, so no
coverage percentage is claimed. UAsset creation/wiring, B/Back/X routing,
UIManager/`ModuleContentHost` integration, legal Equip/Enhance transactions,
Consumable Use, Disassemble, travel teardown, Editor reopen, and PIE remain
outside 002. They are intentionally deferred to Inventory-003/004 and the
user's Editor/PIE gate.

## Delivery boundary

No `.claude/**` files, Content/UAsset files, Config, Build.cs, `.uproject`,
Inventory/Equipment Authority, Reward Summary, UIManager, HUD, Router, or Save
files were modified for this package. No Git stage, commit, push, reset,
clean, or history rewrite was performed.
