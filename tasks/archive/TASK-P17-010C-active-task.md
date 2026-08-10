# TASK-P17-010C - Challenge Directory Progression Projection

Status: `ARCHIVED / USER ACCEPTED / INDEPENDENT REVIEW NOT RUN`

## Sole observable outcome

Challenge Directory projects `Available`, `Locked`, `Completed`, and
`Unavailable` from runtime challenge progression and Encounter definition
data. Static `FHSRChallengeDirectorySource::bUnlocked` is compatibility-only;
only `Available` continues through the existing Pre-Battle and
BattleTransition path.

## Ownership and data flow

- `UHSREncounterDefinition` owns static Encounter identity, prerequisites,
  BattleMap, Buff, Reward, and EXP definitions.
- `UHSRChallengeProgressionSubsystem` owns completed Encounter IDs, revision,
  completion idempotence, and progression restore/capture DTOs.
- Challenge Directory ViewModel/Widget owns only transient projection and
  selection; it does not own completion state or unlock authority.
- BattleGameMode completes an Encounter only after SettlementAuthority returns
  `Success` or `NoOp` for victory. Defeat never completes it.
- SaveSubsystem owns the Save envelope and restore transaction; it does not
  become the progression authority.

## Delivered scope

- Runtime progression projection and prerequisite-based status priority:
  `Unavailable > Completed > Locked > Available`.
- Save schema 8 capture/restore and conservative migration from older schema.
- Victory settlement completion with idempotent progression updates.
- Blueprint-safe `Status` projection for the Directory Entry presentation.

## Implementation allowlist

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

## Evidence boundary

- RED checkpoint: `24e3f35`.
- GREEN implementation: `82a0b98`.
- Fresh `HSREditor Win64 Development` Build passed.
- Combined `HSR.Battle+HSR.Challenge+HSR.Save+HSR.UI.ChallengeDirectory`
  Automation passed `36/36`.
- User PIE confirmed three projected entries, valid Stage Buff application,
  Phase5 victory, prerequisite unlock, locked-entry victory, and successful
  return travel. The `invalid buff` failure no longer reproduced.
- Independent review was not run; the task is user-accepted, not claimed as
  independently reviewed.

## Explicit non-goals and remaining boundaries

No new reward grant, resource deduction, Buff rule, battle rule, repeat reward,
new module, Config change, UI root refactor, or direct AddToViewport path was
added. Standalone, Packaged, Shipping, physical controller, and network
behavior remain `NOT VERIFIED`. P17-005 final acceptance remains separate.
