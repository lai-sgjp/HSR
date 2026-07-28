# Phase 17 Patch 03 Execution Plan — 八子系统总整合

> 状态：`GATE 0 PASS / PATCH-03A TASK GATE REQUIRED`
> 基线：2026-07-28，P17-005 checkpoint commit `4ef49f7`；未 push。
> 用户决定：暂停 P17 正常序列，把本补丁视为独立新阶段，整合战斗、交互、角色养成、装备、背包、地图、存档与 UI。
> 约束：本计划不把 P17-005 partial PIE 外推为最终 PASS，不自动授权 Source/Content/Config/Git 实施。

## 1. Current facts and Gate 0

- P17-005 Frontend code gate and 11/11 narrow Automation passed; user Input/WBP assets exist，Character route opens and returns correctly.
- Character Detail domain initialization currently reports `PartySlotEmpty`. Exploration Pawn exists, but production Exploration bootstrap does not prove Catalog/Profile/Party/CharacterId alignment.
- Party、Map、Challenge、Save remain P17-005 placeholder roots. Current domain systems have independent tests and prior phase evidence, but no single clean-save flow proves all eight systems agree on stable identity, transaction ordering, travel lifecycle and save rebuild.
- Existing user changes under Enemy/Player Character、Enemy DataAsset、Map、`Content/AI/**`、`.claude/**` and learning docs remain isolated unless a later exact task allowlist explicitly classifies them.

Gate 0 initial verdicts: Coordinator=`REVISE`、Independent Reviewer=`REVISE`、Teacher=`REVISE`、Implementation feasibility=`REVISE`. The draft must pass a second review before PATCH-03A implementation may be authorized.

Gate 0 final verdicts after revision: Coordinator=`PASS`、Independent Reviewer=`PASS`、Teacher=`PASS`（其唯一末轮状态 blocker 已校正）、Implementation feasibility=`PASS`. This PASS authorizes only creation/review of the PATCH-03A task card; it does not authorize implementation.

- Phase 17's canonical design says the global ScreenStack owns top-level context only and Frontend modules belong to the internal Router. The P17-005 checkpoint currently mirrors the active module into a third global ScreenStack entry and its tests assert depth 3. This duplicate history contract must be reconciled before domain modules expand.

## 2. Acceptance Brief

### Goal

From a clean new game, one stable selected character can explore, interact, enter and resolve battle, receive exactly-once reward/progression, inspect/equip owned inventory, travel, save, restart and load, while every UI screen reflects authoritative committed snapshots and every failure leaves prior state usable.

### In scope

- Production bootstrap and stable CharacterId/Party/Profile/Actor projection.
- Existing Interaction -> Encounter/Battle -> Result/Reward/Return seams.
- Progression、Inventory、Equipment/Relic、Map、Save read/command integration.
- UI ViewModels、Frontend module replacement、travel teardown/rebuild and structured feedback.
- Pure-value operation IDs、typed results/events、revision/idempotency、failure/compensation evidence.

### Out of scope

- Gacha Authority、cloud save、independent Inventory save file、multiplayer/network prediction.
- New Runtime module/plugin/CommonUI、bulk Config/Content migration、third-party presentation assets.
- Phase 18 art/audio/VFX、pixel-copy UI、Packaged/Shipping claims without evidence.
- Replacing existing domain authorities with one global manager.

### Risk review

| Risk | Handling |
|---|---|
| Persistent data/compatibility | Preserve Phase 16 schema unless a separately reviewed migration is unavoidable; candidate-first global restore and backup evidence required. |
| Cross-domain partial commit | PATCH-03D uses one bounded SettlementAuthority, one aggregate candidate and a non-failing install phase; no UI-sequenced pseudo-transaction. |
| World lifetime/travel | Stable IDs only; old host/token/callback rejection; no Actor/Widget in context or Save. |
| UI authority leakage | UIManager owns session only; ViewModel reads; command facade submits; Blueprint presents. |
| Scope/learner overload | Eight serial implementation packages plus closeout; each has one bounded result and its own Task/Asset/Review gate. |

### Required acceptance criteria

#### AC-001 — Stable character bootstrap
- Scenario: clean new game or accepted Save candidate.
- Action: enter Exploration and open Character.
- Expected: Pawn、Party slot、Profile、Definition、Equipment owner and UI selection share one valid CharacterId; Character snapshot is valid.
- Must not: hardcode fallback character in Widget or create Profile/Party from UI.
- Verification: focused Automation + Editor restart PIE + empty-party failure PIE.

