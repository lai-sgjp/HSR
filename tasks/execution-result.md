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

## Failure matrix: displaced mapping TDD RED

- Extended `HSR.Equipment.Movement.Transaction.ReplaceNetCapacity` with a conflicting reverse mapping for the displaced instance: the incoming mapping is valid, while the displaced Equipment definition is explicitly mapped to the wrong slot.
- The test requires `MappingRejected`, zero Inventory/Equipment mutation, stable revisions and zero domain delegates. It then retries the same OperationId with the canonical catalog to prove failed requests are not cached.
- `HSREditor Win64 Development` compiled and linked the test successfully (6/6 actions, exit 0).
- `Saved/Logs/03E2-FailureMatrix-DisplacedMapping-RED.log`: valid runtime RED. The test executed and failed because the conflicting mapping committed, advanced both revisions, changed placement and published both delegates. No production code changed before this RED was confirmed.

## Failure matrix: displaced mapping GREEN

- Replace now validates the displaced reverse mapping against the Registry payload, request kind/slot and registered Equipment definition before preparing either domain candidate.
- Missing or incompatible displaced mappings return `MappingRejected`; placement/owner/Registry failures remain `EquipmentRejected`.
- `HSREditor Win64 Development` passed (6/6 actions, exit 0).
- `Saved/Logs/03E2-FailureMatrix-DisplacedMapping-GREEN.log`: the same `ReplaceNetCapacity` test passed 1/1, including zero publication on rejection and successful same-OperationId retry with the canonical catalog.

## Domain preflight failure matrix TDD RED

- Added `HSR.Equipment.Movement.FailureMatrix.DomainPreflight` for stale Inventory revision, stale Equipment revision, mismatched revision pair, foreign owner, wrong slot/placement, missing Registry, missing mapping, Inventory/Registry payload collision, capacity rejection, zero mutation/publication and failed-OperationId reuse.
- `HSREditor Win64 Development` compiled and linked the matrix (4/4 actions, exit 0 after the diagnostic expectation correction).
- `Saved/Logs/03E2-FailureMatrix-DomainPreflight-RED.log`: valid runtime RED with one failing assertion. Wrong expected placement did not return `EquipmentRejected`; the current Unequip path prepares Inventory addition first and collapses the duplicate membership into `InventoryRejected`. All later zero-pollution assertions remained GREEN.
- No production code changed before this RED was confirmed.

## Domain preflight failure matrix GREEN

- Unequip now validates Character ownership and exact expected placement before preparing the Inventory addition candidate, preserving `EquipmentRejected` diagnostics for wrong-owner/placement requests.
- `HSREditor Win64 Development` passed (6/6 actions, exit 0).
- `Saved/Logs/03E2-FailureMatrix-DomainPreflight-GREEN.log`: `HSR.Equipment.Movement.FailureMatrix.DomainPreflight` passed 1/1. The matrix proves stable Inventory/Equipment revisions, membership, placement, Registry payload and zero delegates for every covered rejection; the corrected capacity request with the same OperationId then committed and published each domain once.

## Projection preflight TDD RED

- Extended the bag/equip journey to require a resolved candidate-loadout preflight, typed `ProjectionRejected`, zero mutation/publication on rejection, failed-OperationId reuse and ordered `Inventory -> Equipment -> Projection` publication after a successful retry.
- `HSREditor Win64 Development` produced the intended compile RED (exit 6): `SetMovementProjection`, `FMovementProjectionPreflight`, `FMovementProjectionCommit` and `ProjectionRejected` do not exist. The new test is the first reported failure source; no unrelated regression precedes it.
- No production code changed before this RED was confirmed.

## Projection preflight GREEN

- Added an optional paired movement projection contract. A missing half or rejected resolved candidate returns `ProjectionRejected` before either domain installs state; an unbound pair preserves existing movement behavior.
- The post-commit callback is non-fallible and executes only after Inventory and Equipment publication, so movement does not introduce a fallible compensation path after installation.
- `HSREditor Win64 Development` passed with UHT, compile, lib/dll link and metadata (15/15 actions, exit 0).
- `Saved/Logs/03E2-ProjectionPreflight-GREEN.log`: the same `BagToEquip` test passed 1/1, proving rejection zero pollution, failed-OperationId reuse and ordered `Inventory -> Equipment -> Projection` publication.

## Restore ledger lifecycle TDD RED

- Extended the successful movement journey to export and commit the same schema-7 Registry/Placement candidate, then issue the pre-restore request again.
- `HSREditor Win64 Development` passed (6/6 actions, exit 0).
- `Saved/Logs/03E2-RestoreLedger-RED.log`: valid runtime RED. `CommitRestore` retained the transient ledger, so the old OperationId returned cached success with `bReplay=true` instead of revalidating the restored Inventory/Equipment authority.
- No production code changed before this RED was confirmed.

