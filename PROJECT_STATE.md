# HSR Project State

> 最后更新：2026-07-16  
> 作用：CC-SWITCH 切换模型后的快速恢复快照。高级模型负责维护；低级模型不得把本文件作为执行入口。

## 当前 Phase

- **Phase 0 准备阶段（Not verified）**。
- Phase 0–20 路线规划与协作文档已完成，但 UE5.6 工程尚未创建，Phase 0 的构建、Editor 和 PIE 门禁均未开始验证。

## 当前任务

- 活动任务卡：[`tasks/active-task.md`](tasks/active-task.md)。
- 任务编号：`TASK-P0-001`。
- 任务名称：创建 UE5.6 Blank C++ 工程基线并采集首轮证据。
- 当前状态：**待用户明确授权，不得因任务卡存在而自动执行**。

## 当前阻塞点

- 文件化上下文机制本身没有阻塞。
- Phase 0 推进尚缺用户对工程创建任务的明确授权。
- 尚无 `.uproject`、Development Editor 构建、Editor 启动或空白 PIE 的真实证据，因此不能声明 Phase 0 已开始或已通过。

## 已完成事项

- 已建立 HSR 协作规则、Phase 0–20 路线图、MVP/第一周/第一月边界和 GAS 学习路线。
- 已建立低级模型任务模板、Loop Engineering、Codex/UE Editor 分工和阶段建议 Skill。
- 已建立 CC-SWITCH 文件化上下文机制：项目快照、活动任务卡、任务/审查模板、归档目录和模型切换 Prompt。
- 已将“高级模型规划、低级模型执行、审查模型验收、结果回传”的职责写入项目文档。

## 未完成事项

- 未创建 UE5.6 Blank C++ 工程、`HSR` Runtime 模块、Config 或 Content。
- 未验证 Visual Studio Community 2026 与 UE5.6 的 UBT/UHT、Development Editor 构建兼容性。
- 未启动 UE Editor，未执行空白 PIE，未产生任何 Gameplay、GAS、Blueprint 或资产验证。
- `TASK-P0-001` 尚未获得执行授权，也没有编译或 Editor 证据。

## 当前代码状态

- 仓库当前没有 `.uproject`、`Source/`、`Config/`、`Content/`、`Plugins/` 或游戏 C++ 代码。
- 根目录存在 `Intermediate/`，但它不是功能实现证据，也不得作为项目状态真源。
- 当前可确认成果仅为 Markdown、项目规则和可选 Skill；所有 Gameplay Phase 与 GAS Stage 仍未实施。

## 当前设计决策

- 模型切换后的主要记忆来自仓库文件，不依赖聊天历史。
- `PROJECT_STATE.md` 保存项目快照；`tasks/active-task.md` 是唯一活动任务契约；`worklog.md` 保存真实证据；`todo_plan.md` 保存进度；`learning-journal.md` 保存可复用知识。
- 高级模型负责读取项目全局上下文、拆解任务、生成/更新任务卡与项目快照。
- 低级模型只以 `tasks/active-task.md` 作为上下文入口，不自行读取全局状态文档；实现过程中只可读取和修改任务卡明确授权的目标文件。
- 审查模型依据任务卡、审查模板、实际改动文件和验证证据给出结论，不凭执行模型自述放行。
- 一次只能有一张活动任务卡；完成或取消后先归档，再创建下一张。

## 重要限制

- 用户当前回合的明确要求始终高于本快照和模板。
- 没有真实编译、Editor、PIE 或专项测试证据时，不得把计划、代码或文档标记为功能完成。
- 未列入活动任务卡允许清单的文件默认禁止修改；需要扩权时必须停止并请求用户授权。
- 禁止擅自删除、覆盖历史、跨 Phase、批量修改 Config、创建未经授权的 Blueprint/资产或导入第三方资源。
- 禁止执行 `git reset`、`git clean`；add、commit、push 也必须单独取得用户授权。

## 下一个推荐任务

- **唯一推荐任务：** 用户确认 `TASK-P0-001` 的文件范围和 Editor 步骤后，明确授权创建 UE5.6 Blank C++ 工程基线。
- Codex/高级模型职责：执行前复核任务卡；执行后只根据用户提供的真实文件、构建日志和 Editor 结果更新状态。
- 用户职责：在 UE5.6 Project Browser 中创建项目、完成 Editor/IDE 操作并回传构建与空白 PIE 证据。
- 本推荐不会自动启动 Phase 0，也不包含 Character、Controller、GAS、Blueprint、UMG 或 Gameplay。

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

当文件内容不一致时，按以下顺序处理：用户当前明确要求 → `.agents/agents.md` 长期规则 → `tasks/active-task.md` 本轮契约 → `PROJECT_STATE.md` 快照 → 专项设计文档 → `todo_plan.md` → `worklog.md`。发现冲突时不得猜测，应由高级模型根据证据修正文件。
