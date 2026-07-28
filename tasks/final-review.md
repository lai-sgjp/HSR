# TASK-P17-PATCH-03D2 Task Gate Rereview

Status: `PASS / IMPLEMENTATION NOT AUTHORIZED`

## Review Object

- Task: `TASK-P17-PATCH-03D2`
- Task name: Battle Result Settlement Integration
- Reviewer role: Independent Reviewer / Codex
- Review date: 2026-07-28
- Reviewed state: current `tasks/active-task.md` after Coordinator revise commit `6c1be18`

## Review Inputs

- `tasks/active-task.md`
- `tasks/execution-result.md`
- `tasks/review-template.md`
- `PROJECT_STATE.md`
- `todo_plan.md`
- latest `worklog.md` entries
- `docs/CODEX-NAVIGATION-GUIDE.md`
- `docs/phase-17-patch-03-execution-plan.md`
- `tasks/archive/TASK-P17-PATCH-03D1-final-review.md`
- Current read-only source around BattleGameMode, BattleTransition, Encounter DTOs, EncounterDefinition, BattleCoordinator reward request, Reward/Inventory/Profile settlement seams and Reward Summary UI references
- Content path existence check for `/Game/UI/WBP_RewardSummary_P13`, `/Game/UI/WBP_Reward_Summary_P13`, `/Game/Data/Encounters/DA_Encounter_Phase5Test`, and `/Game/Data/Rewards/DA_Reward_P13_Standard`

## Findings

No blocking findings.

The revised 03D2 task card closes the prior Task Gate blockers:

- Victory EXP now has an authoritative cross-World source. `UHSREncounterDefinition` must add non-negative `VictoryExperience`; admission copies it into pure-value `FHSREncounterRequest`; BattleGameMode stores the consumed request for the Battle World lifetime. Blueprint is explicitly barred from calculating EXP.
- Settlement retry is now immutable. The first victory confirmation snapshots Inventory/Profile/Reward revisions, builds one `FHSRSettlementRequest`, then caches that exact request and the committed receipt. Retries may reuse only the cached request/receipt and must not recalculate expected revisions.
- Post-settlement return failure is covered. `Success` and matching `NoOp` are committed settlement outcomes; if return initiation rejects after settlement, the cached committed receipt/result remains retryable with no second domain mutation.
- Result consume/return ordering is now correct. The card requires settlement commit before Coordinator result consumption and return request, preserving the authoritative result for failed settlement and avoiding the current legacy risk where reward can commit before result consume.
- Reward Summary ambiguity is resolved. `/Game/UI/WBP_RewardSummary_P13` is canonical; `/Game/UI/WBP_Reward_Summary_P13` is excluded user-owned duplicate content.

## Frozen Minimum Source Allowlist

Task Gate reduces the implementation write set to this exact minimum:

- `Source/HSR/Battle/HSRBattleGameMode.h`
- `Source/HSR/Battle/HSRBattleGameMode.cpp`
- `Source/HSR/Battle/HSREncounterTypes.h`
- `Source/HSR/Battle/HSRBattleTransitionSubsystem.cpp`
- `Source/HSR/Data/Definitions/HSREncounterDefinition.h`
- new `Source/HSR/Tests/HSRBattleSettlementIntegrationTests.cpp`
- `tasks/execution-result.md`

Files intentionally excluded from 03D2 implementation writes:

- `Source/HSR/Battle/HSRBattleCoordinator.h/.cpp`
- `Source/HSR/Battle/HSRBattleTransitionSubsystem.h`
- `Source/HSR/Reward/*`
- `Source/HSR/Inventory/*`
- `Source/HSR/Progression/*`
- `Source/HSR/UI/*`
- `Content/*`, except user Editor Gate actions explicitly performed by the user

`HSRBattleTransitionSubsystem.cpp` remains in scope only for admission copying of `VictoryExperience` into `FHSREncounterRequest`; existing public return APIs are sufficient for the return-rejection test and do not require a new header seam.

## TDD And Evidence Gate

The required TDD matrix is sufficient and must be implemented before production edits are accepted:

- victory commits exactly one Inventory/Profile/Reward settlement and one receipt;
- duplicate confirm/replay/return callback uses stored request/receipt with no second mutation/event;
- defeat performs no prepare/install/publication;
- missing reward, missing character, stale result identity and stale revisions reject before mutation;
- same transaction with different reward/character/seed/revisions returns typed conflict;
- prepare failure restores retry semantics and does not travel;
- settlement success followed by injected return rejection retries from cached request/receipt with no second revision/event;
- zero configured EXP commits Inventory/Reward but preserves Profile revision/event; positive configured EXP advances selected Profile once;
- return is requested only from committed settlement or typed no-settlement state;
- Battle, Settlement.Foundation, Inventory, Reward, Progression, Map and InventoryReward UI regressions remain compatible.

No Build, Automation, PIE or Editor work has been performed for 03D2 yet, which matches the current gate state.

## Scope And Boundary Review

- 03D1 is archived as `PASS` and may be used as the settlement authority contract; 03D2 may not weaken it.
- Production `SubmitReward` usage in BattleGameMode must be replaced by `UHSRSettlementAuthority`; direct `SubmitReward`, `ApplyGrants`, `ApplyGrantsInternal` and `GrantExperience` calls remain forbidden from the aggregate path.
- Defeat remains zero-settlement.
- Victory without `RewardDefinitionId` is a typed configuration failure and must restore confirm; it is not a silent no-reward/defeat path.
- Blueprint may display committed receipt values through the existing read-only InventoryReward ViewModel but may not grant, retry, recalculate or invent settlement data.
- Editor Gate is limited to existing reward/encounter/UI assets and user Save All/reopen plus victory/defeat/duplicate-confirm PIE evidence.

## Verdict

`PASS`

The revised task card is implementable and testable inside the reduced minimum allowlist above. This PASS authorizes only the implementation role's restatement and the user's separate confirmation step; it does not authorize Source, Content, Build, Automation, PIE or asset work by itself.
