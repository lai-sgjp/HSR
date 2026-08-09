# TASK-P17-DIALOGUE-002 TDD Evidence

Date: 2026-08-09
Source intent: `docs/phase-17-execution-plan.md`, sections 6.8, 7.1 Dialogue
Authorization: explicit user authorization for `TASK-P17-DIALOGUE-002`

## Sole outcome

Project authored speaker/body/choice display data into a pure-value Dialogue
presentation snapshot. Begin, repeat-open, selection, stale rejection, exit and
unavailable/failure behavior are event-driven and do not enter the Pause Hub
Router. Quest/Encounter/Reward branch submission remains Dialogue-003.

## User journeys and guarantees

- As a player, an active Dialogue query displays authored speaker, body and
  stable choice identity without retaining an Actor pointer.
- As a player, repeating the same query does not create a second session, while
  a stale query cannot replace the current snapshot.
- As a player, selecting a valid choice advances to the next authored node or
  closes a terminal dialogue; presentation preview does not submit Quest events.
- As a player, invalid, unavailable or failed requests preserve the last
  complete snapshot and expose a typed presentation result plus raw Authority
  result.

## RED evidence

Before production implementation, the real UE5.6 UBT command reached the new
test and failed at the intentionally absent header:

```text
HSRDialoguePresentationTests.cpp(6,1): fatal error C1083:
Cannot open include file: '../UI/Dialogue/HSRDialoguePresentationTypes.h': No such file or directory
```

The same build reported the unrelated `NameLess` duplicate definition in the
dirty worktree's Save/Challenge unity compilation. It was not changed here.

## Implemented contract

- `FHSRDialoguePresentationRequest` carries `FGuid QueryId`, `DialogueId` and
  `NodeId`.
- `FHSRDialoguePresentationChoiceRequest` carries the same active identity plus
  stable `ChoiceId`.
- `FHSRDialoguePresentationSnapshot` contains status, identity, authored
  `SpeakerText`, `BodyText`, display choices and raw Authority result; it has no
  UObject/AActor pointer.
- `UHSRDialoguePresentationViewModel` publishes only complete snapshots,
  rejects stale/invalid/unavailable commands without replacement, and exposes
  Blueprint-safe delegates and result accessors.
- `UHSRDialogueSubsystem::GetNode` and `PreviewChoice` are read-only seams;
  `PreviewChoice` intentionally does not submit Quest events.

## Test specification

| # | Guarantee | Test | Type | Result |
|---|---|---|---|---|
| 1 | Active query projects authored speaker/body/choice text and repeat-open is a no-op | `HSR.Dialogue.Presentation.ActiveQuery` | UE Automation | PASS |
| 2 | Invalid request preserves the last complete snapshot | `HSR.Dialogue.Presentation.FailurePreservesSnapshot` | UE Automation | PASS |
| 3 | Valid selection advances by stable IDs; stale selection preserves current snapshot and no Quest branch is submitted | `HSR.Dialogue.Presentation.Selection` | UE Automation | PASS |
| 4 | Missing Authority is explicit and matching query exit closes cleanly | `HSR.Dialogue.Presentation.ExitAndUnavailable` | UE Automation | PASS |

## Verification boundary

`HSREditor Win64 Development` returned `Result: Succeeded` after the fixture
correction. Focused `HSR.Dialogue.Presentation` discovered 4 tests and all 4
returned `Result={Success}` with command exit code 0. Existing
`HSR.QuestDialogue` discovered 1 test and returned `Result={Success}` with exit
code 0. No coverage command is configured for UE Automation in this project;
the four focused behavior cases plus the Quest/Dialogue regression are the
current evidence.

No Dialogue Overlay/UAsset, Editor, PIE, Git stage/commit/push, or `.claude/**`
operation was performed.
