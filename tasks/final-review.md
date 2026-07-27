# TASK-P17-PATCH-01A Harness Assertion Fix Re-review — `06f84db`

Status: `PASS WITH FOLLOW-UP`

## Static gate

- Commit `06f84db` changes exactly two authorized files: one expectation token in `Source/HSR/Battle/HSRBattleGameMode.cpp` and the execution record. No production Status validation/runtime code changed.
- The harness now expects `InvalidDefinition` for the copied DoT object whose `StatusId` is changed to `Status.Unsupported` while its GrantedTag remains `Status.Debuff.DamageOverTime`. This matches the frozen generic semantics: `Status.Unsupported` has a valid `Status.*` root, but exact Tag/Id identity fails.
- Missing infinite GE, missing DamageRule, and missing DamageType expectations remain unchanged. The prior PIE failure and all earlier review verdicts remain recorded below.
- The reported Development Editor Build is credible for the one-token C++ harness change. The latest `Saved/Logs/HSR.log` independently shows one matched `HSR.Battle.Patch.StatusGeneric`, `Result={Success}`, and `TEST COMPLETE. EXIT CODE: 0` at 2026-07-27 18:38 local time.
- Independent `git diff --check` exits 0. Dirty Coordinator/user files remain outside the implementation commit.

## Required user P9 PIE rerun

The static/Automation gate passes, but the corrected development harness must still be executed in PIE. Acceptance requires all of these exact markers in the new post-fix log:

- `P9-001 Status Harness=COMPLETE`
- `P9-002 Stack Case=OldRemoveFailure Result=PASS`
- `P9-002 Stack Harness=COMPLETE`
- `P9-003 DotBreak Case=InvalidDefinition_GE_Rule_DamageType Result=PASS`
- `P9-003 DotBreak Harness=COMPLETE`
- no `P9-001`, `P9-002`, or `P9-003` `Result=FAIL`, `Harness=INCOMPLETE`, or unexpected `SKIPPED` marker

## Verdict and route

`PASS WITH FOLLOW-UP`. The authorized assertion repair is correct, minimal, compiled, and leaves production behavior untouched. Route to the user for one P9 PIE rerun. If the exact acceptance markers above are present, return the log to this Reviewer for final `PASS`; otherwise return to the Implementation Agent only for the first real failing case.

---

# Prior Review — TASK-P17-PATCH-01A User PIE Evidence Review — 2026-07-27

Status: `BLOCKED`

## Evidence inspected

- Independently inspected user-supplied PIE log `C:/Users/Lai/.codex/attachments/5932ed77-67b8-4dbd-99ce-d29d33824c8a/pasted-text.txt` rather than relying on the mechanical summary.
- `P9-001 Status Harness=COMPLETE`.
- `P9-002 Stack Harness=COMPLETE`, including `OldRemoveFailure Result=PASS` with the retained old handle, `RollbackAttack=120.00`, successful later clear, and expected remove count.
- P9-003 normal DoT/Break runtime cases pass, including add/no-immediate-damage, duplicate add, turn/epoch guards, failure retry, final-turn damage then expiry, duplicate Break operation, DoT/Break coexistence, invalid target, missing ASC, manager replacement, EndPlay cleanup, finished-state isolation, reset, and lethal paths.
- The sole failed case is `P9-003 DotBreak Case=InvalidDefinition_GE_Rule_DamageType`; consequently `P9-003 DotBreak Harness=INCOMPLETE FailedCases=1`.

## Root cause and contract assessment

This is a stale harness expectation, not evidence that the generic production validation should restore an ID whitelist. At `HSRBattleGameMode.cpp:1164-1170`, `InvalidId` changes the copied DoT Definition's ID to `Status.Unsupported` but leaves its GrantedTag as `Status.Debuff.DamageOverTime`, then expects `InvalidStatusId`. Under PATCH-01A's frozen field-driven contract, `Status.Unsupported` has a valid `Status.*` root; the actual defect in this object is the exact Tag/Id mismatch, whose structured result is correctly `InvalidDefinition`. Returning `InvalidStatusId` here would require reintroducing unsupported-name knowledge or changing the agreed validation precedence.

The sibling checks remain consistent with the new validator: missing infinite GE returns `MissingGameplayEffect`, while missing DamageRule or DamageType returns `InvalidDefinition`. The new Automation already covers a mismatched Tag/Id as `InvalidDefinition` and an actually invalid root (`Buff.Shield`) as `InvalidStatusId`.

## Blocking authorization boundary

