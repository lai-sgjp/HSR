# TASK-P17-PATCH-02 Recovery-Seam and Remaining-Contract Review

## Review metadata

- Reviewer: Independent Reviewer / Safety Reviewer
- Reviewed revision: `ebac7b4`
- Result: `REVISE` (recovery revision passes; transaction and full-return gates remain)
- Date: 2026-07-28

## Recovery revision accepted

- Move-failure coverage drives the production `PublishSpawnOriginRecoveryIntent`: it records `MoveFailed`, ends in `ReturningToSpawnOrigin`, writes `PatrolLocation=SpawnOrigin`, and leaves the C++ Nav-ready retry unarmed.
- The controlled invalid-target path clears `TargetActor`, records `LostTarget`, publishes the same bounded return intent/location, and does not create an Encounter request.
- No direct Move, retry loop, Encounter DTO, or user asset change was introduced.
- The report and raw logs support Build success (7 actions) plus `BehaviorTreeAdapter` and `MapContract` Success/exit `0`. Revision provenance remains inside the Controller/test/result allowlist; user assets remain isolated.

## Remaining blocking gates

### 1. Duplicate and post-resolved Encounter contract

The current PATCH-02 allowlist cannot create and inspect a deterministic admitted Transition transaction without initiating travel. Controller `TryRequestEncounterFromCharacter` returns `void`, while the Transition subsystem's useful pending request, resolved IDs, travel state, and side-effect counters are private. The existing public results (`AlreadyPending`, `AlreadyConsumed`) are suitable frozen equivalents, but no existing fixture exposes the before/after transaction snapshot needed by the card.

Minimum allowlist expansion:

- `Source/HSR/Battle/HSRBattleTransitionSubsystem.h`
- `Source/HSR/Battle/HSRBattleTransitionSubsystem.cpp`
- Keep existing `Source/HSR/Battle/HSREncounterTypes.h` and `Source/HSR/Tests/HSRCombatPatchTests.cpp` allowed; no production asset or new file is required.

Minimum dev-only API, guarded by `WITH_DEV_AUTOMATION_TESTS`:

- `FHSRTransitionAutomationSnapshot GetAutomationSnapshot() const`, containing `CurrentState`, `PendingRequest`, `TravelKind`, `TravelRequestId`, pending/resolved counts or membership, and a request-admission mutation count.
- `void SeedPendingEncounterForAutomation(const FHSREncounterRequest& Request)`, establishing one already-admitted pending transaction without OpenLevel.
- `void SeedResolvedEncounterForAutomation(FName EncounterId)`, establishing the post-resolved state without travel.
- `void ResetEncounterAutomationFixture()`, restoring an empty deterministic fixture.

Required assertions using the real `RequestEncounter` rejection branches:

- Seed one pending request, snapshot, call the same request path twice in the same logical frame, and require `AlreadyPending`, no new RequestId, unchanged pending request/state/travel fields, and zero admission-count increase.
- Seed the EncounterId as resolved, snapshot, call `RequestEncounter`, and require `AlreadyConsumed`, invalid/no new RequestId, unchanged state/pending/travel fields, and zero admission-count increase.
- Log before/after state, original and returned RequestIds, result type/reason, epoch/context identity, and mutation counts.

Do not add a second mock implementation of admission logic. The seeds prepare private state; the rejection must execute the production `RequestEncounter` branches. If Coordinator requires controller-level overlap attempt counts too, add only a dev-only getter for the last `FHSREncounterResult` to `AHSREnemyAIController`; do not change the public gameplay signature.

### 2. Full return completion — minimal user PIE

1. Start in patrol and acquire the player; record `Chasing`, epoch, target validity, current location, and SpawnOrigin.
2. Leave sight and do not re-enter perception or overlap; record `LostTarget -> ReturningToSpawnOrigin` and cleared `TargetActor`.
3. Wait until the stock `Move To` completes. Record final Actor location, distance to SpawnOrigin within the configured acceptance radius, final state/Blackboard values, and absence of a C++ Move/timer retry.
4. Preserve the first failure/SKIPPED reason if NavMesh or perception prevents completion.

## Conclusion

`REVISE`: `ebac7b4` closes the deterministic recovery-intent cases, but PATCH-02 still requires the narrowly scoped Transition dev fixture above and one user PIE full-return completion. Do not archive yet.
