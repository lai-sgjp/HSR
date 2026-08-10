# TASK-P17-DIALOGUE-004 TDD Evidence

Status: `COMPLETE / GATE 0 PASS / TDD RED CONFIRMED / GREEN PASS / FOCUSED AUTOMATION PASS / EDITOR-PIE PASS / USER ACCEPTED`

This report covers the Phase 17 execution-plan meaning of P17-012. It does not
reuse the archived `TASK-P17-012` five-module dynamic-mount task.

## Source contract and user journeys

The source contract is the Gate 0 section of
`tasks/active-task.md`. The implementation proves these journeys at the code
boundary:

- As a player, I press F on a valid Dialogue interactable and receive one
  event-driven Overlay with a typed DialogueId/NodeId start payload.
- As a player, I navigate authored choices through the Presentation ViewModel;
  invalid or stale choices do not replace the last complete snapshot.
- As a player, I close with Escape, Gamepad Back, X, a terminal choice or the
  explicit close intent, and the input policy returns to exploration.
- As a player, I should never see a second/stale Overlay after duplicate opens,
  create/attach/policy/focus failures or authorized travel.

## RED evidence

Before production implementation, the real UE5.6 UBT run compiled
`Source/HSR/Tests/HSRDialogueOverlayLifecycleTests.cpp` and reached the
intended compile-time RED:

```text
HSRDialogueOverlayLifecycleTests.cpp: Cannot open include file:
'../UI/Dialogue/HSRDialogueOverlayWidget.h'
```

The missing header was the absent production Overlay seam referenced by the
new test. This is the valid RED signal for this package; it is not being
reclassified as a runtime fixture or dependency failure.

## GREEN implementation summary

- `FHSRInteractionResult` now carries an explicit typed Dialogue payload.
- `AHSRHUD` forwards the payload to `UHSRUIManagerSubsystem`; it does not infer
  Dialogue identity from an Actor or prompt text.
- `UHSRUIManagerSubsystem` owns exactly one Overlay/ViewModel pair, attaches it
  above the Exploration HUD, applies UIOnly input and preferred focus, blocks
  Frontend modules while active, and compensates failed operations.
- `UHSRDialogueOverlayWidget` exposes snapshot/choice seams with safe bounds
  checks and submits only ViewModel/UIManager intents. It does not call an
  Authority or add itself to the viewport.
- Travel teardown releases the Overlay and does not restore its old query.

## Verification commands and results

Build:

```text
"E:/programs/Epic Games/UE_5.6/Engine/Build/BatchFiles/Build.bat" HSREditor Win64 Development -Project="E:/work/unreal_projects/HSR/HSR.uproject" -WaitMutex -NoHotReload
```

Result: `Succeeded` (`Target is up to date`).

Focused UE Automation commands were run with the equivalent quoted command
line below and independent logs under `Saved/Logs/`:

```text
Automation RunTests HSR.Dialogue.Overlay; Quit
Automation RunTests HSR.Dialogue.Authority; Quit
Automation RunTests HSR.Dialogue.Presentation; Quit
Automation RunTests HSR.QuestDialogue; Quit
```

| # | What is guaranteed | Test target | Test type | Result | Evidence |
|---|---|---|---|---|---|
| 1 | Interaction returns explicit Dialogue payload with stable IDs | `HSR.Dialogue.Overlay.InteractionPayload` | UE Automation | PASS | Overlay suite 4/4 |
| 2 | Unbound Widget access is safe; choice bounds and Escape/Back/X seams are deterministic | `HSR.Dialogue.Overlay.WidgetSeams` | UE Automation | PASS | Overlay suite 4/4 |
| 3 | Open/close is single-instance; duplicate open and repeat close do not corrupt ownership | `HSR.Dialogue.Overlay.Lifecycle` | UE Automation | PASS | Overlay suite 4/4 |
| 4 | Missing class, create, attach, policy and focus failures leave no stale Overlay and preserve compensation | `HSR.Dialogue.Overlay.FailureAndTravel` | UE Automation | PASS | Overlay suite 4/4 |
| 5 | Travel teardown releases the Overlay and arrival does not restore stale Dialogue | `HSR.Dialogue.Overlay.FailureAndTravel` | UE Automation | PASS | Overlay suite 4/4 |
| 6 | Dialogue branch Authority forwarding and failure snapshot retention remain green | `HSR.Dialogue.Authority` | UE Automation regression | PASS | 5/5, exit code 0 |
| 7 | Presentation active query/selection/exit and Quest regression remain green | `HSR.Dialogue.Presentation`, `HSR.QuestDialogue` | UE Automation regression | PASS | 4/4 and 1/1, exit code 0 |

All discovered tests returned `Result={Success}`. `git diff --check` passed; the
only printed messages were the repository's known line-ending conversion
warnings. UE Automation does not emit a source coverage percentage for this
target, so no coverage number is claimed.

## Editor/PIE acceptance handoff (completed by user)

