# TASK-P17-PATCH-03G - Save UI and Integrated Restore

Status: `TASK GATE / IMPLEMENTATION AUTHORIZED / TDD REQUIRED`

## Sole outcome

The frontend provides manual Save, Load and Overwrite requests with typed results. After a cold restart, Load restores the authoritative Character, Party, Inventory, Equipment, Reward and Map chain exactly once, and the UI presents only committed Save results.

## Authority contract

- `UHSRSaveSubsystem` owns slot naming, envelope validation, migration, disk write/read, backup selection and the global restore transaction.
- Save ViewModel reads committed Save state and submits typed commands. A Save Widget presents slots/results only; it may not serialize domain state, mutate authorities or call map travel directly.
- UIManager owns frontend route, input/focus and host rebuilding. Map arrival remains owned by MapSubsystem and is consumed through the existing restore path.
- A failed save/load/overwrite must preserve the prior committed runtime state and present one typed failure result.

## Frozen C++ allowlist

- `Source/HSR/Save/HSRSaveSubsystem.h`
- `Source/HSR/Save/HSRSaveSubsystem.cpp`
- `Source/HSR/Save/HSRSaveTypes.h`
- new `Source/HSR/UI/HSRSaveViewModel.h/.cpp`
- new `Source/HSR/UI/HSRSaveWidget.h/.cpp`
- `Source/HSR/UI/HSRUIManagerSubsystem.h`
- `Source/HSR/UI/HSRUIManagerSubsystem.cpp`
- focused new Save UI and integrated-restore Automation tests
- `tasks/execution-result.md`

All Blueprint, UMG, Content, Config, map and DataAsset edits remain user-owned. No Save schema rewrite, new module, battle-transition rewrite, inventory/equipment authority change or visual redesign is in scope.

## Required TDD coverage

- Empty slot save, overwrite confirmation, successful load and structured result presentation.
- Invalid/missing/corrupt/unsupported slot data rejects before any live authority commits.
- Save/load requests cannot overlap; duplicate requests return a typed no-op/rejection without a second write or restore.
- A successful cold restore rebuilds each domain exactly once and resumes at the committed Map location without stale UI/focus state.
- Existing `HSR.Save`, affected `HSR.Map`, `HSR.Equipment`, `HSR.Inventory` and `HSR.UI` automation stays GREEN.

## User Editor boundary

After C++ GREEN, the user creates or updates only the task-selected Save frontend WBP, binds read-only slot/result presentation and typed Save/Load/Overwrite intents, then performs Save All/reopen and PIE save -> stop -> restart -> load evidence. The Widget must not access disk APIs, mutate Save DTOs or initiate `OpenLevel`.

## Stops

Stop for a Save schema change, a file outside the allowlist, any binary asset mutation by Codex, an ambiguous overwrite UX decision, or an Editor-only failure that cannot be reproduced through the permitted C++/Automation surface.
