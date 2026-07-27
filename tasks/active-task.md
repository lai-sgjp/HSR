# TASK-P17-PATCH-01B — Repeatable Break Edge Transactions

Status: `PLANNED / IMPLEMENTATION RESTATEMENT REQUIRED`

## Single outcome

同一存活目标每次 Toughness 从大于零降到零都恰好发布一个独立 Break；恢复到正数后再次归零可再次 Break，而同一 ActionId replay、零到零、恢复本身、请求准入前死亡、Finished、Reset 与跨地图重建不会伪造或重复 Break/Status/Delay。

## Frozen ownership and data flow

- 本次权威 Toughness damage transaction 的 `Before > 0 && After == 0` 是 Break 边沿真源。
- Coordinator 继续拥有 ActionId 幂等结果；不得在 Widget、GameplayCue、Status 或 TurnManager 中复制 Break 判定。
- 移除 `FHSRBattleParticipant::bBreakResultPublished` 角色终身闩锁；不得用另一个布尔字段改名替代。
- 每个成功边沿分别 exactly-once 请求 Break Status 与 Turn Delay；PATCH-01B 不改变 Delay 的现有 skip-once 语义，完整行动值迁移属于 PATCH-01C。
- Toughness 恢复由既有 authority 或 Automation fixture 提供；本卡不新增自动恢复 Gameplay 规则。
- 同一合法伤害事务若让入场时存活目标同时 `HP > 0 -> 0` 且 `Toughness > 0 -> 0`，保持现有管线优先级：先发布该事务唯一 Break/Status/Delay，再由 `ResolveDefeat` 收尾；不得因稍后的死亡清理抹掉已发布结果。请求准入前已经死亡的目标仍拒绝且不产生 Break。

## Exact allowlist

- `Source/HSR/Data/HSRBreakTypes.h`
- `Source/HSR/Battle/HSRBattleParticipant.h`
- `Source/HSR/Battle/HSRBattleCoordinator.h`
- `Source/HSR/Battle/HSRBattleCoordinator.cpp`
- `Source/HSR/Battle/HSRBattleGameMode.cpp`（仅 P8/P9 repeatable-Break development harness）
- `Source/HSR/Battle/HSRBattleGameMode.h`（用户精确扩权：仅在 `#if WITH_DEV_AUTOMATION_TESTS` 下添加非 UFUNCTION/非 Blueprint/非 Shipping 的 `CreateRepeatableBreakAutomationFixture` 静态入口；禁止新增属性、BeginPlay 开关或通用状态 mutator）
- `Source/HSR/Tests/HSRCombatPatchTests.cpp`
- `tasks/execution-result.md`

Implementation Agent 不得修改本活动卡、计划、PROJECT_STATE、worklog、todo、学习文档、Config 或 Content。

## Required validation

- 首次 `>0 -> 0` 必须记录：BreakResult triggered/event 增量 `+1`、Break Status request/result 增量 `+1`、Delay registration `+1`、Toughness 归零，当前行动只按既有流程 resolve 一次。
- 同 BattleId + 同 ActionId replay 返回缓存 Resolution；Break/Status/Delay/Toughness/Turn 增量全部为 `+0`。
- 恢复到 `>0` 本身所有副作用增量为 `+0`；第二个新 ActionId 再归零时 Break/Status/Delay 分别再 `+1`，且两次 Break ActionId 不同。
- 初始/持续 `0 -> 0`、未归零、无弱点、Finished、请求准入前死亡均要求所有副作用计数 `+0`。
- Reset 后旧 BattleId 请求结构化拒绝且计数 `+0`；新 BattleId 下复用旧 ActionId 视为新 Battle-local 事务，可在新的正数到零边沿触发一次。不得测试不存在的“旧 target callback”抽象。
- 同帧致死+击破按冻结优先级各发布一次 Break/Status/Delay，随后完成 Defeat；不得重复行动或结果。
- Development Editor Build、`HSR.Battle.Patch.RepeatableBreak`、适用 P8/P9/Battle 回归、`git diff --check`。
- Automation 负责真实/受控 runtime 的 first/replay/recovery/second/0->0/Finished/Reset/stale BattleId/reused ActionId 与精确计数；不得以 Definition-only 测试代替 runtime。
- PIE 复用现有 `bRunP9DotBreakHarness` 与 `P9-003 DotBreak Harness`，只在现有开关中增加 repeatable-Break 案例和日志；不得新增 GameMode 属性。Automation factory 只读取传入 ConfiguredGameModeClass CDO 的现有配置，通过正式 `SubmitBattleRequest -> BuildParticipants` 构建 transient Coordinator，不修改 CDO/Content。用户 PIE 必须提供两次独立 Break ActionId、各副作用计数与零 FAIL/INCOMPLETE/SKIPPED。

## Explicit non-goals and stop conditions

不实现行动值、Speed/Advance/Delay/Slow 重排；不修改 TurnManager；不新增 Toughness 自动恢复、Status Definition、GE、Tag、Content、Config、Save、UI、网络或 Tick。需要白名单外生产文件、资产或契约扩张时立即停止请求最小授权。
