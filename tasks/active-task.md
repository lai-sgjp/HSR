# TASK-P17-PATCH-03B - Production Bootstrap and Character Identity

Status: `TASK GATE PASS / USER CONFIRMATION REQUIRED`

## Role Lock

This card does not authorize implementation. The next action is Task Gate review only. Source, Content, Config, Build, Automation, Editor work and implementation Git commits remain prohibited until the reviewed card is presented and the user explicitly confirms execution of `TASK-P17-PATCH-03B`. Task Gate Markdown review commits remain allowed.

## Current Evidence

- PATCH-03A is archived as `PASS WITH FOLLOW-UP`; Frontend navigation is no longer the blocker.
- Real PIE reaches Character but logs `P11-005 DetailWidgetInit Result=FAIL SelectionResult=6`, mapped to `PartySlotEmpty`.
- The Exploration Pawn exists as the World projection, but a stable CharacterId is not yet proven consistent across Definition, Profile, Party, Pawn and Character UI.

## Sole Outcome

From a clean Exploration start, the project resolves one stable selected CharacterId and Character Detail displays a valid Profile/Party read model. Empty or invalid bootstrap data produces a controlled unavailable result with zero partial mutation and no hardcoded UI fallback. Equipment aggregation remains PATCH-03E.

## Frozen Source Write Allowlist

- `Source/HSR/Framework/HSRGameModeBase.h/.cpp`
- `Source/HSR/Character/HSRCharacterBase.h/.cpp`
- `Source/HSR/UI/HSRCharacterDetailWidget.h`
- `Source/HSR/UI/HSRCharacterDetailWidget.cpp`
- new `Source/HSR/Tests/HSRProductionBootstrapTests.cpp`
- `tasks/execution-result.md`

`HSRExplorationCharacter` needs no identity storage of its own because it inherits the projection contract from `AHSRCharacterBase`. Profile, Party, Save, CharacterDetailViewModel and existing test sources are read-only dependencies. Files not listed above are read-only.

## Candidate Editor Assets - Separate Asset Gate Required

- Existing GameMode/default pawn references.
- Existing Character Catalog and initial Character/Party configuration.
- `Content/Blueprints/Character/Player/BP_HSRExplorationCharacter.uasset` only if provenance and exact required binding are approved.
- Existing Character WBP references and presentation fields.
- No map modification is authorized by this planned card.

## Contract to Freeze at Task Gate

1. Definition owns immutable design data; Profile owns persistent progression; Party owns roster and selected slot; Pawn/ASC is a World projection; ViewModel reads committed snapshots only.
2. `AHSRGameModeBase` owns production orchestration configuration: Character Catalog, InitialCharacterId and an explicit bootstrap mode (`NewGameDefaults` or `UseCommittedRuntime`). It does not become a Domain authority. The result must be a typed enum covering Success, NoOp, MissingCatalog, InvalidInitialCharacter, CatalogConflict, ProfileRegistrationFailed, PartyUnavailable, NoCommittedSelection and PawnProjectionFailed. Production invokes bootstrap from `AHSRGameModeBase::RestartPlayer` only after `Super::RestartPlayer` has produced/possessed the Pawn; Widget construction is never a bootstrap trigger.
3. Before any Domain mutation, bootstrap resolves the locally possessed Pawn and proves it is an `AHSRCharacterBase`; missing/wrong Pawn returns `PawnProjectionFailed`. It then validates the complete configured Catalog and InitialCharacterId from Definition CDOs, including null entries, duplicate IDs, required initial ID and required curves. Catalog state has three legal outcomes: none registered -> atomically call the existing registration seam; every entry already registered to the same Definition -> NoOp; partial registration or conflicting Definition -> `CatalogConflict` with zero mutation. No Party or Pawn publication occurs if this stage fails.
4. After Definitions exist, the caller chooses exactly one branch. `UseCommittedRuntime` only validates and projects the already committed Party selection; it does not read disk. `NewGameDefaults` may seed an empty Party. A committed non-empty Party always wins and cannot be overwritten. Disk-slot selection and automatic cold load remain PATCH-03G.
5. New-game default preflight proves the initial Profile exists. If Party is empty, the existing `AddCharacter` path is structurally non-failing after its profile/slot/duplicate preconditions are proven; an unexpected result is an invariant failure and cannot be reported as success. If Party is already non-empty, bootstrap never seeds or replaces it: it validates slot 0, projects that committed ID and returns NoOp. Only after Party is committed/validated may the same CharacterId be projected onto the preflighted `AHSRCharacterBase`.
6. Bootstrap resolves a stable CharacterId from committed Party slot 0. Widget, Pawn name, array index, display text and Blueprint cannot invent identity. Remove the current `SelectCharacter(TEXT("Character.A"))` Widget path; Character Detail selects only through Party.
7. Empty Party is a valid controlled unavailable state. The Widget exposes the typed failure to presentation without manufacturing a valid snapshot.
8. Repeated bootstrap and UI rebuild are idempotent: no duplicate Profile/Party entry, revision, event or delegate binding. Wrong/missing Pawn leaves committed Profile/Party unchanged. Pawn projection is the final publication step and may only set the committed CharacterId or clear no prior valid identity.
9. Blueprint cannot create Profile/Party records, mutate progression or supply a hidden fallback CharacterId.

