# TASK-P17-PATCH-02 Execution Result

Status: `BLOCKED ASSET GATE`

## Asset Gate evidence

Read-only inspection found the existing enemy integration assets:

- `/Game/Blueprints/Character/Enemy/BP_HSREnemy_Phase4Test`
- `/Game/Data/Enemies/DA_Enemy_Phase4Test`
- `/Game/Data/Enemies/DA_Enemy_Phase5Test1`

The source currently contains the perception-driven `AHSREnemyAIController`, `AHSREnemyCharacter`, and `UHSREnemyDefinition`, but there are no loadable Behavior Tree, Blackboard, BT Task, BT Service, or BT Decorator assets under `Content/`. `UHSREnemyDefinition` presently provides patrol/chase scalar settings and an Encounter definition only; it has no confirmed Behavior Tree/Blackboard binding fields. Therefore the BT migration cannot safely begin.

## Required user Editor work and confirmation

Create, save, and reopen the following minimal assets in Unreal Editor (or provide the exact existing alternatives):

1. Blackboard: `/Game/AI/Enemy/BB_HSREnemy_Exploration`
   - `TargetActor`: `Object`, base class `Actor`.
   - `SpawnOrigin`: `Vector`.
   - `PatrolLocation`: `Vector`.
   - `AIState`: `Enum`, using the project enemy exploration-state enum (or provide the exact compatible enum asset/type).
   - `TreeEpoch`: `Int`.
   - `EncounterRequestId`: `Name`.
2. Behavior Tree: `/Game/AI/Enemy/BT_HSREnemy_Exploration`, assigned to that Blackboard.
   - It must include distinct event-driven branches for Patrol, Chasing, LostTarget-to-SpawnOrigin, MoveFailed-to-SpawnOrigin, and EncounterPending.
   - Do not add a polling/tick Service. If any Service is necessary, provide its exact path and confirm it has no interval-based state polling.
3. If Blueprint BT nodes are required, create and provide exact paths, parent classes, and exposed fields for each Task/Decorator/Service. At minimum state whether movement is performed by stock `Move To` nodes or a custom task, and how the EncounterPending branch waits for the authoritative C++ result.
4. In `/Game/Blueprints/Character/Enemy/BP_HSREnemy_Phase4Test`, set **AI Controller Class** to `AHSREnemyAIController` and confirm **Auto Possess AI** is the intended placed/spawned setting. Preserve its existing enemy-definition assignment and collision configuration.
5. In each intended enemy definition (at least identify whether `DA_Enemy_Phase4Test` or `DA_Enemy_Phase5Test1` is the acceptance asset), confirm the desired Encounter definition, PatrolRadius, PatrolWaitTime, and ChaseAcceptanceRadius values. Also authorize adding only the explicit C++ asset-reference fields needed to bind the confirmed BT and Blackboard, if such fields are still absent after the asset check.
6. Confirm the sight/perception settings used by the selected Enemy Blueprint/Controller: sight radius, lose-sight radius, peripheral vision, detection affiliation, and the exact attack/encounter admission interface. The existing project interface is `AHSREnemyAIController::TryRequestEncounterFromCharacter`; confirm this remains the sole submitter.

## Reopen verification requested from user

After saving, close and reopen the Editor, load `BP_HSREnemy_Phase4Test` and `BT_HSREnemy_Exploration`, and provide evidence that the BT opens with `BB_HSREnemy_Exploration` assigned and all listed keys resolve with the stated types. Also provide the final asset paths if they differ from this requested layout.

## Minimum authorization request

Authorize creation/editing of only the listed `Content/AI/Enemy` assets and the specified existing Enemy Blueprint/DataAsset binding fields. Once the saved asset paths, Blackboard schema, perception values, and binding evidence are supplied, Implementation may resume within the frozen PATCH-02 C++ allowlist. No production source or Config file was modified in this Asset Gate pass.

## Asset Gate follow-up — user-provided `/Game/AI` paths

