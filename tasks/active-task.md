# TASK-P17-PATCH-03E - Inventory and Equipment Integration Task Gate

Status: `PLANNED / TASK GATE REQUIRED / IMPLEMENTATION NOT AUTHORIZED`

## Candidate outcome

Inventory-owned equipment instances can be equipped and unequipped through one authoritative transaction, update the selected character's equipment projection exactly once, and remain coherent across UI refresh, retry and failure.

## Gate boundary

- This card currently authorizes evidence gathering and Task Gate planning only.
- No Source, Content, Config, Build, Automation, PIE or asset mutation is authorized.
- The Task Gate must inspect current Inventory, Equipment, Profile, Battle projection, Save DTO and UI read-model ownership before freezing an implementation allowlist.
- 03D2 is archived as `PASS`; its SettlementAuthority contract must not be weakened or repurposed as Equipment authority.
- Do not begin 03F or later work packages.

## Required Task Gate decisions

- Freeze the authoritative equipment instance identity, ownership and slot transaction.
- Define Inventory quantity/instance interaction without duplicating Equipment state.
- Define selected-character/profile identity and Battle ASC projection refresh semantics.
- Define equip, unequip, replace, duplicate request, stale revision and rollback behavior.
- Define UI intent/read-model boundaries; Widgets may not mutate Inventory, Equipment or ASC directly.
- Determine whether Save DTO compatibility is read-only regression scope or requires an explicitly separate schema task.
- Reduce the candidate write set to exact files and freeze a TDD matrix before requesting implementation authorization.

## Current gate

03E Task Gate has been opened after 03D2 final `PASS`. Evidence review has not started; implementation remains unauthorized.
