# TASK-P17-PATCH-03D1 - Atomic Settlement Foundation

Status: `PLANNED / TASK GATE PASS / USER CONFIRMATION REQUIRED`

## Role Lock

This card is a planning artifact. No Source, Content, Config, Build, Automation, PIE or implementation commit is authorized until Independent Reviewer and feasibility review pass the Task Gate, the implementation role restates the frozen contract, and the user separately confirms `TASK-P17-PATCH-03D1`.

## Prerequisite and sole outcome

- `TASK-P17-PATCH-03C` is archived as `PASS`; Battle admission and stable player identity are available.
- Reward, Inventory and Character Profile can each prepare a pure-value candidate without live mutation, and one bounded SettlementAuthority can install one fully validated aggregate through non-failing internal primitives.
- Production Battle settlement is not switched in 03D1. Existing callers remain behavior-compatible until 03D2.

## Ownership contract

1. `UHSRRewardSubsystem` remains the reward-definition, idempotency-ledger and receipt authority. It prepares the reward-ledger candidate and publishes only after all domain states are installed.
2. `UHSRInventorySubsystem` remains item ownership/count authority. It prepares a complete post-transaction inventory candidate without changing live stacks, revisions or events.
3. `UHSRCharacterProfileSubsystem` remains progression authority. It prepares the complete target Profile/EXP candidate without changing live profiles, ASC projection, revisions or events.
4. New `UHSRSettlementAuthority` is the only aggregate coordinator permitted to invoke all three private/internal install seams. It does not become the owner of Inventory, Profile or Reward state.
5. Candidate DTOs contain values only. They may contain stable IDs, copied entries, expected revisions and deterministic transaction metadata; they contain no Actor, Widget, World, subsystem, ASC, GE handle or mutable UObject reference.
6. Prepare may fail and must be side-effect free. Install may not fail: it may only move/swap already allocated candidate state into its owning subsystem.
7. Fixed installation order is Inventory -> Profile -> Reward ledger. Revisions, delegates, receipts and presentation become visible only after all three installs complete at one publication boundary.
8. Each domain must add a distinct private/friend `PrepareSettlementCandidate`, `InstallSettlementCandidateNoFail` and `PublishSettlementCommit` seam. Only SettlementAuthority may coordinate all three domains; these seams are not `UFUNCTION` and are unavailable to Blueprint.

## Atomicity contract

1. SettlementAuthority validates TransactionId, RewardDefinition, PlayerCharacterId and all expected revisions before prepare.
2. Each domain prepares independently from one immutable aggregate input. Definition resolution, arithmetic/overflow checks, capacity checks, allocation and duplicate detection occur before install.
3. A prepare failure leaves all three live states, revisions, events, receipts and projections byte-for-byte unchanged.
4. After all candidates are complete, install primitives cannot allocate, resolve Definitions, apply GameplayEffects, broadcast, increment revisions or return a business failure.
5. Only after the three installs succeed does one publication step advance the relevant revisions and emit committed domain events plus one receipt.
6. Duplicate TransactionId is idempotent: it returns the existing committed result/receipt and creates no new candidate, revision or event.
7. Compensation, rollback-by-opposite-command and an `Inconsistent` terminal state cannot satisfy 03D1. If non-failing install cannot be proven inside the frozen boundaries, stop and request redesign.
8. The aggregate commit path is forbidden from calling existing `SubmitReward`, `ApplyGrants`, `ApplyGrantsInternal` or `GrantExperience`; those APIs combine fallible preparation with live mutation/publication and remain only production-compatible legacy callers until 03D2.
9. The Reward candidate owns the complete prebuilt post-commit receipt map, fully constructed receipt payload and next revision before aggregate-ready. Reward install moves/swaps that prepared map; it cannot call `TMap::Add` or otherwise allocate after commit begins.
10. Inventory and Profile candidates likewise own complete post-commit containers/snapshots and their next revisions before aggregate-ready. Publish may only assign prepared revision scalars and broadcast already-prepared payload values after all installs.

## Frozen Source write allowlist

- new `Source/HSR/Reward/HSRSettlementTypes.h`
- new `Source/HSR/Reward/HSRSettlementAuthority.h`
- new `Source/HSR/Reward/HSRSettlementAuthority.cpp`
- `Source/HSR/Reward/HSRRewardSubsystem.h`
- `Source/HSR/Reward/HSRRewardSubsystem.cpp`
- `Source/HSR/Reward/HSRRewardTypes.h`
- `Source/HSR/Inventory/HSRInventorySubsystem.h`
- `Source/HSR/Inventory/HSRInventorySubsystem.cpp`
- `Source/HSR/Inventory/HSRItemTypes.h`
- `Source/HSR/Progression/HSRCharacterProfileSubsystem.h`
- `Source/HSR/Progression/HSRCharacterProfileSubsystem.cpp`
- `Source/HSR/Progression/HSRCharacterProfileTypes.h`
- new `Source/HSR/Tests/HSRSettlementAuthorityTests.cpp`
- `tasks/execution-result.md`

