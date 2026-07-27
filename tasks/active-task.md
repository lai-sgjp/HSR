# TASK-P17-004 — Travel UI Teardown/Rebuild and Logical Focus Restore

Status: `PASS / ARCHIVED`

## Single outcome

Across registered-map A→B travel and B→Battle→B return, the old World's Exploration UI is deterministically torn down, the new HUD registers exactly once, and a restorable Menu screen is rebuilt only after both the new host and authoritative map-arrival commit exist. Input, cursor, Exploration IMC and preferred/root focus match the rebuilt logical stack without retaining any old-World UObject.

## Frozen ownership and descriptor

- `UHSRUIManagerSubsystem` remains the LocalPlayer-scoped owner of Screen Stack, input/focus policy, registered host and on-demand screen instances.
- The travel restore descriptor is pure value only: descriptor generation/token, captured host generation, minimum expected arrival-commit generation, and optional ScreenId. It stores no World, HUD, PC, Widget, ViewModel, delegate owner or focus-widget pointer.
- Only `CharacterDetail` and `Inventory` Menu screens are restorable. Pause/Modal is never restored across World travel; root-only remains root-only.
- Travel capture accepts `EndPlay(LevelTransition)`, or `Destroyed` only with an authorized Map/Battle pending travel, as required by the project's observed OpenLevel lifecycle. Manual HUD rebuild, PIE stop and unqualified destruction use ordinary unregister and create no descriptor. `RemoveExplorationHUD` by itself never implies travel.
- UIManager exposes one atomic matched-host API that validates `(HUD, PC, current host generation)`, captures or rejects the descriptor, tears down, and unregisters in the same transaction. A stale HUD cannot reserve/overwrite a descriptor or mutate a newer host.
- Map authority adds a dedicated pure-value `ArrivalCommitted` event, emitted exactly once only after successful `CommitPendingArrival` or `CommitBattleReturnLocation`. Its payload contains monotonic ArrivalCommitGeneration, MapId, ArrivalId and `OrdinaryTravel|BattleReturn`; unlock/flag/current-location/save-restore events never emit it.
- UIManager subscribes to the dedicated arrival event during subsystem initialization, before any descriptor/host ordering is possible, and removes the handle on deinitialize. New HUD registration and a qualifying arrival commit newer than the descriptor baseline are independent prerequisites; whichever arrives second attempts one candidate-first restore.
- Successful restore consumes the descriptor exactly once. A new matched LevelTransition capture supersedes any older pending descriptor and invalidates its arrival latch. Duplicate/stale arrivals and host callbacks are zero-change.
- A failed candidate restore consumes/cancels the descriptor. Pre-push failures remain root-only. Recoverable one-shot post-push failures must compensate to root and reapply Exploration policy. Persistent compensation/policy failure must report Inconsistent with logical root/no transient ownership when achievable; actual input recovery is explicitly `FAILED / NOT VERIFIED`, never claimed safe.
- Travel failure/cancel that leaves the old World alive performs no HUD teardown and therefore preserves the existing host/stack. Stale old-host unregister and stale arrival callbacks cannot mutate a newer host generation.

## Transaction order

1. On an authorized travel EndPlay, atomically validate the matched old host and inspect its complete stack/ownership snapshot.
2. Freeze only root or exactly one supported Menu ScreenId, plus old-host and arrival-generation baselines, into a monotonically generated descriptor. Pause, foreign top, stack depth >2, half-pair or inconsistent state produces root-only/no-menu descriptor only after forced cleanup; it never selects a buried Menu.
3. Tear down old widget subscriptions/instances, unregister the host, and reduce the logical stack to root.
4. Register the new HUD/PC/root idempotently.
5. Observe the dedicated authoritative `ArrivalCommitted` event without owning or mutating map/travel state.
6. When both prerequisites match the pending generation, open the supported Menu through the existing `OpenCharacterDetailScreen` or `OpenInventoryScreen` transaction.
7. Apply preferred focus after successful widget attach. Recoverable rejection compensates to root with Exploration policy and no partial ownership; persistent compensation/policy failure reports Inconsistent with logical root/no ownership where achievable and actual input `FAILED / NOT VERIFIED`.

## Exact allowlist

- `Source/HSR/UI/HSRUIManagerSubsystem.h`
- `Source/HSR/UI/HSRUIManagerSubsystem.cpp`
- `Source/HSR/UI/HSRHUD.h`
- `Source/HSR/UI/HSRHUD.cpp`
- `Source/HSR/Map/HSRMapTypes.h`
- `Source/HSR/Map/HSRMapSubsystem.h`
- `Source/HSR/Map/HSRMapSubsystem.cpp`
- `Source/HSR/Tests/HSRUIScreenLifecycleTests.cpp`
- `Source/HSR/Tests/HSRMapSubsystemTests.cpp`
- `tasks/active-task.md`
- `tasks/execution-result.md`
- `tasks/final-review.md`

Read-only dependencies: ScreenStack, InputModeCoordinator, PlayerController, all other `Source/HSR/Map/**`, all `Source/HSR/Battle/**`, Save and gameplay authorities. The Map change is limited to the typed notification seam after existing successful commits; it cannot change validation, placement, travel, pending, rollback or Save behavior.

