# TASK-P14-001：Quest / Dialogue / Save v4 Runtime 修复与验证

Status: `ARCHIVED / PASS WITH FOLLOW-UP`

## 1. 授权来源

用户在 2026-07-26 明确要求继续 Phase 14，并指出阶段开始必须先写详细子任务规划。当前任务以 `docs/phase-14-execution-plan.md` 为阶段真源。此前产生的 Quest/Dialogue/Save 代码草稿未经过 Phase 14 入口规划，必须在本任务中重新纳入 allowlist、编译、Automation 和审查后才可作为 Phase 14 交付。

## 2. 唯一可验收结果

一个最小 Quest 可由 Dialogue Choice 提交事件推进到 Completed，并通过 Phase 13 RewardSubsystem 的稳定 ClaimId 幂等发奖；Save v4 可保存/恢复 Quest 状态和奖励领取状态；重复 Choice/Event/Claim/Load 不重复发奖；v1-v3 Save 迁移为空 Quest 状态。Development Editor Build 与 Phase 14 Automation 必须通过。

## 3. Codex 允许修改文件

- `docs/phase-14-execution-plan.md`
- `tasks/active-task.md`
- `tasks/execution-result.md`
- `tasks/final-review.md`
- `Source/HSR/Quest/HSRQuestTypes.h`
- `Source/HSR/Quest/HSRQuestSubsystem.h`
- `Source/HSR/Quest/HSRQuestSubsystem.cpp`
- `Source/HSR/Dialogue/HSRDialogueTypes.h`
- `Source/HSR/Dialogue/HSRDialogueSubsystem.h`
- `Source/HSR/Dialogue/HSRDialogueSubsystem.cpp`
- `Source/HSR/Data/Definitions/HSRQuestDefinition.h`
- `Source/HSR/Data/Definitions/HSRDialogueDefinition.h`
- `Source/HSR/Save/HSRSaveTypes.h`
- `Source/HSR/Save/HSRSaveSubsystem.h`
- `Source/HSR/Save/HSRSaveSubsystem.cpp`
- `Source/HSR/Tests/HSRQuestDialogueTests.cpp`
- `Source/HSR/Tests/HSRInventoryRewardSaveTests.cpp`（仅 schema v4 断言兼容）
- `Source/HSR/Reward/HSRRewardSubsystem.h`（仅开发测试初始化的 out-of-line 编译边界修复）
- `Source/HSR/Reward/HSRRewardSubsystem.cpp`（仅对应开发测试初始化实现）
- `PROJECT_STATE.md`
- `worklog.md`
- `todo_plan.md`
- `learning-journal.md`（仅 Teacher/收尾阶段）

未列入文件默认禁止修改。不得修改 `.claude/settings.local.json`、`Content/UI/WBP_Reward_Summary_P13.uasset`、`learn/AI.md` 或用户既有学习文档改动；不得删除、reset、clean 或覆盖用户变更。

## 4. 子任务清单

1. `P14-000`：补齐 Phase 14 总计划、活动任务卡、执行报告和审查占位，记录入口修正规则。
2. `P14-001`：校准 Quest types/subsystem/definition，确保 UHT、GC、Reward 幂等和失败路径安全。
3. `P14-002`：校准 Dialogue types/subsystem/definition，确保 Choice -> QuestEvent 桥接和无效分支零副作用。
4. `P14-003`：校准 Save v4 schema、Validate、candidate restore、旧 schema 空 Quest 迁移和 restore 通知。
5. `P14-004`：运行 `HSREditor Win64 Development` 编译；修复首错；运行 Phase 14 Automation；记录真实日志。
6. `P14-005`：完成 Teacher 复盘、Independent Review、worklog/todo/PROJECT_STATE 同步和任务归档。

## 5. 验收矩阵

- Quest：register/start/event/complete/claim/repeat/failure/restore。
- Dialogue：register/start/get node/choose branch/invalid choice/QuestEvent bridge。
- Save：v4 capture/load、v1-v3 empty Quest migration、bad Quest data reject、repeated Load no duplicate reward。
- Integration：Dialogue Choice -> Quest complete -> Reward once -> Save -> Load -> repeat no duplicate。
- Tooling：`git diff --check`、Development Editor Build、Automation tests。

## 6. 用户 Editor Gate

当前 C++/Automation 不依赖用户创建资产。若需要真实 DataAsset/PIE 证据，必须停在 Editor Gate 并要求用户创建原创 Quest/Dialogue DataAsset，提供路径、字段、保存重开、PIE 现象或日志。Codex 不得代替用户创建 Blueprint/Content 资产。

## 7. 停止条件

需要扩大 allowlist、修改 Config、删除/移动资产、引入新模块/第三方依赖、重写 Reward/Inventory/Battle 主合约、或构建需要写入沙箱外目录时，必须记录原因并请求最小授权。
