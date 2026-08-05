# TASK-P17-010A - Data-driven Challenge Directory

Status: `PASS / USER ACCEPTED`

## Sole observable outcome

The Challenge module projects a deterministic read-only directory from configured Encounter Definitions. An unlocked valid entry can build a pre-battle template; locked, duplicate, unknown, or invalid entries preserve the current UI and never submit travel.

## Authority boundaries

- Encounter definitions remain read-only DataAssets.
- Unlock state is read-only input supplied by the caller; this task does not invent Save authority.
- Challenge UI never calls OpenLevel or mutates Party, Inventory, Reward, Buff GameplayEffects, or BattleTransition pending/traveling state.
- BattleTransition remains the only template builder and encounter submission authority.

## Acceptance criteria

1. Directory projection is deterministic and exposes stable EncounterId, enemy ID, map path, availability, and diagnostic state.
2. Null definitions, duplicate IDs, missing IDs/enemy/map, locked entries, and unknown selections are controlled results.
3. Selection only resolves a definition; template construction delegates to BattleTransition.
4. Existing PreBattle and Admission tests remain green.
5. Development Editor Build and focused Automation pass.

## Allowlist

- `Source/HSR/UI/HSRChallengeDirectoryTypes.h`
- `Source/HSR/UI/HSRChallengeDirectoryViewModel.h`
- `Source/HSR/UI/HSRChallengeDirectoryViewModel.cpp`
- `Source/HSR/UI/HSRChallengeDirectoryWidget.h`
- `Source/HSR/UI/HSRChallengeDirectoryWidget.cpp`
- `Source/HSR/Tests/HSRChallengeDirectoryTests.cpp`
- `Content/UI/P17/Frontend/WBP_FrontendModuleRoot_P17.uasset`
- `tasks/active-task.md`
- `worklog.md`
- `PROJECT_STATE.md`
- `todo_plan.md`

## Explicit non-goals

- Dynamic progression locks, challenge completion Save data, costs, rewards, Buff effects, OpenLevel, or BattleTransition state inspection.
- Material/high-difficulty rule implementation or multiple challenge categories.
