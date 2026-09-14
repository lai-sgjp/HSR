# Codex Navigation Guide

This guide supplements `.agents/agents.md`. It routes Codex to the smallest authoritative context and prevents broad repository scans from silently expanding task scope.

## Start here

Use the current user request as the task scope. For a focused fix or document edit, read the target files and relevant rules directly.

- For phase planning or resuming project work, read `PROJECT_STATE.md` and the relevant phase plan; consult `todo_plan.md` or recent `worklog.md` entries only to resolve missing or conflicting state.
- Use relevant sections of `.agents/agents.md` for project constraints.
- When the user explicitly adopts the task-card workflow, read `tasks/active-task.md` and honor its allowlist. For a formal review, add the actual diff and named evidence; load review templates only if that workflow requires them.
- Read domain design documents when the affected behavior crosses those boundaries. These are routes, not a mandatory reading stack.

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

- C++ and Markdown edits must stay within the current user-authorized task. In an explicitly adopted task-card workflow, honor its exact path allowlist.
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
