# Phase 14 详细执行计划：任务、对话与 Save v4

> 基线：2026-07-26，Phase 13 为 `Ready with inherited follow-ups`；`tasks/active-task.md` 原状态为 `PHASE 14 NOT STARTED`。
> Gate 0 状态：`REPAIR IN PROGRESS`。用户已明确进入 Phase 14，并指出阶段开始必须先写详细子任务规划；本文件即为 Phase 14 的阶段入口修正。此前已产生的 Quest/Dialogue/Save 代码只能视为未验收草稿，必须重新纳入本计划、活动任务卡、编译、Automation、审查和文档证据后才可计入完成。

## 1. 当前事实、证据与 Gate 0

- `AGENT VERIFIED / REPO`：Phase 13 已归档，当前仓库快照仍写明“Phase 14 尚未规划或实施”。
- `AGENT VERIFIED / GAP`：本轮开始时缺少 `docs/phase-14-execution-plan.md`，`tasks/active-task.md` 仍为 `No Active Task`。
- `AGENT VERIFIED / DRAFT CODE`：工作树已有 Quest、Dialogue、Definition、Save v4 和测试草稿；这些改动尚未经过 Phase 14 任务卡授权、Development Editor 编译、Automation 或 Independent Reviewer 放行。
- `USER AUTHORIZED`：用户明确要求继续 Phase 14 并尽快完成，同时强调阶段入口规划规则必须执行。
- `NOT YET VERIFIED`：UE Build、UHT、Automation、PIE、Editor 资产、Save 重开、Teacher 掌握度和 Independent Review 均未闭合。
- `WORKTREE NOTE`：`.claude/settings.local.json`、`Content/UI/WBP_Reward_Summary_P13.uasset`、`learn/AI.md`、既有 `learn/CppEngineDepth.md` 改动属于用户/既有工作，Phase 14 代码任务不得认领或回退。

Gate 0 判定：`REPAIR IN PROGRESS`。允许先补齐 Phase 14 计划与活动任务卡；随后只允许在本计划和任务卡 allowlist 内修正已有草稿并验证。

## 2. 阶段唯一最终结果

一个原创最小任务可以由对话选择提交领域事件推进：Quest Subsystem 以稳定 QuestId/Objectives 管理 NotStarted/Active/Completed 状态；Dialogue Subsystem 以 DialogueId/NodeId/ChoiceId 解析分支并向 Quest 提交事件；任务完成后通过 Phase 13 RewardSubsystem 的稳定 ClaimId 幂等发奖；Save v4 保存并恢复任务状态、任务奖励领取状态和 revision。重复对话选择、重复任务事件、重复领奖、Save/Load 后重放都不得重复奖励或产生半提交。

Phase 14 只交付最小 Quest/Dialogue runtime、DataAsset 定义、Save v4 和自动化验证。不创建正式任务 UI、不创建地图传送、不新增剧情内容、不导入第三方资产、不改变 Battle/Encounter 主链。

## 3. 所有权与数据流不变量

### 3.1 数据分层

- **Quest Definition**：`UPrimaryDataAsset` 或 `UDataAsset` 只保存 QuestId、Objective 定义、RewardDefinitionId、RewardSeed、是否自动领奖；不保存运行状态、Widget、Actor 或领取账本。
- **Quest Authority**：`UGameInstanceSubsystem` 保存 QuestRuntimeState、Objective 进度、RewardClaimId、bRewardClaimed 和 Revision；它是任务运行状态唯一真源。
- **Dialogue Definition**：只保存 DialogueId、NodeId、Text、Choices、Choice Target 和可选 QuestEvent；不保存当前对话游标。
- **Dialogue Runtime**：只负责注册定义、解析起始节点和选择结果，提交 QuestEvent；不直接改 Reward、Inventory 或 Save。
- **Reward Authority**：Quest 完成后的发奖必须走 Phase 13 `UHSRRewardSubsystem::SubmitReward`；Quest 不直接写 Inventory，也不重复实现 Claim ledger。
- **Save Authority**：`UHSRSaveSubsystem` 负责 schema v4 捕获和恢复 Quest save DTO；v1-v3 加载时 Quest 状态迁移为空状态。
- **UI/Editor**：Phase 14 不要求正式 UI；如果用户在 Editor 创建占位 DataAsset，必须记录路径、字段、保存重开和作者归属。