Status remains: `BLOCKED ASSET GATE`.

Read-only package inspection now confirms both supplied assets exist and are internally paired:

- `/Game/AI/Enemy/BB_HSREnemy_Exploration` is a `BlackboardData` asset. Its serialized keys include `TargetActor` (`Object`, base `Actor`), `SpawnOrigin` (`Vector`), `PatrolLocation` (`Vector`), `AIState` (`Enum`, `EHSREnemyExplorationState`), `TreeEpoch` (`Int`), and `EncounterRequestId` (`Name`).
- `/Game/AI/Enemy/BT_HSREnemy_Exploration` is a `BehaviorTree` asset and serializes `BlackboardAsset=/Game/AI/Enemy/BB_HSREnemy_Exploration`.
- `BP_HSREnemy_Phase4Test` serializes `AIControllerClass` and `HSREnemyAIController`; the current C++ perception values are SightRadius `1000`, LoseSightRadius `1500`, PeripheralVision `90`, all affiliations enabled, and MaxAge `5`. The intended encounter entry point remains `AHSREnemyAIController::TryRequestEncounterFromCharacter`.

The Behavior Tree is not yet executable for the frozen migration contract. Its serialized graph contains only `BehaviorTreeGraphNode_Root` and `SingleComposite`; it contains no movement, Blackboard, Task, Decorator, Service, Patrol, Chasing, LostTarget, MoveFailed, or EncounterPending branch node. There is consequently no evidence of the required SpawnOrigin recovery paths, authoritative Encounter wait/result handling, stale-epoch cleanup, or no-polling implementation.

### Exact remaining Editor work

In `/Game/AI/Enemy/BT_HSREnemy_Exploration`, add and save explicit branches that consume the confirmed keys:

1. Patrol: choose/write `PatrolLocation`, move there, then wait without a polling Service.
2. Chasing: require valid `TargetActor`, move toward it, and let the existing C++ controller remain the sole Encounter submitter.
3. LostTarget: clear `TargetActor`, then execute `Move To SpawnOrigin`; it must not select a random patrol location as its recovery action.
4. MoveFailed: abort the failed action and execute bounded recovery via `Move To SpawnOrigin`.
5. EncounterPending: block/complete only from the authoritative C++ request/result transition; it must not call the battle subsystem directly or issue a second request.

For every non-stock node, provide its exact asset path, parent class, exposed Blackboard key selectors, and whether it reacts only to C++-emitted events. Confirm there is no interval/tick Service. Save the BT, close and reopen the Editor, then provide either an Editor screenshot or a node/path listing showing the five branches and assigned Blackboard. Only after that evidence is supplied can C++ BT/BB reference fields and lifecycle binding be implemented.

## Stage A implementation result

Status: `IMPLEMENTED / STAGE-A BUILD PASS / STAGE-B USER EDITOR GATE PENDING`.

- `UHSREnemyDefinition` now exposes default soft references to the user-owned `/Game/AI/Enemy/BT_HSREnemy_Exploration` and `/Game/AI/Enemy/BB_HSREnemy_Exploration` assets. No `.uasset` was edited.
- `AHSREnemyAIController` validates the paired references, starts the tree/Blackboard only on successful Possess initialization, writes the six confirmed runtime keys, and stops/clears them before lifecycle teardown. `TargetActor` is cleared on loss and cleanup; epoch increments invalidate stale callbacks.
- LostTarget and MoveFailed both create a bounded return-to-SpawnOrigin intent and movement request. Encounter submission remains solely in `TryRequestEncounterFromCharacter`; an admitted request ID rejects duplicate calls before reaching the subsystem. Actor Tick remains disabled.
- Added `HSR.Exploration.Patch.BehaviorTreeAdapter` automation coverage for the exact soft paths, distinct recovery state, no-Tick default, and initial epoch. It compiled with the module but was not executed in this pass.

### Build evidence

`HSREditor Win64 Development -Project=HSR.uproject -WaitMutex`: `PASS` on 2026-07-27.

