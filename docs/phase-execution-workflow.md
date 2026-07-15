# HSR 阶段执行与“下一步应该做什么”流程

## 1. 文档定位

本文件是 HSR Phase 0–20 的通用执行流程和下一步判断依据。它服务于学习和作品集，不是强制自动化脚本；用户当前回合的明确要求、文件安全限制和实际 UE5.6 证据优先于本文件。

详细路线仍以 [`phase-roadmap-0-20.md`](phase-roadmap-0-20.md) 为阶段内容真源；本文件只规定每个阶段如何推进、如何验收以及何时推荐下一步。

## 2. 每个阶段的通用循环

### 2.1 读取与确认

开始前读取：

1. `.agents/agents.md`；
2. `todo_plan.md`；
3. `worklog.md`；
4. `README.md`；
5. `docs/phase-roadmap-0-20.md`；
6. 当前 Phase 文档和相关专项文档。

确认当前状态属于：规划、实施中、已编译但未运行、已通过 PIE，还是阶段完成。不能把计划、代码草稿或资产存在误判为完成。

### 2.2 范围与门禁

- 每轮只选择一个可独立验收的小目标。
- 写出明确的成功标准、允许修改的文件和明确不修改的内容。
- 检查前置 Phase、模块依赖、Editor 手动操作、测试环境和授权条件。
- 如果需要跨 Phase、跨模块、批量资源或危险 Git 操作，先停下并请求授权。
- 可选的学习或表现层工作必须标记为“Optional”，不能阻塞 MVP 门禁。

### 2.3 设计与审查

实施前说明：

- 谁发起请求、谁验证、谁拥有 Runtime 状态、谁接收结果；
- C++、Blueprint、DataAsset、Runtime、SaveGame 的边界；
- GAS 与 TurnSystem 的边界；
- UE Editor 由用户手动完成的操作；
- 反射、GC、Tick、UI、版权和未来联机风险。

复杂任务可使用 Loop Engineering：Planner → Prompt Reviewer → Architect → Implementation → Code Reviewer → Learning/Safety Review。小任务不必模拟全部角色，但必须保留相同的范围和证据门禁。

### 2.4 实施与 Editor 协作

- Codex 负责获得授权的 C++ 和 Markdown 修改。
- 用户负责 UE Editor 中的派生蓝图、资产创建、引用绑定、Gameplay Tags、UMG 布局、动画绑定、地图摆放和人工观察。
- Codex 必须逐项说明 Editor 操作的目的、位置、依赖和验证方式。
- 蓝图只承担配置、派生、动画、UI 表现和资源绑定；核心规则不隐藏在蓝图中。

### 2.5 验证与归档

每阶段至少记录：

1. 相关 Target 的编译结果；
2. UHT/UBT/Link 是否有错误；
3. 一条 PIE 主路径；
4. 一条失败或边界路径；
5. 资产、输入、Gameplay Tags 和蓝图引用检查；
6. 尚未验证的 Editor、性能、授权或网络内容；
7. worklog 证据、todo 状态、learning journal 知识和阶段文档总结。

只有完成这些证据后，才可以把阶段门禁标记为通过，并推荐相邻的下一项任务。

## 3. “我下一步应该做什么？”判断模板

回答时按以下顺序：

1. **当前阶段**：引用 todo、worklog 和编译/PIE 证据。
2. **门禁状态**：Ready、Blocked、Not verified 或 Optional。
3. **唯一下一任务**：只推荐一个最小可执行目标。
4. **Codex 工作**：明确文件路径和职责；没有授权则写“无需修改”。
5. **用户 Editor 工作**：逐项列出手动操作。
6. **验证证据**：编译、测试、PIE、截图、日志或错误信息。
7. **文档归档**：worklog、todo、learning/design 文档需要记录什么。
8. **非目标与风险**：明确本轮不做什么。

## 4. Phase 0–20 阶段推进矩阵

