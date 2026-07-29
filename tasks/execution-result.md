# TASK-P17-PATCH-03E2 Task Gate Evidence

Status: `IMPLEMENTATION IN PROGRESS / REGRESSION BLOCKER RESOLVED / MAPPING CHECKPOINT GREEN`

- 03E1 is archived with Build, Automation, Independent Review and user Editor/PIE compatibility evidence.
- No 03E2 Source, test, Content, Build, Automation or PIE action has started.
- The draft now records the real Inventory (`AddUnique/RemoveUnique`, capacity, revision), Equipment (Registry/placement, replace/unequip, revision) and EffectBridge boundaries, plus candidate-first transaction, OperationId replay, net-capacity replace and post-commit projection requirements.
- Read-only definition audit found no explicit Item-to-Equipment mapping: `UHSRItemDefinition` has `ItemId/StorageKind/MaxStack`, and `UHSREquipmentDefinition` has `DefinitionId/Slot/EnhancementCap`. 03E2 cannot implement cross-domain movement until the Gate freezes an authoritative mapping owner and its persistence/Editor contract.

## Role handoff: Teacher pre-review

Status: `PASS WITH PLANNING FOLLOW-UP`

- Teacher confirms five decisions must be frozen before implementation: mapping owner, stable key, one-to-one validation, static-catalog versus Save persistence behavior, and exact Editor ownership/boundary.
- Recommended direction is a dedicated static Item-to-Equipment/Relic mapping catalog. Inventory remains membership/capacity authority; Equipment remains complete payload/placement authority; `InstanceId` remains runtime ownership identity; ASC and UI remain derived/read-only.
- Missing, duplicate, conflicting, wrong-storage-kind, missing-target or incompatible-slot mappings must reject before either domain mutates. Name, prefix, display text or DefinitionId similarity inference remains prohibited.
- Recommendation is not treated as user authorization; the Gate remains `REVISE / PLANNING BLOCKER` until the mapping contract is independently reviewed and accepted.

## User authorization and RED checkpoint

- User confirmed the dedicated static Item-to-Equipment/Relic mapping catalog scheme and authorized implementation.
- RED test added at `Source/HSR/Tests/HSREquipmentMovementTests.cpp` for explicit mapping insertion, duplicate rejection and lookup.
- Mapping catalog implementation is now limited to `Source/HSR/Data/Definitions/HSRItemEquipmentMappingCatalog.h/.cpp`; no Content asset has been created.

## 2026-07-29 UE5.6 Build attempt

- UE5.6 path: `E:\programs\Epic Games\UE_5.6`.
- First Build attempt was blocked by UBT user-cache permission; the identical command was rerun with elevated permission.
- Elevated `HSREditor` Build reached compilation but exited 6 on pre-existing test/API mismatches outside the 03E2 allowlist, including `HSRSaveValidationTests.cpp` and `HSRCharacterDetailViewModelTests.cpp` references to missing `InitializeForDevelopmentTest` and restore diagnostic APIs.
- No mapping-catalog compiler error was reported before the unrelated failures. No GREEN or Automation claim is made.
- Required next authorization is either a narrow regression-test allowlist expansion for those existing API mismatches or an alternate focused target that excludes them; no unrelated source or Content files were changed.

### Regression-surface audit

- Follow-up log inspection shows the failure is broader than the first two files: existing Battle, Status, EquipmentEffect, EquipmentSaveProjection and Save tests also reference development seams absent from current headers.
- Several seams are guarded by `#if WITH_EDITOR` while Automation tests compile under `WITH_DEV_AUTOMATION_TESTS`, so this is a historical test/compile-guard contract mismatch rather than a 03E2 mapping failure.
- Restoring that surface requires a separate, explicitly scoped regression-compatibility task or allowlist. 03E2 implementation remains paused after the mapping catalog checkpoint.

## Role handoff: Independent Reviewer

Verdict: `REVISE`

- Reviewer confirms the definition and subsystem seam evidence and agrees that no authoritative Item-to-Equipment mapping path exists.
- Reviewer confirms the execution packet is truthful: no 03E2 Source, test, Content, Build or PIE work has started.
- Reviewer recommends freezing mapping owner, stable key, validation, Save impact and Editor boundary before implementation or Gate PASS.

## 03R resolution and resumed 03E2 checkpoint

- `TASK-P17-PATCH-03R` restored the existing Automation development seams and is archived with final Independent Reviewer `PASS` (`3321bf4`) and Coordinator closeout `ba747e7`.
- Required `HSREditor Win64 Development` is GREEN after 03R; the historical regression blocker no longer blocks 03E2.
- `Saved/Logs/03E2-Mapping-Checkpoint.log`: `HSR.Equipment.Movement.MappingContract` completed 1/1 Success, exit 0.
- The committed catalog checkpoint is GREEN, not the remaining transaction RED. The next TDD RED must cover the authorized bag/equip/unequip/replace aggregate, dual revisions, replay, capacity and zero-mutation failure contract.
- No 03E2 production transaction, Content, Config, Save schema, SettlementAuthority or UI asset change is claimed by this checkpoint.

## Atomic movement TDD RED

- Added `HSR.Equipment.Movement.Transaction.BagToEquip` to require the pure-value movement request/result and aggregate execution boundary.
- `Saved/Logs/03E2-Transaction-RED-Build.log`: `HSREditor Win64 Development` exited 6 because `FHSREquipmentMovementRequest`, `EHSREquipmentMovementIntent`, `FHSREquipmentMovementResult`, `EHSREquipmentMovementResultCode`, and `UHSREquipmentSubsystem::ExecuteMovement` do not yet exist.
- The RED is intentional and target-specific: the new test compiled far enough to exercise the missing API contract; no unrelated source failure precedes it.
- Production code remains unchanged at this RED checkpoint.

