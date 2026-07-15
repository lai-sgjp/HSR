# TASK-P0-004 活动任务卡

## Role Lock / 角色锁定

低级执行模型当前且仅能担任 `Implementation Agent / 低级执行模型`。不得切换为高级模型、Prompt Planner、Prompt Reviewer、Architect、Code Reviewer、Safety Reviewer、Learning Reviewer、Teacher 或项目负责人，不得改写目标、扩大文件范围、改变 Phase 或验收标准。

低级模型首次只能读取本文件并复述计划。复述必须以 `等待用户确认执行 TASK-P0-004。` 结束并立即停止。只有用户在该复述之后单独明确确认当前任务，才允许调用工具、读取目标文件或执行任何操作；确认前必须保持零工具调用。

## 任务编号

`TASK-P0-004`

## 任务名称

建立并验证 Gameplay Tags 配置基线

## 当前 Phase

- `Phase 0 / Not verified`。
- `TASK-P0-003` 已由高级审查者判定 `PASS WITH FOLLOW-UP` 并归档，审查 commit 为 `96a3858a5fd4e3a292b1fd7fe4ee8b4023a16250`。
- P004 只处理 Gameplay Tags 配置与 Editor 持久性证据；不得进入地图、PIE 或 Phase 1。

## 任务目标

只通过 `Config/DefaultGameplayTags.ini` 建立 Gameplay Tags 配置导入入口和八个根 Tag：`Ability`、`State`、`Status`、`Event`、`Damage`、`Element`、`Cooldown`、`UI`，并由用户在 UE5.6 Editor 中确认可查询、无重复冲突且关闭重开后仍存在。

明确非目标：不创建任何子 Tag、GameplayAbility、GameplayEffect、ASC、AttributeSet、C++、Blueprint、地图、输入资产、UI 或其他 Content 资产，不运行构建或 PIE。

## 规划审查摘要

### 1. 子 Agent 分析

- **Prompt Planner：** P003 已证明 Gameplay Tags 模块依赖可加载，下一相邻门禁是建立最小 Tags 配置真源；本轮只形成八个根命名空间和导入入口。
- **Prompt Reviewer：** 活动卡必须区分 Config 文件存在、Editor 可查询和 Editor 重开持久化三层证据；未验证项不得推断完成。
- **Architect：** 使用 UE5.6 的 `GameplayTagsSettings` 配置段和 `GameplayTagList` 条目作为项目级配置，不添加 C++ Native Tags 或 Gameplay 对象。项目长期文档曾列出 `Weakness.*`，但用户本轮明确指定八项，P004 不创建 `Weakness`。
- **Safety Reviewer：** Config 修改精确限制为 `Config/DefaultGameplayTags.ini`；不得批量修改 Config，不得允许 Editor 顺带保存其他 `.ini` 或资产。若 UE5.6 实际保存需要额外文件，必须停止、报告精确路径和原因并请求扩权。

### 2. 是否适合当前 Phase

适合。P001 工程基线、P002 构建入口和 P003 Gameplay Tags 模块依赖已有审查证据；Gameplay Tags 配置是 Phase 0 尚未完成的相邻 checkpoint。P004 不依赖默认地图或 PIE，也不能替代后续地图、Editor/PIE 总门禁。

### 3. 是否需要拆分

不需要。单个配置文件、八个根 Tag、一次 Editor 查询和一次关闭重开验证可独立验收。以下内容明确推迟：

- `TASK-P0-005`：创建并设置 `Map_ProjectSetup`。
- `TASK-P0-006`：Editor 重开总验证与空白 PIE 门禁。
- `TASK-P0-007`：Phase 0 最终审查和归档。
- Gameplay 子 Tag、Native Gameplay Tags 和所有 Gameplay/GAS 实现：后续 Phase 按真实消费者逐项增加。

### 4. 本轮最小任务

创建 `Config/DefaultGameplayTags.ini`，启用从 Config 导入 Gameplay Tags，并仅声明八个根 Tag；记录 Config diff、Editor 查询结果、重复/解析错误检查以及关闭重开后的持久性结果。

### 5. 风险审查

**Blocking：**

- 用户尚未在低级模型复述后单独确认 `TASK-P0-004`。
- UE5.6 若要求修改 `DefaultEngine.ini`、`DefaultGame.ini` 或其他精确路径，当前授权不足，必须停止并申请扩权。
- 发现已有同名 Tag 来源、重复条目、解析错误，或 Editor 自动生成范围外改动时，必须保留第一处真实错误并停止。

