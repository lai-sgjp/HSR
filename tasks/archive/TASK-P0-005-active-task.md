# TASK-P0-005：创建并设置 Map_ProjectSetup

## Role Lock / 角色锁定

低级执行模型当前且仅能担任 `Implementation Agent / 低级执行模型`。它只可读取本活动卡，不能切换为规划、架构、审查、教师或项目管理角色。确认前不得调用工具、修改文件或执行 Editor 操作。

## 当前 Phase

- Phase 0：项目初始化与规范落地
- 当前门禁：`Not verified`
- P004：`PASS WITH FOLLOW-UP`，审查 commit：`ef1425001c2e957c110aa6c68a7ec5d3f8efdd28`
- 当前状态：P005 仅完成协调规划，尚未实施；当前没有 P006

## 任务目标

只创建并设置一个最小 Empty Level `Map_ProjectSetup`，并让它成为 Editor Startup Map 与 Game Default Map。不得运行 PIE，不得创建玩法或表现内容。

## 规划审查摘要

### 1. 子 Agent 分析

- Prompt Planner：将 P005 限定为一个地图资产与两个地图设置的独立验收任务。
- Prompt Reviewer：要求记录精确资产路径、仅允许的 Config diff、Editor 重开后的地图与设置证据，并保留未执行 PIE。
- Architect：使用 UE5.6 Empty Level 和单一 `Map_ProjectSetup`；不引入 World Partition、大型模板、GameMode 或 Blueprint。
- Safety Reviewer：只允许三条精确路径；任何额外持久文件、Config 键、资产或目录批量创建都必须停止申请扩权；禁止构建、PIE、P006、push。

### 2. 是否适合当前 Phase

适合。地图基线是 Phase 0 的相邻 checkpoint，位于插件/Tags 验证之后、Editor 重开与空白 PIE 之前。它不能被当作 Phase 0 完成证据，也不能替代后续 PIE 门禁。

### 3. 是否需要拆分

不需要。创建 Empty Level、设置两个默认地图和重开 Editor 验证属于同一个可独立验收的配置闭环。构建、PIE、Gameplay、GameMode 和 P006 明确推迟。

### 4. 本轮最小任务

由用户在 UE5.6 Editor 创建 Empty Level，保存为 `/Game/Maps/Map_ProjectSetup`；在 Project Settings > Maps & Modes 中将 `Editor Startup Map` 和 `Game Default Map` 都设置为该地图；关闭并重开 Editor 验证持久化。

### 5. 风险审查

Blocking：如果 UE5.6 需要写入除以下三条路径之外的持久文件，必须停止并申请扩权；如果活动卡与实际状态冲突，也必须停止。

允许路径仅为：

- `Content/Maps/Map_ProjectSetup.umap`
- `Config/DefaultEngine.ini`
- `tasks/execution-result.md`

`DefaultEngine.ini` 只能合并 `/Script/EngineSettings.GameMapsSettings` 下的 `EditorStartupMap` 与 `GameDefaultMap`，必须保留现有 Android File Server 设置，不得覆盖历史内容。禁止 World Partition、大型模板、GameMode、Blueprint、角色、灯光美化、Gameplay 资产、其他目录批量创建、构建、PIE、P006、Git reset、Git clean、push、强制或无关 Git 操作。执行者完成后只可提交本任务三个允许路径。

### 6. 最终低级模型 Prompt

你现在只能担任 `Implementation Agent / 低级执行模型`，执行 `TASK-P0-005 / Phase 0`。首次只读取本活动任务卡；不要读取其他项目文档来扩大范围。

在任何工具调用、文件修改或 UE Editor 操作前，必须复述：任务编号、当前 Phase、唯一目标、精确允许路径、Editor 操作、验收证据、未执行项、禁止内容、风险和停止条件。

