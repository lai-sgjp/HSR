# TASK-P17-PATCH-02 Transition-Fixture Review

## Review metadata

- Reviewer: Independent Reviewer / Safety Reviewer
- Reviewed revision: `b181534`
- Result: `REVISE`
- Date: 2026-07-28

## Fixture design verified

- `RequestEncounter` keeps its production signature and rejection flow. New snapshot/seed/reset APIs and the admission counter are guarded by `WITH_DEV_AUTOMATION_TESTS`.
- The fixture uses a valid `UGameInstance` outer. Pending and resolved seeds only prepare private state; both rejection checks execute production `RequestEncounter`.
- Pending rejection returns `AlreadyPending`; resolved rejection returns `AlreadyConsumed`; state/travel/request-ID membership and admission-count fields currently compared by the test remain unchanged.
- Build, `BehaviorTreeAdapter`, and `MapContract` are reported and logged as successful with exit `0`. The earlier invalid-outer failure remains preserved. Revision provenance is limited to the approved Transition/test/result expansion, and user assets remain isolated.

## Blocking assertion gap

The test text claims “full snapshot zero mutation” and “no new ID”, but it does not prove either completely:

- It never asserts that `PendingDuplicate.RequestId` and `ResolvedReplay.RequestId` are invalid/no-new-ID.
- Although the snapshot contains a full `FHSREncounterRequest`, comparison checks only `PendingRequest.RequestId`. Mutations to EncounterId, EnemyDefinitionId, Initiative, BattleMapPath, ReturnTransform, ExplorationMapPath, RewardDefinitionId, or RewardSeed would pass unnoticed.
- It does not assert a nonempty structured failure message/reason payload.

Minimum correction inside the existing allowlist:

- Add a test-local equality helper for every `FHSREncounterRequest` field listed above and use it for pending/resolved before/after snapshots.
- Assert both returned rejection RequestIds are invalid, both result types are exact, and both failure messages are nonempty.
- Retain the existing state, TravelKind, TravelRequestId, resolved-membership, and admission-count equality checks.
- Rebuild and rerun `BehaviorTreeAdapter` and `MapContract`, preserving the prior invalid-outer first error.

## Remaining task gate after that correction

If those deterministic assertions pass, the only remaining acceptance item is user PIE proof of full stock `Move To SpawnOrigin` completion:

1. Acquire the player, then leave sight without reacquiring or overlapping.
2. Capture `Chasing -> LostTarget -> ReturningToSpawnOrigin`, epoch, cleared target, start location, and SpawnOrigin.
3. Wait for Move To completion and capture final location/distance within acceptance radius plus final state/Blackboard values.
4. Confirm no C++ Move or repeating retry occurred; preserve the first failure/SKIPPED reason if completion cannot be produced.

## Conclusion

`REVISE`: the dev-only fixture correctly reaches the production rejection branches, but its current assertions do not yet establish the claimed no-new-ID and full-request zero-mutation guarantees. After this narrow test fix, only full-return user PIE should remain.
