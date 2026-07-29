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
