# TASK-P0-007 活动任务卡

> 状态：已规划，等待执行 Phase 0 最终文档审查与门禁判定。P006 已由用户明确验收并跳过独立审查；不得把该事实写成审查者结论。

## Role Lock / 角色锁定

本任务主要由 `高级模型协调者` 执行文档审查与归档。若交给低级模型，低级模型只能担任 `Implementation Agent`，首次只读取本卡，完整复述后以 `等待用户确认执行 TASK-P0-007。` 结束并停止；用户随后单独确认前不得调用工具或修改文件。

## 任务编号

`TASK-P0-007`

## 任务名称

Phase 0 阶段审查、文档归档与门禁判定

## 当前 Phase

- Phase 0：项目初始化与规范落地。
- 当前门禁：`Not verified`。
- P001-P005 已有归档审查证据；P006 执行者 commit 为 `f18269a8f056c110f2e6cf673271cbd2201e19d1`，用户已亲自确认无问题并明确跳过独立审查。
- Build/Editor 重开/默认地图/插件/Tags/空白 PIE 已有真实证据。
- 实际 C++ 标准仍无构建日志或编译命令证据，因此不得勾选完整工具链 checkpoint，不得宣布 Phase 0 通过。

## 任务目标

只审查并归档 Phase 0 文档与任务证据，如实判定门禁状态，明确剩余实际 C++ 标准补证需求；不修改工程实现，不进入 Phase 1，不自动开始补证任务。

## 规划审查摘要

### 1. 子 Agent 分析

- Prompt Planner：P007 只做文档一致性、证据追溯、阶段状态判定和任务归档。
- Prompt Reviewer：必须区分 P001-P005 独立审查、P006 用户验收和实际 C++ 标准缺证；不得伪造 P006 审查报告。
- Architect：工程文件全部只读，Phase 0 保持单 Runtime 模块，Phase 1 Gameplay 不得提前进入。
- Safety Reviewer：只允许下列文档和归档路径；禁止构建、Editor、PIE、工程文件、push 和未来补证任务实施。

### 2. 是否适合当前 Phase

适合。P001-P006 已执行并具有可追溯证据，P007 是 Phase 0 七步计划中的最终文档审查任务。当前证据不足以通过总门禁，因此本任务必须输出 `Not verified` 和精确缺口，而不是进入 Phase 1。

### 3. 是否需要拆分

P007 文档审查本身不需要拆分。实际 C++ 标准补证必须作为后续独立最小任务，由高级协调者另行规划；本任务只记录，不创建、不执行该任务。

### 4. 本轮最小任务

核对 P001-P006 证据，完成 Phase 0 文档一致性审查和归档，给出可追溯的门禁结论及唯一剩余补证项。

### 5. 风险审查

- Blocking：没有实际 C++ 标准证据，Phase 0 不能标记 Ready/完成。
- Blocking：若需要修改 C++、Config、Content 或 `.uproject`，立即停止并申请独立任务授权。
- Risk：P006 没有独立审查；只能记录用户明确验收，不能生成或暗示审查者 PASS。
- Risk：P006 本轮构建是 up-to-date；不得把 P003 历史实际编译与 P006 本轮结果合并。
- 非目标：构建、Editor、PIE、清缓存、Gameplay、Phase 1、P008/补证任务实施、push。

### 6. 最终低级模型 Prompt

你是 `Implementation Agent / 低级执行模型`，只执行 `TASK-P0-007` 的文档一致性核对。首次只读取本活动卡，不调用工具。

你必须复述任务编号、当前 `Not verified` 状态、P006 用户验收但无独立审查、允许文件、禁止工程改动、实际 C++ 标准缺证、验收与停止条件。复述最后必须逐字写：

`等待用户确认执行 TASK-P0-007。`

随后立即停止。只有用户在复述后单独确认当前任务编号，才可读取证据并修改允许文档。不得运行构建、Editor、PIE、Git push，不得创建或执行未来补证任务。