**Risk：**

- 根 Tag 与未来子 Tag 语义不同；本轮不得提前猜测子层级。
- Editor UI 成功显示不等于磁盘配置持久化，必须关闭并重开 Editor 后复查。
- Editor 可能改写无关 Config；提交前必须检查 diff 并排除所有范围外改动。

**明确非目标：**

- 不创建 `Weakness` 或任何第九项根 Tag。
- 不修改 `HSR.uproject`、`Source/`、其他 `Config/` 文件、`Content/`、`Plugins/`、任务卡、项目状态或日志。
- 不运行构建、PIE、自动测试、缓存清理、Git push；不创建 P005。

### 6. 最终低级模型 Prompt

你是 HSR 项目的 `Implementation Agent / 低级执行模型`，本轮只执行 `TASK-P0-004`。

首次响应阶段：

1. 只能读取本活动卡；确认前不得调用任何工具。
2. 复述任务编号、当前 Phase、唯一目标、允许修改文件、禁止内容、实施步骤、用户 Editor 操作、正常/失败证据、风险和停止条件。
3. 必须以完全一致的句子 `等待用户确认执行 TASK-P0-004。` 结束，然后停止。
4. 只有用户在复述之后单独确认当前任务，才可开始执行；此前任何旧确认均无效。

获得确认后：

1. 只读检查 `Config/DefaultGameplayTags.ini` 是否存在；不得读取全局规划文档扩大范围。
2. 若文件不存在，创建它；若存在，先检查现有配置，禁止覆盖未知内容。
3. 在 UE5.6 实际支持的 `/Script/GameplayTags.GameplayTagsSettings` 配置段中启用 Config Tag 导入，并仅加入 `Ability`、`State`、`Status`、`Event`、`Damage`、`Element`、`Cooldown`、`UI` 八个根 Tag。不得添加子 Tag 或 `Weakness`。
4. 检查文件 diff，确认没有重复 Tag、无关设置或其他文件改动。
5. 指导用户启动 UE5.6 Editor，打开 `Edit > Project Settings > Project > Gameplay Tags`，搜索并确认八个根 Tag 均可查询；检查 Output Log/Message Log 中无 Gameplay Tags 解析、重复或冲突错误。
6. 指导用户正常关闭 Editor，再重新打开项目并回到同一页面，确认八个根 Tag 仍存在且可查询。
7. 不运行构建、PIE 或地图操作。若 Editor 需要修改额外 Config 文件，或出现解析/重复错误，只记录第一处真实错误并停止，不得自动修复或扩权。
8. 将真实结果写入 `tasks/execution-result.md`，明确区分：Config diff、Editor 首次查询、关闭重开持久性、错误检查、未执行项和未验证项。
9. 提交前检查 `git status`、`git diff`、允许文件和 UE 派生产物；只提交本任务允许文件，commit message 使用 `低级执行模型+Implementation Agent+TASK-P0-004/Phase 0+建立并验证Gameplay Tags配置基线`，不得 push。若 Git 权限失败，只报告并停止，不反复重试。

## 执行模型

- 低级执行模型：获得二次确认后修改允许文件、记录证据并提交。
- 用户：执行 UE Editor 查询、关闭、重开和复查，提供真实现象或日志。
- 高级审查者：执行后独立审查 Config、证据和 commit，不凭执行者自述放行。
- 当前授权状态：待低级模型复述，随后待用户单独确认。

## 执行前必须读取的文件

- 首次只读取：`tasks/active-task.md`。
- 用户确认后仅按需读取：`Config/DefaultGameplayTags.ini`（若存在）。
- 不得自行读取其他全局文档扩大上下文或权限。

## 允许修改文件

- `Config/DefaultGameplayTags.ini`
- `tasks/execution-result.md`

未列出的文件默认禁止修改。如 UE5.6 真实配置需要额外精确路径，必须停止并报告路径、必要性和预期 diff，等待高级协调者与用户扩权；不得默认批量修改 `Config/`。

## 禁止修改内容

- 禁止修改 `HSR.uproject`、`Source/`、`Config/DefaultEngine.ini`、`Config/DefaultGame.ini`、`Config/DefaultInput.ini`、`Content/`、`Plugins/` 和任何其他文件。
- 禁止创建 GameplayAbility、GameplayEffect、ASC、AttributeSet、C++、Blueprint、地图、输入、UI 或其他资产。
- 禁止添加八项以外的根 Tag、任何子 Tag 或 `Weakness`。
- 禁止构建、PIE、自动测试、清理缓存、删除文件、`git reset`、`git clean`、push 和创建 P005。
- 禁止低级模型修改 `tasks/active-task.md`、`PROJECT_STATE.md`、`todo_plan.md` 或 `worklog.md`。