- First attempt yielded the preserved real compile error `C4458`: local `Blackboard` hid `AAIController::Blackboard` in `HSREnemyAIController.cpp`.
- The local was renamed to `BlackboardData` within the allowlist.
- Retried build: 4 actions (compile, two link actions, metadata), exit `0`; only the pre-existing engine `AISystem.h` C4996 deprecation warning appeared.

### Not verified

- The new automation test was compiled but not run.
- PIE/runtime evidence and the five Stage-B stock-node branches remain user-owned and pending. Do not treat the Stage-A build as proof of target acquisition/loss, travel failure, or end-to-end Encounter behavior.

### Stage-A Automation and movement-ownership follow-up

`HSR.Exploration.Patch.BehaviorTreeAdapter`: `PASS` in `UnrealEditor-Cmd` (test completed `Success`, process exit `0`).

The automation review exposed a future Stage-B ownership conflict: the legacy `BeginPlay` patrol timer and perception `MoveToActor` request could run alongside the user-built stock BT `Move To` nodes. The Controller was corrected within the existing allowlist:

- `BeginPlay` no longer starts the legacy `StartPatrol` timer.
- Perception only publishes `TargetActor` and `Chasing`; it no longer calls `MoveToActor`.
- LostTarget/MoveFailed only publish `PatrolLocation=SpawnOrigin` and recovery state; they no longer call `MoveToLocation`.

Therefore the Stage-B stock `Move To`/`Wait` graph will be the sole movement request owner, while C++ retains state, epoch, Blackboard-key and Encounter-admission authority. The post-fix `HSREditor Win64 Development` build passed (6 actions, exit `0`) and the Automation passed again. Stage B remains pending user-owned Editor work.

### Reviewer revision — legacy movement owner fully removed

Independent review found that the dormant `StartPatrol` implementation could still issue `MoveToLocation` and schedule a timer if reached from a later stock-BT completion. The Controller adapter was narrowed again within the same allowlist:

- Removed `StartPatrol`, both legacy timer handles, all timer callbacks, and the legacy navigation-system dependency.
- `OnMoveCompleted` now only forwards a failed result into the existing Blackboard recovery-state publisher or observes a lost weak target; it never schedules or issues movement.
- A source-level allowlist check found zero remaining matches for `StartPatrol`, `MoveToLocation`, `MoveToActor`, `SetTimer`, or `TimerHandle` in the Controller header/implementation.

Post-revision validation:

- `HSREditor Win64 Development`: `PASS`, 6 actions, exit `0` (only the existing engine AISystem C4996 warning).
- `HSR.Exploration.Patch.BehaviorTreeAdapter`: `PASS`, `Success`, exit `0`.

This closes Stage-A C++ movement ownership: only Stage-B user-owned stock BT nodes may request movement. Stage B and PIE remain pending.

### Stage-B PIE revision — initial patrol intent

PIE diagnosis found that removing the legacy C++ patrol owner left a valid BT with no initial selector state: the Controller remained `Idle` while the user-built patrol branch requires a patrol state and `PatrolLocation`. A perception event could therefore transition directly from Alert to Chasing and submit the existing authoritative Encounter request without any stock BT Move To evidence.

The minimal Controller-only repair publishes initial intent after `UseBlackboard` succeeds and before `RunBehaviorTree` evaluates the graph:

- `AIState` becomes `MovingToPatrol`.
- `PatrolLocation` and `SpawnOrigin` are both the Character's `SpawnOrigin`.
- No navigation query, timer, direct movement request, BT node class, or user asset edit was added.

The shared publication function is exercised by `HSR.Exploration.Patch.BehaviorTreeAdapter`, which asserts a non-Idle initial patrol state and the exact published SpawnOrigin patrol location. The Automation fixture was deliberately kept independent of a bare `UWorld` AI-runtime startup, because that fixture cannot initialize `UseBlackboard`/`RunBehaviorTree`; the production path still uses the same shared publisher immediately before `RunBehaviorTree`.

