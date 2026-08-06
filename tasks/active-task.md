# TASK-P17-005R - Unified Frontend Module Host for Character and Inventory

Status: `COMPLETE`

## Sole observable outcome

Character and Inventory open through the same `FrontendModuleRoot` host path
used by Map, Party, Challenge, Quest, and Save. The host owns viewport
attachment, module route presentation, focus, Back/X lifecycle, and content
replacement. Character Detail and Inventory remain specialized content widgets
that own only their existing snapshot/ViewModel presentation contracts.

## Contract

- `UHSRFrontendModuleRootWidget` gains one optional named content host,
  `ModuleContentHost`, and only it attaches/removes specialized module content.
- `UHSRUIManagerSubsystem` creates the module root before a Character or
  Inventory content candidate, attaches the candidate to the root, then
  commits the route. It must roll back root, content, ViewModel, route, focus,
  and input policy on every failure.
- The user will add an `Overlay` or `CanvasPanel` named `ModuleContentHost` to
  `WBP_FrontendModuleRoot_P17`; Codex will not modify UAssets.
- No Party, Inventory, Character, Save, Battle, Map, or routing authority
  changes. Existing Character and Inventory ViewModels are not rewritten.

## Allowed files

- `Source/HSR/UI/Frontend/HSRFrontendModuleRootWidget.h`
- `Source/HSR/UI/Frontend/HSRFrontendModuleRootWidget.cpp`
- `Source/HSR/UI/HSRUIManagerSubsystem.h`
- `Source/HSR/UI/HSRUIManagerSubsystem.cpp`
- `Source/HSR/Tests/HSRFrontendNavigationTests.cpp`
- `tasks/active-task.md`
- `tasks/execution-result.md`

## Required checks

1. RED Automation proved Character and Inventory did not hold a module-root
   host.
2. GREEN `HSR.UI.FrontendNavigation` proves shared host ownership, replace,
   Back/X, and failure compensation.
3. Fresh Development Editor build and `git diff --check`.
4. User configured `ModuleContentHost`, compiled/saved/reopened, and validated
   Character, Inventory, Map, Back/X, and post-return Pause in PIE.

## Non-goals

- No UAsset edit by Codex, no visual redesign, no new data binding, no module
  registry/DataAsset, and no unrelated frontend refactor.