## Bag-to-equip GREEN checkpoint

- Added the minimum candidate-first Inventory removal seam and Equipment aggregate `ExecuteMovement` entry for the authorized bag-to-equip slice.
- Both revisions are validated before mutation; explicit ItemId mapping, Registry definition/kind and target slot must agree.
- Candidate state is installed in both domains before revisions and delegates are published. Existing public `RemoveUnique`/`EquipById` mutation paths are not composed, avoiding an early one-domain broadcast.
- First runtime attempt was invalid test-fixture evidence: subsystem objects were created without a `UGameInstance` Outer and UE reported a handled ensure. The fixture was corrected without changing production behavior.
- `Saved/Logs/03E2-BagToEquip-GREEN-Build-02.log`: `HSREditor Win64 Development` succeeded, 5 actions, exit 0.
- `Saved/Logs/03E2-BagToEquip-GREEN-Automation-02.log`: `HSR.Equipment.Movement` 2/2 Success, exit 0.
- This checkpoint covers only bag-to-equip and mapping. Unequip, replace, OperationId replay, projection preflight, capacity/failure injection, Save round-trip and UI intent remain pending and are not claimed.

## OperationId TDD RED

- Extended the movement transaction test to require cached same-request replay, zero second publication/revision, and changed-request rejection under the same OperationId.
- `Saved/Logs/03E2-OperationId-RED-Build.log`: intentional compile RED on missing `FHSREquipmentMovementResult::bReplay` and `EHSREquipmentMovementResultCode::OperationIdConflict`; exit 6.
- No production code changed before this RED was confirmed.

## Replace GREEN checkpoint

- Added a dedicated Inventory swap candidate that removes the incoming membership and adds the displaced membership in one copied state, then validates only final net capacity.
- Replace validates incoming and displaced Registry payloads, both exact static mappings, expected revisions, owner and target placement before either domain installs state.
- Successful replace reports `DisplacedInstanceId`, returns the old mapped membership, places the new InstanceId, preserves both Registry payloads and advances/publishes both revisions exactly once.
- `Saved/Logs/03E2-Replace-GREEN-Build.log`: `HSREditor Win64 Development` succeeded, 11 actions, exit 0.
- The first Automation invocation produced no process/log because permission approval timed out; the identical command was retried once.
- `Saved/Logs/03E2-Replace-GREEN-Automation.log`: `HSR.Equipment.Movement` 3/3 Success, exit 0.
- Post-replace focused regressions are GREEN: `HSR.Inventory` 3/3, `HSR.Equipment.Registry` 3/3 and `HSR.Equipment` 11/11, all exit 0 with zero failed tests (`03E2-Replace-Regression-*.log`).

## OperationId GREEN checkpoint

- Added a bounded 128-entry successful-operation ledger owned only by the Equipment movement integration boundary; it is runtime transaction state, not Inventory/Registry authority and not Save schema data.
- Identical request replay returns the cached success result with `bReplay=true` and `bCommitted=false`; no revision or delegate advances.
- Reusing an OperationId with any changed request field returns `OperationIdConflict` with zero mutation.
- `Saved/Logs/03E2-OperationId-GREEN-Build.log`: `HSREditor Win64 Development` succeeded, 15 actions, exit 0.
- `Saved/Logs/03E2-OperationId-GREEN-Automation.log`: `HSR.Equipment.Movement` 2/2 Success, exit 0.
- Ledger restore/pending-save policy, unequip, replace, capacity and projection failure matrices remain pending.

## Unequip TDD RED

- Extended the successful bag-to-equip journey with equip-to-unequip requirements: return the same InstanceId/mapped ItemId to Inventory, remove placement, retain Registry payload/enhancement, and advance both revisions exactly once.
- The first run crashed only because the test indexed the expected membership after the unimplemented operation returned an empty array; assertion access was guarded and rerun without production changes.
- `Saved/Logs/03E2-Unequip-RED-Automation-02.log`: valid runtime RED, exit 255. Unequip was not committed, membership stayed absent, placement stayed present, and both revisions stayed unchanged; Registry retention remained intact.
- No production code changed before the valid runtime RED was confirmed.

## Unequip GREEN checkpoint

- Added an explicit catalog reverse lookup by exact `EquipmentDefinitionId`; it returns the unique configured mapping and performs no name/prefix/similarity inference.
- Added an Inventory addition candidate with definition/storage/capacity validation and no mutation/publication during preparation.
- Unequip validates Registry mapping, expected revisions, owner, exact placement and capacity before installing either candidate; successful commit removes placement, returns membership, retains Registry payload, advances both revisions once and publishes once per domain.
- `Saved/Logs/03E2-Unequip-GREEN-Build.log`: `HSREditor Win64 Development` succeeded, 13 actions, exit 0.
- `Saved/Logs/03E2-Unequip-GREEN-Automation.log`: `HSR.Equipment.Movement` 2/2 Success, exit 0.
- Replace and its net-capacity semantics remain pending.

## Replace TDD RED

- Added `HSR.Equipment.Movement.Transaction.ReplaceNetCapacity` with Inventory capacity fixed at one: the new instance fills the bag, while the old instance occupies the target slot.
- The required successful final state swaps bag membership and placement atomically, returns `DisplacedInstanceId`, retains both Registry payloads/enhancement levels and advances each revision once.
- `Saved/Logs/03E2-Replace-RED-Automation.log`: valid runtime RED, exit 255. Replace remained uncommitted; the new instance stayed in Inventory, the old instance stayed placed, revisions stayed unchanged, and Registry payload checks continued to pass.
- No production code changed before this RED was confirmed.
