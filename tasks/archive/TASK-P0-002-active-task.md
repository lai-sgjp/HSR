# Active Task — TASK-P0-002

> 状态：低级执行者已完成执行，等待高级审查者审批。
> 本卡当前不允许重复构建；审查者必须先核对 `tasks/execution-result.md` 与真实证据。

## Role Lock

低级模型只能承担 `Implementation Agent / 低级执行模型`。不得切换为 Planner、Reviewer、Architect、Teacher 或其他更高权限角色；发现错误、歧义或需要扩大范围时必须停止并报告。

## 任务目标

只验证 `HSREditor Development Win64` 的首次构建链路，并记录真实构建证据；不修复尚未诊断的问题，不推进插件、Gameplay Tags、地图、PIE 或任何 Gameplay 功能。

## 前置与审查结论

- `TASK-P0-001` 已归档，审查结论为 `PASS WITH FOLLOW-UP`。
- Phase 0 仍为 `Not verified`；本任务只处理工具链构建证据。
- Prompt Planner、Prompt Reviewer、Architect、Safety Reviewer 均确认单一目标、无功能改动、失败即停止。

## 执行模型

- 高级模型维护边界、接收证据并在审查后更新状态。
- 用户在 IDE/UE 环境中发起构建并回传命令、日志、退出码和第一处错误（如有）。
- 低级模型仅在用户确认后执行本卡允许的只读核对，并填写执行报告。

## 允许修改文件

- `tasks/execution-result.md`（仅执行后填写真实报告；不得修改其他文件）

## 禁止修改

- 禁止修改 `HSR.uproject`、`Source/`、`Config/`、`Plugins/`、`Content/`、Target、Build.cs、SDK 或缓存。
- 禁止清理、删除、覆盖文件和无关 Git 操作；只允许按本卡“Git 交付要求”完成角色 commit，当前禁止 push。
- 禁止创建 Character、Controller、GAS、Gameplay Tags、Blueprint、地图、UI、PIE 流程或其他 Phase 0 子任务。

## 实施步骤

1. 低级执行者已完成用户确认后的只读核对和构建证据采集。
2. 审查者核对 `tasks/execution-result.md`、真实构建日志、退出码和确认链。
3. 审查者给出 `PASS`、`PASS WITH FOLLOW-UP`、`REVISE` 或 `BLOCKED`。
4. 审批通过后，按 Git 交付规则提交角色产物，归档本卡，再由高级模型生成下一张活动卡。

## UE/Editor 操作

本任务不要求 Editor 重开、地图配置或 PIE；用户只需执行一次 Development Editor 构建并回传证据。

## 验收标准

- [ ] 用户已单独确认执行 `TASK-P0-002`。
- [ ] 目标明确为 `HSREditor Development Win64`，无功能代码改动。
- [ ] 真实构建命令、工具链版本、退出码和日志证据已记录。
- [ ] 成功或第一处真实错误已明确；未验证内容保持未验证。
- [ ] 未执行清理、删除、覆盖、Git 或范围外修改。

## 当前执行结果

- 低级执行者已完成；结果见 `tasks/execution-result.md`。
- 当前审批状态：等待高级审查者独立核验。
- 当前未完成：角色 commit、审查结论、任务卡归档和 `TASK-P0-003` 创建。

## Git 交付要求

- 低级执行者任务结果提交格式：`低级执行模型+Implementation Agent+TASK-P0-002/Phase 0+记录工具链构建证据`
- 审查者审批结果提交格式：`高级模型+审查者+TASK-P0-002/Phase 0+完成构建证据独立审批`
- 协调者状态归档提交格式：`高级模型+协调者+TASK-P0-002/Phase 0+同步状态并归档任务`
- 每个角色提交前必须检查 diff、排除 `Binaries/`、`Intermediate/`、`Saved/`、`.vs/` 和其他派生产物，记录 commit hash。
- Phase 0 全部子任务完成并通过最终门禁后，另行创建阶段收尾 commit 并推送远端；当前不得 push。