Validation after the final seam correction:

- `HSREditor Win64 Development`: `PASS`, 4 actions, exit `0` (only engine AISystem C4996 warning).
- `HSR.Exploration.Patch.BehaviorTreeAdapter`: `PASS`, `Success`, exit `0`.

No Stage-B `.uasset` was modified. User PIE must now verify that the stock patrol branch consumes this published intent before a target is sensed.

### Stage-B PIE revision — observable patrol candidates

The first initial-intent repair used `SpawnOrigin` itself as `PatrolLocation`. That is safe but a stock `Move To` is already at that point, so it does not preserve observable patrol behavior. The Controller now uses `UNavigationSystemV1::GetRandomReachablePointInRadius(SpawnOrigin, PatrolRadius)` only to generate data:

- On a reachable result, it publishes that candidate to `PatrolLocation` and `MovingToPatrol`.
- On stock BT move completion, it publishes the next candidate intent; C++ still makes no movement request and schedules no timer.
- If navigation is unavailable or no point is reachable, it logs a structured fallback, publishes `PatrolLocation=SpawnOrigin`, and enters `PatrolWaiting`; no C++ retry loop is formed.

`HSR.Exploration.Patch.BehaviorTreeAdapter` now uses a controllable candidate seam to verify candidate/state publication and the no-candidate waiting fallback. Static scan again found no `MoveToLocation`, `MoveToActor`, `SetTimer`, or `TimerHandle` in the Controller adapter.

Validation:

- `HSREditor Win64 Development`: `PASS`, 7 actions, exit `0` (only engine AISystem C4996 warning).
- `HSR.Exploration.Patch.BehaviorTreeAdapter`: `PASS`, exit `0`.

No user `.uasset` was modified. Stage-B PIE must verify that patrol moves to the generated candidate and that a completed stock Move To consumes a newly published candidate after the user graph's Wait branch.

### Stage-B PIE revision — safe SpawnOrigin before Enemy BeginPlay

PIE lifecycle evidence showed `AHSREnemyAIController::OnPossess` can start BT initialization before `AHSREnemyCharacter::BeginPlay`. The former stored `SpawnOrigin` was then still the default zero vector, so the initial Blackboard origin/location and navigation query could target the wrong place.

`AHSREnemyCharacter::GetSpawnOrigin` now returns `GetActorLocation()` until BeginPlay captures the formal origin, and returns that captured value thereafter. Controller initialization, Blackboard `SpawnOrigin`, patrol candidate query, and recovery all already use this one getter, so they now share the same valid location in both lifecycle orders. No BT asset, NavMesh, direct movement, or timer changed.

`HSR.Exploration.Patch.BehaviorTreeAdapter` covers a non-zero pre-BeginPlay ActorLocation fallback and verifies that the captured post-BeginPlay origin remains stable after the Actor moves.

Validation:

- `HSREditor Win64 Development`: `PASS`, 7 actions, exit `0` (only engine AISystem C4996 warning).
- `HSR.Exploration.Patch.BehaviorTreeAdapter`: `PASS`, exit `0`.

User-owned `Content/Data/Enemies/DA_Enemy_Phase5Test1.uasset`, `Content/Maps/Map_Phase1_Exploration.umap`, and `Content/AI/**` were observed as dirty and were not touched or staged.

### Stage-B PIE revision — one-shot navigation readiness retry

After the SpawnOrigin repair, PIE still showed the initial patrol query occurring in `OnPossess` before the Recast NavData was ready. A saved NavMesh does not guarantee NavData is available at that earlier lifecycle point.

The Controller now keeps BT/Blackboard startup immediate, then schedules exactly one 0.2-second Nav-ready intent retry after BeginPlay/runtime availability. The retry:

- captures `BehaviorTreeEpoch`, is armed only once, and consumes itself before work;
- is cancelled and invalidated during Stop/UnPossess/EndPlay;
- rejects stale epochs without mutation;
- only calls `PublishNextPatrolIntent`, which queries navigation and writes Blackboard state/location. It never issues movement, starts a repeating timer, or polls.