The following operations are intentionally user-owned because the agent cannot
author, save/reopen or visually inspect UMG/DataAsset/map content.

1. Reopen `E:/work/unreal_projects/HSR/HSR.uproject` after the build. In the
   Content Browser create `/Game/UI/P17/Dialogue/WBP_DialogueOverlay_P17` as a
   User Widget. Open Class Settings and set Parent Class to
   `UHSRDialogueOverlayWidget`; Compile and Save.
2. Add a full-screen Overlay/Canvas layout with a panel, Speaker Text, Body
   Text, several focusable choice Buttons and an always-visible Close Button.
   Implement `OnDialogueSnapshotChanged`: break the snapshot, set SpeakerText
   and BodyText, call `GetChoiceCount`, and call bounds-safe `GetChoiceAt` for
   each fixed button slot. Set each button's text from `DisplayText` and hide
   slots beyond the count. Each slot's OnClicked calls
   `SubmitChoiceByIndex(0)`, `(1)`, etc.; the Close Button calls
   `RequestCloseDialogue()`.
3. Implement `GetPreferredFocusWidget` in the Widget Blueprint and return the
   always-visible Close Button (or a guaranteed-visible first choice Button).
   Do not call `AddToViewport`, `OpenLevel`, Router, Quest, Reward, Encounter,
   Inventory or Save from the graph; UIManager owns attachment and lifecycle.
4. Create `/Game/Data/Dialogue/DA_Dialogue_P17_Demo` as a
   `UHSRDialogueDefinition`. Set `DialogueId=Dialogue.P17.Demo` and
   `StartNodeId=Node.Start`. Author non-empty SpeakerText, body Text and
   choice DisplayText. Add `Node.Start` with one `None` branch choice targeting
   `Node.End`, then `Node.End` with one `None` branch choice whose
   `TargetNodeId` is empty to exercise terminal close. Save the asset.
5. Place `AHSRDialogueInteractable` in the exploration map. Assign the exact
   same `DialogueId` and `DialogueDefinition`; leave QuestDefinition empty for
   this first no-authority lifecycle pass. Place it inside the existing
   interaction range and save the level.
6. Open `BP_HSRHUD` Class Defaults. Set `HUD|P17 -> Dialogue Overlay Widget
   Class` to `WBP_DialogueOverlay_P17`; Compile and Save. Use existing
   `FrontendNavigationMappingContext` only—no new InputAction/MappingContext or
   Config is needed.
7. Use Save All, close the Editor, reopen it, and verify the WBP, Data Asset,
   map actor and HUD class reference persist. Then start PIE on the exploration
   map, approach the actor, confirm `Talk`, and press F. Confirm one overlay,
   initial focus, speaker/body/choices, next-node update, terminal close,
   Escape/Gamepad Back/X close, and that frontend module opens are rejected
   while Dialogue is active.
8. With the Overlay open, perform one authorized map travel. Confirm exactly
   one teardown, no old Dialogue restoration on arrival, a clean Exploration
   Root and GameOnly input. In Output Log check for no Blueprint Runtime Error,
   Ensure or array-index error, and record any first concrete failure before
   closing the task.

## User acceptance evidence

User created and saved the following Editor assets:

- `Content/Data/Dialogue/DA_Dialogue_P17_Demo.uasset`: `DialogueId` is
  `Dialogue.P17.Demo`, `StartNodeId` is `Node.Start`, Start has a None-branch
  `Choice.Proceed -> Node.End`, and End has terminal `Choice.End` with an empty
  `TargetNodeId`. Quest/Reward/Encounter fields are empty for this pass.
- `Content/UI/P17/Dialogue/WBP_DialogueOverlay_P17.uasset`: parent is
  `UHSRDialogueOverlayWidget`; the full-screen layout contains Speaker, Body,
  four choice buttons and CloseButton. Choice/close Blueprint events compile
  and persist.
- `Content/Blueprints/UI/BP_HSRHUD.uasset`: `DialogueOverlayWidgetClass` is
  bound to the WBP. `Map_Exploration_P15_A` contains the configured
  `AHSRDialogueInteractable` at `(0,300,92)`.

The base interactable's discoverability was corrected with a visible sphere
marker (`NoCollision`) and interaction radius 260. User PIE evidence observed
the Talk candidate at distance 140, a successful typed payload for
`Dialogue.P17.Demo / Node.Start`, Overlay open result 0 with `FocusResult=1`,
and frontend-module rejection while Dialogue was active (`AlreadyOpen`, result
12). The user confirmed choice progression, CloseButton, Escape, Gamepad Back,
X and authorized travel teardown/no stale restoration. No Blueprint Runtime
Error, Ensure or array-bounds error was observed.

The known `HSR.UI.ScreenLifecycle` fixture failures and unrelated `NameLess`
dirty-worktree duplicate-definition issue remain outside this task's product
scope. No Git stage, commit, push, reset, clean or `.claude/**` operation was
performed.
