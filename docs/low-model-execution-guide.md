# HSR 低级模型执行指南

## 适用场景

低级模型适合执行边界明确、文件少、可以独立验证的任务，例如文档归档、一个小型 C++ 类型、一个明确的编译错误修复或一项 Editor 配置说明。它不适合自行拆解大型系统、设计跨 Phase 架构或在没有证据时猜测 UE5.6 API。

实际 Prompt 使用 [低级模型任务模板 A/B/C](low-level-model-task-templates.md)：

- A：纯 Markdown；
- B：1–5 个文件的小型 C++ 功能；
- C：有明确现象、错误证据和复现路径的 Debug。

## 交付低级模型前

1. 由 Prompt Planner 说明目标和成功标准；
2. 明确目标 Phase、允许修改文件和禁止修改文件；
3. 替换全部任务占位符；
4. 由 Prompt Reviewer 检查范围、前置条件、测试和文档要求；
5. 涉及 C++ 时完成 Architect/Code Reviewer 的最小职责审查；
6. 涉及删除、覆盖、Git、第三方资源或批量修改时完成 Safety Reviewer 审查。

## 唯一上下文入口

低级模型启动后只读取 `tasks/active-task.md`，并立即按卡片执行或停止：

- 不自行读取 `PROJECT_STATE.md`、`.agents/agents.md`、`todo_plan.md`、`worklog.md`、Phase 文档或专项设计文档来补充隐含上下文。
- 高级模型必须在交付前把必要规则、数据流、证据、允许文件和禁止事项完整写入活动任务卡。
- 实现过程中只能读取和修改活动任务卡明确列出的目标文件；读取目标文件是实施需要，不代表可以把其他全局文档引入上下文。
- 卡片有占位符、授权不明确、执行模型不匹配、必须读取/修改清单外文件或缺少 UE5.6 证据时，低级模型必须停止并请求高级模型重写任务卡。
- 任务卡与聊天冲突时不得自行选择；停止并由高级模型根据用户当前要求修正文件。

## 执行模型必须先说

- 任务编号、执行模型是否匹配以及是否已有用户授权；
- 本轮目标和完成标准；
- 将创建/修改的明确文件及其职责；
- 不修改的文件、系统和下一 Phase；
- 最小数据流；
- Editor 手动操作；
- 编译、测试和失败路径。

## 固定禁止项

- 不删除文件、不覆盖历史、不做无关重构；
- 不执行 `git reset`、`git clean`，不自动 commit/push；
- 不批量修改 Config，不导入大量资源；
- 不把核心逻辑写入 Blueprint、Widget 或一个巨型 Character；
- 不保存 Actor、ASC、Widget、Ability 实例或 Active GE Handle 到 SaveGame；
- 不以删除缓存、关闭警告或吞掉错误伪装修复；
- 不编造 UE5.6 API。
- 不自行更新 `PROJECT_STATE.md`、agents、todo 或 worklog；除非这些文件被活动任务卡逐项列入允许修改清单。

## 完成后的强制报告

报告必须分开写：

1. 已修改内容；
2. 编译和测试实际结果；
3. Editor 需要用户完成的操作；
4. 反射、GAS、GC、Tick、UI、DataAsset/SaveGame 检查；
5. 未验证内容；
6. worklog、todo、learning journal 和专项文档更新；
7. 只建议一个相邻小任务。

“代码写完”不等于“功能完成”；“编译通过”也不等于“PIE 和失败路径通过”。
