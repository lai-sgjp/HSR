# Active Task — TASK-P0-003

> 状态：协调规划完成，等待低级执行模型复述后由用户单独确认。
> 创建任务卡、协调者提交或旧任务授权均不等于 P003 执行授权。

## Role Lock / 角色锁定

低级模型当前且仅能担任 `Implementation Agent / 低级执行模型`。不得切换为 Planner、Reviewer、Architect、Teacher 或高级模型，不得扩大文件范围、改变 Phase 或创建后续任务。需要范围外文件或高阶决策时立即停止并请求协调者补卡。

## 任务编号

`TASK-P0-003`

## 任务名称

配置并验证 Enhanced Input 与 GAS 基础插件/模块依赖

## 当前 Phase

Phase 0，门禁为 `Not verified`。P001 工程基线和 P002 UBT/Target 入口已经审查；实际 C++ 编译、插件加载、Tags、地图、Editor 重开和 PIE 仍未完成。

## 任务目标

只完成以下最小闭环：在 `HSR.uproject` 启用 `EnhancedInput`、`GameplayAbilities` 插件，在 `Source/HSR/HSR.Build.cs` 声明 `EnhancedInput`、`GameplayAbilities`、`GameplayTags`、`GameplayTasks` 模块依赖，并取得一次本任务发生后的 Development Editor 构建与插件/模块加载证据。

本任务不创建 Gameplay Tags 内容、ASC、AttributeSet、Ability、GameplayEffect、Character、Blueprint、地图、UI 或 PIE 流程。

## 规划审查摘要

### 1. 子 Agent 分析

- Prompt Planner：目标只有插件声明、模块依赖和对应验证，不混入 P004 Tags 或 P005 地图。
- Prompt Reviewer：P002 的 `Target is up to date` 不能作为 P003 构建证据；必须记录 P003 修改后的新构建结果。
- Architect：`.uproject` 只负责启用 `EnhancedInput`、`GameplayAbilities` 插件；Build.cs 负责四个模块依赖。GameplayTags 与 GameplayTasks 是模块依赖，不要求伪造同名插件条目。
- Safety Reviewer：禁止修改 Config、其他 Source 文件、清缓存、升级 SDK、创建 Gameplay 内容、执行 Git push 或进入后续任务。

### 2. 是否适合当前 Phase

适合。插件与模块基线是 Phase 0 的第三个相邻任务，也是 Gameplay Tags 配置、地图和最终 PIE 门禁的前置条件。Phase 0 尚未完成，因此不能进入 Phase 1。

### 3. 是否需要拆分

已经拆分。本任务只处理插件声明与模块依赖。以下内容推迟：

- P004：Gameplay Tags 根命名空间与配置。
- P005：`Map_ProjectSetup` 与默认地图。
- P006：Editor 重开与空白 PIE 总门禁。
- P007：Phase 0 文档归档和最终判定。

### 4. 本轮最小任务

1. 核对当前 `.uproject` 与 Build.cs 基线。
2. 最小增加两个插件声明和四个模块依赖，不重排或重构其他内容。
3. 执行一次 P003 修改后的 `HSREditor Development Win64` 构建。
4. 用户打开 Editor，核对两个插件已启用且模块加载无错误；不运行 PIE。
5. 将实际 diff、命令、日志、退出码、第一处错误和未验证项写入 `tasks/execution-result.md`。

### 5. 风险审查

- Blocking：插件名、JSON 结构或模块名不被 UE5.6 识别。
- Blocking：构建失败、Editor 报模块缺失，或需要修改允许清单外文件。
- Risk：把插件启用误当成 Build.cs 已依赖，或反过来只加模块而未启用 GameplayAbilities 插件。
- Risk：复用 P002 的 up-to-date 证据，未执行 P003 修改后的真实构建。
- 停止条件：需要清理缓存、修改 SDK/Target/Config/其他 Source、创建资产或进入 P004/P005/P006。

### 6. 最终低级模型 Prompt

你是 HSR 的 `Implementation Agent / 低级执行模型`。聊天上下文不可信，本活动卡是唯一上下文入口。

首次读取后只能：

1. 复述任务编号、Phase、唯一目标和非目标。
2. 复述插件层与模块层的区别、三个允许修改路径、Editor 用户步骤、构建证据和停止条件。
3. 检查卡片是否存在占位符、执行模型是否匹配，并说明尚未获得复述后的单独确认。
4. 最后一行原样写：`等待用户确认执行 TASK-P0-003。`
5. 立即停止；同一回复不得调用工具、读取目标文件、修改文件、运行构建或执行 Git。