## 实现步骤

1. 完成复述并等待用户单独确认；确认前零工具调用。
2. 检查目标 Config 是否存在及其真实内容，避免覆盖。
3. 创建或最小编辑 `Config/DefaultGameplayTags.ini`，启用 Config 导入并声明八个根 Tag。
4. 检查 diff 和重复项；范围扩大时停止。
5. 由用户在 Editor 中查询八个根 Tag，并检查第一处解析/重复错误。
6. 由用户关闭并重开 Editor，再次查询确认持久性。
7. 将真实结果写入 `tasks/execution-result.md`；未执行和未验证项如实记录。
8. 检查 status/diff 后创建执行者 commit，不 push。

## UE Editor 手动操作

1. 用户在低级模型完成 Config 修改后启动 UE5.6 Editor 并打开 HSR 项目。
2. 打开 `Edit > Project Settings > Project > Gameplay Tags`。
3. 分别搜索 `Ability`、`State`、`Status`、`Event`、`Damage`、`Element`、`Cooldown`、`UI`，确认八项均可查询且没有重复来源或冲突。
4. 查看 Output Log/Message Log；如有 Gameplay Tags 解析、重复或冲突错误，回传第一处完整错误并停止。
5. 正常关闭 Editor，重新打开 HSR 项目，再回到 Gameplay Tags 页面重复查询，确认八项仍存在。
6. 不启动 PIE，不创建或打开地图，不通过 Editor 新增其他 Tag，不保存无关设置或资产。

## 验收标准

- [ ] `Config/DefaultGameplayTags.ini` 是唯一 Config 改动，包含有效导入入口和且仅有八个指定根 Tag。
- [ ] Config diff 无重复、无子 Tag、无 `Weakness`、无无关设置。
- [ ] Editor 首次打开后八个根 Tag 均可查询，无未解释的解析/重复/冲突错误。
- [ ] 正常关闭并重开 Editor 后，八个根 Tag 仍可查询。
- [ ] `tasks/execution-result.md` 分别记录 Config diff、Editor 查询、重开持久性、第一处错误或无错误证据。
- [ ] 未运行构建、PIE、地图或 Gameplay 实现；未验证项保持未完成。
- [ ] 实际改动仅限允许文件，执行者 commit 符合固定格式且未 push。

## 审查清单

- [ ] 用户确认是否发生在低级模型完整复述之后？
- [ ] 八个根 Tag 是否精确，是否错误加入 `Weakness`、子 Tag 或重复来源？
- [ ] Config diff 是否只涉及 `DefaultGameplayTags.ini`，语法是否与 UE5.6 实际解析一致？
- [ ] Editor 查询和关闭重开持久性是否都有真实证据？
- [ ] 是否记录第一处解析/重复错误，且没有越权自动修复？
- [ ] 是否没有代码、Blueprint、地图、PIE、构建、清缓存、范围外 Config 或 push？
- [ ] 未验证项和 Phase 0 总门禁是否仍如实保持未完成？

## 执行后必须更新的文件

- 低级执行模型只更新 `tasks/execution-result.md`；`Config/DefaultGameplayTags.ini` 是本任务实现产物。
- 审查通过后由高级审查者/协调者按职责归档并更新 `PROJECT_STATE.md`、`worklog.md`、`todo_plan.md`（仅真实 checkpoint）和审查产物。
- 本任务不得创建 `TASK-P0-005`。

## 执行结果

- 已修改：未执行。
- 已验证：未执行。
- 未验证：八个根 Tag 配置、Editor 查询、关闭重开持久性。
- Editor 待办：待执行者完成 Config 后由用户操作。
- 阻塞/扩权请求：当前等待低级模型复述及用户单独确认。
- 审查结论：待执行和独立审查。

## Git 交付

- 角色：低级执行模型。
- 固定 commit message：`低级执行模型+Implementation Agent+TASK-P0-004/Phase 0+建立并验证Gameplay Tags配置基线`
- 提交前检查：`git status`、`git diff`、允许文件、UE 派生产物排除。
- commit hash：待执行。
- Phase 收尾 push：P004 不 push；Phase 0 全部子任务通过并归档后由协调者统一执行。