Patrol logs now include controller, center, radius, NavSystem, NavData, candidate/result, plus retry schedule and stale-epoch details. Static scan found no `MoveToLocation` or `MoveToActor` in the Controller.

`HSR.Exploration.Patch.BehaviorTreeAdapter` verifies one pending arm, duplicate-arm rejection, stale epoch rejection, one matching consumption, and no second consumption.

Validation:

- `HSREditor Win64 Development`: `PASS`, 7 actions, exit `0` (only engine AISystem C4996 warning).
- `HSR.Exploration.Patch.BehaviorTreeAdapter`: `PASS`, exit `0`.

User map, enemy DataAsset, and AI assets remain untouched. PIE must now verify a `NavReadyRetry` log followed by a Reachable patrol candidate and stock BT movement.

### Stage-B PIE revision — project patrol center before random reachability

Latest PIE evidence showed the delayed retry had a valid `RecastNavMesh-Default`, but its origin center was off the navigable surface. `GetRandomReachablePointInRadius` therefore fell back even with NavData available.

The Controller now first calls `ProjectPointToNavigation` with extent `(100, 100, 300)`, then uses the projected center for `GetRandomReachablePointInRadius`. The branches are observable and bounded:

- projection success plus random success publishes the random candidate and `MovingToPatrol`;
- projection failure logs `ProjectPointFailed`, then publishes the existing SpawnOrigin/`PatrolWaiting` fallback;
- random failure after successful projection logs `RandomReachableFailed`, then uses the same fallback.

No direct movement, recurring timer, polling service, NavMesh/map edit, or user asset change was made. The PatrolProjection log records input center, extent, projected center, and distinct projection/random results.

Validation:

- `HSREditor Win64 Development`: `PASS`, 5 actions, exit `0` (only engine AISystem C4996 warning).
- `HSR.Exploration.Patch.BehaviorTreeAdapter`: `PASS`, exit `0`; controllable candidate and bounded fallback seam assertions pass.

### Stage-B reviewer revision — distinguish projection and random-reachability failures

The patrol intent seam now uses `EHSRPatrolIntentResult` with three explicit outcomes: `Reachable`, `ProjectPointFailed`, and `RandomReachableFailed`. Production maps the projection and random-query results to those outcomes before publishing Blackboard intent.

Automation independently verifies a reachable candidate, a projection-failure fallback, and a random-reachability-failure fallback. Both failures retain the required bounded `PatrolWaiting`/SpawnOrigin fallback, while their result values remain distinct.

Validation:

- `HSREditor Win64 Development`: `PASS`, 8 actions, exit `0` (only engine AISystem C4996 warning).
- `HSR.Exploration.Patch.BehaviorTreeAdapter`: `PASS`, exit `0`.

No user-owned map, enemy DataAsset, or `Content/AI/**` asset was changed.

### Stage-B PIE acceptance evidence — USER PROVIDED

User-provided PIE log and screenshot confirm the runtime handoff after the one-shot navigation-readiness retry:

- `NavData` is valid as `RecastNavMesh-Default` after the retry; patrol-center projection succeeds and publishes reachable candidate `(1718.833, -1064.412, 0)`.
- Blackboard patrol state transitions `1 -> 2` (`PatrolWaiting` to `MovingToPatrol`).
- A later reachable candidate `(1656.780, -942.160, 0)` is published after the stock Behavior Tree `MoveTo` completes, confirming that `OnMoveCompleted` advances patrol intent rather than any legacy Controller movement loop.
- The accompanying PIE screenshot shows the full green NavMesh coverage required by this traversal.

The observed Crowd warning occurs only during teardown and does not supersede the successful patrol evidence above. This entry records user-provided verification only; no map, DataAsset, or `Content/AI/**` asset was modified.

### Stage-B follow-up — perception publishes chase; overlap commits encounter

