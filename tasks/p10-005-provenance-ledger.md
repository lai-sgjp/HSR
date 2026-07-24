# P10-005 Dirty Provenance Ledger

Snapshot: 2026-07-24, `git status --short`; classification is by current path and declared delivery boundary, not authorship inference from Git.

| Class | Current paths / roots | Boundary |
|---|---|---|
| Implementation Source | `Source/HSR/Battle/HSRBattleCoordinator.{h,cpp}`, `HSRBattleGameMode.{h,cpp}`, `HSRBattleTransitionSubsystem.{h,cpp}`; `Source/HSR/GAS/Ability/HSRAbilityTypes.h`; `Source/HSR/UI/HSRBattleCommandTypes.h`, `HSRBattleCommandViewModel.{h,cpp}`, `HSRBattleCommandWidget.{h,cpp}` | Shared P10 implementation worktree; mixed staged/unstaged state, no phase closeout commit authorization. |
| User Content | `Content/Blueprints/Framework/BP_HSRBattleGameMode.uasset`; four `Content/Data/Skills/DA_*_Test.uasset`; `Content/UI/WBP_BattleCommandPanel.uasset`; `Content/Maps/Map_ProjectSetup.umap` | User/Editor-owned assets; no binary-field inference or agent modification claimed. |
| Phase Docs | `tasks/active-task.md`, `tasks/execution-result.md`, `tasks/final-review.md`, `tasks/archive/TASK-P10-*`, `tasks/p10-001a-teacher-review.md`, `tasks/p10-005-provenance-ledger.md`, `docs/phase-10-execution-plan.md` | Phase 10 records; mixed Coordinator/Implementation/Reviewer/Teacher containers preserve embedded ownership. |
| Other Docs / Prior Work | `docs/phase-7-execution-plan.md`, `learn/GAS.md`, `.agents/CLAUDE.md -> .claude/CLAUDE.md`, `.claude/skills/hsr-ui/**`, `.mcp.json`, `.tastemaker/style-lock.md` | Outside P10 closeout; do not stage as a Phase 10 delivery. |
| Unrelated tools/plugins/cache | `Plugins/UnrealMCPython/**`, `Tools/mcp-server/**`, `Plugins/UnrealMCPython/**/__pycache__/**`, `.claude/settings.local.json` | External/tooling/cache scope; excluded from P10 closeout. |
| Unresolved | No additional P10 runtime/content path is classified unresolved in this snapshot. Mixed staged/unstaged ownership inside the listed shared Source and document containers remains a Git-delivery boundary, not a claim of sole authorship. |

This ledger does not authorize staging, committing, reverting, cleaning, deleting, or changing any listed path.

## Proposed stage / commit grouping (Coordinator decision required)

| Group | Candidate scope | Recommendation |
|---|---|---|
| Implementation Source | Only the listed `Source/HSR/**` P10 paths, after final Source diff review | Separate implementation commit; do not include root task containers or assets. |
| User Content | Listed `Content/**` Battle GameMode, Skill DA, WBP, and map paths | User-owned asset commit only; preserve User Provided evidence boundary. |
| Coordinator Docs | `README.md`, `todo_plan.md`, `PROJECT_STATE.md`, `worklog.md`, `learning-journal.md`, `docs/phase-10-execution-plan.md`, `docs/ui-guidelines.md`, P10-005 index/ledger and closeout records | Final Coordinator documentation commit after Reviewer Gate. |
| Reviewer / Teacher archives | `tasks/archive/TASK-P10-001*`, `TASK-P10-001A-teacher-review.md`, `TASK-P10-003-teacher-review.md`, `TASK-P10-004*`, `TASK-P10-005-teacher-review.md` | Preserve role-labelled records; stage only after Coordinator validates no embedded authorship is overwritten. |
| Excluded unrelated tools/plugins/cache | `Plugins/UnrealMCPython/**`, `Tools/mcp-server/**`, `.claude/**`, `.mcp.json`, `.tastemaker/**`, `__pycache__/**` | Exclude from every P10 delivery commit. |

Shared root containers (`tasks/active-task.md`, `tasks/execution-result.md`, `tasks/final-review.md`) contain embedded Implementation, Reviewer, Teacher, and Coordinator facts. Do not stage them wholesale in a role commit; the final Coordinator commit may carry the complete container only after the embedded records and provenance ledger are reviewed.
