# Phase 18 Execution Plan: Presentation and Authorized Asset Introduction

## Current Gate

`TASK-P18-PRESENTATION-001` passed its code contract gate. The current task is
`TASK-P18-DEMO-CONTENT-001`: freeze the formal content catalog and add read-only
asset validation before user Editor authoring. Teacher remains excluded. Codex
does not create UAsset/map binaries or change Gameplay authority here.

The handoff source is `docs/phase-20-demo-content-catalog.md`. The focused
`HSR.Demo.ContentCatalog` suite remains RED until the exact assets exist under
`/Game/Data/VerticalSlice`; old test assets cannot make it GREEN. After content
GREEN, proceed to Phase 19 and then Phase 20 final integration.

## Phase 18 outcome

Introduce presentation assets and mappings without changing Gameplay, Save,
Inventory, Equipment, Battle or TurnSystem authority. Presentation consumes
pure-value events and may fail back to placeholders without changing the
authoritative result.

## User-mandated Demo data boundary

The final playable Demo must not reuse current test naming or configuration as
its authored content. New Demo definitions must be created for maps, skills,
characters and relics under a dedicated Demo namespace/path. Existing assets
such as `DA_Encounter_Phase5Test`, `DA_BasicAttack_Single_Test`, `P11/P13/P15`
definitions and `P17` catalogs remain regression fixtures or historical data
until separately replaced and verified.

## Serial packages

1. `TASK-P18-PRESENTATION-001`: event/fallback contract and exact Demo naming
   boundary; code contract passed.
2. `TASK-P18-DEMO-CONTENT-001`: formal content catalog plus a read-only
   RED/GREEN asset gate.
3. `TASK-P18-PRESENTATION-002`: one authorized skeleton/animation/Montage and
   camera/UI presentation slice.
4. `TASK-P18-PRESENTATION-003`: audio, VFX, GameplayCue and hit feedback slice.
5. `TASK-P18-PRESENTATION-004`: duplicate event, interruption, low quality,
   missing asset, performance and Phase 18 closeout.

## First proposed vertical slice

One newly authored Demo basic-attack presentation mapping consumes an existing
successful `FHSRBattlePresentationEvent` Damage event and plays one attack/hit
presentation. It must not decide damage, advance turns, mutate Save, or own a
Gameplay result. Missing assets produce a placeholder/fallback and preserve the
same battle resolution.

## Current evidence

- `UHSRBattleCoordinator` already emits pure-value Damage/Toughness/Break/Heal
  presentation events after authoritative resolution.
- Presentation resolver Build and focused Automation passed 3/3.
- Formal content catalog and four read-only Automation tests are authored.
- Content Build/runtime RED is not yet verified: the first UBT permission
  request ended at the approval service before the command ran.
- Asset licensing is user-guaranteed and is not a current blocker.

## Ownership

Codex may author the authorized C++ presentation contract and Automation after
Gate 0 approval. The user owns Editor import, UAsset/UMG creation, source and
license records, Save All/reopen, map placement and visual PIE evidence.

## Explicit non-goals

- No Teacher teaching package for the current two-phase scope.
- No reuse or silent rename of test assets as final Demo data.
- No third-party asset migration before provenance and isolation approval.
- No GameplayCue/Notify as damage or turn authority.
- No Phase 19 full GM system or Phase 20 portfolio closeout in this package.
