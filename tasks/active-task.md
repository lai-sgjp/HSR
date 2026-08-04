# TASK-P17-009B — Permanent Party Candidate Editing and Confirmation

Status: `ACTIVE / TASK GATE`

## Plan mapping

This is the next prerequisite slice of canonical `P17-009 Party 与战前编队/Buff`. P17-009A supplied the accepted read-only projection. P17-009B does not complete the pre-battle formation/Buff portion of P17-009.

## Sole observable outcome

The player can create a candidate permanent party from authoritative available characters, add/remove/replace/swap slots, and confirm it through Party authority; cancelling or any rejected confirmation leaves the current permanent party unchanged.

## Authority and safety

- PartySubsystem remains the only owner of the permanent party.
- ViewModel owns only a local pure-value candidate and submits one confirmation intent.
- Widget never mutates PartySubsystem arrays directly and never treats a candidate as committed state.
- Confirmation must validate duplicates, invalid CharacterIds, slot bounds and revision/stale-candidate behavior before installation.
- Existing valid party remains intact on every failure path.

## Acceptance criteria

1. Available-character projection comes from the authoritative profile/character source, not hard-coded Blueprint IDs.
2. Add/remove/replace/swap affect only the candidate before confirmation.
3. Valid confirmation updates the permanent party exactly once and publishes one coherent snapshot.
4. Invalid, duplicate, stale or cancelled candidates leave the previous permanent party and revision unchanged.
5. Party UI still supports Empty/Unavailable, Back/Close and travel rebuild.
6. Focused Party tests, existing Party tests, FrontendNavigation regression and Development Editor Build pass.
7. User compiles/saves the derived UMG and verifies edit, confirm, cancel, failure feedback, Back/Close and reopen in PIE.

## Initial inspection allowlist

- `Source/HSR/Party/**`
- `Source/HSR/Character/**`
- `Source/HSR/Progression/**`
- `Source/HSR/UI/HSRPartyViewModel.h`
- `Source/HSR/UI/HSRPartyViewModel.cpp`
- `Source/HSR/UI/HSRPartyWidget.h`
- `Source/HSR/UI/HSRPartyWidget.cpp`
- `Source/HSR/Tests/HSRPartyFrontendTests.cpp`
- existing Party/Profile tests and named Phase 11/17 documentation

No production write allowlist is granted until inspection freezes the smallest API surface and RED test matrix.

## Explicit non-goals

- Pre-battle candidate party, stage Buff, Encounter Request or BattleTransition changes (future P17-009C).
- Four-character expansion, party presets, drag/drop polish, Main Menu, Settings, or Phase 18 presentation.

## TDD gate

Inspect the existing Party/Profile authority first, freeze exact files, then add and execute a valid RED before changing production behavior.
