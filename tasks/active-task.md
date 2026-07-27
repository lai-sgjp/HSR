# TASK-P17-PATCH-01B — Repeatable Break Edge Transactions

Status: `PLANNED / IMPLEMENTATION RESTATEMENT REQUIRED`

## Single outcome

同一存活目标每次 Toughness 从大于零降到零都恰好发布一个独立 Break；恢复到正数后再次归零可再次 Break，而同一 ActionId replay、零到零、恢复本身、死亡、Finished、Reset 与跨地图重建不会伪造或重复 Break/Status/Delay。

## Frozen ownership and data flow

- 本次权威 Toughness damage transaction 的 `Before > 0 && After == 0` 是 Break 边沿真源。
- Coordinator 继续拥有 ActionId 幂等结果；不得在 Widget、GameplayCue、Status 或 TurnManager 中复制 Break 判定。
- 移除 `FHSRBattleParticipant::bBreakResultPublished` 角色终身闩锁；不得用另一个布尔字段改名替代。
- 每个成功边沿分别 exactly-once 请求 Break Status 与 Turn Delay；PATCH-01B 不改变 Delay 的现有 skip-once 语义，完整行动值迁移属于 PATCH-01C。
- Toughness 恢复由既有 authority 或 Automation fixture 提供；本卡不新增自动恢复 Gameplay 规则。

## Exact allowlist

- `Source/HSR/Data/HSRBreakTypes.h`
- `Source/HSR/Battle/HSRBattleParticipant.h`
- `Source/HSR/Battle/HSRBattleCoordinator.h`
- `Source/HSR/Battle/HSRBattleCoordinator.cpp`
- `Source/HSR/Battle/HSRBattleGameMode.cpp`（仅 P8/P9 repeatable-Break development harness）
- `Source/HSR/Tests/HSRCombatPatchTests.cpp`
- `tasks/execution-result.md`

Implementation Agent 不得修改本活动卡、计划、PROJECT_STATE、worklog、todo、学习文档、Config 或 Content。

## Required validation

- `>0 -> 0` 首次触发一次；相同 ActionId replay 零新增。
- 恢复到 `>0` 后用新 ActionId 再归零，再触发一次 Break、Status、Delay。
- 初始/持续为零、只恢复、未归零、无弱点、死亡、Finished、Reset/rebuild 不触发。
- 两次 Break 的 ActionId/Event 彼此独立；旧 Battle/旧 target 回调零副作用。
- Development Editor Build、`HSR.Battle.Patch.RepeatableBreak`、适用 P8/P9/Battle 回归、`git diff --check`。
- 若真实两次 Break 需要 Editor harness，用户 PIE 必须提供事件计数与零 FAIL/INCOMPLETE/SKIPPED。

## Explicit non-goals and stop conditions

不实现行动值、Speed/Advance/Delay/Slow 重排；不修改 TurnManager；不新增 Toughness 自动恢复、Status Definition、GE、Tag、Content、Config、Save、UI、网络或 Tick。需要白名单外生产文件、资产或契约扩张时立即停止请求最小授权。