#### AC-002 — Interaction and battle admission exactly once
- Scenario: one valid encounter candidate.
- Action: interact repeatedly or lose/destroy candidate during admission.
- Expected: at most one EncounterRequest/Battle consumes; rejected paths preserve Exploration state.
- Verification: Interaction/BattleTransition Automation + normal and duplicate/lost-target PIE.

#### AC-003 — Battle settlement consistency
- Scenario: terminal victory or defeat.
- Action: consume result/retry/return, including duplicate callback.
- Expected: victory settlement produces one RewardTransaction and intended Profile/Inventory revisions; defeat produces none; return occurs once.
- Must not: leave partial reward/progression commit or let Reward Summary grant data.
- Verification: battle/reward/inventory/progression integration Automation + two-round PIE.

#### AC-004 — Inventory/equipment atomic operation
- Scenario: selected CharacterId and owned compatible item instance.
- Action: equip/replace/unequip, repeat, or submit stale/wrong-owner instance.
- Expected: ownership、slot mapping、source GE handles and derived read model agree after success; failure preserves old equipment and inventory.
- Verification: Equipment/Inventory Automation + PIE stat/source comparison.

#### AC-005 — Map/travel/UI lifecycle
- Scenario: Frontend session open on valid or locked destination.
- Action: submit map travel or encounter return.
- Expected: valid travel tears UI down once and rebuilds exact Exploration root after committed arrival; rejection retains old World/runtime/UI usability.
- Verification: Map/Transition/UI Automation + A->B->A and Battle->return PIE.

#### AC-006 — Save/load global consistency
- Scenario: committed Profile/Party/Inventory/Equipment/Reward/Map state.
- Action: save, mutate, load, repeat load, corrupt Primary in approved fixture, or request during travel/battle return.
- Expected: only fully valid candidate commits; repeat load is no-op; invalid/busy paths have zero pollution; backup recovery is truthful.
- Verification: existing Save suites + new integration test + cold Editor process/restart evidence.

#### AC-007 — UI is read/intent only
- Scenario: any module open, duplicated input, stale selection, missing class or rebuild.
- Action: navigate and submit supported command.
- Expected: route/input/focus and Widget lifecycle stay consistent; domain result is rendered from committed snapshot.
- Must not: Widget/Level BP mutate Domain, call OpenLevel, apply GE or serialize Save.
- Verification: source review + Frontend Automation + two-resolution manual PIE.

#### AC-008 — One complete operation flow
- Scenario: clean save and original/authorized placeholder assets.
- Action: Explore -> Interact -> Battle -> Victory -> Reward -> Inventory -> Equip -> Character -> Map -> Save -> cold Load.
- Expected: stable IDs and revisions form one auditable chain and restored state matches the last committed checkpoint.
- Verification: closeout Automation index, user PIE log/screenshots and independent review.

## 3. Ownership and architecture decision

Canonical detailed flow is [system-operation-flow.md](system-operation-flow.md).

- Existing Domain subsystems remain authorities.
- No new global coordinator is authorized by Gate 0.
- A future saga layer may be proposed only if a later package proves existing typed seams cannot express a cross-World operation. Its allowed state would be OperationId、step、stable IDs、stale-callback guards and audit snapshot only.
- This decision is recorded here as `PROPOSED`. Creating an ECC ADR directory/file requires separate explicit user approval under the ADR skill.

## 4. Serial work packages

### PATCH-03A — Frontend Boundary Contract Reconciliation

Unique result: Frontend module navigation uses one authoritative internal Router history while the global ScreenStack remains exactly `ExplorationRoot -> FrontendShell`; Character/Inventory/placeholder switching、Back、X、pause、focus and travel teardown preserve the existing visible behavior without a third global module entry.

Candidate Source allowlist (freeze exact subset in task card):

- `Source/HSR/UI/HSRUIManagerSubsystem.h/.cpp`
- `Source/HSR/UI/HSRScreenStack.h/.cpp`
- `Source/HSR/UI/HSRScreenStackTypes.h`
- `Source/HSR/UI/Frontend/HSRFrontendRouter.h/.cpp`
- `Source/HSR/UI/Frontend/HSRFrontendShellWidget.h/.cpp`
- `Source/HSR/UI/Frontend/HSRFrontendModuleRootWidget.h/.cpp`
- `Source/HSR/Tests/HSRFrontendNavigationTests.cpp`

