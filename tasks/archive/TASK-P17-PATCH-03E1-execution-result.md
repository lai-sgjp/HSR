# TASK-P17-PATCH-03E1 Task Gate Evidence

Status: `TASK GATE PASS / IMPLEMENTATION NOT AUTHORIZED`

- User accepted Equipment Registry as complete instance authority.
- Current Save schema is 6 and encodes complete payload only in equipped rows; schema 7 is required to preserve unplaced instances.
- Existing schema-6 equipped rows can migrate losslessly into registry + placement.
- Inventory unique rows cannot safely be inferred as equipment and remain unchanged during migration.
- 03E1 excludes Inventory transactions, ASC live changes and UI routing; those remain 03E2.
- No Source, Content, Config, Build, Automation or PIE mutation was performed during this gate.

## 2026-07-29 implementation checkpoint

Status: `BLOCKED AT REGRESSION ALLOWLIST`

- Gate commit: `4510bcd`.
- RED commit: `fccfad1`; first UBT failure was the missing Registry API surface.
- `HSREditor Win64 Development` builds successfully.
- Registry Ownership, Placement, Persistence and all `HSR.Equipment` Automation tests pass.
- Save Validation Preflight, DiskFailures and EnvelopeV1 pass.
- Full `HSR.Save` remains red only where old tests hard-code schema 5 or treat schema 7 as future.
- Required updates touch `HSRMapSaveIntegrationTests.cpp`, `HSRQuestDialogueTests.cpp` and `HSRSaveRecoveryTests.cpp`, which are outside the frozen allowlist.
- No GREEN commit or completion claim has been made.

## 2026-07-29 GREEN result

Status: `IMPLEMENTATION GREEN / REVIEW REQUIRED`

- The user expanded the regression allowlist for Map, Quest and Recovery schema assertions.
- `HSREditor Win64 Development`: succeeded.
- `HSR.Equipment.Registry`: 3/3 succeeded.
- `HSR.Equipment`: 8/8 succeeded.
- `HSR.Save`: 16/16 succeeded.
- `HSR.Inventory`: 3/3 succeeded.
- `HSR.UI.EquipmentDetail.ViewModel`: 1/1 succeeded.
- `git diff --check`: clean for every 03E1 implementation, test and evidence path.
- Editor/PIE compatibility observation remains a post-review user boundary; 03E1 requires no Content mutation.

### Supplemental audit correction

- Removed legacy `SchemaVersion=5` coercion after successful primary/backup envelope decoding; disk restore now preserves migrated schema 7 Registry data.
- Legacy `Equip/Replace` now rolls back a newly registered instance when placement fails.
- Rebuilt successfully; Registry 3/3 and Save 16/16 reran successfully with zero failure lines.

### Reviewer REVISE correction

- Schema-7 decode now enforces strict placement order by `CharacterId`, `Kind`, then `Slot` and rejects duplicate slots, invalid kind/slot ranges and negative revisions as `NonCanonical`.
- Added a byte-level negative codec test that swaps two serialized placement rows.
- Build succeeded; focused VersionClassification and full Save 16/16 succeeded.

## 2026-07-29 Editor/PIE boundary

Status: `USER EDITOR-PIE PASS WITH EVIDENCE LIMITATION`

- User confirmed the exploration flow starts, the player character is possessed, and movement/input are normal.
- The provided Output Log records `Map_Exploration_P15_B` PIE creation, `BP_HSRGameMode_C` load, `BP_HSRExplorationCharacter_C_0` possession, Enhanced Input mappings and bindings, successful `Character.A` bootstrap, normal PIE teardown, and no Equipment Registry/Save restore error.
- `LogAbilitySystem` reports the existing non-blocking fallback that no `GameplayCueNotifyPaths` are configured; this is outside the 03E1 write boundary and is not attributed to the Registry patch.
- Current Character/Inventory/Equipment UI and Save/Load UI are not complete. UI-driven resolved equipment inspection and PIE save/load comparison are therefore `NOT APPLICABLE / NOT VERIFIED`; no temporary Widget, Blueprint, command, or Content mutation was introduced to manufacture that evidence.
- Registry, schema-7 round-trip, schema-6 migration, placement validation, disk recovery and Equipment Detail ViewModel compatibility remain covered by the recorded Automation evidence.
