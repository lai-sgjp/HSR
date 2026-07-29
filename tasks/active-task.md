# TASK-P17-PATCH-03E2 - Atomic Equipment Movement and Runtime Projection

Status: `TASK GATE PREPARATION ONLY / INDEPENDENT REVIEW REQUIRED / IMPLEMENTATION NOT AUTHORIZED`

## Gate boundary

03E1 is archived. This card freezes the evidence-driven 03E2 Task Gate scope. Before any Source, test, Content, Build, Automation or PIE work, the Gate must freeze one aggregate transaction across the existing Inventory `AddUnique/RemoveUnique` boundary, Equipment Registry/placement, expected Inventory and Equipment revisions, OperationId replay behavior, capacity/replace semantics, delayed publication, ASC runtime projection and UI intent boundaries.

## Proposed authority contract

- A pure-value request carries OperationId, CharacterId, InstanceId, source membership intent, target kind/slot and expected Inventory/Equipment revisions.
- Inventory remains authoritative for bag membership/capacity; Equipment remains authoritative for complete payload and placement.
- A coordinator-owned candidate validates both revisions, instance identity, source membership, target slot, capacity and ownership before mutating either subsystem.
- Commit order is candidate-first and publication is delayed until both domains commit. Any pre-commit failure leaves both domains and both revisions unchanged.
- OperationId replay returns the cached result and produces no second membership, placement, revision, delegate or ASC effect mutation. A reused OperationId with a different request is rejected.
- Replacing an occupied slot explicitly returns the displaced InstanceId to Inventory in the same transaction; capacity is checked against the net candidate, not sequential intermediate state.
- ASC/EffectBridge receives a post-commit resolved aggregate only. Projection failure invokes the frozen compensation policy; it must not become a second authority or partially publish a placement.

## Required failure matrix

- stale Inventory revision; stale Equipment revision; mismatched pair;
- missing/foreign/duplicate InstanceId; Inventory unique item versus Registry payload collision;
- invalid CharacterId, kind, slot, definition or occupied slot;
- insufficient capacity for unequip/replace net result;
- duplicate OperationId replay and same OperationId with changed request;
- Inventory commit failure, Equipment commit failure, delegate publication failure and ASC projection failure;
- save/restore boundary while a candidate is pending; no half-committed aggregate may be serialized.

## Required TDD matrix

- bag -> equip moves membership and placement exactly once;
- equip -> unequip returns the same InstanceId to Inventory while Registry payload remains byte-equivalent;
- replace atomically displaces the old InstanceId and places the new one with net capacity validation;
- stale revisions and invalid requests produce zero mutation and zero publication;
- replayed OperationId is cached/no-op; changed payload under the same OperationId is rejected;
- every successful commit publishes one Inventory revision and one Equipment revision, then one derived ASC projection;
- injected domain/projection failure leaves a recoverable, observable result with no split authority;
- save/restore after each successful transaction round-trips membership, Registry payload and placement.

## Proposed implementation allowlist for Gate review only

- `Source/HSR/Inventory/HSRInventorySubsystem.h/.cpp` and `HSRItemTypes.h` only for the minimum candidate/commit seam;
- `Source/HSR/Equipment/HSREquipmentSubsystem.h/.cpp` and `HSREquipmentTypes.h` only for the minimum candidate/commit seam;
- one new aggregate coordinator/request type in the owning subsystem boundary;
- `Source/HSR/Equipment/HSREquipmentEffectBridge.*` only for post-commit projection/compensation integration;
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

## Next gate

Prepare and independently review the exact 03E2 ownership, failure, TDD, allowlist and Editor contracts. A Gate `PASS` does not authorize implementation; implementation requires a separate user confirmation.
