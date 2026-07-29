# TASK-P17-PATCH-03E2 Task Gate Review

Status: `REVISE / SAVE-SCOPE AUTHORIZATION REQUIRED`

Coordinator pre-review found an unresolved authority gap: no explicit Item-to-Equipment mapping exists in the current definition types. No 03E2 Gate PASS or implementation authorization exists.

## Independent Reviewer

Verdict: `REVISE`

- The blocker is confirmed in code: `UHSRItemDefinition` exposes only `ItemId/StorageKind/MaxStack`, `UHSREquipmentDefinition` exposes only `DefinitionId/Slot/EnhancementCap`, and Inventory/Equipment expose independent registration and mutation seams with no authoritative cross-map bridge.
- The task card correctly identifies, but does not yet freeze, mapping owner, stable key, validation rules, Save schema impact and Editor boundary.
- Execution evidence consistently shows no 03E2 Source, test, Content, Build or PIE work has started.
- Recommendation: keep `REVISE / PLANNING BLOCKER`; resolve the five mapping decisions before any implementation or Gate PASS.

## User authorization update

The user subsequently confirmed the dedicated static Item-to-Equipment/Relic mapping catalog scheme and authorized implementation. The prior planning blocker is resolved for implementation scope; independent review remains pending for the RED/GREEN implementation.

## Build boundary update

UE5.6 elevated Build reached C++ compilation but failed on existing test/API mismatches outside the 03E2 allowlist (`HSRSaveValidationTests.cpp`, `HSRCharacterDetailViewModelTests.cpp`). The new mapping files were not reported as the failure source. Verdict remains blocked until an explicit regression allowlist decision is made; no implementation GREEN is claimed.

Subsequent log audit found the same mismatch across Battle, Status, EquipmentEffect and EquipmentSaveProjection test seams. This confirms a pre-existing compile-guard/regression-surface issue, not a mapping implementation failure.

## 03E2 implementation re-review

Verdict: `REVISE`

- P0: schema-7 global Save validation rejects a bagged equipment instance because `ExportSaveData` includes every Registry payload while `Validate` treats Registry InstanceIds and Inventory unique InstanceIds as mutually exclusive. The 03E2 model intentionally requires the same InstanceId in Registry payload and Inventory membership after unequip/replace. Focused Save tests do not execute this successful movement -> `SaveSnapshot` path.
- P1: authored mapping DataAssets can bypass `AddMapping` through public `EditAnywhere` rows; forward/reverse lookup selects the first duplicate instead of rejecting a duplicate/conflicting ItemId or EquipmentDefinitionId.
- P1: the new movement projection contract is bound only by Automation tests. Runtime Battle wiring binds Restore projection only, so no live ASC/EffectBridge movement projection is proven. EffectBridge application/removal remains fallible after domain commit.
- P2: successful operations are written to the replay ledger after Inventory/Equipment/projection callbacks, so a synchronous reentrant same-OperationId request cannot receive the cached replay result.
- P2: invalid Blueprint-supplied movement intent enum values take the Replace path instead of returning `InvalidRequest`.

Verification evidence:

- Final `HSREditor Win64 Development`: PASS (up to date, exit 0).
- `HSR.Equipment`: 12/12 PASS; `HSR.Inventory`: 3/3 PASS; `HSR.Save`: 16/16 PASS; `HSR.UI.EquipmentDetail`: 2/2 PASS.
- These results do not close the P0 global schema-7 movement Save path or the listed independent-review findings.

Required authorization boundary:

- Resolving P0 requires changing `UHSRSaveSubsystem::Validate` schema-7 ownership validation. The active task instructions explicitly prohibit Save schema/Save scope modification, so no corrective implementation was performed after this finding.

## Authorized Save-scope correction re-review

Verdict: `REVISE`

- The authorized schema-7 correction is complete: unplaced Registry payload plus matching Inventory membership now Load/Save round-trips; direct mapping duplicates, invalid movement intent and delegate-time OperationId replay also have RED/GREEN checkpoints.
- P1 remains: no production caller binds `SetMovementProjection`; Battle runtime registers Restore projection only. Movement can therefore commit without an ASC/EffectBridge update, and the test-only callback does not prove preflightable source handles or a non-fallible post-commit EffectBridge path.
- P1 remains: schema-7 Save validation permits a shared unplaced Registry/Inventory InstanceId without validating the explicit static Item-to-Equipment mapping. A save containing `Registry(Id, Equipment.A)` and `Inventory(Id, Item.B)` can restore when both definitions exist, leaving poisoned authority for later movement rejection.
- Closing these findings requires an explicit BattleCoordinator/EffectBridge integration scope and a Save mapping-catalog validation dependency plus malformed-save test. Neither is a safe local extension of the current transaction implementation.

Post-authorization evidence:

- `HSREditor Win64 Development`: PASS.
- `HSR.Equipment`: 12/12 PASS; `HSR.Save`: 16/16 PASS after the authorized fixes.
- These broad suite logs predate the last small mapping/reentrant commits; the focused GREEN logs cover those checkpoints, but a final full-suite rerun remains required once the two P1 findings are resolved.

## Final Independent Review

Verdict: `PASS`

- The production Mapping Catalog is strongly retained, loads from the committed DataAsset, and schema-7 shared Registry/Inventory identities validate ItemId, EquipmentDefinitionId, Kind, and Slot compatibility.
- `ExecuteMovement` runs the fallible runtime projection apply stage before Inventory/Equipment installation. Actual Apply/Remove failure returns `ProjectionRejected` before any domain mutation; only the notification callback remains after domain publication.
- Runtime tests cover successful equip/replace/unequip, actual Apply/Remove injected failure, source preservation, and zero domain pollution. Final Automation regression is GREEN: Equipment 13/13, Inventory 3/3, Save 16/16, EquipmentDetail 2/2.
- Independent review at `1d2c0ce` found no remaining P0/P1 issue. Editor/PIE observation remains a manual validation gap only; it is not a code gate failure.
