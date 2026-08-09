# TASK-P17-INVENTORY-004 TDD Evidence

Date: 2026-08-09
Source intent: `docs/phase-17-execution-plan.md`, section 6.4, P17-008, and
section 7.1 Inventory-004
Authorization: explicit user authorization in context for
`TASK-P17-INVENTORY-004`

## Gate 0

The package freezes the shared Frontend Shell/Router/UIManager and
`WBP_FrontendModuleRoot_P17.ModuleContentHost` as the only Inventory mount path,
retains the P13 Inventory widget as a fallback, and leaves Inventory/Equipment
authority ownership unchanged. The P17 module owns no direct viewport state.
Use/Disassemble remain typed unavailable without an Authority; Equip/Enhance
continue through Equipment Authority with the existing stable-ID/revision/
operation contract. Any failure must retain the last complete snapshot and a
coherent UI lifecycle.

## User journeys

- As a player, I want `B` to open the categorized Inventory in the shared
  Frontend Shell so that the module uses the same pause/input/focus lifecycle as
  the other P17 screens.
- As a player, I want category, filter, sort, and selection changes to remain
  stable across a committed refresh so that the selected stack or unique item
  does not jump or lose identity.
- As a player, I want Equip/Enhance to submit only through the supported
  Authority and unsupported Use/Disassemble to report unavailable so that the UI
  never fakes a business success.
- As a player, I want Back/X and travel teardown/arrival restore to release and
  rebuild the dynamic content exactly once so that input and focus remain usable.
- As a player, I want a failed route, command, or snapshot build to preserve the
  complete previous view so that an error does not blank the Inventory.

## RED evidence

The new dynamic-route tests were compiled with the real project UBT before the
production integration was completed. The intended compiler failure was:

```text
'ConfigureAutomationInventoryModuleBackend':
is not a member of 'UHSRUIManagerSubsystem'
```

This was a valid compile-time RED for the missing test seam. It was not caused by
an unrelated syntax error, missing dependency, or broken fixture.

## GREEN evidence

The production route and lifecycle seam was implemented in the HUD/UIManager and
P17 Inventory module widget. The final verification also fixed a C++ switch-case
scope error in the new Inventory focus-restore fallback. The actual validation
commands were:

```text
"E:/programs/Epic Games/UE_5.6/Engine/Build/BatchFiles/Build.bat" HSREditor Win64 Development -Project="E:/work/unreal_projects/HSR/HSR.uproject"
UnrealEditor-Cmd.exe HSR.uproject -ExecCmds="Automation RunTests HSR.UI.Inventory; Quit" -TestExit="Automation Test Queue Empty" -unattended -nop4 -nosplash -NullRHI -NoSound -log
UnrealEditor-Cmd.exe HSR.uproject -ExecCmds="Automation RunTests HSR.UI.FrontendNavigation; Quit" -TestExit="Automation Test Queue Empty" -unattended -nop4 -nosplash -NullRHI -NoSound -log
git diff --check
```

Results:

- Development Editor Build: `Result: Succeeded`.
- `HSR.UI.Inventory`: 9 discovered, 9/9 `Result={Success}`.
- `HSR.UI.FrontendNavigation`: 11 discovered, 11/11 `Result={Success}`.
- `git diff --check`: passed; only existing line-ending conversion warnings were
  printed.

## Test specification