Map Automation may add a `WITH_DEV_AUTOMATION_TESTS` controlled post-commit publisher that calls the same private `PublishArrivalCommitted` helper used by both production success callsites. Unit tests prove shared payload/generation/exactly-once behavior and prove unrelated state mutations do not publish; they do not claim to execute real World/Pawn placement. Actual `CommitPendingArrival` and `CommitBattleReturnLocation` callsite emission requires structured PIE log evidence.

## Automation Gate

- Root-only teardown/new-host arrival rebuilds root once and owns no transient screen.
- CharacterDetail and Inventory each freeze, tear down old pair/delegates, register a distinct host, wait for arrival, then restore exactly once with a fresh widget/VM and preferred focus result.
- Arrival-before-host and host-before-arrival both converge to the same snapshot and consume one descriptor; these internal orderings are Automation-only evidence.
- Typed Map tests prove the shared ordinary/battle post-commit publisher payload and generation contract; unlock, flag, current-location and restore-like state changes emit zero arrival events. Real successful placement/callsite emission is PIE evidence, not unit-fixture evidence.
- Duplicate arrival, wrong/old arrival generation, duplicate host registration, stale old-host capture/unregister and stale descriptor callback are zero-change. A→B→C supersedes the older descriptor/latch deterministically.
- Manual Phase2 HUD rebuild, PIE stop and any EndPlay without authorized travel create no descriptor.
- Pause never restores; travel teardown unpauses only manager-owned pause and new host starts at root.
- Missing class, VM initialization and widget creation failures leave root-only/no ownership and consume the descriptor. One-shot attach/policy failure compensates to root; persistent policy/compensation failure is Inconsistent and must not claim actual input recovery.
- Foreign top, half-pair and inconsistent teardown remain structured failures; forced cleanup holds widget-before-VM ordering.
- Travel failure/cancel without old-host teardown preserves the complete current snapshot and input policy.

## User Editor / PIE Gate

- In both `Map_Exploration_P15_A/B`, verify World Settings uses `BP_HSRGameMode`; its defaults use `BP_HSRHUD` and `BP_HSRPlayerController`, with the existing P17 widget classes. `Map_Battle` must retain `BP_HSRBattleGameMode`; do not replace it with the Exploration GameMode.
- Save All, close/reopen Editor. Use Selected Viewport PIE and the existing graybox interaction (`F`) with `Teleport.AB` / `Teleport.BA`; one cycle means A→B→A. Run root-only A→B→A twice and manually confirm movement/cursor after each arrival.
- Run CharacterDetail and Inventory separately: open the Menu and trigger the existing authorized travel entry only where interaction remains possible. The user confirms the same logical page/content and visible preferred Back focus; structured logs/Automation—not visual inference—prove a fresh widget identity/generation, fresh bind count and zero callbacks from the old instance. Back once to root, then return. If UIOnly prevents the interaction, record that page/travel combination `NOT EXECUTABLE` rather than adding Blueprint OpenLevel logic.
- In B, use the existing `Graybox Interactable` (`F`) to enter `Enc_Test_Phase5`, win, Confirm, and return to B twice. Confirm root/HUD rebuild, cursor/movement and no duplicate Reward Summary receipt. Menu persistence through Battle is required only if a legitimate non-UI travel entry can be invoked; otherwise mark it `NOT EXECUTABLE`.
- Pause is never restored after legitimate travel. If UIOnly prevents any existing non-UI travel entry, the user marks this path `NOT EXECUTABLE`; Pause non-restore, arrival-before-host/host-before-arrival and travel failure injection remain Automation evidence, not user claims.
- Safe rejection only: temporarily clear either HUD CharacterDetail or Inventory Widget Class, make three Open requests expecting the existing structured missing-class result with root stack/no ownership, restore the class, Save All, reopen, and verify normal Open/Back. Do not break Map assets or Config.
- User records visible preferred focus style, cursor, actual W/A/S/D movement, one Back action, and page content. Logs record descriptor/host/arrival generations, ScreenId/depth, ownership, input policy/IMC and bind/unbind counts. Physical gamepad remains optional/`NOT VERIFIED` when unavailable.

## Evidence required

- Development Editor Build.
- `HSR.UI.ScreenLifecycle`, `HSR.UI.ScreenStack`, and any new narrow travel-lifecycle Automation all explicit Success.
- Automation logs explicitly cover arrival-before-host, host-before-arrival and failure/no-teardown preservation.
- PIE logs cover two ordinary A→B→A cycles, two battle returns, real typed arrival emission at both production callsites, and final root/menu convergence where legally executable; PIE does not claim which internal prerequisite arrived first. Unsafe failure injection is `NOT USER VERIFIED`.
- `git diff --check`; no stage, commit or push without separate instruction.

## Explicit non-goals

No Map/Battle/Save authority changes; no OpenLevel from UI; no Tick, arbitrary Delay or latent retry; no old-World UObject persistence; no Pause restoration; no new module/plugin/CommonUI/InputAction/Config; no Quest/Map/Settings page work; no Phase 18 art/audio/VFX; no asset deletion, Git stage, commit or push.