## Frozen API Shape

- `EHSRCharacterBootstrapMode`: `NewGameDefaults`, `UseCommittedRuntime`.
- `EHSRCharacterBootstrapResult`: the typed outcomes listed in contract item 2.
- `AHSRGameModeBase::BootstrapCharacterIdentity(EHSRCharacterBootstrapMode Mode)` is the only orchestration entry and returns the typed result.
- `AHSRGameModeBase::GetLastCharacterBootstrapResult()` and `GetResolvedCharacterId()` expose diagnostics/read-only evidence. A narrow `WITH_DEV_AUTOMATION_TESTS` configuration seam may set Catalog/InitialCharacterId/mode but cannot bypass production validation.
- `AHSRCharacterBase::SetProjectedCharacterId(FName)` is a private, non-UFUNCTION write seam available only to friend `AHSRGameModeBase`; it accepts only a non-empty ID selected from committed Party state. `GetProjectedCharacterId()` is the public read-only accessor. The GameMode validates that the possessed Pawn is an `AHSRCharacterBase` before calling the write seam.
- `UHSRCharacterDetailWidget::OnDetailUnavailable(EHSRCharacterDetailResult)` is presentation-only. Failure clears no Domain state and does not emit a valid snapshot.

## Acceptance Matrix

- New game: initial CharacterId matches Profile, Party, Exploration Pawn and Character read model.
- Existing committed runtime (including state already restored through Save): selection is preserved; new-game defaults do not execute or overwrite it. Automatic disk loading is not claimed.
- Empty initial party: controlled unavailable result, no crash and no partial record creation.
- Missing Catalog entry/Character Definition: structured rejection before Profile/Party/Pawn/UI publication.
- Missing or wrong Pawn type: rejection with committed Profile/Party selection preserved and no false projection.
- Widget literal regression: Character Detail cannot display a CharacterId that did not come from committed Party slot 0.
- Repeated initialization and UI reopen: no duplicate entries, revision increments or delegate bindings.
- Editor Save All/reopen: authorized references persist.

## Required Evidence

- Fresh `HSREditor Win64 Development` Build with UHT/compile/link/metadata truthfully reported.
- New `HSR.ProductionBootstrap.CharacterIdentity` Automation in `HSRProductionBootstrapTests.cpp` covers clean new game, existing/restored selection precedence, empty/invalid config, missing Definition, wrong Pawn, repeated bootstrap and Party/Pawn/UI ID equality.
- Run without editing: `HSR.UI.CharacterDetail.ViewModel`, `HSR.Party.FixedSlots`, `HSR.Progression.Profile.Authority`, `HSR.Save` and `HSR.UI.FrontendNavigation`.
- User PIE: new game -> Character happy path and one intentionally empty-party failure fixture.
- Actor/Profile/Party/UI CharacterId and relevant revisions must match.
- Physical controller, Standalone, Packaged and Shipping remain outside this task unless separately authorized.

## Explicit Non-goals

- No Character creation/customization, party editing UI, leveling, Equipment read-model aggregation or mutation, inventory mutation, save schema redesign, slot selection or automatic disk load.
- No changes to battle, interaction, map travel, settlement or Frontend navigation architecture.
- No new plugin/module, third-party asset, broad Blueprint repair or map edit.
- Do not begin PATCH-03C or later packages.

## Current Gate

First Task Gate review=`REVISE`: the allowlist was unfrozen, save/default ordering was ambiguous, Widget hardcoded `Character.A`, Equipment was unsupported scope and tests were vague. Revision 1 closed those items. Revision 2 froze typed bootstrap modes/results, limited existing-state handling to already committed runtime, deferred disk loading to 03G and made Pawn projection the final publication step. Dual-review iterations then froze Catalog conflict handling, Pawn writer ownership and the post-`Super::RestartPlayer` trigger. Final dual review=`PASS / PASS`; Task Gate=`PASS`. Implementation must first restate this frozen card and stop with: `等待用户确认执行 TASK-P17-PATCH-03B。`
