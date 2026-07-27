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
- `Source/HSR/Data/Definitions/HSREnemyDefinition.h`
- `Source/HSR/Data/Definitions/HSREnemyDefinition.cpp`
- `Source/HSR/Tests/HSRCombatPatchTests.cpp`
- `tasks/execution-result.md`

Implementation 不得在 Task Gate/复述阶段修改文件；正式 Behavior Tree、Blackboard、服务/任务节点、Blueprint/DataAsset 和感知资产需要用户 Asset Gate 与最小授权，不能从本卡推断授权。

## Frozen non-goals and stop conditions

- 不修改 Battle/Turn/Status/Save/Network/UI、P17-005 或商业游戏公式；不引入 Tick 轮询。
- 若迁移需要新增生产 C++ 文件、Content/Blueprint/DataAsset、Build.cs 或改变 Encounter 契约，必须停止并申请最小扩权。
- Task Gate 需先审查现有 Controller 状态机和资产缺口；PASS 后只允许 Implementation 只读复述，实际实现仍需用户单独确认 `TASK-P17-PATCH-02`。
