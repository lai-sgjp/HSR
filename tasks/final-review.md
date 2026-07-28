# TASK-P17-PATCH-03E1 Implementation Review

Reviewed range: `4510bcd..6fe6268`

Status: `PASS`

## Findings

No blocking findings remain.

The previous `REVISE` item is resolved. Schema-7 placement decode now validates each placement row for valid GUIDs, kind range, slot range, non-negative revision, and strict canonical ordering by `CharacterId`, `Kind`, then `Slot` before accepting the payload (`Source/HSR/Save/HSRSaveVersion.cpp:111`). The encoder already sorts by the same canonical keys before writing placements (`Source/HSR/Save/HSRSaveVersion.cpp:97`), so alternate placement ordering is no longer accepted as a second byte representation.

## Evidence

- Equipment Registry is the complete instance authority: `RegisterInstance` owns idempotency/conflict checks, `EquipById` / `ReplaceById` place only by `InstanceId`, and legacy `Equip` / `Replace` roll back a newly registered instance if placement fails (`Source/HSR/Equipment/HSREquipmentSubsystem.cpp:120-190`).
- Unequip and replace preserve registry payloads, and enhancement mutates the registered payload before publishing the affected loadout revision (`Source/HSR/Equipment/HSREquipmentSubsystem.cpp:197-232`).
- Schema-7 save projection exports registry rows separately from placement rows, sorted deterministically (`Source/HSR/Equipment/HSREquipmentSubsystem.cpp:26-40`).
- Schema-7 validation rejects duplicate registry IDs, duplicate placements/slots, missing registry references, and Inventory/placed-instance collisions before restore mutation (`Source/HSR/Save/HSRSaveSubsystem.cpp:159-162`).
- Restore prepares all domain candidates and equipment projection before committing live state (`Source/HSR/Save/HSRSaveSubsystem.cpp:197-210`), while equipment `PrepareRestore` rejects duplicate/orphan/incompatible placement before `CommitRestore` (`Source/HSR/Equipment/HSREquipmentSubsystem.cpp:55-61`).
- Schema-6 migration converts equipped rows into registry plus placement rows and does not infer equipment from Inventory unique rows (`Source/HSR/Save/HSRSaveVersion.cpp:117`; covered by `Source/HSR/Tests/HSREquipmentInstanceRegistryTests.cpp:143-163`).
- The codec regression now includes a byte-level negative test that swaps two serialized 44-byte placement records and expects `NonCanonical` (`Source/HSR/Tests/HSRSaveVersionTests.cpp:62`).

## Verification

- `git diff --check 4510bcd 6fe6268`: clean.
- Local log evidence: `HSR.Save.VersionClassification` found 1 test and completed with exit code 0 in `Saved/Logs/HSR-backup-2026.07.28-17.26.58.log`.
- Local log evidence: full `HSR.Save` found 16 tests and completed with exit code 0 in `Saved/Logs/HSR.log`.
- `tasks/execution-result.md:47-49` records the reviewer correction plus Build, focused VersionClassification, and full Save 16/16 success.

## Notes

- The combined implementation still includes the Map, Quest, and Recovery schema assertion updates that were outside the original frozen allowlist; `tasks/execution-result.md:29` records the user-expanded regression allowlist for those files.
- Existing dirty worktree entries are unrelated Content/local files; this review edits only `tasks/final-review.md`.

## Verdict

`PASS`
