# TASK-P17-PATCH-03B Independent Review

Status: `TASK GATE REVISE / REVISION 2 NARROW RE-REVIEW REQUIRED`

## First Task Gate Review

`REVISE`

- Freeze the exact Source and new test paths before user confirmation.
- Define Definitions -> save-or-new-game branch -> Party -> Pawn projection ordering so defaults cannot overwrite restored state.
- Remove the Widget-owned `Character.A` fallback and prove UI identity comes from committed Party selection.
- Defer unsupported Equipment read-model scope to PATCH-03E.
- Replace vague regression discovery with exact new and existing Automation names.

## Revision 1 and 2 response

- Exact write paths and the new Automation file are frozen.
- Existing Profile/Party/Save/ViewModel surfaces are read-only; their current seams are sufficient.
- Bootstrap now has explicit `NewGameDefaults` versus `UseCommittedRuntime` modes and typed results. It does not claim disk loading; PATCH-03G owns slot selection and cold load.
- Catalog/InitialCharacter validation precedes Profile registration; Party commit precedes Pawn projection; Widget selection comes only from committed Party slot 0.
- Equipment aggregation is explicitly deferred to 03E and the test/run matrix is exact.

Revision 2 is awaiting narrow Reviewer, Teacher and Implementation feasibility re-review. This file does not authorize implementation.
