# TASK-P17-009C-A - Pure Pre-Battle Candidate and Encounter Request

Status: `ACTIVE / TASK GATE`

## Canonical mapping

Prerequisite slice of Phase 17 `P17-009 Party 与战前编队/Buff`. P17-009A is read-only Party projection; P17-009B is permanent-party editing. This task establishes the independent pre-battle candidate contract without starting travel.

## Sole observable outcome

The frontend can hold an independent candidate Party and optional Buff IDs, cancel without changing permanent Party/resources/BattleTransition, and confirm into a pure-value `FHSREncounterRequest` without invoking `RequestEncounter` or mutating transition state.

## Authority boundaries

- Permanent Party remains owned by `UHSRPartySubsystem`.
- Encounter travel remains owned by `UHSRBattleTransitionSubsystem`; this task never calls `RequestEncounter`.
- Candidate and Buff selection are local pure values owned by the ViewModel.
- Encounter definition data is read-only input; no Blueprint hard-coded CharacterId/EncounterId values.

## Acceptance criteria

1. Candidate starts from the committed Party snapshot but remains independent after edits.
2. Candidate validates slot bounds, duplicate/unknown CharacterIds, missing EncounterId and invalid candidate state.
3. Optional Buff IDs are local and deterministic; no resource or GameplayEffect mutation exists in this slice.
4. Cancel restores the candidate from the latest committed Party without changing Party revision or BattleTransition state.
5. Confirm returns a pure `FHSREncounterRequest` containing candidate identity and encounter definition fields; no travel or pending transition mutation occurs.
6. RED/GREEN automation and Development Editor Build pass.

## Initial allowlist

- `Source/HSR/UI/HSRPreBattleCandidateTypes.h`
- `Source/HSR/UI/HSRPreBattleCandidateViewModel.h`
- `Source/HSR/UI/HSRPreBattleCandidateViewModel.cpp`
- `Source/HSR/Tests/HSRPreBattleCandidateTests.cpp`
- `tasks/active-task.md`
- `tasks/execution-result.md`
- `PROJECT_STATE.md`
- `todo_plan.md`
- `worklog.md`

User-owned future assets, not edited by Codex in this slice:

- `Content/UI/P17/Frontend/WBP_HSRPreBattlePanel_P17.uasset`
- `Content/Data/Definitions/DA_Encounter*.uasset`

## Explicit non-goals

- Calling BattleTransition, OpenLevel, consuming resources, applying Buff GameplayEffects, Challenge Directory route, stage Buff authority, battle admission, or travel failure handling.
- Replacing the existing committed-party `RequestEncounter` path.

## TDD order

1. Add pure candidate/request RED tests and execute them.
2. Implement minimal DTO/ViewModel and execute the same tests GREEN.
3. Hand off future UMG wiring only after code evidence passes.