只有用户在该复述后另发 `确认执行 TASK-P0-003`，才可继续。确认只适用于 P003。

## 执行模型

- 高级协调者：已规划活动卡，实施期间不代替低级模型扩权。
- 低级执行模型：确认后执行最小文件修改、构建核对、执行报告和自己的角色 commit。
- 用户：打开 UE Editor，确认插件启用和模块加载现象，回传日志/截图。
- 审查者：执行完成后独立核对 diff、构建、Editor 证据和 commit。

## 执行前必须读取的文件

首次只读取：

- `tasks/active-task.md`

用户确认后才允许读取：

- `HSR.uproject`
- `Source/HSR/HSR.Build.cs`

## 允许修改文件

- `HSR.uproject`
- `Source/HSR/HSR.Build.cs`
- `tasks/execution-result.md`

未列出的文件默认禁止读取或修改；需要扩权时停止。

## 禁止修改内容

- 禁止修改 `Config/`、Target.cs、模块入口、其他 Source、Content、Plugins 目录和现有归档。
- 禁止创建 Gameplay Tags 内容、ASC、AttributeSet、Ability、Effect、Character、Blueprint、地图、UI 或任何 Gameplay 类/资产。
- 禁止清缓存、删除、覆盖、升级 SDK/引擎、执行 reset/clean、强制提交或 push。
- 禁止创建或执行 `TASK-P0-004`。

## 实现步骤

1. 确认当前 `.uproject` 只有一个 `HSR` Runtime 模块，记录现有插件声明。
2. 在 `.uproject` 最小启用 `EnhancedInput`、`GameplayAbilities` 插件；保持 JSON 有效和单 Runtime 模块不变。
3. 在 Build.cs 的现有依赖列表中最小增加 `EnhancedInput`、`GameplayAbilities`、`GameplayTags`、`GameplayTasks`；不进行无关重排。
4. 展示实际 diff，确认只修改两个工程文件。
5. 执行 `HSREditor Development Win64` 构建，记录命令、工具链、是否触发编译/链接、退出码和第一处错误。失败立即停止，不自动修复。
6. 用户打开 Editor，在 Plugins/Output Log 中确认两个插件启用且没有四个模块的加载错误；不运行 PIE。
7. 将事实写入 `tasks/execution-result.md`，区分已修改、已构建、已加载、未验证。
8. 检查 status/diff，只提交本任务三个允许文件，不 push。

## UE Editor 手动操作

1. 构建成功后打开 `HSR.uproject`。
2. 在 Edit → Plugins 中查询 `Enhanced Input` 与 `Gameplay Ability System`，确认已启用。
3. 查看 Output Log，搜索 `EnhancedInput`、`GameplayAbilities`、`GameplayTags`、`GameplayTasks` 及 `Error`/`Missing`。
4. 回传是否要求重启、模块是否加载、第一处相关错误；本任务不运行 PIE。

## 验收标准

- [ ] 用户确认发生在低级模型复述之后。
- [ ] `.uproject` 保持单 `HSR` Runtime 模块，只增加正确的插件声明。
- [ ] Build.cs 只增加四个目标模块依赖，无无关重构。
- [ ] P003 修改后的 Development Editor 构建有真实命令、日志和退出码。
- [ ] Editor 中两个插件启用，四个模块无未解释加载错误。
- [ ] 未修改 Config/其他 Source，未创建 Gameplay、Tags、地图或资产。
- [ ] 未验证内容如实保留，失败时记录第一处真实错误。
- [ ] 低级执行者完成角色 commit，未 push。

## Git 交付

- 成功 commit：`低级执行模型+Implementation Agent+TASK-P0-003/Phase 0+配置并验证插件与模块依赖`
- 失败证据 commit：`低级执行模型+Implementation Agent+TASK-P0-003/Phase 0+记录插件或模块依赖失败证据`
- 提交前检查 status、diff、允许文件和派生产物，只提交本任务产物并回报 hash。
- Phase 0 未完成，禁止 push。

## 当前执行结果

- 已修改：尚未执行。
- 已验证：尚未执行。
- 未验证：插件/模块依赖、P003 构建、Editor 加载、Tags、地图、PIE。
- Editor 待办：等待低级模型复述与用户确认后执行。
- 审查结论：待执行后由独立审查者填写。
