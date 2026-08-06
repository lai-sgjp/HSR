# TASK-P17-005 - Frontend Shell Final Acceptance After Party Bootstrap

Status: `READY / FINAL ACCEPTANCE GATE`

## Context

P17-005 previously reached a code-gate pass and a user PIE checkpoint, but
final acceptance was paused because Character Detail resolved to
`PartySlotEmpty`. P17-009A/009B now provide the Party projection and permanent
candidate confirmation, and the accepted 009D PIE route proves battle return
and post-return Pause reopen. This task closes only the remaining P17-005
evidence boundary; it must not reopen Party, Challenge, Battle, or Save
authority.

## Sole observable outcome

In Exploration, the existing Frontend Shell opens the correct Pause, Party,
Map, Challenge, and Character routes; one Frontend pause session is retained
through module navigation; Back returns one level; `X` closes to Exploration;
repeated input has no duplicate side effects; and after a successful battle or
map return, Exploration input, cursor, focus, and the user's `1` Pause mapping
remain usable. Character Detail must no longer fail solely because the Party
bootstrap is empty.

## Verification scope

- Existing P17-005 Frontend transaction implementation and Automation only.
- Current Party/Challenge/Map/return integrations are read-only evidence
  dependencies; do not change their authority contracts.
- User-owned UAssets remain outside Codex ownership. No new or modified UAsset,
  Config, Build.cs, Save schema, module, dependency, or Git delivery work.

## Exact Codex allowlist

- `Source/HSR/UI/Frontend/HSRFrontendRouteTypes.h`
- `Source/HSR/UI/Frontend/HSRFrontendRouter.h`
- `Source/HSR/UI/Frontend/HSRFrontendRouter.cpp`
- `Source/HSR/UI/Frontend/HSRFrontendShellWidget.h`
- `Source/HSR/UI/Frontend/HSRFrontendShellWidget.cpp`
- `Source/HSR/UI/Frontend/HSRFrontendModuleRootWidget.h`
- `Source/HSR/UI/Frontend/HSRFrontendModuleRootWidget.cpp`
- `Source/HSR/UI/HSRUIManagerSubsystem.h`
- `Source/HSR/UI/HSRUIManagerSubsystem.cpp`
- `Source/HSR/UI/HSRScreenWidget.h`
- `Source/HSR/UI/HSRScreenWidget.cpp`
- `Source/HSR/UI/HSRScreenStackTypes.h`
- `Source/HSR/UI/HSRScreenStack.h`
- `Source/HSR/UI/HSRScreenStack.cpp`
- `Source/HSR/UI/HSRInputModeCoordinator.h`
- `Source/HSR/UI/HSRInputModeCoordinator.cpp`
- `Source/HSR/UI/HSRHUD.h`
- `Source/HSR/UI/HSRHUD.cpp`
- `Source/HSR/Player/HSRPlayerController.h`
- `Source/HSR/Player/HSRPlayerController.cpp`
- `Source/HSR/Tests/HSRFrontendNavigationTests.cpp`
- `tasks/active-task.md`
- `tasks/execution-result.md`

Production source edits are permitted only if a focused verification exposes
a P17-005 defect and the edit remains inside this allowlist. Otherwise this
task is evidence-only.

## Required evidence

1. Fresh `HSREditor Win64 Development` build.
2. `HSR.UI.FrontendNavigation` and focused Party/Challenge/Map/return
   regressions, all with explicit Success results.
3. `git diff --check`.
4. User Editor Save All, close/reopen, and PIE evidence for keyboard/mouse
   navigation, Character Detail, Back/X, post-return Pause using `1`, and
   1920x1080 plus 1280x720. Physical controller, Standalone, Packaged, and
   Shipping remain `NOT VERIFIED` unless separately demonstrated.

## Explicit non-goals

- No new frontend features, full Character progression, Save UI, Dialogue,
  Battle HUD, Gacha, Party mutation redesign, or Challenge rules.
- No claim of final PASS from automation alone; user Editor/PIE evidence and
  independent review remain separate gates.