## Restore ledger lifecycle GREEN

- Both legacy and schema-7 `CommitRestore` paths now clear `MovementLedger` and `MovementLedgerOrder` after installing restored authority. Projection wiring and Save schema remain unchanged.
- `HSREditor Win64 Development` passed (5/5 actions after the exact expectation correction, exit 0).
- `Saved/Logs/03E2-RestoreLedger-GREEN-02.log`: the same `BagToEquip` test passed 1/1. The old OperationId is no longer replayed and is rejected by current Inventory revision validation.

## Pending candidate and persistence characterization coverage

- Projection preflight now observes both the resolved candidate loadout and a concurrent read of committed authority. The candidate contains the requested placement while Inventory snapshot and schema-7 Equipment export still expose the pre-commit bag membership, Registry payload and empty Placement.
- Replace coverage now round-trips Inventory membership plus schema-7 Registry/Placement and rechecks displaced InstanceId/ItemId, both enhancement payloads and the new equipped placement.
- These characterization assertions passed immediately; no RED was manufactured and no production change was needed.
- `HSREditor Win64 Development` passed (4/4 actions, exit 0). `Saved/Logs/03E2-Movement-Persistence-Coverage.log`: `HSR.Equipment.Movement` passed 4/4, exit 0.

## ViewModel committed snapshot characterization coverage

- Added `HSR.UI.EquipmentDetail.MovementRefresh` using the aggregate movement API rather than legacy direct Equipment mutation.
- Successful equip and unequip each rebuild exactly one committed snapshot with the Movement result revision; OperationId replay emits no second ViewModel refresh.
- The characterization passed immediately; no production UI or Content change was needed.
- `HSREditor Win64 Development` passed (6/6 actions, exit 0). `Saved/Logs/03E2-ViewModel-MovementRefresh.log`: `HSR.UI.EquipmentDetail` passed 2/2, exit 0.

## Schema-7 bagged equipment Save TDD RED

- After explicit user authorization to expand Save scope, extended `HSR.Save.Validation.Preflight` with a schema-7 Registry payload and Inventory membership sharing the same InstanceId while no Placement exists. Legacy schema-6 duplicate ownership remains required to reject.
- `HSREditor Win64 Development` passed (6/6 actions, exit 0).
- `Saved/Logs/03E2-Schema7-BaggedSave-RED.log`: valid runtime RED. `LoadSnapshot` returned `InvalidData`, and the bagged Registry/Inventory records did not round-trip.
- No production Save code changed before this RED was confirmed.

## Schema-7 bagged equipment Save GREEN

- Schema-7 validation now separates Registry payload identity, equipped Placement identity and Inventory membership identity. Inventory membership may reference a Registry payload only while it has no Placement; duplicate Inventory IDs and any Placement/Inventory overlap still reject. Legacy schema ownership rules are unchanged.
- `HSREditor Win64 Development` passed (6/6 actions, exit 0).
- `Saved/Logs/03E2-Schema7-BaggedSave-GREEN.log`: `HSR.Save.Validation.Preflight` passed 1/1. The bagged Registry payload and Inventory membership now Load/Save round-trip with the same InstanceId.

## Mapping conflict and invalid intent TDD RED

- Added direct authored duplicate mapping coverage, bypassing `AddMapping`, plus an invalid Blueprint-supplied movement intent to the domain preflight matrix.
- `HSREditor Win64 Development` passed (6/6 actions, exit 0).
- `Saved/Logs/03E2-MappingIntent-RED.log`: valid runtime RED. Mapping forward/reverse lookup selected the first duplicate; invalid intent did not return `InvalidRequest`. The other Movement tests remained GREEN.
- No production code changed before this RED was confirmed.

## Mapping conflict and invalid intent GREEN

- Forward and reverse mapping resolution now reject any directly authored duplicate key instead of selecting a first row. `ExecuteMovement` rejects enum values outside Equip/Unequip/Replace as `InvalidRequest` before candidate preparation.
- `HSREditor Win64 Development` passed (5/5 actions, exit 0).
- `Saved/Logs/03E2-MappingIntent-GREEN-02.log`: `HSR.Equipment.Movement` passed 4/4, including direct duplicate mapping and invalid intent coverage.

## Reentrant OperationId ledger TDD RED

- Added a synchronous same-OperationId replay from the Inventory commit delegate during the first successful bag-to-equip transaction.
- The first compile attempt exposed a missing test delegate parameter and was corrected without production changes. The corrected `HSREditor Win64 Development` build passed (4/4 actions, exit 0).
- `Saved/Logs/03E2-ReentrantLedger-RED.log`: valid runtime RED. The reentrant request did not return cached success or `bReplay=true` because the successful ledger entry was recorded after publication.
- No production code changed before this RED was confirmed.

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
