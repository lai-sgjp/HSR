# Archived Task — PASS WITH FOLLOW-UP (2026-07-16)

> 审查结论：已核对 `tasks/execution-result.md` 与实际工程基线文件；首次 Development Editor 构建、Editor 重开和 PIE 仍未验证，作为后续独立任务处理。

# Active Task

> 状态：待用户明确授权
>
> 注意：任务卡存在不代表已经获得执行授权。任何模型不得自动开始本任务。

## Role Lock / 角色锁定

低级模型只能担任 `Implementation Agent / 低级执行模型`；未经用户明确授权或本任务卡明确授权，不得切换为高级模型、Prompt Planner、Prompt Reviewer、Architect、Code Reviewer、Safety Reviewer、Learning Reviewer、Teacher 或其他权限、职责更大的角色。不得以安全、自我反思或更好完成任务为由扩权、改写目标、扩大文件范围、改变 Phase 或验收标准。

如发现风险、歧义、上下文缺失、模型不匹配或需要高阶角色，必须停止并说明超出权限原因、所需角色、所需用户授权，并在获得授权前不修改文件。任何获授权的角色切换都必须在输出中说明触发依据和授权来源；未经授权的切换视为任务失败。

## 任务编号

`TASK-P0-001`

## 任务名称

创建 UE5.6 Blank C++ 工程基线并采集首轮证据

## 当前 Phase

Phase 0 准备阶段；Phase 0 尚未开始，门禁状态为 `Not verified`。

## 任务目标

在用户明确授权后，只创建 HSR 的 UE5.6 Blank C++ 工程基线，并取得“工程已生成、模块可被识别”的真实证据。不得在同一任务中实现 Character、Controller、GAS、战斗、存档或 UI。

## 执行模型

- 高级模型：复核边界、指导步骤、整理用户回传证据并更新文档。
- 用户：执行 UE Project Browser、IDE 和 UE Editor 中的手动操作。
- 低级模型：**本卡未指定低级模型执行，读取后必须停止并报告“执行模型不匹配且尚未授权”。**

## 执行前必须读取的文件

- 高级模型：`PROJECT_STATE.md`、`.agents/agents.md`、本文件、`docs/phase-0-project-setup.md`、`todo_plan.md`、`worklog.md` 最新记录。
- 用户：本文件中的“UE Editor 手动操作”和“验收标准”。
- 低级模型：只读取本文件；不得自行补读其他上下文。

## 允许修改文件

当前未授权，**本轮允许修改文件为空**。

用户未来明确授权后，高级模型必须先把实际允许创建/修改的精确路径写入本节；未列出的文件仍然禁止修改。不得把本段示例文字视为授权。

## 禁止修改内容

- 在用户授权前禁止创建或修改 `.uproject`、`Source/`、`Config/`、`Content/`、`Plugins/`、Blueprint 或资产。
- 禁止实现 Character、Controller、Enhanced Input Gameplay、GAS、TurnSystem、Battle、SaveGame、UMG 或下一 Phase 内容。
- 禁止删除、覆盖历史、批量移动、导入第三方资源或清理缓存。
- 禁止执行 `git reset`、`git clean`、add、commit 或 push。

## 实现步骤

1. 等待用户明确授权；未授权时立即停止。
2. 高级模型核对 UE5.6 安装、项目位置、项目名 `HSR`、Blank、C++、Desktop/Console、Maximum Quality、Starter Content 关闭等选项。
3. 把生成器预计创建的精确文件路径写入“允许修改文件”，再次确认不会覆盖现有文件。
4. 用户在 UE5.6 Project Browser 中创建工程。
5. 高级模型根据实际生成结果核对 `.uproject`、Target、Module 和 Build.cs；不追加 Gameplay 代码。
6. 用户回传工程创建结果、首个构建/启动结果和第一处真实错误（如有）。
7. 只有取得真实证据后，才更新 `PROJECT_STATE.md`、`worklog.md` 和对应 todo；未验证项保持未完成。

## UE Editor 手动操作

1. 启动 Unreal Engine 5.6 Project Browser。
2. 选择 Games → Blank → C++。
3. 目标平台选择 Desktop/Console，质量选择 Maximum Quality，关闭 Starter Content。
4. 项目名填写 `HSR`，路径确认在当前仓库根目录且不会生成嵌套的第二个 `HSR/`。
5. 点击 Create 前再次核对“允许修改文件”已经由高级模型补齐且用户已授权。
6. 创建后记录 Editor 是否成功打开、模块是否加载；如执行构建或空白 PIE，保留日志/截图并区分“已创建”“已构建”“已 PIE”。

## 验收标准

- 用户已明确授权，且实际文件没有超出更新后的允许清单。
- UE5.6 成功生成 `HSR.uproject` 和最小 C++ 模块文件，项目路径没有错误嵌套。
- 没有新增 Gameplay 类、Blueprint、GAS、战斗、存档、UMG 或第三方资源。
- 实际创建、构建、Editor、PIE 状态分别记录；未执行的验证明确标为未验证。
- 出错时保留第一处真实错误，不使用删除缓存或批量改配置掩盖问题。

## 审查清单

- [ ] 用户授权是否明确且晚于本任务卡审查？
- [ ] 允许文件是否为精确路径，且实际改动没有越界？
- [ ] 是否仍只处理 Phase 0 工程基线？
- [ ] 是否没有 Character、Controller、GAS、Blueprint、Config 批量修改或第三方资产？
- [ ] `.uproject`、Target、Module、Build.cs 是否保持最小？
- [ ] 创建、构建、Editor 和 PIE 证据是否分开记录？
- [ ] 未验证内容是否没有被标记为完成？
- [ ] 是否没有执行任何 Git 操作？

## 执行后必须更新的文件

- `tasks/execution-result.md`：低级模型只在此提交执行报告，记录实际修改、证据、未验证项和阻塞；不得修改本活动任务卡。
- `PROJECT_STATE.md`：更新当前代码状态、阻塞点、已完成/未完成和下一任务。
- `worklog.md`：记录用户授权、实际生成文件、日志、Editor 操作与验证证据。
- `todo_plan.md`：只勾选已有真实证据的 Phase 0 子项。
- `learning-journal.md`：仅在产生可复用的 UE 工程或工具链知识时更新。
- `tasks/active-task.md`：补齐结果后移动到 `tasks/archive/`，再由高级模型创建下一张活动卡。

> 职责分工：低级模型仅更新 `tasks/execution-result.md`。审查模型审查该报告、实际文件和独立证据；只有高级模型可以据审查结论更新本节列出的其他文件与归档状态。