| Phase | 当前阶段目标 | 通过门禁后最合理的下一步 |
|---|---|---|
| 0 | Blank C++、工具链、单 Runtime 模块、插件/Tags 基线和文档 | 进入 Phase 1，从零搭建第三人称探索角色 |
| 1 | Character、Controller、Enhanced Input、Camera 和探索 HUD | 进入 Phase 2，接入最小 ASC/AttributeSet |
| 2 | ASC、Owner/Avatar、属性初始化 GE、Attribute Delegate | 进入 Phase 3，建立一个共享交互接口/组件 |
| 3 | 一个灰盒交互对象通过 Interface/Component 工作 | 设计 Phase 4 的 EncounterDefinition 和稳定 Context |
| 4 | 敌人探索表现、Encounter 请求和独立 Battle Map 消费入口 | 进入 Phase 5，重建 Battle Actor 并完成 1v1 回合闭环 |
| 5 | TurnManager、基础攻击、胜负、固定奖励和返回探索 | 进入 Phase 6，增加正式 Ability/目标/资源边界 |
| 6 | 普攻/战技/终结技占位、Cost、Energy、战技点和 Target | 进入 Phase 7，统一伤害公式和 Breakdown |
| 7 | Attack/Defense/Crit、ExecutionCalculation 和可解释伤害 | 进入 Phase 8，加入原创弱点/韧性/击破 |
| 8 | Toughness、Break Debuff、击破伤害和 Turn Delay | 进入 Phase 9，建立回合状态效果生命周期 |
| 9 | Buff/Debuff、叠层、免疫、驱散和持续 N 回合 | 进入 Phase 10，完善事件驱动战斗 UI |
| 10 | 战斗 ViewModel、行动条、技能/目标/状态和结果页 | 进入 Phase 11，建立角色定义与最小成长存档 |
| 11 | CharacterData、等级/经验、队伍和 Save DTO | 进入 Phase 12，接入装备/遗器来源效果 |
| 12 | 装备/遗器实例、属性来源追踪、套装和可逆 GE | 进入 Phase 13，建立库存与幂等奖励事务 |
| 13 | Item、Inventory、Reward 和 Drop 的统一事务入口 | 进入 Phase 14，建立任务/对话事件流 |
| 14 | Quest/Dialogue 定义、目标推进、分支和奖励 | 进入 Phase 15，整合地图/传送/返回上下文 |
| 15 | 地图、传送点、区域状态和跨地图流程 | 进入 Phase 16，完成版本化存档与迁移 |
| 16 | Save/Load、校验、备份、迁移和失败恢复 | 进入 Phase 17，统一 UI 屏幕栈和输入模式 |
| 17 | 主菜单、暂停、角色、背包、任务、地图和设置导航 | 进入 Phase 18，逐项导入合法表现资产 |
| 18 | 动画、音频、VFX、GameplayCue 和授权台账 | 进入 Phase 19，建立 Debug/自动化证据 |
| 19 | 快照、GM、日志、自动化测试和问题复盘 | 进入 Phase 20，锁定干净存档垂直切片 |
| 20 | 完整原创垂直切片、性能基线和作品集材料 | 只做展示、修复和后续版本规划，不无边界扩张 |

## 5. 特殊边界

### 5.1 第一轮、第一周和第一月

- 第一轮：只维护 Markdown 和项目规则，不等于 Phase 0 已开始。
- 第一周：只完成 Phase 0 工程初始化与验证。
- 第一月：只覆盖 Phase 0、Phase 1、Phase 2 最小 GAS 和 Phase 3 单对象交互；不开始 Phase 4 代码。

### 5.2 MVP 与 Phase 20

MVP 是 1v1 探索、Encounter、独立 Battle Map、基础攻击、固定奖励、返回和最小 Save。Phase 20 是三名角色、Boss、任务、成长、装备、状态和完整展示的垂直切片；二者不能混用验收标准。

### 5.3 可选 Skill

`.agents/skills/phase-next-steps/` 只提供建议。它不能替代用户授权、不能自动推进 Phase、不能自动修改文件，也不能把“下一步建议”写成已完成状态。