Latest user PIE evidence showed that patrol completes repeatedly, but a successful perception event had immediately submitted an encounter request. That changed the Blackboard flow from Alert/Chasing directly to `EncounterPending`, so the stock Behavior Tree chase `MoveTo` had no opportunity to run or receive a lost-target event.

The successful-perception path now writes only `TargetActor` and the `Alert -> Chasing` state transition. It does not call `TryRequestEncounterFromCharacter`, does not create an active request, and does not enter `EncounterPending`. Encounter submission remains exclusively at `AHSREnemyCharacter::NotifyActorBeginOverlap -> TryRequestEncounter -> AHSREnemyAIController::TryRequestEncounterFromCharacter`; the existing active-request duplicate guard remains unchanged.

`HSR.Exploration.Patch.BehaviorTreeAdapter` now asserts that a perception transition remains `Chasing`, has no active encounter request, and makes zero submission attempts; an explicit Character-overlap transaction entry then makes one submission attempt. No Encounter DTO or user asset changed.

Validation:

- `HSREditor Win64 Development`: `PASS`, 7 actions, exit `0` (only engine AISystem C4996 warning).
- `HSR.Exploration.Patch.BehaviorTreeAdapter`: `PASS`, exit `0`.
- `HSR.BattleReturn.MapContract`: `PASS`, exit `0`.

### Stage-B return/chase/overlap evidence — USER PROVIDED

User-provided PIE log confirms: perception enters chasing; lost sight transitions `4 -> 7 -> 8`; `ReturnComplete` reports distance `41.38` with zero request id and resumes patrol `8 -> 2`. An overlap while still returning is correctly rejected (`TryRequestEncounter` state `8`), and a subsequent normal chase reaches one successful encounter request and battle consume. This records observed PIE evidence only; user assets were not modified.

### Stage-B transition admission fixture — deterministic duplicate and resolved rejection

`UHSRBattleTransitionSubsystem` now exposes `WITH_DEV_AUTOMATION_TESTS`-only fixture methods and `FHSRTransitionAutomationSnapshot`; production `RequestEncounter` signature and rejection behavior remain unchanged. The snapshot includes encounter state, full pending request, travel kind/request id, resolved-membership, and an admission-mutation counter.

Adapter automation seeds a pending request and calls production `RequestEncounter`, asserting `AlreadyPending` and full snapshot zero mutation. It separately seeds a resolved encounter and asserts production `AlreadyConsumed` with the same zero-mutation guarantee. The fixture owner is a valid `UGameInstance`, avoiding the prior invalid-outer ensure.

Validation: Development Build `PASS`; `HSR.Exploration.Patch.BehaviorTreeAdapter` `PASS`; `HSR.BattleReturn.MapContract` `PASS` (all exit `0`). User-owned assets remain untouched.

Reviewer assertion strengthening: both rejection paths now assert invalid result RequestId, nonempty failure message, and complete `FHSREncounterRequest` equality (all existing request fields) within an otherwise identical transition snapshot.

### Stage-B full return completion — return-to-spawn resumes patrol

`OnMoveCompleted` now handles successful stock BT `MoveTo` completion while in `ReturningToSpawnOrigin`: it logs actor location, SpawnOrigin, distance, and request id, then publishes the next patrol intent. A reachable candidate resumes `MovingToPatrol`; navigation failure retains the existing bounded `PatrolWaiting`/SpawnOrigin fallback. No direct C++ Move request, recurring timer, or encounter request is added.

Adapter automation covers return completion to a reachable next patrol candidate and the bounded random-reachability fallback, including no encounter creation. Development Build, `BehaviorTreeAdapter`, and `MapContract` all pass (exit `0`). User-owned assets remain untouched.

### Stage-B PIE follow-up — intentional BT branch-switch abort

