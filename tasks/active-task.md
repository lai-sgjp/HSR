# TASK-P17-010C - Challenge Directory Progression Projection

Status: `IMPLEMENTED / C++ GREEN / USER EDITOR GATE PENDING`

## Sole observable outcome

Challenge Directory projects `Available`, `Locked`, `Completed`, and
`Unavailable` from runtime challenge progression and Encounter definition
data. Static `FHSRChallengeDirectorySource::bUnlocked` is compatibility-only
and does not decide runtime status. Only `Available` continues through the
existing Pre-Battle and BattleTransition path.

## Frozen ownership and data flow

- `UHSREncounterDefinition` owns static Encounter identity, prerequisites,
  BattleMap, Buff, Reward, and EXP definitions.
- `UHSRChallengeProgressionSubsystem` owns completed Encounter IDs, revision,
  completion idempotence, and progression restore/capture DTOs.
- Challenge Directory ViewModel/Widget owns only the transient projection and
  selected ID; it does not own completion state or unlock authority.
- BattleGameMode submits completion only after SettlementAuthority returns
  `Success` or `NoOp` for victory. Defeat never completes a challenge.
- SaveSubsystem owns the Save envelope and restore transaction; it does not
  become the progression authority.

## Status rules

`Unavailable > Completed > Locked > Available`.

Empty prerequisites are available. All prerequisites must be completed for an
entry to be available. Unknown prerequisite references and incomplete runtime
definitions are unavailable. Completed entries are display-only in 010C;
repeat/reward replay rules are explicitly deferred.

## Exact implementation allowlist

- `Source/HSR/Challenge/HSRChallengeProgressionTypes.h`
- `Source/HSR/Challenge/HSRChallengeProgressionSubsystem.h`
- `Source/HSR/Challenge/HSRChallengeProgressionSubsystem.cpp`
- `Source/HSR/Data/Definitions/HSREncounterDefinition.h`
- `Source/HSR/UI/HSRChallengeDirectoryTypes.h`
- `Source/HSR/UI/HSRChallengeDirectoryViewModel.h`
- `Source/HSR/UI/HSRChallengeDirectoryViewModel.cpp`
- `Source/HSR/UI/HSRChallengeDirectoryWidget.h`
- `Source/HSR/UI/HSRChallengeDirectoryWidget.cpp`
- `Source/HSR/Save/HSRSaveTypes.h`
- `Source/HSR/Save/HSRSaveSubsystem.h`
- `Source/HSR/Save/HSRSaveSubsystem.cpp`
- `Source/HSR/Save/HSRSaveVersion.h`
- `Source/HSR/Save/HSRSaveVersion.cpp`
- `Source/HSR/Battle/HSRBattleGameMode.h`
- `Source/HSR/Battle/HSRBattleGameMode.cpp`
- `Source/HSR/Tests/HSRChallengeProgressionTests.cpp`
- `Source/HSR/Tests/HSRChallengeDirectoryTests.cpp`
- `Source/HSR/Tests/HSRSaveSubsystemTests.cpp`
- `Source/HSR/Tests/HSRSaveVersionTests.cpp`
- `Source/HSR/Tests/HSRSaveValidationTests.cpp`
- `Source/HSR/Tests/HSRSaveColdRecoveryTests.cpp`
- `Source/HSR/Tests/HSRBattleSettlementIntegrationTests.cpp`

## Explicitly read-only

Reward, Inventory, Character Profile, Quest, Map, Party, BattleTransition,
009D, 010A/010B production filtering/failure behavior, FrontendModuleRoot,
`ModuleContentHost`, Pause input `1`, and all unrelated Content/Config/module
files.

## User Editor gate

User may modify only the locked Encounter DataAsset and the Challenge Directory
and Entry widgets. Configure `DA_Encounter_Phase5Test1_Locked` with prerequisite
`Enc_Test_Phase5`; preserve existing Reward, Buff, BattleMap, and IDs. Keep the
three existing directory sources, compile/save/reopen, and bind all four status
presentations. Do not modify the unified frontend root.

## TDD and verification plan

1. Add progression, projection, save, settlement, and failure-matrix tests.
2. Run the focused target in RED before production implementation.
3. Implement the smallest runtime slice and rerun the same target GREEN.
4. Run fresh Development Editor Build, focused Automation, approved regressions,
   `git diff --check`, then collect user Editor/PIE evidence.
5. Record unverified physical-controller, Standalone, Packaged, and Shipping
   checks honestly; do not claim them from keyboard PIE.

## RED evidence

`HSREditor Win64 Development` was executed with the new tests. It failed on
the intended missing implementation: `HSRChallengeProgressionSubsystem.h`,
`FHSRSaveData::ChallengeProgression`, and schema-8 interfaces do not exist
yet. The test-only fixture assertion issue was corrected before the GREEN run.

## Non-goals

No new reward grant, resource deduction, Buff rule, battle rule, repeat reward,
new module, Config change, UI root refactor, or direct AddToViewport path.

## Implementation evidence

- RED checkpoint: `24e3f35`.
- Fresh `HSREditor Win64 Development` build passed.
- Combined `HSR.Battle+HSR.Challenge+HSR.Save+HSR.UI.ChallengeDirectory`
  Automation passed 36/36.
- User Editor prerequisite/status binding and PIE acceptance remain pending;
  no UAsset was modified by Codex.