### 3.2 ID、状态与幂等契约

- `QuestId`、`ObjectiveId`、`EventId`、`DialogueId`、`NodeId`、`ChoiceId` 都必须是稳定业务 ID；显示文本、数组下标、Actor 名称和资产路径不得作为运行 ID。
- 同一 QuestId 的重复注册：完全相同定义返回 no-op；语义不同定义返回 DuplicateDefinitionId 或 InvalidDefinition。
- `StartQuest` 对已存在 Quest 返回当前状态 no-op，不重置进度。
- `SubmitEvent` 只推进 Active Quest 中匹配 EventId 且未完成的 Objective；非法 EventId/Count 零副作用。
- Quest 从 Active 到 Completed 只能发生一次；Completed 后重复事件不增加 revision、不重复广播、不重复发奖。
- `RewardClaimId` 必须由 QuestId 稳定派生或稳定保存；同 QuestId 重放、Save/Load 后重放应由 Reward ledger 与 Quest bRewardClaimed 双重保护。
- 自动领奖失败不得标记 `bRewardClaimed`；成功或 RewardSubsystem no-op 时才可视为已领取。

### 3.3 Save v4 契约

- `FHSRSaveData.SchemaVersion` 升级为 4，新增 `FHSRQuestSaveData Quests`。
- v1/v2/v3 读入时，Profiles/Party/Equipment/Inventory/Rewards 保持既有迁移语义，Quests 迁移为空状态和 revision 0。
- v4 Validate 必须拒绝重复 QuestId、未知 QuestDefinition、目标数量越界、Objective 顺序/RequiredCount 不匹配、Completed 与 Objective 完成状态不一致、无效 RewardClaimId、revision 负数或超出总 revision。
- Restore 必须 candidate-first：所有子系统 Prepare 成功后才 Commit；任何 Quest 数据错误不得污染旧 Runtime。
- Restore 通知必须区分 `bQuestsChanged`，但不得把旧版本空迁移误报为奖励或库存变化。

## 4. 串行工作包

### P14-000：阶段入口修正与详细规划

**唯一验收结果：** Phase 14 拥有可复核的总计划、唯一活动任务卡、执行/审查占位和真实状态记录；既有代码草稿被明确归类为“未验收实现”，不能绕过编译、Automation 或审查。

**Codex allowlist：**

- `docs/phase-14-execution-plan.md`
- `tasks/active-task.md`
- `tasks/execution-result.md`
- `tasks/final-review.md`
- `PROJECT_STATE.md`
- `worklog.md`
- `todo_plan.md`

**用户 Editor Gate：** 无。

**完成证据：** 文件落盘、`git diff --check`、工作树范围核对。

### P14-001：Quest runtime 与 Reward 幂等接入

**唯一验收结果：** 一个 Quest Definition 可注册、启动、由事件推进到完成，并通过 RewardSubsystem 以稳定 ClaimId 领奖；重复启动、重复事件、重复领奖、奖励失败全部零副作用或 no-op。

**Codex allowlist：**

- `Source/HSR/Quest/HSRQuestTypes.h`
- `Source/HSR/Quest/HSRQuestSubsystem.h`
- `Source/HSR/Quest/HSRQuestSubsystem.cpp`
- `Source/HSR/Data/Definitions/HSRQuestDefinition.h`
- `Source/HSR/Tests/HSRQuestDialogueTests.cpp`（仅 Quest 用例）
- `tasks/active-task.md`
- `tasks/execution-result.md`
- `tasks/final-review.md`

**用户 Editor Gate：** C++/Automation 通过后，可选创建一个原创 Quest Definition DataAsset；记录路径、字段、保存重开证据。没有该资产时 Automation 仍可使用 C++ transient definition 验证核心合约。

### P14-002：Dialogue runtime 与 Quest Event 桥接

