# TASK-P17-INVENTORY-003 TDD Evidence

Date: 2026-08-09
Source intent: `docs/phase-17-execution-plan.md`, section 6.4, P17-008, and
section 7.1 Inventory-003
Authorization: explicit user authorization in context for
`TASK-P17-INVENTORY-003`

## Gate 0

The task card freezes Inventory/Equipment authority ownership, stable unique
IDs, real revision fields, operation IDs, command context, typed unavailable
behavior, raw result-code logging, and complete-snapshot preservation. It does
not authorize B/Back/X, route mounting, UAsset work, travel, or PIE.

## User journeys

- As a player, I want Equip to submit the selected stable item through
  Equipment Authority so that Inventory and Equipment move atomically.
- As a player, I want Enhance to submit only a valid Authority request with
  the current revision and target level so that stale or invalid requests do
  not consume materials.
- As a player, I want Use and Disassemble to report unavailable when no
  Authority exists so that the UI never fakes a business success.
- As a player, I want every rejected command to retain the complete visible
  snapshot so that failure does not blank or desynchronize the UI.

## RED evidence

The command tests were added to `Source/HSR/Tests/HSRInventoryViewModelTests.cpp`
and compiled with the real UBT before production command code was implemented.
The intended compile-time RED failures were:

- `SetCommandContext is not a member`
- `SubmitAction does not take 2 arguments`
- `AuthorityRejected undeclared`

## GREEN evidence

The production seam was then implemented only in the allowlisted Inventory UI
files. The focused command was run with the real UE 5.6 commandlet:

```text
UnrealEditor-Cmd.exe HSR.uproject -ExecCmds="Automation RunTests HSR.UI.Inventory; Quit" -TestExit="Automation Test Queue Empty" -unattended -nop4 -nosplash -NullRHI -NoSound -log
```

The log found 7 tests and every test completed with `Result={Success}`:

| Guarantee | Automation target | Result |
|---|---|---|
| Equip delegates the selected stable unique instance through Equipment Authority | `HSR.UI.Inventory.ViewModel.Commands.EquipThroughAuthority` | PASS |
| Enhancement rejection preserves the complete snapshot and does not consume material | `HSR.UI.Inventory.ViewModel.Commands.EnhancementFailurePreservesSnapshot` | PASS |
| Unsupported/absent command Authority preserves the snapshot | `HSR.UI.Inventory.ViewModel.Commands.UnsupportedPreserveSnapshot` | PASS |
| Category, filter, sort, and stable selection regression | `HSR.UI.Inventory.ViewModel.ClassificationSortSelection` | PASS |
| Inventory unsupported-action regression | `HSR.UI.Inventory.ViewModel.UnsupportedActionsPreserveSnapshot` | PASS |
| Widget read-only lifecycle regression | `HSR.UI.Inventory.Widget.ReadOnlyLifecycle` | PASS |
| Reward/Inventory lifecycle regression | `HSR.UI.InventoryReward.Lifecycle` | PASS |

The project command
`"E:/programs/Epic Games/UE_5.6/Engine/Build/BatchFiles/Build.bat" HSREditor Win64 Development -Project="E:/work/unreal_projects/HSR/HSR.uproject"`
returned `Result: Succeeded` (target up to date), and `git diff --check`
passed.

## Known boundary

The existing Equipment Authority requires an owned instance for enhancement.
Inventory-003 does not invent a second possession model or silently equip an
item as a prerequisite for enhancement. A bag item that is not Authority-owned
is reported as typed `AuthorityRejected` and its snapshot remains unchanged;
this is a deliberate command-boundary result, not a successful enhancement
claim. User asset assignment, UMG integration, B/Back/X, travel teardown, and
PIE remain outside this code gate and are deferred to Inventory-004.
