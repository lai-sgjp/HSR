# TASK-P17-PATCH-03B - Production Bootstrap and Character Identity

Status: `PLANNED / TASK GATE REQUIRED / USER CONFIRMATION REQUIRED`

## Role Lock

This card does not authorize implementation. The next action is Task Gate review only. Source, Content, Config, Build, Automation, Editor work and Git commit remain prohibited until the reviewed card is presented and the user explicitly confirms execution of `TASK-P17-PATCH-03B`.

## Current Evidence

- PATCH-03A is archived as `PASS WITH FOLLOW-UP`; Frontend navigation is no longer the blocker.
- Real PIE reaches Character but logs `P11-005 DetailWidgetInit Result=FAIL SelectionResult=6`, mapped to `PartySlotEmpty`.
- The Exploration Pawn exists as the World projection, but a stable CharacterId is not yet proven consistent across Definition, Profile, Party, Pawn and Character UI.

## Sole Outcome

From a clean Exploration start, the project resolves one stable selected CharacterId and Character Detail displays a valid Profile/Party/Equipment read model. Empty or invalid bootstrap data produces a controlled unavailable result with zero partial mutation and no hardcoded UI fallback.

## Candidate Source Allowlist - Not Yet Frozen

- `Source/HSR/Framework/HSRGameModeBase.h/.cpp`
- `Source/HSR/Character/HSRCharacterBase.h/.cpp`
- `Source/HSR/Character/HSRExplorationCharacter.h/.cpp`
- `Source/HSR/Progression/HSRCharacterProfileSubsystem.h/.cpp`
- `Source/HSR/Party/HSRPartySubsystem.h/.cpp`
- `Source/HSR/UI/HSRCharacterDetailWidget.h/.cpp`
- `Source/HSR/UI/HSRCharacterDetailViewModel.h/.cpp`
- One new focused test under `Source/HSR/Tests/`, exact path to be frozen by Task Gate.
- `tasks/execution-result.md`

Task Gate must reduce this candidate list to the exact minimal write allowlist after tracing current bootstrap and identity ownership. Files not frozen in the reviewed card are read-only.

## Candidate Editor Assets - Separate Asset Gate Required

- Existing GameMode/default pawn references.
- Existing Character Catalog and initial Character/Party configuration.
- `Content/Blueprints/Character/Player/BP_HSRExplorationCharacter.uasset` only if provenance and exact required binding are approved.
- Existing Character WBP references and presentation fields.
- No map modification is authorized by this planned card.

## Contract to Freeze at Task Gate

1. Definition owns immutable design data; Profile owns persistent progression; Party owns roster and selection; Pawn/ASC is a World projection; ViewModel reads committed snapshots only.
2. Bootstrap resolves a configured stable CharacterId. Widget, Pawn name, array index and display text cannot invent identity.
3. All fallible Definition/Profile/Party validation precedes publication. Empty party, missing definition, invalid owner and stale selection leave prior committed state unchanged.
4. Existing saves remain authoritative and are not overwritten by new-game defaults.
5. Repeated bootstrap and UI rebuild are idempotent and do not duplicate Profile/Party entries or revisions.
6. Blueprint cannot create Profile/Party records, mutate progression or supply a hidden fallback CharacterId.

## Acceptance Matrix

- New game: initial CharacterId matches Profile, Party, Exploration Pawn and Character read model.
- Existing save: restored selection is preserved; defaults do not overwrite it.
- Empty initial party: controlled unavailable result, no crash and no partial record creation.
- Missing Character Definition: structured rejection before Profile/Party/Pawn/UI publication.
- Wrong owner or stale selection: rejection with old committed selection preserved.
- Repeated initialization and UI reopen: no duplicate entries, revision increments or delegate bindings.
- Editor Save All/reopen: authorized references persist.

## Required Evidence

- Fresh `HSREditor Win64 Development` Build with UHT/compile/link/metadata truthfully reported.
- Focused new tests plus affected Progression, Save and Character Detail regressions discovered by Task Gate.
- User PIE: new game -> Character happy path and one intentionally empty-party failure fixture.
- Actor/Profile/Party/UI CharacterId and relevant revisions must match.
- Physical controller, Standalone, Packaged and Shipping remain outside this task unless separately authorized.

## Explicit Non-goals

- No Character creation/customization, party editing UI, leveling, equipment mutation, inventory mutation or save schema redesign.
- No changes to battle, interaction, map travel, settlement or Frontend navigation architecture.
- No new plugin/module, third-party asset, broad Blueprint repair or map edit.
- Do not begin PATCH-03C or later packages.

## Current Gate

`TASK GATE REQUIRED`. After review, the Implementation role must first restate the frozen card and stop with: `等待用户确认执行 TASK-P17-PATCH-03B。`