User Asset Gate: existing `WBP_FrontendShell_P17` may need an explicit module host/switcher binding; no domain UI or Gameplay asset is authorized. Evidence covers direct shortcuts、cross-type replace、Back/X、duplicate/same-frame input、all compensation stages、travel discard and global depth fixed at 2.

User Editor exercise: bind the existing C++ module host/switcher in `/Game/UI/P17/Frontend/WBP_FrontendShell_P17`; Blueprint may not maintain route history or pause/input policy. Save All/reopen, then PIE Character -> Inventory -> Back -> X; clear one allowlisted module class reference for the controlled failure. Explain why Router owns module history while ScreenStack owns only ExplorationRoot/FrontendShell.

This package is a correction of the P17-005 architectural seam, not a claim that P17-005 final visual/resolution/persistence evidence passed.

### PATCH-03B — Production Bootstrap and Character Identity

Unique result: clean Exploration resolves a stable selected CharacterId and Character Detail displays a valid Profile/Party/Equipment read model; empty/invalid bootstrap is controlled and zero-pollution.

Candidate Source allowlist (freeze exact subset in task card):

- `Source/HSR/Framework/HSRGameModeBase.h/.cpp`
- `Source/HSR/Character/HSRCharacterBase.h/.cpp`
- `Source/HSR/Character/HSRExplorationCharacter.h/.cpp`
- `Source/HSR/Progression/HSRCharacterProfileSubsystem.h/.cpp`
- `Source/HSR/Party/HSRPartySubsystem.h/.cpp`
- `Source/HSR/UI/HSRCharacterDetailWidget.h/.cpp`
- `Source/HSR/UI/HSRCharacterDetailViewModel.h/.cpp`
- new focused test under `Source/HSR/Tests/`

Editor candidates: existing GameMode、Character Catalog、initial Character/Party configuration、Player Character BP and Character WBP references. Map modifications require separate Asset Gate and exact path.

User Editor exercise: bind the task-frozen GameMode/default-pawn/catalog/initial-party references and Character WBP presentation fields; Blueprint may not create Profile/Party records or invent a fallback CharacterId. Save All/reopen, then PIE new game -> Character; use an intentionally empty initial Party fixture for failure. Explain Definition/Profile/Party/Pawn/ViewModel ownership.

Evidence: new game、existing save、empty party、missing definition、wrong owner、Editor reopen; Actor/Profile/Party/UI CharacterId and revisions must match.

### PATCH-03C — Interaction -> Encounter -> Battle Admission

Unique result: one valid interaction creates and consumes one pure-value EncounterRequest; lost/duplicate/resolved targets do not enter Battle.

Candidate Source set (exact subset frozen in task card): `Source/HSR/Interaction/HSRInteractionComponent.h/.cpp`, `HSRInteractionTypes.h`, `HSRInteractableInterface.h/.cpp`, `Source/HSR/Battle/HSREncounterTypes.h`, `HSRBattleTransitionSubsystem.h/.cpp`, `Source/HSR/Data/Definitions/HSREncounterDefinition.h/.cpp`, and focused tests.

Candidate Asset set (separate exact Task Asset Gate): `/Game/Blueprints/Exploration/BP_HSRNeutralEncounterTest`, one existing `/Game/Data/Encounters/DA_Encounter_*`, `/Game/Maps/Map_Battle`, and existing Interaction input binding. Behavior Tree/Enemy assets remain excluded.

User Editor exercise: configure one encounter actor with one Encounter DataAsset and verify Interact binding; Blueprint may not travel or resolve admission. Save All/reopen, then PIE approach -> F -> Battle once; destroy/leave the candidate before F and repeat F for failures. Explain Interaction candidate ownership, BattleTransition admission and stable-ID rebuild.

Evidence: candidate acquire/loss/destruction、repeat F、pending/resolved rejection、travel teardown、Battle participant rebuild from stable IDs.

### PATCH-03D1 — Atomic Settlement Foundation

Unique result: Reward, Inventory and Profile can each prepare a pure-value candidate without live mutation, and one bounded SettlementAuthority can install a fully validated aggregate through non-failing internal primitives; production battle flow is not switched yet.

Candidate Source allowlist (freeze exact subset in task card):

- new `Source/HSR/Reward/HSRSettlementTypes.h`
- new `Source/HSR/Reward/HSRSettlementAuthority.h/.cpp`
- `Source/HSR/Reward/HSRRewardSubsystem.h/.cpp`
- `Source/HSR/Reward/HSRRewardTypes.h`
- `Source/HSR/Inventory/HSRInventorySubsystem.h/.cpp`
- `Source/HSR/Inventory/HSRItemTypes.h`
- `Source/HSR/Progression/HSRCharacterProfileSubsystem.h/.cpp`
- `Source/HSR/Progression/HSRCharacterProfileTypes.h`
- new `Source/HSR/Tests/HSRSettlementAuthorityTests.cpp`
- affected existing Reward/Inventory/Profile focused tests named by the task audit