复述必须明确：用户负责在 UE5.6 Editor 创建 Empty Level 并保存为 `/Game/Maps/Map_ProjectSetup`；在 `Project Settings > Maps & Modes` 将 `Editor Startup Map` 与 `Game Default Map` 都设为该地图；关闭并重开 Editor；不运行 PIE。

确认后才可执行：检查目标路径和 `DefaultEngine.ini` 历史；指导用户创建和设置地图；记录精确资产路径、Config diff、重开证据、两个设置、未执行的构建/PIE及首个错误。若 UE 生成任何额外持久文件或额外 Config 键，立即停止并申请扩权。

验收必须分别证明：地图资产精确位于 `Content/Maps/Map_ProjectSetup.umap`；`DefaultEngine.ini` 仅新增/合并目标 `GameMapsSettings` 两项且保留 Android File Server；关闭重开 Editor 后自动打开该地图；两个默认地图设置仍正确；PIE 未运行。

禁止 World Partition、大型模板、GameMode、Blueprint、角色、灯光美化、Gameplay 资产、其他目录批量创建、构建、PIE、P006、Git reset、Git clean、push、强制或无关 Git 操作。执行前不得调用任何工具。

最终必须先复述完整计划，然后以以下原文结束并停止：

`等待用户确认执行 TASK-P0-005。`

## 执行模型

低级执行模型负责执行；用户负责 UE5.6 Editor 可视化操作和证据回传；高级审查者负责独立验收；高级协调者负责状态同步与归档。当前尚未授权实施。

## 执行前必须读取的文件

- `tasks/active-task.md`
- 确认后仅可按本卡读取 `Config/DefaultEngine.ini`

## 允许修改文件

- `Content/Maps/Map_ProjectSetup.umap`
- `Config/DefaultEngine.ini`
- `tasks/execution-result.md`

未列出的文件默认禁止修改；如需扩展范围，停止并请求用户授权。

## UE Editor 手动操作

确认后由用户执行：打开 UE5.6 Editor，创建 Empty Level；保存为 `/Game/Maps/Map_ProjectSetup`；打开 `Project Settings > Maps & Modes`，设置 `Editor Startup Map` 与 `Game Default Map`；保存；关闭并重开 Editor，确认自动打开该地图且两个设置仍正确。不要运行 PIE。

## 验收标准

- [ ] `Content/Maps/Map_ProjectSetup.umap` 存在且路径精确。
- [ ] `DefaultEngine.ini` 仅合并目标 `GameMapsSettings` 两项，现有 Android File Server 设置保留。
- [ ] 关闭并重开 Editor 后自动打开 `Map_ProjectSetup`。
- [ ] 重开后两个默认地图设置仍正确。
- [ ] `tasks/execution-result.md` 记录真实证据、未验证项和未运行 PIE。
- [ ] 无额外资产、Config、目录或玩法内容；未创建 P006。

## 执行后必须更新的文件

低级执行者只可更新 `tasks/execution-result.md`。后续由审查者和协调者依据真实证据更新 `PROJECT_STATE.md`、`worklog.md`、`todo_plan.md` 并归档任务。P005 实施前不得勾选 todo checkpoint。

## Git 交付

- 执行者完成后只可提交以下三个允许路径：`Content/Maps/Map_ProjectSetup.umap`、`Config/DefaultEngine.ini`、`tasks/execution-result.md`；不得将其他文件带入该 commit。
- 成功 commit message：`低级执行模型+Implementation Agent+TASK-P0-005/Phase 0+创建并设置项目基线地图`
- 如果提交失败，必须保留第一处真实错误，并使用固定结构说明失败，例如：`低级执行模型+Implementation Agent+TASK-P0-005/Phase 0+提交失败：<第一处真实错误>`。
- 提交前必须检查 `git status`、`git diff`、允许路径和派生产物；禁止 `reset`、`clean`、push、强制提交或无关 Git 操作。
- 规划角色的协调者 commit 不属于本活动卡执行阶段；Phase 未收尾前不得 push。
