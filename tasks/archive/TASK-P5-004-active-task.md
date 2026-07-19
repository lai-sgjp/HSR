# TASK-P5-004：死亡、胜负、BattleResult 单次消费与返回

## Role Lock

执行者只能扮演 `Implementation Agent / 低级执行模型`。首次读取本卡后必须复述任务并等待用户独立确认；确认前不得调用工具、修改文件或构建。

## 前置状态

P5-003 已通过 `PASS WITH FOLLOW-UP`：基础普攻、固定伤害、失败目标和 exactly-once 行为已验证。现在实现 P5 MVP 的战斗结束闭环。

## 唯一可验收结果

当任一参与者 HP<=0 时，Coordinator 只触发一次死亡与胜负判定，生成纯值 BattleResult 并 exactly-once 消费；清理 Battle runtime 后返回探索地图原始 Transform。玩家胜利和失败两条路径都必须可恢复，重复 Resolve/返回不得重复奖励或卡死 Pending。

## 允许修改文件

- `Source/HSR/Battle/HSRBattleCoordinator.h/.cpp`
- `Source/HSR/Battle/HSRBattleTypes.h/.cpp`
- `Source/HSR/Battle/HSRTurnManager.h/.cpp`（仅死亡后回合停止/结果接线）
- `Source/HSR/Battle/HSRBattleGameMode.h/.cpp`（仅结果/返回接线与 Development 测试入口）
- `Source/HSR/Battle/HSRBattleParticipant.h/.cpp`（仅死亡状态）
- `Source/HSR/Battle/HSRBattleTransitionSubsystem.h/.cpp`（仅纯值 Result/Return DTO 接线；若超出需停下请求扩权）
- `tasks/execution-result.md`

用户 Editor 资产如需创建，执行者必须先提交代码并提供详细指引；不得代替用户创建或提交未知资产。

## 非目标

不实现复杂技能、奖励系统、完整战斗 UI、Cost/Cooldown/Energy、状态效果、复杂 AI、SaveGame、网络、Replication、Prediction 或 Tick。

## 必须验证

- Player victory 与 Player defeat 各一条路径。
- HP<=0 只触发一次死亡/胜负/Result。
- BattleResult DTO 不含 Actor、ASC、Widget 或 GE Handle。
- 重复 Result/重复返回/重复奖励请求安全失败。
- 返回探索 Map 与原始 Transform 恢复；旅行失败不留下 Pending 卡死。
- Actor/ASC/Delegate/Timer teardown、连续两轮 PIE 无旧 World 引用。
- fresh `HSREditor Win64 Development` Build/UHT/C++/Link。

## 交接

每次交接前执行 allowlist、diff、派生产物和 Git commit 核对。Reviewer 未放行前不得创建 P5-005。

## 状态

用户已明确确认执行 TASK-P5-004。执行者必须先检查工作树并遵守 Clean Handoff 规则；需要 Editor 资产时先提交代码并给出详细指引，再等待用户证据。

`AUTHORIZED — IMPLEMENTATION`
