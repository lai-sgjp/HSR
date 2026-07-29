# TASK-P17-PATCH-03E2 - Atomic Equipment Movement and Runtime Projection

Status: `TASK GATE PREPARATION ONLY / IMPLEMENTATION NOT AUTHORIZED`

## Gate boundary

03E1 is archived. This card is only a placeholder for evidence-driven 03E2 Task Gate preparation. Before any Source, test, Content, Build, Automation or PIE work, the Gate must freeze the aggregate transaction across Inventory membership, Equipment Registry/placement, expected Inventory and Equipment revisions, OperationId replay behavior, capacity/replace semantics, delayed publication, ASC runtime projection and UI intent boundaries.

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
