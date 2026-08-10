# TASK-P17-DIALOGUE-001 TDD Evidence

Date: 2026-08-09
Source intent: `docs/phase-17-execution-plan.md`, sections 6.8, 7.1 Dialogue,
and 8.1
Authorization: explicit user authorization for `TASK-P17-DIALOGUE-001`

## Sole outcome

Freeze the Interaction/Dialogue contract and establish a real RED boundary for
the missing presentation seam. This package does not implement Dialogue UI,
speaker/choice display data, Authority branch forwarding, UAsset wiring, PIE,
or Phase 18.

## User journeys and contract guarantees

- As a player, pressing `F` against a Dialogue candidate must identify one
  stable active query without entering the Pause Hub Router.
- As a player, the Dialogue presentation must project stable DialogueId, NodeId,
  and ChoiceId values from the Dialogue authority rather than holding an Actor
  pointer or parsing a log message.
- As a player, repeating the same interaction while the query is active must
  not create a second presentation session.
- As a player, an invalid or stale request must preserve the last complete
  presentation snapshot and expose a typed failure separately.

## Gate 0 decisions

1. `UHSRInteractionComponent` owns the current candidate and `F` attempt.
   Dialogue Overlay is an Exploration Overlay and does not enter the Pause Hub
   Frontend Router.
2. `UHSRDialogueSubsystem` owns registered definitions, node lookup, choice
   progression, and Quest event submission. Quest, Reward, Encounter, and
   BattleTransition remain their own authorities.
3. A future presentation request uses pure values: `FGuid QueryId`,
   `DialogueId`, and current `NodeId`. A future snapshot contains stable
   `ChoiceId` values and no UObject/AActor pointer.
4. Repeating the active query is a no-op. Invalid/stale begin or choice failure
   must not replace the last complete snapshot.
5. `FHSRDialogueNodeDefinition` currently has body text only. Speaker display
   data and authored choice labels require an explicit later data-contract
   decision; the UI must not invent them from IDs.

## RED evidence

Test file:

- `Source/HSR/Tests/HSRDialoguePresentationTests.cpp`

The test intentionally references the future presentation contract:

- `FHSRDialoguePresentationRequest`
- `FHSRDialoguePresentationSnapshot`
- `EHSRDialoguePresentationResult`
- `UHSRDialoguePresentationViewModel`

The production headers and implementation are intentionally absent in this
package. After the user fully exited Unreal Editor, the real UE5.6 UBT run
reached the intended first failure:

```text
HSRDialoguePresentationTests.cpp(6,1): fatal error C1083:
Cannot open include file: '../UI/Dialogue/HSRDialoguePresentationTypes.h': No such file or directory
```

This confirms the requested compile-time RED. The same build also reported the
unrelated duplicate `NameLess` definition in `HSRSaveVersion.cpp` and
`HSRChallengeProgressionSubsystem.cpp`; it is outside the Dialogue-001
allowlist and was not changed.

## Test specification

| # | Guarantee | Test | Type | Result |
|---|---|---|---|---|
| 1 | One active query projects stable DialogueId/NodeId/ChoiceId values and repeated open is a no-op | `HSR.Dialogue.Presentation.ActiveQuery` | UE Automation compile-time RED | RED CONFIRMED |
| 2 | Invalid request does not replace the last complete snapshot | `HSR.Dialogue.Presentation.FailurePreservesSnapshot` | UE Automation compile-time RED | RED CONFIRMED |

## Allowlist and non-goals

Writable production scope is empty for this Gate 0/RED package. Only the test
file and task/evidence Markdown files are changed. No Dialogue Overlay,
ViewModel implementation, Interaction/Dialogue Authority mutation, UAsset,
Config, PIE, Phase 18, Git stage/commit/push, or `.claude/**` change is allowed.

## Merge evidence

No Git checkpoint was created because the user authorized the task, not an
automatic commit. Gate 0 and the compile-time RED are complete; GREEN and all
later Editor/PIE work remain separately authorized tasks.