| # | What is guaranteed | Test target | Test type | Result | Evidence |
|---|---|---|---|---|---|
| 1 | Dynamic Inventory opens through the shared frontend route, Back returns to the hub, and X closes the shell | `HSR.UI.Inventory.Frontend.DynamicRoute` | UE Automation integration | PASS | 9/9 Inventory suite; log `DynamicRoute` `Result={Success}` |
| 2 | Travel teardown captures the dynamic Inventory route and arrival restores exactly one content widget | `HSR.UI.Inventory.Frontend.DynamicTravelRestore` | UE Automation lifecycle | PASS | log `DynamicTravelRestore` `Result={Success}` |
| 3 | Equip uses the existing Equipment Authority with the selected stable unique instance | `HSR.UI.Inventory.ViewModel.Commands.EquipThroughAuthority` | UE Automation integration | PASS | Inventory suite |
| 4 | Enhancement rejection retains the complete snapshot and does not consume materials | `HSR.UI.Inventory.ViewModel.Commands.EnhancementFailurePreservesSnapshot` | UE Automation failure path | PASS | Inventory suite |
| 5 | Unsupported Use/Disassemble/Enhance cases return typed unavailable and retain the complete snapshot | `HSR.UI.Inventory.ViewModel.Commands.UnsupportedPreserveSnapshot`, `HSR.UI.Inventory.ViewModel.UnsupportedActionsPreserveSnapshot` | UE Automation failure path | PASS | Inventory suite |
| 6 | Category, filter, sort, stable selection, and read-only Widget lifecycle remain green | `HSR.UI.Inventory.ViewModel.ClassificationSortSelection`, `HSR.UI.Inventory.Widget.ReadOnlyLifecycle` | UE Automation regression | PASS | Inventory suite |
| 7 | Shared FrontendNavigation route, focus, compensation, Back, and travel regressions remain green | `HSR.UI.FrontendNavigation` | UE Automation regression | PASS | 11/11 `Result={Success}` |

## Coverage and known gaps

The UE Automation target does not emit source coverage, so no coverage
percentage is claimed. The user has supplied Editor integration and core PIE
evidence; final manual mouse/gamepad visual and complete interaction experience
remains pending.

The repository's known `HSR.UI.ScreenLifecycle` CharacterDetail/Inventory/
TravelRestore fixture failures (including `bFocusSucceeds=false` and the
pre-existing Inventory attach hook issue) are outside this package's product
scope and were not reclassified here.

No Git checkpoint commit was created because the user explicitly prohibited
automatic stage/commit/push operations. No `.claude/**` files were modified.

## Merge evidence

The code-gate evidence is Build success, 9/9 Inventory Automation, 11/11
FrontendNavigation Automation, and `git diff --check`. The remaining closeout
requires the user's Editor/PIE evidence; this report does not claim a PIE pass.
## User Editor and core PIE evidence

This latest section supersedes the earlier code-gate-only boundary above.

User-provided asset evidence:

- /Game/Data/Items/DA_InventoryCatalog_P17 is an HSRInventoryCatalog with 12
  real ItemIds, unique entries, non-empty DisplayNames, configured SortOrder,
  and successful Validate().
- /Game/UI/P17/Inventory/WBP_Inventory_P17 is reparented to
  UHSRInventoryModuleWidget and contains the 41-control HSR-style layout,
  Catalog/MappingCatalog/EnhancementCatalog assignments, and guarded category/
  search Blueprint entry points.
- BP_HSRHUD points InventoryModuleWidgetClass to WBP_Inventory_P17 while
  retaining the P13 fallback. BP_HSRPlayerController and
  IMC_FrontendNavigation retain the Inventory=B and CloseToRoot=X mappings.
- The allowlisted C++ widget additions provide pure-value Blueprint accessors,
  eager initialization, dynamic row creation/click bridging, detail projection,
  and action-button enablement. They do not modify Inventory/Equipment
  Authority.

User-provided core PIE evidence from Map_Exploration_P15_A:

- Inventory opens with HSRUI P17 Inventory RequestOpen Result=0 HasInventory=true.
- Back returns to Pause Hub with RequestBack Result=0 HasPause=true.
- X returns to exploration and re-adds the exploration input context.
- The reported run contains no Blueprint Runtime Error, Ensure, or invalid
  snapshot log. The SortBox U+25BE glyph warning was fixed.

The remaining user check is visual/manual: category, filter, sort, selection,
Stack/Unique detail, Equip/Enhance and unavailable actions, normal travel
restore, and failure snapshot preservation.

Latest closeout status: code, Editor integration, and core PIE are complete;
final visual acceptance is pending. No stage, commit, or push was performed.
