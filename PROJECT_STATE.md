# HSR Project State

> 最后更新：2026-07-16
> 作用：CC-SWITCH 切换模型后的快速恢复快照。高级模型负责维护；低级模型不得把本文件作为执行入口。

## 当前 Phase

- **Phase 0 准备阶段（Not verified）**。
- UE5.6 Blank C++ 工程基线已创建并经审查通过；Development Editor 构建、插件/模块依赖、Gameplay Tags、默认地图、Editor 重开与 PIE 仍未验证。

## 当前任务

- 活动任务卡：`tasks/active-task.md`。
- 当前任务：`TASK-P0-002` — 验证工具链和首次 Development Editor 构建。
- 当前状态：低级执行者已完成执行并提交 `tasks/execution-result.md`，等待高级审查者审批。
- 最近完成任务：`TASK-P0-001` — 创建 UE5.6 Blank C++ 工程基线并采集首轮证据，结论为 `PASS WITH FOLLOW-UP`。

## 当前阻塞点

- 文件化上下文机制本身没有阻塞。
- 尚无 Development Editor 构建与空白 PIE 的真实证据，因此 Phase 0 保持 `Not verified`，不得宣布通过。
- `TASK-P0-002` 的执行结果已存在，但尚未完成独立审批、归档和角色提交；不得开始 `TASK-P0-003`。

## 已完成事项

- 已建立 HSR 协作规则、Phase 0–20 路线图、MVP/第一周/第一月边界和 GAS 学习路线。
- 已建立低级模型任务模板、Loop Engineering、Codex/UE Editor 分工和阶段建议 Skill。
- 已建立 CC-SWITCH 文件化上下文机制：项目快照、活动任务卡、任务/审查模板、归档目录和模型切换 Prompt。
- 已将“高级模型规划、低级模型执行、审查模型验收、结果回传”的职责写入项目文档。
- 已建立 Phase 0 七个串行任务的详细执行计划，并把 Prompt Planner、Prompt Reviewer、Architect、Safety Reviewer 的协调人格与低级模型二次确认门禁写入长期真源。
- `TASK-P0-001` 已通过审查：工程位于仓库根目录，无 `HSR/HSR/` 嵌套；`.uproject` 关联 UE5.6，且只有一个 `HSR` Runtime 模块与最小模块入口。
- `TASK-P0-002` 已由低级执行者完成：UBT 入口与 `HSREditor Development Win64` 目标退出码为 0，但 Target up-to-date，实际 C++ 编译尚未触发；当前等待审查者审批。

## 未完成事项

- 未验证 Visual Studio Community 2026 与 UE5.6 的 UBT/UHT、Development Editor 构建兼容性。
- 已报告完成首次 Editor 打开，但未完成 Editor 重开或空白 PIE；尚未产生任何 Gameplay、GAS、Blueprint 或资产验证。
- 尚未完成 `TASK-P0-002` 的独立审查、角色 commit 和任务归档。

## 当前代码状态

- 已存在 `HSR.uproject`、`Source/HSR.Target.cs`、`Source/HSREditor.Target.cs`、`Source/HSR/HSR.Build.cs`、`Source/HSR/HSR.h`、`Source/HSR/HSR.cpp`、`Config/DefaultEngine.ini` 和 `Config/DefaultInput.ini`。
- `HSR.sln` 与 `.vs/`、`Binaries/`、`Intermediate/`、`Saved/`、`DerivedDataCache/` 为工具/Editor 产物，不作为构建、PIE 或 Gameplay 通过证据。
- 当前仅完成工程基线；所有 Gameplay Phase 与 GAS Stage 仍未实施。

## 当前设计决策

- 模型切换后的主要记忆来自仓库文件，不依赖聊天历史。
- `PROJECT_STATE.md` 保存项目快照；`tasks/active-task.md` 是唯一活动任务契约；`worklog.md` 保存真实证据；`todo_plan.md` 保存进度；`learning-journal.md` 保存可复用知识。
- 高级模型负责读取项目全局上下文、拆解任务、生成/更新任务卡与项目快照。
- 低级模型只以 `tasks/active-task.md` 作为上下文入口，不自行读取全局状态文档；实现过程中只可读取和修改任务卡明确授权的目标文件。
- 审查模型依据任务卡、审查模板、实际改动文件和验证证据给出结论，不凭执行模型自述放行。
- 一次只能有一张活动任务卡；完成或取消后先归档，再创建下一张。
- 每个角色完成本轮任务后，必须按固定格式提交自己的任务产物；一个 Phase 的全部子任务通过审查并完成阶段收尾后，由协调者提交阶段收尾 commit 并推送远端。

## 重要限制

- 用户当前回合的明确要求始终高于本快照和模板。
- 没有真实编译、Editor、PIE 或专项测试证据时，不得把计划、代码或文档标记为功能完成。
- 未列入活动任务卡允许清单的文件默认禁止修改；需要扩权时必须停止并请求用户授权。
- 禁止擅自删除、覆盖历史、跨 Phase、批量修改 Config、创建未经授权的 Blueprint/资产或导入第三方资源。
- 禁止执行 `git reset`、`git clean`。本项目已获得用户对“角色任务完成后 commit、Phase 子任务全部结束后 commit 并 push”的长期授权，但每次仍必须先检查 diff、排除派生产物、确认没有越权文件，并记录 commit hash 与 push 结果。

## 下一个推荐任务

- **唯一推荐任务：** 由高级审查者审批 `TASK-P0-002` 的执行结果；审批前不得创建或开始 `TASK-P0-003`。
- 审批通过后，先完成低级执行者、高级协调者和审查者各自的角色提交与任务归档，再创建 `TASK-P0-003`。
- 本推荐不会自动启动任务，也不包含 Character、Controller、GAS、Blueprint、UMG 或 Gameplay。

## 新模型接入时必须读取的文件

### 高级模型

1. `PROJECT_STATE.md`
2. `.agents/agents.md`
3. `tasks/active-task.md`
4. `todo_plan.md`
5. `worklog.md` 的最新一条记录
6. 当前 Phase 文档及任务卡点名的专项文档

### 低级模型

1. 只读取 `tasks/active-task.md` 作为上下文入口。
2. 仅在任务卡明确允许时，读取/修改其中列出的目标文件；不得自行补读全局文档来扩大范围。

### 审查模型

1. `tasks/active-task.md`
2. `tasks/review-template.md`
3. 任务卡允许范围内的实际改动文件与验证证据
4. 如需判断项目级一致性，再读取 `PROJECT_STATE.md` 和 `.agents/agents.md`

## 状态冲突处理

## 2026-07-16 协调更新

- 根据 `TASK-P0-001` 的 `PASS WITH FOLLOW-UP`，已创建唯一活动任务 `TASK-P0-002`。
- `TASK-P0-002` 只验证 `HSREditor Development Win64` 首次构建；尚未获得用户执行确认，不得启动构建。
- 构建失败时仅记录第一处真实错误；不得修改 SDK、Target、Build.cs、源文件或缓存。

当文件内容不一致时，按以下顺序处理：用户当前明确要求 → `.agents/agents.md` 长期规则 → `tasks/active-task.md` 本轮契约 → `PROJECT_STATE.md` 快照 → 专项设计文档 → `todo_plan.md` → `worklog.md`。发现冲突时不得猜测，应由高级模型根据证据修正文件。
