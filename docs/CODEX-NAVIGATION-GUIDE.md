# Codex Navigation Guide

This guide supplements `.agents/agents.md`. It routes Codex to the smallest authoritative context and prevents broad repository scans from silently expanding task scope.

## Start here

High-level planning and coordination:

1. `PROJECT_STATE.md`
2. `.agents/agents.md`
3. `tasks/active-task.md`
4. `todo_plan.md`
5. latest relevant `worklog.md` entry
6. current Phase/patch execution plan
7. only the design documents and Source named by that plan

Implementation models read only `tasks/active-task.md` first and follow its exact read/write allowlist. Review models read the active task、execution result、review template、actual diff and named evidence.

## Domain ownership map

| Surface | Primary paths | Authority |
|---|---|---|
| Character/Progression | `Source/HSR/Character/`, `Progression/`, `Data/Definitions/HSRCharacter*` | Profile owns persistent progression; Actor/ASC is a World projection. |
| Interaction | `Source/HSR/Interaction/` | Component owns current candidate observation; target authority accepts intent. |
| Encounter/Battle | `Source/HSR/Encounter/`, `Battle/`, `Ability/`, `Status/`, `GAS/` | BattleTransition owns cross-map admission/return; Coordinator/Turn/GAS own battle-local rules. |
| Equipment/Relic | `Source/HSR/Equipment/`, related definitions | Equipment owns instances/slots/source projection; ASC owns applied runtime effects. |
| Inventory/Reward | `Source/HSR/Inventory/`, `Reward/` | Inventory owns item state; Reward owns transaction ledger/receipt. |
| Map/Travel | `Source/HSR/Map/`, `Travel/`, map definitions | Map owns location/unlocks/travel request; no Widget calls OpenLevel. |
| Save | `Source/HSR/Save/`, `docs/save-system-design.md` | Save owns envelope、validation、migration and global restore transaction. |
| UI | `Source/HSR/UI/`, `Player/HSRPlayerController.*` | UIManager owns session/input/focus; ViewModels read; Widgets submit intents only. |

Canonical cross-domain flow is documented in `docs/system-operation-flow.md` once P17-PATCH-03 Gate 0 is accepted.

## Asset ownership

- C++ and Markdown may be edited only when the active task authorizes exact paths.
- User owns Blueprint、UMG、DataAsset、GameplayEffect、InputAction/IMC、map and visual inspection work.
- Binary assets cannot be meaningfully line-diffed. Require path allowlist、author/provenance、Editor compile/save/reopen and PIE evidence.
- `Binaries/`、`Intermediate/`、`Saved/`、`DerivedDataCache/`、`.vs/` and local `.claude/` state are not source deliverables unless an explicit task says otherwise.

## PR/diff packet

Before stage/commit/review, report:

1. task/phase and one observable outcome;
2. `git status --short` and exact included/excluded files;
3. textual diff/stat and binary asset provenance;
4. Build/Automation/PIE/Editor evidence with truthful levels;
5. first real failure and unresolved `NOT VERIFIED` boundaries;
6. commit hash if a commit was explicitly authorized.

Never use `git add .` in a dirty HSR worktree. Stage exact task paths only; do not reset、clean、delete、rebase、push or rewrite history without explicit authorization.