**唯一验收结果：** 一个 Dialogue Definition 可注册并从 StartNode 读取节点；选择 Choice 后返回 NextNode，并在 Choice 配置 QuestEventId 时向 QuestSubsystem 提交事件；无效 Dialogue/Node/Choice、循环分支和空事件零副作用。

**Codex allowlist：**

- `Source/HSR/Dialogue/HSRDialogueTypes.h`
- `Source/HSR/Dialogue/HSRDialogueSubsystem.h`
- `Source/HSR/Dialogue/HSRDialogueSubsystem.cpp`
- `Source/HSR/Data/Definitions/HSRDialogueDefinition.h`
- `Source/HSR/Quest/HSRQuestTypes.h`（仅必要事件 DTO 兼容）
- `Source/HSR/Quest/HSRQuestSubsystem.h/.cpp`（仅必要桥接修复）
- `Source/HSR/Tests/HSRQuestDialogueTests.cpp`（Dialogue 用例）
- 任务三件套

**用户 Editor Gate：** 可选创建一个原创 Dialogue Definition DataAsset；Blueprint 不得直接改 Quest/Reward/Inventory。

### P14-003：Save v4 Quest 持久化与旧 schema 迁移

**唯一验收结果：** Quest Active/Completed/RewardClaimed 状态可 Save/Load 恢复；v1-v3 加载迁移为空 Quest；坏 Quest save data 被拒绝且不污染旧 runtime；重复 Load 不重复广播或重复奖励。

**Codex allowlist：**

- `Source/HSR/Save/HSRSaveTypes.h`
- `Source/HSR/Save/HSRSaveSubsystem.h`
- `Source/HSR/Save/HSRSaveSubsystem.cpp`
- `Source/HSR/Quest/HSRQuestTypes.h`
- `Source/HSR/Quest/HSRQuestSubsystem.h/.cpp`
- `Source/HSR/Tests/HSRQuestDialogueTests.cpp`
- `Source/HSR/Tests/HSRInventoryRewardSaveTests.cpp`（仅 schema 断言兼容）
- 任务三件套

**用户 Editor Gate：** 如果用真实资产验证 Save/Load，用户需在 Editor 保存、关闭重开并回传路径和日志；Automation 先用 transient 定义闭合 runtime。

### P14-004：Build、Automation、PIE/Editor 证据与修复闭环

**唯一验收结果：** `HSREditor Win64 Development` 编译通过；Phase 14 Automation 用例通过；`git diff --check` 通过；若用户提供 DataAsset/PIE 证据，记录真实来源。所有失败首错保留，不用最终成功覆盖历史失败。

**Codex allowlist：**

- P14-001～003 所列源码与测试文件
- `tasks/execution-result.md`
- `worklog.md`
- `todo_plan.md`

**用户 Editor Gate：** 可选 PIE 验证：Start Quest -> 选择 Dialogue Choice -> Quest Complete -> Reward exactly once -> Save -> 重开 Load -> 重复 Choice/Claim 不重复到账。

### P14-005：Teaching、Independent Review 与阶段收尾

**唯一验收结果：** 用户能复盘 Quest/Dialogue/Save v4 的关键 UE5.6/C++ 概念；Independent Reviewer 给出 `PASS` 或 `PASS WITH FOLLOW-UP`；任务三件套归档；PROJECT_STATE/worklog/todo/learning-journal 与 Git 证据同步。不得新增 Gameplay。

**Codex allowlist：**

- `learning-journal.md`
- `PROJECT_STATE.md`
- `worklog.md`
- `todo_plan.md`
- `tasks/active-task.md`
- `tasks/execution-result.md`
- `tasks/final-review.md`
- `tasks/archive/TASK-P14-*.md`
- `docs/phase-14-execution-plan.md`

**用户 Editor Gate：** 回答 Teacher 复盘题；确认任何 Editor 资产归属与证据边界。

## 5. 验证与失败矩阵

必须覆盖：

