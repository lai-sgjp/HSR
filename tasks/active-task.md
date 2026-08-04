# TASK-P17-009C-B - Admission Build/Submit Separation

Status: `ACTIVE / TASK GATE`

## Canonical mapping

Prerequisite slice of Phase 17 `P17-009 Party 与战前编队/Buff`. P17-009A is read-only Party projection; P17-009B is permanent-party editing; P17-009C-A supplied a pure candidate ViewModel. This task separates pure request construction from authoritative transition submission while preserving the existing convenience API.

## Sole observable outcome

Pure admission input can be converted into a complete `FHSREncounterRequest` without World/Party/Reward/Travel side effects; the existing `RequestEncounter` remains a compatibility wrapper around the separated build/submit boundary.

## Authority boundaries

- Permanent Party remains owned by `UHSRPartySubsystem`.
- Encounter travel remains owned by `UHSRBattleTransitionSubsystem`; this task never calls `RequestEncounter`.
- Candidate and Buff selection are local pure values owned by the ViewModel.
- Encounter definition data is read-only input; no Blueprint hard-coded CharacterId/EncounterId values.

## Acceptance criteria

1. Candidate starts from the committed Party snapshot but remains independent after edits.
2. Candidate validates slot bounds, duplicate/unknown CharacterIds, missing EncounterId and invalid candidate state.
3. Optional Buff IDs are local and deterministic; no resource or GameplayEffect mutation exists in this slice.
4. Pure build carries candidate leader, encounter fields and Buff IDs without changing Party revision or BattleTransition state.
5. Existing RequestEncounter behavior remains covered by the prior admission tests.
6. RED/GREEN automation and Development Editor Build pass.

## Initial allowlist

- `Source/HSR/UI/HSRPreBattleCandidateTypes.h`
- `Source/HSR/UI/HSRPreBattleCandidateViewModel.h`
- `Source/HSR/UI/HSRPreBattleCandidateViewModel.cpp`
- `Source/HSR/Battle/HSREncounterTypes.h`
- `Source/HSR/Battle/HSRBattleTransitionSubsystem.h`
- `Source/HSR/Battle/HSRBattleTransitionSubsystem.cpp`
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

1. Add pure admission build RED tests and execute them.
2. Implement DTO/build boundary and execute the same tests GREEN.
3. Extract submit/travel side effects without changing existing wrapper behavior.
