# TASK-P17-PATCH-03D2 - Battle Result Settlement Integration

Status: `PLANNED / TASK GATE REVIEW REQUIRED / IMPLEMENTATION NOT AUTHORIZED`

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

## Candidate Source allowlist for Task Gate review

- `Source/HSR/Battle/HSRBattleCoordinator.h`
- `Source/HSR/Battle/HSRBattleCoordinator.cpp`
- `Source/HSR/Battle/HSRBattleGameMode.h`
- `Source/HSR/Battle/HSRBattleGameMode.cpp`
- `Source/HSR/Battle/HSRBattleTransitionSubsystem.h`
- `Source/HSR/Battle/HSRBattleTransitionSubsystem.cpp`
- `Source/HSR/Reward/HSRSettlementAuthority.h`
- `Source/HSR/Reward/HSRSettlementAuthority.cpp`
- `Source/HSR/Reward/HSRSettlementTypes.h`
- `Source/HSR/Reward/HSRRewardSubsystem.h`
- `Source/HSR/Reward/HSRRewardSubsystem.cpp`
- `Source/HSR/Reward/HSRRewardResolver.h`
- `Source/HSR/Reward/HSRRewardResolver.cpp`
- `Source/HSR/UI/HSRInventoryRewardViewModel.h`
- `Source/HSR/UI/HSRInventoryRewardViewModel.cpp`
- `Source/HSR/UI/HSRInventoryRewardTypes.h`
- `Source/HSR/UI/HSRInventoryRewardWidget.h`
- `Source/HSR/UI/HSRInventoryRewardWidget.cpp`
- new `Source/HSR/Tests/HSRBattleSettlementIntegrationTests.cpp`
- exact affected Battle/Reward tests selected by Task Gate
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

## Required TDD matrix

- victory commits exactly one Inventory/Profile/Reward settlement and one receipt;
- duplicate result/confirm/return callback returns the stored receipt with no second mutation/event;
- defeat performs no prepare/install/publication;
- missing reward, character or result identity rejects before mutation;
- stale Inventory/Profile/Reward revision rejects with global zero pollution;
- same transaction with different reward/character/seed/revisions returns typed conflict;
- settlement prepare failure leaves Battle result retry semantics explicit and does not partially return/travel;
- return occurs only from the final committed or typed no-settlement state;
- existing Battle, Reward, Inventory, Progression and return regressions remain compatible.

## Candidate Editor Gate

- Existing `/Game/Data/Rewards/DA_Reward_P13_Standard`.
- Existing authorized Battle encounter/reward fixture.
- One canonical Reward Summary WBP selected by Task Gate after resolving similarly named assets.
- Save All/reopen, then victory -> one receipt -> return; defeat -> no receipt; duplicate confirm -> no duplicate reward.

## Non-goals

- No Equipment, Save schema/load, Map frontend, cloud, networking, result-screen redesign or new module/plugin.
- No Blueprint item/EXP mutation or OpenLevel ownership.
- Do not start 03E or later packages.

## Current gate

03D2 is planning-only. Independent Reviewer, feasibility and Editor-boundary review must pass before the implementation role restates the frozen contract. Source, Content, Build, Automation and PIE remain unauthorized until the user separately confirms `TASK-P17-PATCH-03D2`.
