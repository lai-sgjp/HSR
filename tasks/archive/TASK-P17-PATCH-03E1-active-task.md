# TASK-P17-PATCH-03E1 - Equipment Instance Ownership Foundation

Status: `COMPLETE / AUTOMATION GREEN / INDEPENDENT REVIEW PASS / USER EDITOR-PIE PASS`

## Sole outcome

`UHSREquipmentSubsystem` becomes the durable authority for every complete equipment/relic instance, independent of whether the instance is bagged or equipped. Loadouts store placement by InstanceId, snapshots resolve through the registry, and Save schema 7 round-trips registry plus placement without duplicated payload.

## Ownership contract

1. Equipment Registry owns exactly one immutable identity and current payload per equipment/relic instance: InstanceId, DefinitionId, Kind, enhancement level and modifiers.
2. Equipment placement owns only CharacterId + kind/slot -> InstanceId. It cannot own a second payload copy.
3. Inventory remains read-only in 03E1 and continues to own generic bag membership/capacity. Cross-domain movement is deferred to 03E2.
4. ASC/EffectBridge and Equipment Detail UI are read-only regressions in 03E1. Runtime equip projection is deferred to 03E2.
5. Save schema 7 stores registry records once and placement records separately. A registry instance may have zero or one placement.
6. Existing public loadout snapshots may still expose resolved `FHSREquipmentInstance` values, but these are projections copied from Registry, never authority.

## Schema and migration contract

- Increment `HSRSaveVersion::CurrentSchema` and default envelope/data schema from 6 to 7.
- Add explicit equipment registry and placement DTOs; do not overload Inventory unique DTOs.
- Schema 6 -> 7 migration converts every existing equipped row into one registry record plus one placement record with byte-equivalent payload.
- Schema 6 Inventory unique rows remain Inventory items. Migration must not guess that an item is equipment from DefinitionId.
- Schema 7 validation rejects duplicate registry IDs, duplicate placements, missing registry references, invalid definitions/kind/slot/enhancement/modifiers, one instance in multiple placements, and Inventory/placed-instance collisions.
- Canonical encoding sorts registry by InstanceId and placements by CharacterId/kind/slot; golden fixture/digest changes are intentional and must be regenerated only from deterministic test output.
- Restore prepares Registry and placements fully before mutation. Repeated restore is idempotent and preserves revisions.

## Compatibility contract

- Existing `Equip/Replace/Unequip/SetEnhancementLevel` behavior remains callable for regression compatibility during 03E1, but implementation must route all payload reads/writes through Registry.
- `Equip/Replace` may register a previously unseen valid instance only for legacy compatibility. This seam is not authorized for UI in 03E2.
- `Unequip` removes only placement; Registry retains the full payload.
- `SetEnhancementLevel` mutates the Registry payload once and publishes the affected loadout revision only when the instance is placed.
- Existing `GetLoadout`, stat aggregation, relic-set snapshots, Save projection and ViewModel behavior must remain compatible.

## Frozen minimum write allowlist

- `Source/HSR/Equipment/HSREquipmentTypes.h`
- `Source/HSR/Equipment/HSREquipmentSubsystem.h`
- `Source/HSR/Equipment/HSREquipmentSubsystem.cpp`
- `Source/HSR/Save/HSRSaveTypes.h`
- `Source/HSR/Save/HSRSaveVersion.h`
- `Source/HSR/Save/HSRSaveVersion.cpp`
- `Source/HSR/Save/HSRSaveSubsystem.cpp`
- new `Source/HSR/Tests/HSREquipmentInstanceRegistryTests.cpp`
- affected tests only when schema-7 compile/runtime RED requires exact updates: `HSREquipmentSubsystemTests.cpp`, `HSREquipmentSaveTests.cpp`, `HSREquipmentSaveProjectionTests.cpp`, `HSRSaveVersionTests.cpp`, `HSRSaveValidationTests.cpp`, `HSRInventoryRewardSaveTests.cpp`, and deterministic schema fixture manifest
- `tasks/execution-result.md`

Everything else is read-only, including Inventory implementation, Battle/EffectBridge, UI, Content and Config.

## Required TDD matrix

- register complete instance once; matching repeat is NoOp and changed-payload repeat conflicts;
- place by ID and resolve a loadout snapshot from Registry;
- unequip retains byte-equivalent enhancement/modifiers in Registry;
- replace changes placement while both old/new registry records remain intact;
- same InstanceId cannot occupy two characters/slots;
- stale/missing registry reference and incompatible kind/slot reject without revision/event;
- enhancement mutates the authoritative registry entry and resolved snapshot once;
- schema 7 canonical round-trip preserves unplaced and placed instances;
- schema 6 migration preserves every equipped payload and creates no registry entry for unrelated Inventory unique items;
- malformed/duplicate/orphan placement fails before live restore or projection;
- repeated Save restore is idempotent and exact projection remains stable;
- existing Equipment, Save, Inventory and Equipment Detail regressions remain green.

## Editor boundary

03E1 requires no Content mutation. User Editor work is limited to Save All/reopen of the existing equipment/relic definitions and one compatibility PIE observation if requested after C++ GREEN. No Blueprint may register instances or mutate placement.

## Non-goals

- No Inventory-to-Equipment movement transaction, OperationId ledger, capacity exchange, live equip UI, runtime ASC add/remove, new WBP, randomized roll generation or cloud work.
- No 03E2, 03F or later implementation.

## Current gate

The user accepted the Registry ownership model on 2026-07-29. Implementation, Build, focused and regression Automation, and Independent Review are `PASS`. The user-provided Editor/PIE log proves the existing exploration map starts, the player character is possessed with Enhanced Input mappings present, and PIE tears down normally. Character/Inventory/Equipment and Save/Load UI are not complete, so UI-driven equipment inspection and save/load observation are `NOT APPLICABLE / NOT VERIFIED`, not 03E1 failures. No Content mutation was required.
