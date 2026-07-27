# TASK-P17-004 Execution Result

Status: `PASS / ENGINEERING AND USER PIE GATES COMPLETE`

## Implementation

- Map authority publishes a typed pure-value `ArrivalCommitted` notification only after successful ordinary-arrival or battle-return commits, with a monotonic generation and map/arrival/kind payload.
- The LocalPlayer UIManager owns a pure-value travel descriptor. Restore requires both a strictly newer registered Host generation and an arrival generation at or above the frozen baseline; either ordering converges without Tick, Delay, latent retry or old-World UObject retention.
- Production capture accepts `EndPlay(LevelTransition)`, or the project's real OpenLevel behavior (`Destroyed`) only while MapSubsystem/BattleTransition exposes an authorized pending travel. Ordinary unregister, manual rebuild, PIE Stop and unqualified destruction create no descriptor.
- CharacterDetail and Inventory are the only restorable screens. Pause/root become root; foreign, depth >2 and half-pair states are forced to exact root and reported Inconsistent rather than preserving a buried or foreign screen.
- Restore consumes the descriptor before attempting candidate construction. Missing-class/pre-push and compensated one-shot failures remain root-only; persistent policy failure reports Inconsistent with logical root/no transient ownership.
- Internal request-token allocation now advances beyond the ScreenStack's last processed token, so forced cleanup remains monotonic even after a foreign request.

## Evidence

- Development Editor Build after final token fix: 4/4 compile/link/metadata actions, `Result: Succeeded`.
- `Saved/Logs/P17-004-ScreenLifecycle-Final4.log`: 7 Success, 0 Fail.
- `Saved/Logs/P17-004-Map-Final.log`: 5 Success, 0 Fail.
- `Saved/Logs/P17-004-ScreenStack-Final.log`: 3 Success, 0 Fail.

## Automation coverage

Root, CharacterDetail and Inventory restore; arrival-before-host and host-before-arrival; duplicate host/arrival; stale and wrong arrival generation; A-to-B-to-C descriptor supersession; Pause non-restore; foreign and depth >2 forced-root cleanup; inventory half-pair cleanup; missing class; one-shot and persistent policy failures; ordinary unregister/non-LevelTransition no descriptor; failure/no-teardown complete snapshot preservation; typed ordinary/battle arrival payload and unrelated-state zero emission.

## Evidence boundary

Automation exercises the controlled publisher through the same private helper as production but does not claim real World/Pawn placement. The completed User PIE gate supplies production callsite, root convergence, cursor/movement and Reward Summary evidence. Menu persistence paths blocked by UIOnly interaction are `NOT EXECUTABLE`. No stage, commit or push.

## Production PIE finding

- User log `3e04d1f3-c90e-47d3-847b-890ce95db33b`: 9 production ArrivalCommitted events (7 ordinary, 2 battle return), 7 ordinary travel issues and 2 committed battle returns.
- The same log contains zero `TravelFreeze`, `TravelArrival` or `TravelRestore Consume` events. Therefore the real OpenLevel flow did not enter the frozen `EndPlay(LevelTransition)` capture branch; successful root recreation alone is not P17-004 restore evidence.
- An EndPlay-reason diagnostic isolated the production lifecycle mismatch without weakening the prior engineering Automation.
- Follow-up PIE proved real OpenLevel reports `EndPlay Reason=Destroyed`. The production predicate now captures `LevelTransition`, or `Destroyed` only while MapSubsystem/BattleTransition exposes an authorized pending travel; PIE Stop and ordinary destruction remain non-travel.
- Post-fix Development Editor Build succeeded (8/8 actions), and `Saved/Logs/P17-004-ScreenLifecycle-LifecycleFix.log` is 7 Success / 0 Fail.
- Production recheck `2ff5d239-3f2a-4d71-ab3b-d002cb8edca4` passed: `Destroyed` with pending travel captured descriptor generation 1 from Host 1; new Host 2 and ordinary Arrival generation 1 converged to one successful root consume (`Result=0`, `Stack=1`). PIE Stop subsequently reported `CaptureTravel=false`.
- Full production log `cf1fc4b8-f2cc-444d-adff-e95e16db2f68`: 9 Freeze, 9 Arrival and 9 Consume events pair exactly across Host generations 1-to-10 and Arrival generations 1-to-9. Seven ordinary travels and two battle returns all consume once at root with `Result=0`, `Stack=1`; both battle input restores are Success; zero Inconsistent/Error/FAILED.
- User confirmed every return rebuilt a normal HUD and restored W/A/S/D plus mouse behavior; both battle rounds displayed Reward Summary exactly once.