The task must freeze three candidate DTOs and three private/internal prepare/install seams. Existing `SubmitReward`, `ApplyGrantsInternal` and `GrantExperience` behavior remains production-compatible until 03D2 switches the caller. Install primitives may only move/swap already allocated state; they cannot allocate, resolve Definitions, apply GE, broadcast, increment revisions or return a business failure. SettlementAuthority is the only friend/caller permitted to invoke all three installs.

User Editor exercise: no asset creation or business binding. In the existing authorized Reward test fixture, verify the RewardDefinition and CharacterId references, Save All/reopen, then PIE one ordinary victory/return as a compatibility happy path and defeat as the no-reward path. Blueprint may not call the new prepare/install seams. Explain why this package changes transaction infrastructure without yet changing the production settlement caller.

Evidence: focused candidate purity tests, prepare failure zero-pollution, duplicate TransactionId, allocation/definition failure before install, one aggregate install with revisions/events withheld until publication, and existing Reward/Inventory/Profile regressions.

### PATCH-03D2 — Battle Result -> Reward/Progression/Inventory Settlement

Unique result: one victory commits one logically consistent reward/profile/inventory settlement and one receipt; defeat/replay produces no duplicate mutation.

Candidate Source allowlist (freeze exact subset in task card): `Source/HSR/Battle/HSRBattleCoordinator.h/.cpp`, `Source/HSR/Battle/HSRBattleGameMode.h/.cpp`, `Source/HSR/Battle/HSRBattleTransitionSubsystem.h/.cpp`, `Source/HSR/Reward/HSRSettlementAuthority.h/.cpp`, `Source/HSR/Reward/HSRSettlementTypes.h`, `Source/HSR/Reward/HSRRewardSubsystem.h/.cpp`, `Source/HSR/Reward/HSRRewardResolver.h/.cpp`, `Source/HSR/UI/HSRInventoryRewardViewModel.h/.cpp`, `Source/HSR/UI/HSRInventoryRewardTypes.h`, `Source/HSR/UI/HSRInventoryRewardWidget.h/.cpp`, new `Source/HSR/Tests/HSRBattleSettlementIntegrationTests.cpp`, and affected Battle/Reward integration tests named exactly in the task card.

Transaction contract: one C++ SettlementAuthority owns `RewardTransactionId` and an immutable aggregate Reward/Inventory/Profile candidate. All fallible work precedes commit. Fixed non-failing install order is Inventory -> Profile -> Reward ledger; only after all states install does one publication point expose revisions, committed events and receipt. Failure to provide non-failing internal commit primitives blocks the task; compensation/`Inconsistent` cannot satisfy AC-003.

Candidate Asset set: `/Game/Data/Rewards/DA_Reward_P13_Standard`, the canonical Reward Summary selected after resolving the two similarly named assets, and one authorized Battle reward fixture.

User Editor exercise: bind RewardDefinition to the authorized result fixture and Reward Summary to receipt presentation only; Blueprint may not grant XP/items or retry settlement. Save All/reopen, then PIE victory -> one receipt -> return; use defeat and duplicate result as failures. Explain SettlementAuthority, each Domain authority and the single publication point.

Prerequisite: 03D1 focused tests prove all three candidate/install seams before this task may switch production settlement. Do not rely on rollback-by-opposite-command.

### PATCH-03E — Inventory -> Equipment/Relic -> ASC Projection

Unique result: owned instance equip/replace/unequip atomically updates authoritative equipment mapping and exact source effects, then Character read model refreshes once.

Candidate Source set (exact subset frozen in task card): `Source/HSR/Inventory/HSRInventorySubsystem.h/.cpp`, `Source/HSR/Inventory/HSRItemTypes.h`, `Source/HSR/Equipment/HSREquipmentSubsystem.h/.cpp`, `HSREquipmentTypes.h`, `HSREquipmentEffectBridge.h/.cpp`, `HSREquipmentStatAggregator.h/.cpp`, `HSRRelicSetResolver.h/.cpp`, `Source/HSR/UI/HSREquipmentDetailViewModel.h/.cpp`, `HSREquipmentDetailTypes.h`, `HSREquipmentDetailWidget.h/.cpp`, and exact affected Equipment tests named by the task card.

