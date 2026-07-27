# TASK-P17-PATCH-02 Complete Independent Gate Review

## Review metadata

- Reviewer: Independent Reviewer / Safety Reviewer
- Reviewed implementation chain: `62c593d` through `7aa1205`, including `9bd9200`, `eb0bd14`, `46e86e1`, and `2f96e84`
- Reviewed user-evidence record: `52686ca`
- Result: `REVISE`
- Date: 2026-07-28

## Evidence accepted

- Build evidence: latest relevant editor Build reports success; the final adapter Build is 7 actions, exit `0`.
- Automation evidence: `HSR.Exploration.Patch.BehaviorTreeAdapter` and `HSR.BattleReturn.MapContract` completed with `Success`, exit `0`.
- Automation covers default BT/BB references, no Actor Tick, initial epoch, reachable/fallback patrol publication, distinct projection/random failure reasons, one-shot Nav-ready duplicate/stale/exact consumption, pre-BeginPlay SpawnOrigin fallback/post-capture stability, and perception-versus-explicit Encounter-entry separation.
- User-provided PIE covers repeated reachable patrol candidates, perception `2 -> 3 -> 4` without submission, sight loss `4 -> 7 -> 8`, reacquisition `8 -> 3 -> 4`, one physical-overlap Encounter request/travel, and an observed clean UnPossess log path.
- Read-only package tokens support the six-key Blackboard schema and a stock-node BT graph containing Blackboard decorators, selectors/sequences, `Move To`, `Wait`, Patrol/Encounter/LostTarget/MoveFailed/Returning state keys. No custom polling Service token was found.
- Source changes stayed inside the frozen C++/test/result allowlist. Dirty user `Content/AI/**`, Map, Enemy DataAsset, learning, and `.claude/**` changes remain separated from Implementation/Reviewer commits.

## Blocking code finding

Lifecycle Blackboard cleanup is not atomic. `OnUnPossess` and `EndPlay` call `StopBehaviorTreeRuntime()` and then `ClearState()`. `StopBehaviorTreeRuntime()` clears Blackboard keys but leaves `RuntimeBlackboard` non-null. The subsequent `ClearState()` calls `SetBlackboardTarget(nullptr)` and `WriteBlackboardRuntimeState()`, repopulating `SpawnOrigin`, `AIState`, `TreeEpoch`, and `EncounterRequestId` after teardown. Therefore the frozen requirement that runtime keys are cleared across UnPossess/EndPlay is not currently satisfied, and the user log’s absence of a stale callback does not prove key cleanup.

Minimum correction: after stopping/clearing logic, detach/null `RuntimeBlackboard` before `ClearState` can publish, or reorder teardown so final key clearing is the last Blackboard operation. Add an Automation seam/assertion for all six keys and stale callbacks after both UnPossess-equivalent and EndPlay-equivalent teardown, then rebuild and rerun the adapter test.

## Missing required acceptance evidence

The active-task matrix explicitly requires, but current evidence does not verify:

- complete return-to-SpawnOrigin movement completion;
- movement failure and abort behavior;
- target destruction cleanup;
- same-frame duplicate overlap submission with before/after request ID and result reason;
- already-resolved/post-consumption rejection;
- stale callback after actual re-possess/EndPlay (the Nav-ready seam tests epoch consumption but not the complete controller/Blackboard lifecycle);
- exact before/after state, epoch, target validity, and request/result IDs for those cases.

These are completion-gate requirements, not optional post-task polish. Add deterministic Automation for the transaction/lifecycle cases that do not require Editor navigation, and obtain focused user PIE for full return plus movement failure/abort and target destruction. Preserve failed/SKIPPED evidence separately.

## Asset/provenance boundary

The user-owned BT, BB, Map, and DataAsset remain dirty/untracked and must not be absorbed into an Implementation or Reviewer commit. Before final acceptance, the Coordinator must record their user provenance and saved/reopened evidence; asset submission, if desired, remains a separate user-owned commit/decision.

## Conclusion

`REVISE`: the main path is credible, but teardown currently rewrites cleared Blackboard state and several explicitly required lifecycle/transaction/runtime cases remain unverified. Do not archive PATCH-02 yet.
