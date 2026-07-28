# TASK-P17-PATCH-03C - Interaction to Battle Admission

Status: `PLANNED / TASK GATE PASS / USER CONFIRMATION REQUIRED`

## Role Lock

This card is a planning artifact. No Source, Content, Config, Build, Automation, PIE or implementation commit is authorized until Task Gate PASS, an implementation restatement, and a separate explicit user confirmation for `TASK-P17-PATCH-03C`.

## Prerequisite and sole outcome

- `TASK-P17-PATCH-03B` is archived as PASS; stable player CharacterId bootstrap is available.
- One valid Exploration interaction admits exactly one pure-value `FHSREncounterRequest`, travels once, and the Battle World consumes that same request once. Lost, destroyed, unavailable, duplicate, pending/traveling or resolved candidates do not create a second admission or corrupt the existing transaction.

## Ownership contract

1. `UHSRInteractionComponent` owns observation of the current weak candidate and revalidates the exact candidate at intent time. It does not own encounter state or travel.
2. `AHSRGrayboxInteractable` is the encounter interaction adapter. It validates its live overlap and configured Definition, then submits one intent to `UHSRBattleTransitionSubsystem`; Blueprint cannot call `OpenLevel`, manufacture request IDs or mark an encounter resolved.
3. `UHSRBattleTransitionSubsystem` is the sole admission/travel authority. It validates all Definition-derived fields and runtime prerequisites before publishing Pending/Traveling state, owns the request ID and rejects duplicate/resolved admission without changing the existing snapshot.
4. `FHSREncounterRequest` remains a pure-value cross-World DTO: selected PlayerCharacterId, EncounterId, EnemyDefinitionId, map package names, initiative, return transform and reward identity/seed only. It contains no Actor, Widget, subsystem, DataAsset or other live UObject reference.
5. BattleTransition captures PlayerCharacterId from committed Party slot 0; an empty/invalid selection rejects before admission. Actor name, GameMode defaults and Blueprint cannot invent the player identity.
6. The Battle World consumes the stored DTO exactly once through `ConsumePendingEncounter`. `AHSRBattleGameMode` uses the consumed PlayerCharacterId for Profile/Definition/Class resolution and participant setup instead of its configured fallback. `UHSRBattleCoordinator` remains read-only because the existing GameMode setup seam is sufficient.
7. Resolution remains tied to the existing battle-return outcome: only victory marks an EncounterId resolved; defeat/interruption remains retryable. Settlement/reward granting remains PATCH-03D.

## Admission transaction contract

1. Preflight order: candidate validity/interface -> availability -> live overlap/interactor -> pending/resolved conflict -> Definition presence -> committed Party slot-0 PlayerCharacterId -> stable EncounterId/EnemyDefinitionId -> battle map package -> optional reward-bundle const preflight -> World/player/return context.
2. No preflight rejection may modify candidate ownership, PendingRequest, state, travel tracking, resolved membership, Reward state or issue `OpenLevel`.
3. After every fallible admission check passes, optional Definition metadata registration may commit through the existing atomic Reward bundle registration seam. It may register item/drop/reward definitions only; it cannot grant inventory, create a receipt or publish settlement. The authority then creates one RequestId and immutable request snapshot, publishes Pending/Traveling once, and issues one travel request.
4. Same-frame repeated F, repeated adapter execution, and a second target while Pending/Traveling return typed failure and preserve the first request byte-for-byte.
5. Candidate lost through unregister returns `NoCandidate`; candidate destroyed while registered returns `TargetInvalid`; unavailable/out-of-range remains distinct. None invokes admission.
6. A resolved EncounterId returns `AlreadyConsumed` with no new RequestId or travel. Defeat/interrupt does not add resolved membership.
7. Travel failure/timeout clears only the matching transaction and permits a fresh retry; stale/mismatched failure callbacks cannot clear a newer request. A null-World failure callback is uncorrelatable and must not clear admission state; the bounded timeout remains its recovery path.

## Frozen Source write allowlist

- `Source/HSR/Interaction/HSRInteractionTypes.h`
- `Source/HSR/Interaction/HSRInteractionComponent.h`
- `Source/HSR/Interaction/HSRInteractionComponent.cpp`
- `Source/HSR/Exploration/HSRGrayboxInteractable.h`
- `Source/HSR/Exploration/HSRGrayboxInteractable.cpp`
- `Source/HSR/Battle/HSREncounterTypes.h`
- `Source/HSR/Battle/HSRBattleTransitionSubsystem.h`
- `Source/HSR/Battle/HSRBattleTransitionSubsystem.cpp`
- `Source/HSR/Battle/HSRBattleGameMode.h`
- `Source/HSR/Battle/HSRBattleGameMode.cpp`
- `Source/HSR/Data/Definitions/HSREncounterDefinition.h`
- `Source/HSR/Data/Definitions/HSREncounterDefinition.cpp`
- `Source/HSR/Reward/HSRRewardSubsystem.h`
- `Source/HSR/Reward/HSRRewardSubsystem.cpp`
- new `Source/HSR/Tests/HSRInteractionBattleAdmissionTests.cpp`
- `tasks/execution-result.md`