Candidate Asset set: `/Game/Data/Relics/DA_Relic_*`, `/Game/Data/RelicSets/DA_RelicSet_P12_A`, `/Game/GameplayEffects/GE_Equipment_P12`, `GE_Relic_P12`, `GE_RelicSet_P12_A`, `/Game/UI/WBP_Inventory_P13`, and `/Game/UI/WBP_EquipmentDetail_P12`.

User Editor exercise: configure one owned item and bind selection plus committed stat/source presentation; Blueprint may not alter ownership/slots or apply/remove GE. Save All/reopen, then PIE equip -> replace -> unequip; use wrong-owner/incompatible item for failure. Explain Inventory authority, Equipment mapping, ASC projection and ViewModel refresh.

Evidence: valid equip、replace、unequip、wrong owner、incompatible slot、stale instance、repeated OperationId、Save rebuild idempotency.

### PATCH-03F — Map Frontend and Travel Rebuild

Unique result: Map module reads authoritative unlock/location state and one legal intent travels A<->B while Frontend/Actor/HUD teardown and new-host rebuild remain exact.

Candidate Source set (exact subset frozen in task card): `Source/HSR/Map/HSRMapSubsystem.h/.cpp`, `HSRMapTypes.h`, `HSRMapArrivalConsumer.h/.cpp`, `HSRMapArrivalPoint.h/.cpp`, `Source/HSR/Data/Definitions/HSRMapDefinition.h`, new `Source/HSR/UI/HSRMapViewModel.h/.cpp`, new `Source/HSR/UI/HSRMapWidget.h/.cpp`, `Source/HSR/UI/HSRUIManagerSubsystem.h/.cpp`, `Source/HSR/Tests/HSRMapSubsystemTests.cpp`, `HSRMapSaveIntegrationTests.cpp`, and one new focused UI/travel test named by the task card.

Candidate Asset set: `/Game/Data/Maps/DA_Map_Exploration_p15_A`, `_B`, `DA_TeleportAB_p15`, `DA_TeleportBA_p15`, `/Game/Maps/Map_Exploration_P15_A`, `_B`, and the task-selected Map WBP. Modified `Map_Phase1_Exploration` remains excluded.

User Editor exercise: bind read-only destinations and A/B map/teleport/arrival references; Blueprint may not call `OpenLevel` or commit location. Save All/reopen, then PIE A -> B -> A; choose locked/invalid destination for failure. Explain MapSubsystem validation/commit, UI teardown and arrival consumption.

Evidence: locked/invalid/no-op、pending BattleReturn、old host/token、arrival mismatch、failed travel preserving old state.

### PATCH-03G — Save UI and Integrated Restore

Unique result: manual Save/Load/Overwrite UI displays structured results and a cold restart restores the same committed Character/Party/Inventory/Equipment/Reward/Map chain.

Candidate Source set (exact subset frozen in task card): `Source/HSR/Save/HSRSaveSubsystem.h/.cpp`, `HSRSaveTypes.h`, `HSRSaveGame.h/.cpp`, new `Source/HSR/UI/HSRSaveViewModel.h/.cpp`, new `Source/HSR/UI/HSRSaveWidget.h/.cpp`, `Source/HSR/UI/HSRUIManagerSubsystem.h/.cpp`, `Source/HSR/Tests/HSRSaveSubsystemTests.cpp`, `HSRSaveRecoveryTests.cpp`, `HSRSaveColdRecoveryTests.cpp`, and one new `Source/HSR/Tests/HSRIntegratedRestoreTests.cpp`.

Candidate Asset set: the exact task-selected Save WBP and approved disposable test-slot fixtures only. Production save deletion, schema expansion and cloud remain excluded.

User Editor exercise: bind Save/Load/Overwrite intents and structured results; Blueprint may not serialize DTOs, select Primary/Backup or mutate Domains. Save All/reopen, then PIE save -> mutate -> load -> Editor restart; use approved disposable corrupt-Primary and busy-travel fixtures for failures. Explain candidate validation, global restore publication and UI result ownership.

Evidence: save/mutate/load、repeat load no-op、Primary corruption/Backup recovery in approved fixture、busy travel/battle-return rejection、Editor restart.

### PATCH-03H — End-to-End Closeout

Unique result: AC-008 clean-save operation flow and required failure paths pass without new architecture or hidden manual repair.

