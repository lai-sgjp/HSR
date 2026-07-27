# TASK-P17-PATCH-02 Teardown Revision Review

## Review metadata

- Reviewer: Independent Reviewer / Safety Reviewer
- Reviewed revision: `52250a8`
- Result: `REVISE`
- Date: 2026-07-28

## Production fix verified

- Teardown now stops Brain logic, clears Blackboard state, nulls `RuntimeBlackboard`, invalidates the active request, and advances the epoch only when runtime ownership existed.
- Because the pointer is detached before caller cleanup, subsequent `ClearState` from UnPossess/EndPlay cannot repopulate Blackboard keys. A repeated Stop after teardown is epoch-idempotent.
- The one-shot retry remains cleared/invalidated before stale work. A fresh bind obtains a later epoch.
- Revision provenance stays inside the Controller/test/result allowlist. User Map, Enemy DataAsset, `Content/AI/**`, learning, and `.claude/**` changes remain isolated.
- Raw logs show the final `BehaviorTreeAdapter` and `MapContract` runs succeeded with exit `0`; the report records Build as 7 actions, exit `0`. Earlier failed adapter runs remain preserved.

## Blocking Automation finding

The test does not truly verify the six Blackboard keys. `AreBlackboardRuntimeKeysClearForAutomation` ignores `InBlackboard` values and returns only whether an internal synthetic bitmask is zero. `WriteBlackboardRuntimeState` sets that mask wholesale to `0x3f`, and `ClearBlackboardRuntimeState` resets it wholesale to zero independently of whether each `ClearValue` call remains correct. Thus removing or breaking any one of the six real key clears would still pass every “six keys clear” assertion.

Minimum correction: seed all six actual keys in the world-backed Blackboard with non-default values, call Stop, and query the Blackboard component itself to prove each key is cleared. Repeat the real-value assertions after `ClearState`, repeated Stop, fresh bind/stale callback, and the EndPlay-equivalent ordering. Remove the synthetic mask or retain it only as non-authoritative diagnostics. Rebuild and rerun both tests.

## Remaining completion-gate evidence

Even after the teardown test is corrected, the active acceptance matrix still lacks:

- same-frame duplicate overlap with preserved request ID, structured result/reason, and before/after state;
- post-resolved/already-resolved rejection;
- full stock `Move To SpawnOrigin` completion;
- move failure and abort recovery;
- target destruction cleanup.

These remain blocking for PATCH-02 archive because the active card explicitly requires them. Minimal user PIE steps:

1. Lose sight without reacquiring; wait until the enemy reaches SpawnOrigin and capture state/location before and after completion.
2. Temporarily make the recovery/patrol destination unreachable or interrupt the active stock Move To; capture failure/abort, `MoveFailed -> ReturningToSpawnOrigin`, and absence of a C++ retry loop.
3. Destroy the perceived target during Chasing; capture target validity, cleared `TargetActor`, state transition, and recovery.
4. Trigger two overlap notifications in the same frame, then repeat after the first transaction resolves; capture request IDs, result reasons, state, epoch, and subsystem counts before/after. If the Editor cannot reliably force same-frame overlap, implement a deterministic allowlisted transaction seam instead of inferring it from a single successful overlap.

## Conclusion

`REVISE`: the production teardown ordering fixes the identified rewrite defect, but the purported six-key test is synthetic rather than Blackboard-backed, and the explicitly required transaction/movement/destruction matrix is still incomplete. Do not archive PATCH-02.