`HSRInteractableInterface`, BattleCoordinator, Party, Inventory, Profile, Save, Map and UI sources are read-only. BattleGameMode changes are limited to consuming the request's PlayerCharacterId and replacing production use of the configured fallback for this encounter. Reward changes are limited to extracting/reusing a public const `CanRegisterBundle` preflight and preserving existing atomic metadata registration behavior; SubmitReward, receipts, revisions/events and settlement are prohibited. Enemy/Behavior Tree sources remain read-only. An implementation may use fewer allowlisted files; it may not widen this list itself.

## Candidate Asset Gate - separate user work

- `/Game/Blueprints/Exploration/BP_HSRNeutralEncounterTest`
- one exact existing `/Game/Data/Encounters/DA_Encounter_*` selected during the Asset Gate
- `/Game/Maps/Map_Battle`
- existing Interaction input binding only

No Behavior Tree, Enemy, battle-rules, Reward, map-layout or UI asset change is authorized. The user owns Compile/Save/reopen and PIE asset evidence.

## TDD and acceptance matrix

Create `HSR.InteractionBattle.Admission` before production edits and prove an intended RED.

A narrow `WITH_DEV_AUTOMATION_TESTS` travel dispatcher/counter may suppress real `OpenLevel` while exercising the complete production preflight and state publication path. It may observe/redirect travel only and cannot bypass validation, manufacture request content or mutate authority state directly.

- valid candidate: one interaction result Success, one request ID, one admission mutation, pure DTO fields match committed Party PlayerCharacterId, the Definition and origin context;
- empty Party slot 0: typed admission rejection, no request/travel/reward metadata mutation; configured GameMode/Blueprint CharacterId cannot act as fallback;
- no candidate / proper unregister / destroyed weak candidate / unavailable / out-of-range: distinct typed failures and zero admission mutation;
- missing or invalid Definition, enemy ID, map or reward bundle: const preflight rejection with zero Transition, Inventory and Reward pollution;
- valid optional reward bundle: metadata registration succeeds only after all other admission preflight, with no item grant, receipt or settlement event;
- repeated F and direct duplicate submission while Pending/Traveling: first request preserved, no second ID/mutation/travel;
- resolved replay: `AlreadyConsumed`, zero mutation; defeat/interruption remains retryable;
- consume: Battle receives the identical DTO once; second consume is `AlreadyConsumed`; BattleGameMode resolves the consumed PlayerCharacterId and does not replace it with its configured default;
- matching travel failure/timeout permits retry; stale, mismatched and null-World callbacks preserve the active transaction until a matching failure or timeout;
- teardown clears candidate observation/delegates without clearing unrelated GameInstance admission state.

Required regressions without editing their tests: `HSR.Exploration.Patch.BehaviorTreeAdapter`, relevant `HSR.Battle`, `HSR.Map`, `HSR.Reward`, `HSR.UI.FrontendNavigation`, and `HSR.ProductionBootstrap.CharacterIdentity`. Exact discovered counts and inherited failures must be reported truthfully.

## User Editor exercise and evidence

1. Configure the neutral encounter actor with the selected Encounter DataAsset; verify F still routes through the Exploration InteractionComponent. Blueprint must not travel or resolve admission.
2. Compile/Save/reopen the actor and Definition references.
3. Happy PIE: approach -> prompt -> F -> Battle. Evidence must show one interaction success, one RequestId, one travel, and Battle consuming the same PlayerCharacterId/EncounterId/EnemyDefinitionId/RequestId.
4. Failure PIE uses stable fixtures only: leave overlap before F, set the encounter actor `bAvailable=false` before a separate PIE run, and retry the same actor after a victory return. No rejected case may enter Battle.
5. Return/defeat evidence must distinguish resolved victory from retryable defeat/interruption.
6. Destroyed-candidate and same-frame repeated-F races are Automation evidence, not manual timing exercises; no temporary Level Blueprint or Blueprint-owned admission logic may be created for them.

## Explicit non-goals and stop conditions

- No settlement, reward grant, inventory/profile mutation, Equipment, Save schema/load, map frontend, UI redesign, network, new module/plugin or third-party asset.
- Do not refactor enemy-initiated encounters; existing Behavior Tree/AI admission is regression-only in 03C.
- Do not move travel authority into Interaction, Blueprint, Level Blueprint or Widget.
- If exactly-once Battle consumption or consumed PlayerCharacterId use requires Coordinator changes beyond its existing setup seam, stop and request a reviewed allowlist amendment.
- If const Reward bundle validation cannot be extracted without changing Inventory or reward settlement semantics, stop and request a reviewed allowlist amendment.
- Do not begin PATCH-03D1 or later packages.

## Current Gate

Draft revision 3 closes feasibility review's Reward-boundary blocker, the Coordinator audit's missing player stable-ID handoff, and the Editor review's unreliable timing fixtures. Independent Reviewer=`PASS`; the initial Implementation feasibility condition is closed by the frozen const Reward preflight; Editor exercise review=`PASS`. Final Task Gate=`PASS`. Implementation must first restate task ID, sole outcome, ownership/order, exact write allowlist, TDD targets, Editor work and stop conditions, then end with: `等待用户确认执行 TASK-P17-PATCH-03C。`
