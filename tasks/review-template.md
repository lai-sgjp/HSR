# Task Review Template

## 审查对象

- 任务编号：【TASK-编号】
- 任务名称：【任务名称】
- 审查模型：【模型/角色】
- 审查日期：【YYYY-MM-DD】

## 审查输入

> 必须读取 `tasks/execution-result.md` 作为低级模型执行报告，并将其与活动任务卡、实际改动、日志、截图或用户回传交叉核对。该报告本身不能替代独立验证证据。

- `tasks/active-task.md`
- 任务卡允许范围内的实际改动文件
- 编译、测试、PIE、Editor、日志或截图证据
- 必要时读取 `PROJECT_STATE.md` 与 `.agents/agents.md`

## 范围审查

- [ ] 只实现一个可独立验收的目标。
- [ ] 所有改动均在允许文件清单内。
- [ ] 没有跨 Phase、无关重构、删除、覆盖或批量修改。
- [ ] 没有未授权 Config、Blueprint、资产、第三方资源或 Git 操作。

## 实现审查

- [ ] 文件职责和数据流与任务卡一致。
- [ ] UE 反射、模块依赖、生命周期和 GC 安全符合项目规则。
- [ ] GAS、TurnSystem、UI、DataAsset/Runtime/SaveGame 边界适用时已检查。
- [ ] 没有不必要 Tick、隐藏 Blueprint 核心规则或编造 UE5.6 API。
- [ ] 正常路径和至少一个失败/边界路径已处理。

## 证据审查

- [ ] “已修改”“已编译”“已测试”“已 PIE”“已人工验证”已分开陈述。
- [ ] 证据来自实际日志、文件或用户回传，不只来自执行模型自述。
- [ ] 第一处真实错误和未验证项均被保留。
- [ ] todo 没有把计划或代码存在误标为功能完成。

## 文档一致性

- [ ] `PROJECT_STATE.md` 与实际项目状态一致。
- [ ] `worklog.md` 记录目标、决策、改动、证据、Editor 操作、风险和后续。
- [ ] `todo_plan.md` 只记录真实进度。
- [ ] 有可复用知识时已更新 `learning-journal.md` 或专项文档。

## 问题清单

| 严重度 | 文件/证据 | 问题 | 必须采取的动作 |
|---|---|---|---|
| Blocking / Risk / Optional | 【路径或证据】 | 【问题】 | 【修复、补证据或接受风险】 |

## 审查结论

选择一个：

- `PASS`：范围、实现和证据均满足任务卡，可以归档。
- `PASS WITH FOLLOW-UP`：本任务可归档，但有不阻断的相邻后续任务。
- `REVISE`：需要在原任务范围内修正或补充证据。
- `BLOCKED`：缺少授权、前置条件或必须扩权，当前不得继续。

结论说明：【简要说明】

## Git 交付审查

- [ ] 执行者、审查者和协调者均使用自己的角色/人格提交，没有冒名提交。
- [ ] commit message 符合 `角色+人格+当前任务与阶段+简短告诉自己做了什么`。
- [ ] commit 只包含任务允许范围和对应审查/状态产物。
- [ ] 没有提交 `Binaries/`、`Intermediate/`、`Saved/`、`.vs/`、DerivedDataCache 或未经授权资源。
- [ ] Phase 未收尾前没有 push；Phase 完成后由协调者记录阶段收尾 commit、远端、分支和 push 结果。
- [ ] commit hash 或 push 错误已记录。

## 归档与下一步

> 当结论为 `PASS` 或 `PASS WITH FOLLOW-UP` 时，审查模型必须先同步 `PROJECT_STATE.md`，再将当前 `tasks/active-task.md` 与同任务编号的 `tasks/execution-result.md` 移入 `tasks/archive/`；仅在此后才能由高级模型创建下一张活动卡。低级模型不得执行这些操作。

- 活动卡归档路径：`tasks/archive/【TASK-编号】-【slug】.md`
- 项目快照是否已更新：【是/否】
- 唯一下一任务建议：【不自动执行】