## 执行模型与职责

- 高级模型协调者：审查全局证据，更新文档、状态和归档，提交自己的角色产物。
- 低级模型：仅在用户另行交付本卡时参与，且必须遵守复述与二次确认门禁。
- 用户：确认 P006 的手动 Editor/PIE 证据；用户已明确跳过 P006 独立审查。
- 审查者：本任务不伪造 P006 审查结论；如用户未来要求，可独立审查 P007 文档一致性。

## 执行前必须读取的文件

- `PROJECT_STATE.md`
- `.agents/agents.md`
- `todo_plan.md`
- `worklog.md` 最新记录
- `README.md`
- `learning-journal.md`
- `docs/phase-0-execution-plan.md`
- `docs/phase-0-project-setup.md`
- `docs/phase-execution-workflow.md`
- `tasks/archive/TASK-P0-001-*`
- `tasks/archive/TASK-P0-002-*` 至 `tasks/archive/TASK-P0-006-*`

## 允许修改文件

- `PROJECT_STATE.md`
- `worklog.md`
- `todo_plan.md`
- `learning-journal.md`
- `README.md`
- `docs/phase-0-project-setup.md`
- `tasks/active-task.md`
- `tasks/execution-result.md`
- `tasks/archive/TASK-P0-007-active-task.md`
- `tasks/archive/TASK-P0-007-execution-result.md`
- `tasks/archive/TASK-P0-007-review.md`

未列出的文件默认禁止修改。`tasks/archive/` 只允许上列 P007 精确归档路径；不得改写 P001-P006 历史证据。

## 禁止修改与执行

- 禁止修改 `HSR.uproject`、`Source/`、`Config/`、`Content/`、`Plugins/`、Target、Build.cs 或地图。
- 禁止运行构建、Editor、PIE、Clean/Rebuild、清缓存、reset、clean 或 push。
- 禁止创建 Gameplay 类、Blueprint、Input、UI、GAS 对象、第三方资源或 Phase 1 内容。
- 禁止把实际 C++ 标准从默认设置推断为已验证。
- 禁止创建或自动开始 P008/补证任务。

## 实现步骤

1. 只读核对 P001-P006 归档、Git 提交和实际文件树。
2. 核对 todo 每个 Phase 0 checkpoint 的证据来源。
3. 更新 README、Phase 0 文档、worklog、learning journal、todo 和项目快照，使其只陈述真实结果。
4. 明确记录 P006 是用户验收且跳过独立审查。
5. 将实际 C++ 标准保持未验证，Phase 0 保持 `Not verified`，写出未来独立补证需求但不创建任务。
6. 归档 P007 活动卡和执行结果；只有取得足够证据时才能更改阶段状态。
7. 检查 diff 仅包含允许文档后提交；不得 push。

## UE Editor 手动操作

无。P007 不运行或操作 Editor；P006 的用户证据只作为历史输入。

## 验收标准

- [ ] P001-P006 证据均可追溯，P006 用户验收与独立审查被准确区分。
- [ ] Build/Editor/PIE checkpoint 与实际 C++ 标准 checkpoint 分开判定。
- [ ] README、Phase 0、worklog、learning journal、todo 和 PROJECT_STATE 内容一致。
- [ ] 无 Gameplay 类或资源的判断来自 Git 树和归档证据。
- [ ] Phase 0 状态如实为 `Not verified`，剩余补证项明确。
- [ ] 只修改允许文档，不进入 Phase 1，不创建或执行未来补证任务。

## Git 交付

- 角色：高级模型 / 协调者
- 固定 commit message：`高级模型+协调者+TASK-P0-007/Phase 0+归档运行门禁并判定阶段状态`
- 提交前检查：status、diff、允许文档、无工程文件和派生产物。
- Phase 收尾 push：当前禁止；Phase 0 总门禁尚未通过。