Battle, BattleTransition, Coordinator, Interaction, Party, Equipment, Save, Map and UI sources are read-only. The implementation may use fewer files; it may not widen this list itself.

The following existing tests are read-only regressions; 03D1 may not weaken or edit them:

- `Source/HSR/Tests/HSRInventorySubsystemTests.cpp`
- `Source/HSR/Tests/HSRRewardSubsystemTests.cpp`
- `Source/HSR/Tests/HSRRewardIntegrationTests.cpp`
- `Source/HSR/Tests/HSRCharacterProfileSubsystemTests.cpp`
- `Source/HSR/Tests/HSRCharacterProgressionTests.cpp`
- `Source/HSR/Tests/HSRProgressionEffectContractTests.cpp`

## Candidate Asset Gate - compatibility evidence only

- No new asset or business binding.
- `/Game/Data/Rewards/DA_Reward_P13_Standard` and its existing item/drop references.
- `/Game/Data/Progression/DA_CharacterCatalog_P11`, using stable `Character.A`.
- Blueprint may not call prepare/install/publish seams or grant items/EXP.

## TDD and acceptance matrix

Create `HSR.Settlement.Foundation` before production edits and prove an intended RED.

- pure prepare: Reward, Inventory and Profile candidates match expected values while all live snapshots/revisions/events/receipts remain unchanged;
- aggregate success: one valid immutable request prepares all candidates, installs Inventory -> Profile -> Reward, then publishes exactly one coherent revision/event/receipt set;
- invalid RewardDefinition, CharacterId, item Definition, amount, EXP, capacity, overflow or expected revision: typed failure with zero mutation in every domain;
- Definition/candidate preparation failure is injected before aggregate-ready and cannot occur after commit begins;
- duplicate TransactionId: identical committed result, no second revision/event/receipt;
- same TransactionId with different RewardDefinitionId, PlayerCharacterId, seed or expected revisions: typed conflict with zero candidate install, revision, event or receipt mutation;
- stale expected revision and mismatched candidate TransactionId are rejected before install;
- install primitives are inaccessible to Blueprint and callers other than SettlementAuthority;
- the SettlementAuthority path never calls `SubmitReward`, `ApplyGrants`, `ApplyGrantsInternal` or `GrantExperience`;
- existing `SubmitReward`, Inventory mutation and `GrantExperience` public behavior remains regression-compatible because production flow is not switched;
- no ASC projection, GE application, Save capture, UI refresh or Battle caller change is introduced.

One narrow `WITH_DEV_AUTOMATION_TESTS` failure selector may stop after a selected domain builds its local candidate but before the aggregate becomes ready. It may not mutate live state, bypass production validation, simulate a successful install, or inject failure after commit begins. Real process OOM is `NOT VERIFIED`; the deterministic seam proves the architectural boundary, not allocator behavior.

Required read-only regressions are `HSR.Inventory` (currently 3 tests), `HSR.Reward` (currently 6 tests including Integration) and `HSR.Progression` (currently 3 tests). Exact discovered counts must be reported from the final run rather than assumed from source declarations.

## User Editor exercise and evidence

1. Open `/Game/Data/Rewards/DA_Reward_P13_Standard` and verify its existing item/drop references; open `/Game/Data/Progression/DA_CharacterCatalog_P11` and verify stable `Character.A`.
2. Save All, close and reopen those assets. No new asset and no Blueprint settlement node may be created.
3. PIE one ordinary victory/return through the existing production path as a compatibility check. This does not prove the new authority is used.
4. PIE one defeat and confirm the existing no-reward behavior remains unchanged.
5. Explain the separation between fallible prepare, non-failing install and delayed publication, and why production switching is deferred to 03D2.

## Evidence required

- intended RED and matching GREEN for `HSR.Settlement.Foundation`;
- HSREditor fresh Build with UHT/compile/link evidence;
- exact focused and regression Automation counts;
- snapshots proving prepare and every rejected case are zero-pollution across all three domains;
- event/revision/receipt ordering evidence from one aggregate success;
- user Compile/Save/reopen plus victory/defeat compatibility PIE logs;
- `git diff --check`, exact allowlist audit and isolated role commit.

## Explicit non-goals and stop conditions

- No Battle/Coordinator caller switch; that is 03D2.
- No reward grant integration, result-screen redesign, Equipment, Save schema/load, Map, UI, network, plugin, module or third-party asset work.
- No public Blueprint prepare/install API and no live UObject in a candidate.
- Stop if install requires allocation, Definition lookup, GE application, delegate broadcast, revision increment or a fallible return.
- Stop if atomic publication requires edits outside the frozen allowlist.
- Do not begin 03D2, 03E or later packages.

## Current Gate

Task Gate=`PASS` after revision. The first Independent Review required dedicated three-domain prepare/install/publish seams, a fully prebuilt Reward receipt map before commit, a ban on existing mixed mutation APIs, and typed same-TransactionId/different-payload conflict. Those requirements are now frozen. Feasibility audit found the existing restore candidates demonstrate prebuilt-container `MoveTemp` installation, while the new Settlement seams remain isolated from production callers. Editor work is compatibility evidence only. Implementation must restate this card and receive separate explicit user confirmation.
