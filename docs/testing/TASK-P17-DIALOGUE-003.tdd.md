# TASK-P17-DIALOGUE-003 TDD Evidence

Date: 2026-08-09
Source intent: `docs/phase-17-execution-plan.md`, sections 6.8, 7.1 Dialogue
Authorization: explicit user authorization for `TASK-P17-DIALOGUE-003`

## Sole outcome

Dialogue choices submit Quest, Reward and Encounter branches through their
existing Authorities exactly once. A stable `BranchOperationId` identifies each
explicit Authority branch. Replays are `NoOp`, operation-ID reuse for another
Choice is rejected, and any Authority failure preserves the last complete
Presentation snapshot without recording a successful branch.

## User journeys and guarantees

- As a player, selecting a Quest branch submits the authored Quest event once;
  repeating the same choice does not duplicate downstream effects.
- As a player, selecting a Reward branch submits the authored reward once with
  the stable operation ID as its `ClaimId`; replay returns the same receipt and
  does not duplicate inventory.
- As a player, selecting an Encounter branch forwards the complete authored
  request with the stable operation ID as `RequestId`; the Dialogue layer does
  not invent enemy, map or return context.
- As a player, a missing or rejecting Authority leaves the current dialogue
  presentation intact and exposes the raw branch result; a conflicting reuse
  of an operation ID is rejected without a second submission.

## RED evidence

Before production implementation, the real UE5.6 UBT run compiled the new
`HSRDialogueAuthorityTests.cpp` and reached the intended missing branch
contract/API compiler failures. This was a valid compile-time RED for the
branch request/result and Authority forwarding seam. It was not treated as a
runtime pass and no production implementation was present at that point.

## Implemented contract

- `EHSRDialogueChoiceBranch` explicitly models `None`, `Quest`, `Encounter`
  and `Reward`.
- `FHSRDialogueChoiceRequest` carries stable Dialogue/Node/Choice identity;
  `BranchOperationId` is authored on the choice definition and is required for
  explicit Authority branches.
- Quest dispatch calls `UHSRQuestSubsystem::SubmitEvent`.
- Reward dispatch calls `UHSRRewardSubsystem::SubmitReward` with
  `BranchOperationId` as `FHSRRewardRequest::ClaimId`.
- Encounter dispatch calls
  `UHSRBattleTransitionSubsystem::SubmitEncounterRequestFromUI` with the full
  authored `FHSREncounterRequest` and `BranchOperationId` as `RequestId`.
- The Dialogue branch ledger records only successful/NoOp dispatches. Matching
  replay returns `NoOp`; an operation ID attached to a different choice returns
  `OperationIdConflict`. The Presentation ViewModel retains the previous full
  snapshot on failure and exposes the raw branch result.

## GREEN evidence

The actual validation commands were:

```text
"E:/programs/Epic Games/UE_5.6/Engine/Build/BatchFiles/Build.bat" HSREditor Win64 Development -Project="E:/work/unreal_projects/HSR/HSR.uproject"
UnrealEditor-Cmd.exe HSR.uproject -ExecCmds="Automation RunTests HSR.Dialogue.Authority; Quit" -TestExit="Automation Test Queue Empty" -unattended -nop4 -nosplash -NullRHI -NoSound -log
UnrealEditor-Cmd.exe HSR.uproject -ExecCmds="Automation RunTests HSR.Dialogue.Presentation; Quit" -TestExit="Automation Test Queue Empty" -unattended -nop4 -nosplash -NullRHI -NoSound -log
UnrealEditor-Cmd.exe HSR.uproject -ExecCmds="Automation RunTests HSR.QuestDialogue; Quit" -TestExit="Automation Test Queue Empty" -unattended -nop4 -nosplash -NullRHI -NoSound -log
git diff --check
```

Results:

- Development Editor Build: `Result: Succeeded`.
- `HSR.Dialogue.Authority`: 5 discovered, 5/5 `Result={Success}`.
- `HSR.Dialogue.Presentation`: 4 discovered, 4/4 `Result={Success}`.
- `HSR.QuestDialogue`: 1 discovered, 1/1 `Result={Success}`.
- `git diff --check`: passed.

## Test specification

| # | What is guaranteed | Test target | Test type | Result | Evidence |
|---|---|---|---|---|---|
| 1 | Quest branch submits once; replay is `NoOp` and operation-ID conflict does not mutate inventory | `HSR.Dialogue.Authority.QuestBranchExactlyOnce` | UE Automation authority integration | PASS | Authority suite 5/5 |
| 2 | Reward branch uses the stable operation ID as `ClaimId`; replay does not duplicate reward inventory | `HSR.Dialogue.Authority.RewardBranchExactlyOnce` | UE Automation authority integration | PASS | Authority suite 5/5 |
| 3 | Encounter forwards the authored request with stable `RequestId`; replay does not start travel twice | `HSR.Dialogue.Authority.EncounterBranchExactlyOnce` | UE Automation authority integration | PASS | Authority suite 5/5 |
| 4 | Missing Authority preserves node/body/choices and keeps the raw reward result visible | `HSR.Dialogue.Authority.FailurePreservesPresentation` | UE Automation failure path | PASS | Authority suite 5/5 |
| 5 | Incomplete Encounter context and missing operation ID are rejected at definition registration | `HSR.Dialogue.Authority.BranchDefinitionValidation` | UE Automation validation | PASS | Authority suite 5/5 |
| 6 | Existing Presentation ViewModel and Quest/Dialogue behavior remain green after Authority seam integration | `HSR.Dialogue.Presentation`, `HSR.QuestDialogue` | UE Automation regression | PASS | 4/4 and 1/1 |

## Coverage and known gaps

UE Automation in this project does not emit source coverage, so no coverage
percentage is claimed. The code gate does not include a Dialogue Overlay,
HUD/input mapping, focus management, travel teardown/arrival restore, UAsset or
Editor integration, visual PIE, or user acceptance. Those are intentionally
deferred to the separately authorized `TASK-P17-DIALOGUE-004` package.

No Quest/Reward/Encounter Authority implementation was changed. No Git
stage/commit/push operation was performed, and `.claude/**` remains excluded.