User PIE showed a patrol `MoveTo` abort when perception changed the BT from patrol to Alert/Chasing. `OnMoveCompleted` now only converts failure/abort into `MoveFailed -> ReturningToSpawnOrigin` while still in a movement-owning patrol/recovery state (`MovingToPatrol` or `ReturningToSpawnOrigin`). Branch-switch aborts in Alert/Chasing/EncounterPending/Idle are ignored with `MoveAbortIgnored` structured logging and make no Blackboard or state mutation.

Adapter verifies a Chasing branch-switch abort is ignored and a true patrol failure still records `MoveFailed` then publishes the bounded return intent. Development Build, `BehaviorTreeAdapter`, and `MapContract` pass (exit `0`); user assets remain untouched.

Reviewer matrix expansion: the production failure decision seam now proves both movement-owning states (`MovingToPatrol`, `ReturningToSpawnOrigin`) are handled into bounded recovery, while `Alert`, `Chasing`, `EncounterPending`, and `Idle` are each independently ignored with relevant state, patrol Blackboard value, epoch, encounter attempts, and retry-arm state unchanged.

Reviewer snapshot strengthening: each ignored-state abort now compares a full test-local controller snapshot before/after: controller state and target identity, all six Blackboard runtime keys, active request id, epoch, retry state, encounter attempts, and recovery marker. Build, Adapter, and MapContract pass.

Recovery Blackboard revision: bounded handled recovery now explicitly writes both `SpawnOrigin` and `PatrolLocation` after state publication; Adapter verifies both equal the expected origin with Returning state and cleared target. Build, Adapter, and MapContract pass.

Handled-fixture isolation revision: Moving and Returning failures independently reset the recovery marker and capture controller/Blackboard epochs plus encounter attempts before invoking the production decision seam. Each verifies unchanged epochs/counts, empty request/targets, no retry, and an independently observed `MoveFailed` source marker.

### Stage-B data-driven enemy perception and encounter radii

`UHSREnemyDefinition` now exposes editable `SightRadius`, `LoseSightRadius`, and `EncounterRadius`, preserving the prior defaults `1000`, `1500`, and `200`. On possession the Controller applies Definition sight values and normalizes `LoseSightRadius` to at least `SightRadius`; with no Definition it retains constructor defaults. The Character applies the Definition encounter sphere radius at BeginPlay, with a nonnegative clamp. No BT ordering, state transition, MoveTo, delay, or encounter admission behavior changed.

Validation: `HSREditor Win64 Development` `PASS` (13 actions, exit `0`); `HSR.Exploration.Patch.BehaviorTreeAdapter` `PASS` (exit `0`). User assets were not changed.

Reviewer test-realism revision: Adapter now invokes the same production perception/encounter apply helpers used by possession/BeginPlay and reads actual `SightConfig`/`EncounterCollision` values. It verifies no-Definition `1000/1500/200`, Definition `1200/1000/333` normalization to `1200/1200/333`, and negative-radius clamp behavior. Build and Adapter pass.

User-owned map, enemy DataAsset, and `Content/AI/**` remain unstaged and unmodified by this task.

### Stage-B main-path PIE evidence — USER PROVIDED

User-provided PIE attachment verifies the following observed main path only:

- Multiple patrol cycles publish `Reachable` locations.
- Successful perception transitions state `2 -> 3 -> 4` and logs `BeginChasingTarget` without encounter submission.
- Losing sight transitions `4 -> 7` (`LostTarget`) and then `-> 8` (`ReturningToSpawnOrigin`).
- Reacquiring the target transitions `8 -> 3 -> 4`.
- Only after physical `NotifyActorBeginOverlap` does one `RequestEncounter` succeed, transitioning `4 -> 5` (`EncounterPending`); the travel consume then succeeds.
- `OnUnPossess` performs clean teardown, with no duplicate or stale-callback log observed in this run.

This is user-provided runtime evidence. It does not claim unobserved full-return completion, move-failure handling, target-destruction handling, or duplicate-overlap coverage. User-owned map, enemy DataAsset, and `Content/AI/**` assets remain isolated and unchanged.

