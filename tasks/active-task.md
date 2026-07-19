# TASK-P5-002：TurnManager 与确定性 1v1 回合队列

## Role Lock

执行者只能扮演 `Implementation Agent / 低级执行模型`。首次读取本卡后必须复述任务并等待用户独立确认；确认前不得调用工具、修改文件或构建。

## 前置状态

TASK-P5-001 已通过最终审查并归档：Definition 验证、结构化失败、fresh Build 和用户负向路径证据均已记录。Phase5 仍只做 1v1 最小闭环。

## 唯一可验收结果

基于已有两个 Participant 建立无 Tick 的 TurnManager：按 Speed 降序生成确定性队列，同速使用稳定 ParticipantId 裁决；只有当前 Participant 可提交一次 ActionResolved，重复、越权、失效 Actor 安全失败并推进下一回合。

## 允许修改文件

- `Source/HSR/Battle/HSRTurnManager.h`
- `Source/HSR/Battle/HSRTurnManager.cpp`
- 必要时最小扩展 `Source/HSR/Battle/HSRBattleParticipant.h/.cpp`
- 必要时最小扩展 `Source/HSR/Battle/HSRBattleTypes.h/.cpp`
- 必要时最小扩展 `Source/HSR/Battle/HSRBattleCoordinator.h/.cpp`
- `tasks/execution-result.md`

若需要修改 Content、Config、Transition、Build.cs、GameMode 入口或新增模块，必须停止并请求扩权。

## 非目标

不实现 GameplayAbility、伤害、死亡、Victory/Defeat、BattleResult、Cost/Cooldown/Energy、UI、复杂 AI、SaveGame、网络、Replication、Prediction 或 Tick。

## 必须验证

- Speed 不同与同速排序稳定可复现。
- 重复 Resolve、非当前参与者、失效 Actor、空队列和 Reset 安全失败。
- ActionResolved 后只推进一次。
- World teardown、连续两轮 PIE 无旧引用、无 Tick。
- fresh `HSREditor Win64 Development` Build/UHT/C++/Link。

## 交接

Implementation 完成后只更新 `tasks/execution-result.md` 并停止；Coordinator 核对 allowlist，Reviewer 只读审查。未通过 Reviewer 前不得创建 P5-003。

## 状态

用户已授权执行 TASK-P5-002。执行者必须先复述本卡并开始实现；任何 Editor 必须由用户完成。每次交接前必须提交角色产物、记录 commit hash，并确保工作树中所有剩余变更已分类。

`AUTHORIZED — IMPLEMENTATION`
