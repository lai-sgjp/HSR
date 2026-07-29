# TASK-P17-PATCH-03E2 - Atomic Equipment Movement and Runtime Projection

Status: `TASK GATE REVISE / PLANNING BLOCKER / IMPLEMENTATION NOT AUTHORIZED`

## Gate boundary

03E1 is archived. This card freezes the evidence-driven 03E2 Task Gate scope. Before any Source, test, Content, Build, Automation or PIE work, the Gate must freeze one aggregate transaction across the existing Inventory `AddUnique/RemoveUnique` boundary, Equipment Registry/placement, expected Inventory and Equipment revisions, OperationId replay behavior, capacity/replace semantics, delayed publication, ASC runtime projection and UI intent boundaries.

## Proposed authority contract

- A pure-value request carries OperationId, CharacterId, InstanceId, source membership intent, target kind/slot and expected Inventory/Equipment revisions.
- Inventory remains authoritative for bag membership/capacity; Equipment remains authoritative for complete payload and placement.
- A bagged equipment instance is linked by the same `InstanceId` plus an explicit validated Definition mapping; Registry `Kind`/payload and Inventory storage kind must agree. DefinitionId similarity alone is never sufficient, and a missing or conflicting mapping rejects the request.
- A bounded command/transaction boundary owned by the Equipment/Inventory integration surface validates both revisions, instance identity, source membership, target slot, capacity and ownership before mutating either subsystem. It may own only OperationId, step and pure candidates; it is not a new global data authority.
- Commit order is candidate-first and publication is delayed until both domains commit. Any pre-commit failure leaves both domains and both revisions unchanged.
- OperationId replay returns the cached result and produces no second membership, placement, revision, delegate or ASC effect mutation. A reused OperationId with a different request is rejected.
- The integration boundary returns a typed pure-value result containing OperationId, result code, committed/no-op status, old/new Inventory revision, old/new Equipment revision, displaced InstanceId when applicable and a diagnostic step. Existing domain result enums remain domain-local and are not overloaded for cross-domain outcomes.
- Replacing an occupied slot explicitly returns the displaced InstanceId to Inventory in the same transaction; capacity is checked against the net candidate, not sequential intermediate state.
- ASC/EffectBridge receives a post-commit resolved aggregate only. Projection validity and required source-effect handles must be preflighted before commit; if the projection cannot be proven ready, the transaction is rejected with zero domain mutation. Commit must not depend on a fallible post-commit compensation path.
- Equipment Detail ViewModel/Widget remains a read/intent presentation layer: it may subscribe to the single committed revision and rebuild one resolved snapshot, but it cannot own OperationId, mutate Inventory/Equipment, apply/remove GE or serialize Save data.

## Current planning blocker

The existing definitions do not provide the required explicit mapping: `UHSRItemDefinition` contains only `ItemId`, `StorageKind` and `MaxStack`, while `UHSREquipmentDefinition` contains only `DefinitionId`, `Slot` and `EnhancementCap`. No authoritative Item-to-Equipment catalog or mapping field currently exists. The Gate must choose and freeze the mapping owner, stable mapping key, validation rules, Save schema impact and Editor asset boundary before implementation. Guessing by matching names, prefixes or `DefinitionId` is prohibited.

## Required failure matrix

- stale Inventory revision; stale Equipment revision; mismatched pair;
- missing/foreign/duplicate InstanceId; Inventory unique item versus Registry payload collision;
- invalid CharacterId, kind, slot, definition or occupied slot;
- insufficient capacity for unequip/replace net result;
- duplicate OperationId replay and same OperationId with changed request;
- OperationId cache scope, bounded retention/restore policy and stale callback rejection;
- Inventory candidate/installation failure, Equipment candidate/installation failure, delegate publication failure and ASC projection preflight failure;
- save/restore boundary while a candidate is pending; no half-committed aggregate may be serialized.

## Required TDD matrix

- bag -> equip moves membership and placement exactly once;
- equip -> unequip returns the same InstanceId to Inventory while Registry payload remains byte-equivalent;
- replace atomically displaces the old InstanceId and places the new one with net capacity validation;
- stale revisions and invalid requests produce zero mutation and zero publication;
- replayed OperationId is cached/no-op; changed payload under the same OperationId is rejected;
- every successful commit publishes one Inventory revision and one Equipment revision, then one derived ASC projection;
- injected domain/projection preflight failure leaves a recoverable, observable result with no split authority;
- save/restore after each successful transaction round-trips membership, Registry payload and placement.

## Proposed implementation allowlist for Gate review only

- `Source/HSR/Inventory/HSRInventorySubsystem.h/.cpp` and `HSRItemTypes.h` only for the minimum candidate/commit seam;
- `Source/HSR/Equipment/HSREquipmentSubsystem.h/.cpp` and `HSREquipmentTypes.h` only for the minimum candidate/commit seam;
- one bounded integration command/request/result type in the owning subsystem boundary; no global manager or new runtime module;
- `Source/HSR/Equipment/HSREquipmentEffectBridge.h/.cpp` only for derived source-effect projection with preflightable failure behavior;
- `Source/HSR/Equipment/HSREquipmentStatAggregator.h/.cpp` and `HSRRelicSetResolver.h/.cpp` only for deterministic candidate/read-model aggregation;
- `Source/HSR/UI/HSREquipmentDetailViewModel.h/.cpp`, `HSREquipmentDetailTypes.h`, `HSREquipmentDetailWidget.h/.cpp` only for one committed-snapshot refresh and typed presentation events;
- exact new regression tests for the matrix above;
- task evidence Markdown only.

Inventory, Equipment, Save, Battle, SettlementAuthority, UI and Content changes remain unauthorized until the Gate is independently reviewed and the user separately confirms implementation.

## Frozen inheritance

- Equipment Registry permanently owns complete equipment/relic instance payloads.
- Inventory owns bag membership and capacity only.
- Loadout owns InstanceId placement only.
- ASC is a derived runtime projection.
- Save schema 7 keeps Registry and Placement separate.
- `03D2 SettlementAuthority` remains unchanged and must not be weakened or repurposed.

## Prohibited until separate user confirmation

- No Inventory or Equipment production mutation.
- No OperationId ledger, capacity exchange or atomic movement implementation.
- No ASC dynamic equipment effects.
- No equipment UI operation or new/modified Content asset.
- No 03F or resumed P17 Editor work.

## Editor boundary for the Gate

The candidate asset set is limited to existing `/Game/Data/Relics/DA_Relic_*`, `/Game/Data/RelicSets/DA_RelicSet_P12_A`, `GE_Equipment_P12`, `GE_Relic_P12`, `GE_RelicSet_P12_A`, `/Game/UI/WBP_Inventory_P13` and `/Game/UI/WBP_EquipmentDetail_P12`. User work, if those assets are available, is Compile/Save All/reopen and PIE observation of equip -> replace -> unequip plus wrong-owner/incompatible/stale failures. Blueprint may bind selection, intents and committed snapshots only; it may not create ownership, mutate slots, apply/remove GE or save/load.

## Next gate

Prepare and independently review the exact 03E2 ownership, failure, TDD, allowlist and Editor contracts. A Gate `PASS` does not authorize implementation; implementation requires a separate user confirmation.