No new gameplay rules. Run fresh Development Editor Build、all relevant Automation、two target resolutions、Editor reopen、two continuous PIE rounds and independent review. Unavailable physical controller、Standalone、Packaged/Shipping remain `NOT VERIFIED`.

User Editor closeout exercise: start from a named disposable clean-save fixture, reset it through the approved test setup, Save All/reopen, and capture AC-008 plus one selected failure from each A-G with IDs/revisions. Blueprint repair logic and manual mid-flow state edits are forbidden. Explain the end-to-end ownership chain and which state survives travel, UI rebuild and cold load.

## 5. Unified failure matrix

| Boundary | Required outcome |
|---|---|
| Missing/invalid Definition or Catalog | Reject before Profile/Party/Actor mutation; structured reason. |
| Empty/stale Character selection | Controlled unavailable state; no hardcoded UI fallback. |
| Duplicate/stale OperationId | No-op/AlreadyProcessed; no revision/event duplication. |
| Interaction target lost/destroyed | No EncounterRequest; candidate clears safely. |
| Encounter pending/resolved or travel pending | Reject without overwriting stable transition context. |
| Battle result replay | No duplicate reward、XP、inventory、return or UI receipt. |
| Cross-domain settlement prepare failure | No domain commits. |
| Settlement commit failure | Structurally forbidden: fallible work precedes commit and internal installs are non-failing; inability to prove it blocks PATCH-03D. |
| Other commit/compensation failure | Explicit Inconsistent/diagnostic state; never reported as success. |
| Equip wrong-owner/stale/incompatible | Old slot、inventory and GE sources unchanged. |
| UI missing class/focus/attach | Domain untouched; old route/input/pause remains usable. |
| Map travel failure/arrival mismatch | Old Map runtime remains committed; stale callbacks rejected. |
| Save during battle return/travel/reentry | Busy/invalid-context rejection; disk/runtime unchanged. |
| Load decode/migration/projection failure | No partial restore; truthful Primary/Backup diagnostics. |
| World/HUD teardown during callback | Delegates unbound; old host cannot mutate new World/UI. |

## 6. Editor and teaching gate

Each package must list exact assets before user work. User may create/configure Blueprint、DataAsset、UMG、map references and run PIE; Codex may modify only authorized C++/Markdown. Blueprint may bind intent、snapshot、focus and presentation events, never domain rules.

For every package the user must be able to explain：request initiator、validator、runtime owner、commit owner、result consumer、travel/save survival and one failure path. Record the answer in `learning-journal.md` or the relevant learning document only after the user actually answers.

## 7. Build, Automation and evidence

- Every code package: `HSREditor Win64 Development` Build with UHT/Compile/Link/metadata truthfully recorded.
- Narrow Automation first, then affected regression families: `HSR.Interaction`、`HSR.Battle`、`HSR.Progression`、`HSR.Equipment`、`HSR.Inventory/Reward`、`HSR.Map`、`HSR.Save`、`HSR.UI` as actually available.
- One happy PIE and one real/controlled failure PIE per package; Editor Save All/reopen and exact asset references.
- `git diff --check` and precise allowlist/provenance audit before every role commit.
- Evidence labels remain `AGENT VERIFIED`、`USER PROVIDED`、`USER ACCEPTED` or `NOT VERIFIED`; one type cannot substitute for another.

## 8. Documentation and Git

- Gate 0: this plan、`system-operation-flow.md`、P17-005 paused archive/status reconciliation、PROJECT_STATE/worklog/todo.
- Per package: active/execution/final-review trio、archive、worklog、todo and learning/design evidence.
- Role commits remain separate; no push until the entire Patch 03 closeout is accepted.
- Existing unrelated working-tree changes remain unstaged and are not attributed to Patch 03.

## 9. Stop conditions and non-goals

Stop and request authorization for new module/plugin、Config、save schema migration、bulk Content/Map edits、third-party assets、destructive Git、cloud/network/Gacha、or any file outside the active package allowlist. Preserve the first real Build/Automation/PIE failure.

P17-005 remains paused and independently incomplete. Patch 03 completion will not retroactively mark P17-005 or the remaining P17-006～016 product modules complete.

## 10. Gate 0 completion and only next task

Gate 0 becomes `PASS` only after Coordinator、Independent Reviewer、Teacher and Implementation feasibility all review this actual draft and blocking revisions are incorporated.

After Gate 0 PASS, the only next task is `TASK-P17-PATCH-03A — Frontend Boundary Contract Reconciliation`. It must receive its own exact Task Gate and user confirmation before any implementation.
