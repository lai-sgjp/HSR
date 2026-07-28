# TASK-P17-PATCH-03D2 - Battle Result Settlement Integration

Status: `PLANNED / TASK GATE PASS / USER CONFIRMATION REQUIRED`

## Sole outcome

One authoritative victory result submits one immutable settlement request to `UHSRSettlementAuthority`, commits one coherent Inventory/Profile/Reward receipt, and returns to Exploration. Defeat, replay, stale result and duplicate confirmation produce no duplicate mutation.

## Prerequisite

- 03D1 is archived as `PASS`; its prepare/install/publication contract may be used but not weakened.
- Existing victory and defeat production behavior remains the comparison baseline until this card passes Task Gate and receives separate user authorization.

## Candidate ownership

1. BattleCoordinator owns battle-local outcome resolution only.
2. BattleGameMode consumes a resolved result and submits one settlement intent; it does not grant items or EXP directly.
3. SettlementAuthority remains the only aggregate Inventory/Profile/Reward coordinator.
4. Reward owns transaction ledger and receipt; Inventory owns item state; Profile owns progression state.
5. UI reads the committed receipt and submits presentation/return intent only.

## Revised candidate Source allowlist for Task Gate rereview

- `Source/HSR/Battle/HSRBattleGameMode.h`
- `Source/HSR/Battle/HSRBattleGameMode.cpp`
- `Source/HSR/Battle/HSREncounterTypes.h`
- `Source/HSR/Battle/HSRBattleTransitionSubsystem.cpp`
- `Source/HSR/Data/Definitions/HSREncounterDefinition.h`
- new `Source/HSR/Tests/HSRBattleSettlementIntegrationTests.cpp`
- read-only regressions: `HSR.Battle`, `HSR.Settlement.Foundation`, `HSR.Inventory`, `HSR.Reward`, `HSR.Progression`, `HSR.Map`, `HSR.UI.InventoryReward`
- `tasks/execution-result.md`

This is a candidate list, not an implementation grant. Task Gate must reduce it to the minimum exact subset before implementation.

## Required Task Gate decisions

- Freeze the stable derivation of `RewardTransactionId` from the consumed battle result without generating a new ID on retry.
- Freeze the source of RewardDefinitionId, PlayerCharacterId, reward seed, EXP and all three expected revisions.
- Prove result consumption and settlement submission are exactly once across UI confirm, return travel, replay and World rebuild.
- Define typed handling for victory without reward definition, stale revisions, duplicate matching request and different-payload conflict.
- Keep defeat strictly zero-settlement.
- Preserve 03D1 fixed install/publication contract and forbid direct calls to `SubmitReward`, `ApplyGrants`, `ApplyGrantsInternal` and `GrantExperience` from the aggregate path.
- Select one canonical Reward Summary asset; Blueprint may display receipt values but may not grant, retry or invent settlement data.

## Revised transaction contract

1. The settlement TransactionId is exactly the consumed Encounter/Battle `RequestId`; no confirmation, retry or return callback may generate another ID.
2. `UHSREncounterDefinition` adds a non-negative `VictoryExperience` value. Admission copies it into the pure-value `FHSREncounterRequest`; BattleGameMode stores the consumed request for the Battle World lifetime. Blueprint does not calculate EXP.
3. On the first victory confirmation, BattleGameMode snapshots Inventory/Profile/Reward revisions and constructs one immutable `FHSRSettlementRequest` from the stored Encounter request and `ActivePlayerCharacterId`.
4. BattleGameMode retains that exact request and the committed receipt until return travel succeeds or the Battle World ends. A retry must resubmit the cached request or reuse the cached committed receipt; it must never recalculate expected revisions.
5. Defeat bypasses SettlementAuthority entirely. Victory with no RewardDefinitionId is a typed configuration failure and restores confirm; it is not silently treated as defeat/no-reward.
6. Settlement `Success` and matching `NoOp` are committed outcomes. Any other result restores confirm, preserves the authoritative Battle result, performs no return travel and exposes no success receipt.
7. Only after committed settlement may the Coordinator result be consumed and return requested. If return initiation rejects after settlement, the cached committed receipt/result remains retryable without a second domain mutation.
8. The existing 03D1 Authority and domain implementations are read-only unless runtime RED proves a defect in their frozen contract and Task Gate is reopened.
9. Existing `BuildVictoryRewardRequest` and legacy public Reward APIs remain regression-compatible, but the production GameMode settlement path no longer calls `SubmitReward`.
10. Presentation uses existing read-only `UHSRInventoryRewardViewModel`. Canonical asset is `/Game/UI/WBP_RewardSummary_P13`; `/Game/UI/WBP_Reward_Summary_P13` is an excluded user-owned duplicate.

## Required TDD matrix

- victory commits exactly one Inventory/Profile/Reward settlement and one receipt;
- duplicate result/confirm/return callback returns the stored receipt with no second mutation/event;
- defeat performs no prepare/install/publication;
- missing reward, character or result identity rejects before mutation;
- stale Inventory/Profile/Reward revision rejects with global zero pollution;
- same transaction with different reward/character/seed/revisions returns typed conflict;
- settlement prepare failure leaves Battle result retry semantics explicit and does not partially return/travel;
- settlement success followed by injected return rejection retries from the cached request/receipt with no second revision/event;
- zero configured EXP commits Inventory/Reward while preserving Profile revision/event; positive configured EXP advances the selected Profile once;
- return occurs only from the final committed or typed no-settlement state;
- existing Battle, Reward, Inventory, Progression and return regressions remain compatible.

## Candidate Editor Gate

- Existing `/Game/Data/Rewards/DA_Reward_P13_Standard`.
- Existing authorized Battle encounter/reward fixture.
- Canonical `/Game/UI/WBP_RewardSummary_P13`; the similarly named underscore asset is excluded.
- `/Game/Data/Encounters/DA_Encounter_Phase5Test` adds the user-configured non-negative victory EXP used by the existing authorized encounter fixture.
- Save All/reopen, then victory -> one receipt -> return; defeat -> no receipt; duplicate confirm -> no duplicate reward.

## Non-goals

- No Equipment, Save schema/load, Map frontend, cloud, networking, result-screen redesign or new module/plugin.
- No Blueprint item/EXP mutation or OpenLevel ownership.
- Do not start 03E or later packages.

## Current gate

Initial Coordinator evidence audit returned `REVISE`: the draft had no authoritative EXP source and did not preserve the original expected revisions across post-settlement return retry. The revised contract closes both gaps and removes unnecessary Reward/UI/Coordinator/Transition-header files from the candidate write set. Independent Reviewer rereview commit `6e42517` returned `PASS` and confirmed implementation feasibility, the minimum allowlist, TDD sufficiency and Editor boundaries. Source, Content, Build, Automation and PIE remain unauthorized until the implementation role restates this frozen contract and the user separately confirms `TASK-P17-PATCH-03D2`.
