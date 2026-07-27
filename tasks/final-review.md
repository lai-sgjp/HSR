# TASK-P17-PATCH-01A Final Review

Status: `REVISE`

## Scope and provenance

- Reviewed implementation commits `e93b04c` and `955d5d3` against `tasks/active-task.md` and `docs/phase-17-patch-01-execution-plan.md`.
- The implementation commits contain only authorized files. `Config/DefaultGameplayTags.ini` adds exactly `Status.Buff.SpeedUp` and `Status.Buff.Shield`; the `HSRBattleGameMode.cpp` change is limited to the authorized P9 `OldRemoveFailure` harness assertion.
- Coordinator/user dirty files (`tasks/active-task.md`, `docs/phase-17-patch-01-execution-plan.md`, `learn/SaveSystem.md`, `.claude/**`) remain outside the implementation commits and were not modified by this review.
- Role commits use the Implementation Agent message format. No generated output, Content, Save, Turn, Break, Build.cs, module, or unrelated Config change is present.

## Confirmed implementation properties

- Definition validation no longer contains the three-ID whitelist. It validates the `Status.*` root, exact `StatusId`/GrantedTag identity, enum/configuration values, refresh/stack contract, infinite-GE presence, and DoT field contract.
- Runtime storage is one `TMap<FName, FHSRStatusInstance>` plus one `TSet<FGuid>` OperationId path. AttackUp-specific routing, the secondary handle, and the split operation-ID sets are removed.
- `GetPublicSnapshot` returns structured `UnknownStatus` and preserves the requested ID. Public snapshot lists remain lexically sorted.
- Runtime Definition references are held by a reflected `TMap<FName, TObjectPtr<UHSRStatusDefinition>>`; ASC, TurnManager, and Coordinator references are weak. Delegate unbinding remains in lifecycle cleanup. No Tick or latent/Delay path was introduced.
- Add/refresh/stack, per-ID clear/dispel/expiry, source-invalid removal, and explicit replace all use the unified map. The authorized P9 harness now checks that old-remove failure rolls back the new GE and preserves the old instance before later clear.

## Blocking findings

| Severity | File/evidence | Problem | Required minimal action |
|---|---|---|---|
| Blocking | `Source/HSR/Tests/HSRCombatPatchTests.cpp:13-50`; task Validation | `HSR.Battle.Patch.StatusGeneric` only calls `Validate()` on Attack/Speed/Shield and performs an unknown lookup on an uninitialized component. It does not prove the frozen outcome that all three Definitions traverse the same add/query/refresh/stack/expire/clear runtime path. The execution report explicitly delegates runtime mutation to P9, but P9 covers existing AttackUp/DoT/Break and is `NOT EXECUTED`; it cannot prove SpeedUp/Shield use the new runtime path. | Within the existing test allowlist, add a real initialized transient ASC/TurnManager/status-component fixture and exercise distinct Attack/Speed/Shield through the generic runtime. Cover successful add and typed query for all three, plus refresh/stack as applicable and deterministic expiry/clear. Re-run the named Automation and record actual results. |
| Blocking | `Source/HSR/Status/HSRStatusComponent.cpp:279-300` | `ClearStatus()` records a failed GE removal but unconditionally empties `Statuses` and `RuntimeDefinitions`. A still-active GE can therefore become orphaned from the component, violating the task's handle ownership/clear compensation requirement and making retry impossible. This is especially relevant to EndPlay/reset and multi-status cleanup. | Preserve every entry whose active GE removal fails (and its RuntimeDefinition), remove only successfully cleared or already-inactive entries, return `RemoveFailed`, and add a controlled failure assertion proving ownership/snapshot survives and a later clear succeeds. Do not reintroduce a secondary handle. |

## Evidence assessment

- The reported Development Editor builds and two `StatusGeneric` runs are documented with exit code 0, but the current test's coverage is insufficient for the task's runtime acceptance contract.
- Existing P9 runtime regression is accurately marked `NOT EXECUTED`; therefore add/refresh/stack/dispel/expiry/source-invalid/replace compensation regression is not independently established for this revision.
- Independent `git diff --check` exits 0 (only line-ending warnings on unrelated dirty files).
- No user PIE evidence is required to diagnose the two engineering blockers. After fixes and automated runtime coverage, the existing P9 PIE harness remains an explicit user gate if no standalone equivalent is added.

## Verdict

`REVISE`. The unified architecture and authorized atomic-replace direction are acceptable, but runtime acceptance coverage is materially weaker than the frozen task card, and bulk clear can lose ownership of a live GameplayEffect after removal failure. Return automatically to the same Implementation Agent for the two minimal allowlist fixes, new Build/Automation evidence, and an updated role commit; then route back to this independent Reviewer.

---

# Prior Review — TASK-P17-004

Status: `PASS`

## Engineering review

- Independent implementation review: `PASS`. Prior blockers for exact-root forced cleanup, strict newer-host matching, arrival latch/baseline, stale/superseded events and missing test coverage are closed.
- Development Editor Build succeeded after the final monotonic-token correction.
- `Saved/Logs/P17-004-ScreenLifecycle-Final4.log`: 7 Success, 0 Fail.
- `Saved/Logs/P17-004-Map-Final.log`: 5 Success, 0 Fail.
- `Saved/Logs/P17-004-ScreenStack-Final.log`: 3 Success, 0 Fail.
- `git diff --check` exits 0; line-ending notices only. P17-004 edits remain inside its frozen allowlist; unrelated dirty-worktree files were preserved.

## Evidence boundary

Automation proves the internal transaction, both host/arrival orderings, typed publisher contract, failure compensation and stale/supersession behavior. It does not prove real World/Pawn placement, production ordinary/battle callsite emission, visible Slate focus, physical input or Reward Summary presentation.

## User PIE gate

- Initial PIE exposed that this project's OpenLevel reports HUD `EndPlay(Destroyed)`, not `LevelTransition`; the trigger was repaired with an authorized-pending guard and its Build/Automation recheck passed.
- Post-fix A-to-B proved one exact Freeze/Arrival/Consume across Host 1 to Host 2, with root `Result=0`, `Stack=1`; PIE Stop remained non-capturing.
- The full log proves nine exact production transaction pairs, including two battle returns, with monotonic Host/Arrival generations, root success and no structured failures.
- User confirmation received: HUD, W/A/S/D and mouse behavior were normal after every return, and Reward Summary appeared exactly once in each of the two battle rounds.
- CharacterDetail/Inventory and Pause travel through the current UIOnly interaction are `NOT EXECUTABLE` where no legal travel entry remains available; no OpenLevel workaround was added.
- Unsafe travel-failure injection is `NOT USER VERIFIED`; physical gamepad is `NOT VERIFIED`.

## Verdict

P17-004 passes its engineering, independent review and User PIE gates. Production ordinary travel and battle return both rebuild the root UI exactly once with restored input, cursor and HUD behavior; each tested battle round shows one Reward Summary. No Git stage, commit or push was performed.