The minimal correction is limited to `Source/HSR/Battle/HSRBattleGameMode.cpp:1168`: update the stale `InvalidId->Validate()` expected result from `InvalidStatusId` to `InvalidDefinition`, optionally rename the local/case wording to make “TagIdMismatch” explicit, then rebuild and rerun the same P9 PIE harness until P9-003 reports `COMPLETE`.

However, the current exact allowlist authorizes changes to `HSRBattleGameMode.cpp` **only** for the P9 `OldRemoveFailure` atomic-rollback assertion. This Reviewer cannot expand that authorization or edit implementation/harness code. Do not weaken `UHSRStatusDefinition::Validate()` or add a `Status.Unsupported` special case merely to satisfy the obsolete assertion.

## Verdict and automatic route

`BLOCKED` pending a narrow user/Coordinator allowlist expansion for the single P9-003 validation assertion described above. After authorization, return to the same Implementation Agent for the harness-only correction, Development Editor Build, named `StatusGeneric` Automation, and P9 PIE recheck; then route back to this Reviewer. The earlier engineering `PASS WITH FOLLOW-UP` remains historically valid, but PATCH-01A cannot be archived while its required P9 gate reports `INCOMPLETE`.

---

# Prior Review — TASK-P17-PATCH-01A Re-review of Implementation Fix `d1f8eb0`

Status: `PASS WITH FOLLOW-UP`

## Re-review scope and provenance

- Re-reviewed only Implementation fix commit `d1f8eb0` against the prior `REVISE` findings and the frozen PATCH-01A card.
- The commit changes only the existing allowlisted Status component, Status Automation test, and execution report. It contains no Save, Turn algorithm, Break publication, Content, additional Config, module, or generated-output change.
- Coordinator/user dirty files remain separate and untouched. The prior implementation and reviewer commits are preserved; this is not an amended history.

## Prior blocker closure

1. **Real generic runtime coverage — closed.** `HSRCombatPatchTests.cpp` now creates a transient World and Actor, the project ASC and CoreAttributeSet, initializes ActorInfo and positive MaxHealth/Health/Speed, initializes a one-participant TurnManager, and binds a real StatusComponent. AttackUp, SpeedUp and Shield each execute the same production `AddOrRefreshStatus` and typed query path. The test additionally covers duplicate OperationId, Attack refresh and expiry, Speed stacking and expiry, Shield expiry/re-add/clear, and structured `UnknownStatus`.
2. **Clear failure ownership — closed.** `ClearStatus()` now iterates entries in place, removes the RuntimeDefinition and map entry only after a successful removal or when no active GE exists, and retains the exact instance/Definition/active handle when removal fails. The controlled test proves `RemoveFailed`, retained typed ownership and active GAS handle, followed by a successful retry and `UnknownStatus`. The iterator implementation also preserves failed entries while independently dropping successfully removed entries; no secondary handle was reintroduced.

## Safety and evidence

- Runtime Definition GC ownership remains a reflected `TMap<FName, TObjectPtr<UHSRStatusDefinition>>`; ASC/TurnManager/Coordinator remain weak references. The fix adds no Tick, latent callback, re-entrant action resolution, or persistent/save state.
- Final `Saved/Logs/HSR.log` independently shows exactly one matched `HSR.Battle.Patch.StatusGeneric`, `Result={Success}`, and `TEST COMPLETE. EXIT CODE: 0` at 2026-07-27 18:28 local time. The only recorded test warning is the pre-existing GameplayCue path fallback.
- The execution report preserves the historical `msbuild` PATH failure, expanded-Automation exit `-1` caused by Health/MaxHealth fixture ordering, and intermediate invalid-cast compile failure instead of overwriting first-error history. It reports the final Development Editor Build and Automation exit 0.
- Independent `git diff --check` exits 0; only line-ending warnings refer to unrelated dirty files.
- P9 `OldRemoveFailure` remains an authorized harness-only assertion change. Existing P9 status regression is truthfully `NOT EXECUTED`; no Automation result is presented as production PIE evidence.

## Verdict and follow-up

`PASS WITH FOLLOW-UP`. PATCH-01A's implementation, scope, targeted Build, and new generic runtime Automation satisfy the engineering gate. Before final archival/transition to PATCH-01B, run the existing P9 Battle PIE/development harness to confirm legacy AttackUp/DoT/Break add, stack, refresh, dispel, source-invalid, expiry, replace, and the newly authorized `OldRemoveFailure` atomic-rollback assertion in the real project entry path. This is a user PIE evidence gate, not an implementation revision request.

---

# Prior Review — TASK-P17-PATCH-01A Initial Review

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
