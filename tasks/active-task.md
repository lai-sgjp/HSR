# TASK-P17-PATCH-02 — Behavior Tree AI Migration Planning Gate

Status: `PLANNED / TASK GATE REVIEW REQUIRED`

## Single outcome

为改进方向 4 建立独立 Behavior Tree/Blackboard 迁移契约：保留现有探索敌人感知、巡逻/追击、返回出生点、Encounter 请求幂等、已解决 Encounter 拒绝和无 Tick 所有权；本卡只冻结范围与用户 Asset Gate，不开始实现。

## Exact candidate allowlist for discovery

- `Source/HSR/Enemy/HSREnemyAIController.h`
- `Source/HSR/Enemy/HSREnemyAIController.cpp`
- `Source/HSR/Enemy/HSREnemyCharacter.h`
- `Source/HSR/Enemy/HSREnemyCharacter.cpp`
- `Source/HSR/Enemy/HSREnemyTypes.h`
- `Source/HSR/Battle/HSREncounterTypes.h`
- `Source/HSR/Battle/HSRBattleTransitionSubsystem.h`（仅 `WITH_DEV_AUTOMATION_TESTS` Pending/Resolved seed、snapshot、reset；禁止修改生产请求签名/行为）
- `Source/HSR/Battle/HSRBattleTransitionSubsystem.cpp`（仅实现上述测试 fixture；拒绝必须调用生产 `RequestEncounter`）
- `Source/HSR/Data/Definitions/HSREnemyDefinition.h`
- `Source/HSR/Data/Definitions/HSREnemyDefinition.cpp`
- `Source/HSR/Tests/HSRCombatPatchTests.cpp`
- `Content/AI/Enemy/BB_HSREnemy_Exploration.uasset`（用户资产；仅用户在 Editor 修改/提交）
- `Content/AI/Enemy/BT_HSREnemy_Exploration.uasset`（用户资产；仅用户在 Editor 修改/提交）
- `Content/Blueprints/Character/Enemy/BP_HSREnemy_Phase4Test.uasset`（用户资产；仅 Controller/Auto Possess/Definition 绑定）
- `tasks/execution-result.md`

Implementation 不得在 Task Gate/复述阶段修改文件；正式 Behavior Tree、Blackboard、服务/任务节点、Blueprint/DataAsset 和感知资产需要用户 Asset Gate 与最小授权，不能从本卡推断授权。

## Frozen non-goals and stop conditions

- 不修改 Battle/Turn/Status/Save/Network/UI、P17-005 或商业游戏公式；不引入 Tick 轮询。
- 若迁移需要新增生产 C++ 文件、Content/Blueprint/DataAsset、Build.cs 或改变 Encounter 契约，必须停止并申请最小扩权。
- Task Gate 需先审查现有 Controller 状态机和资产缺口；PASS 后只允许 Implementation 只读复述，实际实现仍需用户单独确认 `TASK-P17-PATCH-02`。

## Frozen ownership and state contract

- Enemy AIController owns `UBehaviorTreeComponent`/Blackboard startup and shutdown; EnemyCharacter owns perception source and movement callbacks; no subsystem or BT node may independently start a second tree. BeginPlay/Possess starts only after valid Blackboard/tree assets; UnPossess/EndPlay stops the tree and unbinds delegates before invalidating the epoch.
- Blackboard keys are ephemeral runtime state only: `TargetActor` is weak/transient and must clear on perception loss, target destruction, UnPossess and EndPlay; stable encounter/request IDs may be plain values but never retain Actor references across map transitions. C++ Controller/Encounter path remains authoritative for Encounter admission, duplicate/resolved rejection and state epoch; BT tasks/services only orchestrate movement or observe results.
- Required one-to-one mapping is frozen: `Idle` (no target, no request), `PatrolWait/PatrolMove` (SpawnOrigin patrol route), `Alert` (valid perceived target, pre-chase), `Chasing` (move toward target), `LostTarget` (clear target then return to SpawnOrigin), `MoveFailed` (abort current task, bounded recovery to SpawnOrigin), `EncounterPending` (request admitted, tree task completes only on result). No random patrol fallback may satisfy LostTarget/MoveFailed.
- Perception delegates and movement completion/failure/abort callbacks produce BT events; no Tick-driven service or polling interval is allowed. A missing/invalid tree or Blackboard is a hard initialization failure with zero Encounter side effects.

## Encounter transaction contract

- The single authoritative submitter is `HSREnemyAIController::TryRequestEncounterFromCharacter`; overlap/perception paths may call it but must converge on one transaction key `(EnemyStableId, TargetStableId, BattleEpoch, RequestId)` and one pending record.
- First admitted request creates one pending transaction. Same-frame duplicate overlap+perception returns `DuplicateRequest` with no new request ID, movement, or tree task. Already pending/traveling returns `AlreadyPending`; already consumed/resolved returns `AlreadyResolved`; invalid definition/target returns structured rejection. Travel failure completes and clears the task/pending record without restarting chase or enqueuing another request.
- Encounter result consumption is exactly once by the authoritative controller; resolved rejection is terminal for that transaction key. A new BattleEpoch or explicit new encounter identity is required for a later request. Automation must cover same-frame duplicate and post-resolved rejection with before/after state, request ID and result reason.

## Two-stage Asset Gate and acceptance matrix

- Stage A is satisfied by the user-confirmed assets `/Game/AI/Enemy/BB_HSREnemy_Exploration` and `/Game/AI/Enemy/BT_HSREnemy_Exploration`, the verified six-key Blackboard schema, confirmed BT→BB assignment, `BP_HSREnemy_Phase4Test` Controller/Auto Possess binding, and explicit authorization for minimal `UHSREnemyDefinition` BT/BB references. The BT graph may still be empty at Stage A.
- After Stage A, Implementation may add only the allowlisted C++ event adapter, BT/BB asset references, epoch/key writes, lifecycle start/stop and Automation seams. It must use stock Blackboard Decorator, Move To and Wait where sufficient; any required new production BT Task/Service/Decorator class or new source file is a hard stop for allowlist expansion.
- Stage B occurs only after the Stage-A C++ adapter builds. Coordinator then gives the user exact Editor node construction using the compiled state/key contract. The user alone edits the three allowlisted `.uasset` files, saves/reopens, and supplies the five-branch screenshot/path evidence. Implementation must not binary-edit or submit those assets.
- Stage B still forbids interval/tick Services. EncounterPending must observe the authoritative C++ result/key transition and must not call the battle subsystem; LostTarget/MoveFailed must use `Move To SpawnOrigin`.
- Required runtime evidence after Asset Gate: acquire/loss, target destruction, move success/failure/abort, return-to-SpawnOrigin, duplicate overlap+perception, already-resolved rejection, stale callback after re-possess/EndPlay, and zero Tick/polling proof. Each case logs before/after state, tree epoch, target validity and Encounter request/result IDs.
- 用户已精确授权 TransitionSubsystem 的 dev-only fixture：`FHSRTransitionAutomationSnapshot` 至少记录 state、pending request、travel kind/id、resolved membership、admission mutation count；`SeedPendingEncounterForAutomation`、`SeedResolvedEncounterForAutomation`、`ResetEncounterAutomationFixture` 只能准备/清理私有测试状态。Automation 必须通过生产 `RequestEncounter` 得到 `AlreadyPending`/`AlreadyConsumed`，并断言无新 RequestId 与 snapshot 零变化。