### Stage-B reviewer revision — teardown Blackboard ownership and stale runtime isolation

`StopBehaviorTreeRuntime` now stops Brain logic, clears the six runtime Blackboard keys, detaches `RuntimeBlackboard`, invalidates the active request, and advances the runtime epoch only when it owned active runtime state. The detach happens before callers continue into `ClearState`, `OnUnPossess`, or `EndPlay`, so those cleanup paths cannot repopulate the cleared Blackboard. Repeated Stop is epoch-idempotent after teardown.

`HSR.Exploration.Patch.BehaviorTreeAdapter` uses an AI World-backed Blackboard fixture and deterministically verifies: all six runtime keys clear on Stop; Stop then ClearState cannot write them back; a second Stop preserves both clear state and epoch; a fresh runtime bind receives a new epoch; and an old retry callback cannot consume or write into that fresh runtime. The test also exercises the same Stop then ClearState ordering used by EndPlay.

Validation:

- `HSREditor Win64 Development`: `PASS`, 7 actions, exit `0` (only engine AISystem C4996 warning).
- `HSR.Exploration.Patch.BehaviorTreeAdapter`: `PASS`, exit `0`.
- `HSR.BattleReturn.MapContract`: `PASS`, exit `0`.

This revision does not claim new live-PIE coverage for same-frame duplicate overlap response payload/reason preservation, post-resolved rejection, stock MoveTo return completion, move failure/abort, or target destruction. Those require a fully wired encounter/BT PIE fixture and remain explicitly unobserved here. No Encounter DTO or user-owned map, DataAsset, or `Content/AI/**` asset changed.

### Stage-B reviewer revision — real Blackboard six-key teardown assertions

The teardown automation no longer uses a synthetic write-mask as a pass oracle. It creates an AI World-backed, initialized Blackboard and directly reads every runtime key. Before teardown it seeds and verifies: a `TargetActor` object, `SpawnOrigin` vector, `PatrolLocation` vector, nondefault `AIState`, nonzero `TreeEpoch`, and nonempty `EncounterRequestId`.

Each key is then directly asserted at its Blackboard default/empty value after Stop, Stop then ClearState, repeated Stop, fresh runtime rebind followed by a stale retry callback, and the EndPlay-equivalent Stop then ClearState ordering. Blackboard Vector clear semantics are correctly checked against `FAISystem::InvalidLocation` rather than a zero vector.

Validation:

- `HSREditor Win64 Development`: `PASS`, 4 actions, exit `0` (only engine AISystem C4996 warning).
- `HSR.Exploration.Patch.BehaviorTreeAdapter`: `PASS`, exit `0`.
- `HSR.BattleReturn.MapContract`: `PASS`, exit `0`.

No production behavior, Encounter DTO, or user-owned map, DataAsset, or `Content/AI/**` asset changed in this revision.

### Stage-B reviewer follow-up — recovery intent coverage

`BehaviorTreeAdapter` now drives the production spawn-origin recovery intent through deterministic seams. A move-failure path records `MoveFailed`, publishes `ReturningToSpawnOrigin`, writes `PatrolLocation=SpawnOrigin`, and does not arm a C++ retry loop. A controlled destroyed chase target clears `TargetActor`, records `LostTarget`, publishes the same return intent/location, and creates no encounter request.

The existing transition API exposes `AlreadyPending` and `AlreadyConsumed`, but a deterministic controller-level first-admission fixture with structured request-attempt/side-effect counters would require a new TransitionSubsystem contract seam or a travel-capable encounter fixture. That seam is not present in the allowed surface, so same-frame duplicate admission and post-resolved zero-mutation are explicitly retained as a contract blocker rather than simulated or claimed. Full return remains USER PIE pending.

Validation:

- `HSREditor Win64 Development`: `PASS`, 7 actions, exit `0`.
- `HSR.Exploration.Patch.BehaviorTreeAdapter`: `PASS`, exit `0`.
- `HSR.BattleReturn.MapContract`: `PASS`, exit `0`.