- Quest：空 QuestId、重复 QuestId 相同/不同定义、空 Objective、重复 ObjectiveId、空 EventId、RequiredCount <= 0、Start unknown、Start repeat、Submit invalid event、Submit count <= 0、partial objective、complete once、completed repeat event no-op、manual claim before completed reject、claim without reward no-op、reward failure no claimed、repeat claim no duplicate。
- Dialogue：空 DialogueId、重复 DialogueId、空 StartNode、重复 NodeId、unknown start node、unknown choice、choice target missing、choice target valid、choice with QuestEvent、choice with empty event、branch loop no mutation beyond selected event。
- Save：schema v1/v2/v3 empty Quest migration、schema v4 capture、duplicate saved QuestId reject、unknown QuestDefinition reject、objective mismatch reject、negative/current over required reject、completed/objective mismatch reject、invalid reward claim id reject、revision negative/too high reject、candidate failure zero side effect、repeated Load no duplicate reward。
- Integration：Dialogue Choice -> Quest Event -> Completed -> Reward Submit -> Save -> Load -> Repeat Choice/Claim exactly once。

证据顺序：计划落盘 -> 静态 diff/allowlist -> Development Editor Build -> Automation -> 可选 Editor DataAsset/PIE -> Teacher -> Independent Review -> 文档归档。每项标记 `AGENT VERIFIED`、`USER PROVIDED`、`STATIC ONLY`、`SKIPPED` 或 `NOT RUN`。

## 6. 教学 Gate

用户在收尾前应能解释：

1. Quest Definition、Quest Runtime State、Dialogue Definition 和 Save DTO 的职责边界。
2. 为什么 Quest/Dialogue 使用稳定 `FName` ID，而不是显示文本、数组下标或 Actor 指针。
3. `UGameInstanceSubsystem` 生命周期适合保存本地单机运行状态的原因，以及未来联网时需要重新审查的边界。
4. `UPROPERTY`、`TObjectPtr`、`TWeakObjectPtr` 在 GC 和 Subsystem 引用中的差别。
5. UHT 对 `USTRUCT`、`UCLASS`、`GENERATED_BODY`、Blueprint 暴露和 include 顺序的要求。
6. 为什么 Quest 发奖必须复用 RewardSubsystem 的 Claim ledger，而不是自己写 Inventory。
7. Save v4 为什么要 candidate-first restore，以及旧 schema 迁移为什么不能伪造 Quest 状态。
8. Automation 为什么要覆盖失败路径和重复路径，而不只测 happy path。

## 7. 风险、停止条件与非目标

- 需要删除/移动资产、修改 Config、引入新模块、导入第三方资源、重写 Reward/Inventory/Equipment/Battle 主合约、批量格式化或跨 Phase 扩张时必须停止并请求授权。
- 若 UBT 首错来自引擎缓存、用户目录权限或外部工具链，需要记录真实错误；必要时请求提升权限运行构建，不得伪造构建证据。
- 若已有 Phase 14 草稿存在反射/GC/Subsystem 生命周期风险，优先修正最小编译与测试路径，不做大重构。
- 正式任务 UI、任务日志界面、本地化、剧情脚本、地图传送、NPC 交互 Actor、复杂条件系统、多任务链、任务放弃/回滚、网络复制、预测、正式美术和完整内容制作均不属于 Phase 14。

## 8. Phase 14 完成定义

P14-000～005 全部闭合；Quest/Dialogue runtime、DataAsset definition、Reward 幂等接入、Save v4 和旧 schema 迁移均通过 Development Editor Build 与 Automation；可选 Editor/PIE 证据真实记录；Teacher 与 Independent Reviewer Gate 完成；任务三件套归档；PROJECT_STATE/worklog/todo/learning-journal 更新；Git 范围干净且没有未分类变更。满足前不得标记 Phase 14 Ready 或进入 Phase 15。

## 9. 当前唯一下一步

执行 P14-001～P14-003 的合并修复切片：在已存在草稿基础上，只修正 allowlist 内 Quest/Dialogue/Save/Test 文件，使其通过 Development Editor Build 与 Phase 14 Automation。不得创建 UI、地图、NPC Actor、Config 或正式内容资产。
