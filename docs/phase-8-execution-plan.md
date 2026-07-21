# Phase 8 详细执行计划：弱点、韧性、击破与行动延后

状态：`READY WITH INHERITED FOLLOW-UPS / REMOTE DELIVERY COMPLETE`（P8-006 Reviewer `PASS WITH FOLLOW-UP`；`main` 与 `origin/main` 同步于 `2c2abc2`）
日期：2026-07-21
适用版本：UE 5.6 / HSR Runtime / GAS

## 1. 阶段目标与唯一可见结果

Phase 8 只交付一个可独立验证的 1v1 垂直闭环：

> 带有 Element 的一次合法攻击命中带 Weakness 的目标，经现有 Phase 7 统一伤害管线削减 Toughness；Toughness 恰好归零时只生成一次 BreakResult（含击破伤害与最小原创击破 Debuff 结果），TurnManager 按确定性规则延后目标行动；重复命令、过量削韧、死亡同帧和终局均无重复副作用；UI/Debug 只读显示，不拥有规则。

本阶段不把“代码存在”“计划完成”当作证据。每项结论必须标注 `AGENT VERIFIED`、`USER PROVIDED`、`STATIC REVIEW` 或 `NOT VERIFIED`。

## 2. 当前状态与 Gate 0

仓库快照显示：Phase 7 的 P7-001～P7-004 已以 `PASS WITH FOLLOW-UP` 归档；唯一活动任务为 `TASK-P7-005`，仍处于等待用户独立确认执行。Phase 8 因此先处于“计划就绪、实施未开始”。

Gate 0 必须先完成：

1. 用户确认 P7-005 的执行范围。
2. Teacher 提交原始教学/学习记录。
3. Independent Reviewer 提交结论（保留 `REVISE` 或 `PASS WITH FOLLOW-UP`，不得改写历史）。
4. Coordinator 将三件套与 P7 follow-up 归档，并更新 `todo_plan.md`、`worklog.md`、`learning-journal.md`。
5. 记录 P7 未验证项：Aggregator NaN、GAS ApplyFailure/Refund、Cost 后 HP、teardown、目标销毁/异步、Save/网络等。

Gate 0 未满足时，Phase 8 只允许设计、审查和文档准备，不允许将新 Gameplay 代码标为阶段成果。

## 3. 架构不变量（执行前冻结）

- 继续使用现有 `UHSRCoreAttributeSet`；不得未经授权新建或默默迁移 `CombatAttributeSet`。
- GameplayTag 是规则标签，不是显示文本；Element/Weakness Definition 为静态 DataAsset，Toughness/Delay 为运行时状态。
- GAS/GE/Execution 负责效果、削韧、击破伤害与结果数据；Ability 只组装命令上下文，不直接写 Toughness、HP 或全局回合队列。
- TurnManager 是行动顺序与 Delay 的唯一所有者；Delay 通过公共值命令/事件提交，不能由 GameplayCue 或 Ability 直接重排。
- Break 采用 ActionId/目标生命周期幂等；终局、目标销毁、Reset 后拒绝迟到结果。
- 无 Tick 扫描；UI 通过 Attribute/事件委托更新并在 teardown 时解绑。
- 本阶段不承诺网络权威、复制、性能基线或 SaveGame 持久化。

## 4. 工作包与执行顺序

### P8-000：现状基线与设计冻结

目标：完成 Gate 0 复核，冻结元素、弱点、Toughness、Break、Delay 语义和未决选择。
输入：P7 归档、当前 `UHSRCoreAttributeSet`、现有 Damage Breakdown/TurnManager 接口。
产物：设计决策表、Tag 生产/消费表、状态转移图、P7 follow-up 清单。
停止条件：无法证明 Phase 7 入口、AttributeSet 归属或终局语义时停止，不写新代码。

### P8-001：Element 契约与静态定义

目标：建立原创 Element GameplayTags、Element Definition DataAsset、缺失/未知元素的失败语义。
建议白名单：`Source/HSR/HSRCoreAttributeSet.*`、`Source/HSR/HSRBreakTypes.h`、`Source/HSR/HSRSkillDefinition.*`、现有敌人/战斗参与者 Definition 文件、`Config/DefaultGameplayTags.ini`，以及经授权的 `Content/Data` 定义资产。
禁止范围：抗性矩阵、网络复制、SaveGame、通用状态系统。
验收：匹配、不匹配、空集、重复和未知标签均有明确结果且零副作用；Editor 重开后引用保持。

### P8-002：Weakness 匹配与 Toughness 属性

目标：让目标配置 WeaknessTags，并在现有 Core AttributeSet 中加入 Toughness/MaxToughness（或经授权采用等价现有字段）。
规则：初始化、Clamp、负值/过量削韧、击破后的恢复/重置策略必须先冻结；运行时数值不写回静态 Definition。
GAS：使用 GE/Meta 属性作为削韧入口，Execution 输出可审计的韧性变化，不由 Ability 直接 Set。
验收：正值、零值、负值、恰好归零、过量和非法输入矩阵全部可复现。

### P8-003：Break Resolution 垂直闭环

建议白名单：新增 `Source/HSR/HSRBreakResolution.*`（纯计算、无 CDO 状态），最小扩展 `HSRBattleCoordinator.*` 与现有 Damage/Resolution 类型；如采用 GAS 击破伤害，使用经授权的 `BP_GE_P8_BreakDamage`，复用现有 `IncomingDamage` 入口。
目标：将“校验 → 弱点匹配 → 削韧 → 归零判定 → 唯一 BreakResult → 击破伤害/最小 Debuff 结果”接入现有 Command/Coordinator/Resolution 链。
不变量：同一 ActionId 不得二次削韧；Toughness<=0 不得重复 Break；死亡同帧必须有冻结的 HP/Break 优先级；终局和目标销毁产生零副作用。
验收：BreakResult 与 Phase 7 Damage Breakdown、HP、Toughness、Turn 结果可逐项对账。

### P8-004：Turn Delay 所有权与确定性排序

建议白名单：`Source/HSR/HSRTurnManager.*` 及其 battle-local 纯值事件/命令；可采用每参与者 `DelayScore`，但必须先冻结“加值、消费/衰减、重新排序”的精确定义。
目标：TurnManager 消费合法 Break/Delay 值并稳定重排行动顺序。
必须冻结：Delay 单位、边界、最小/最大值、同值 tie-break、连续击破、死亡目标和 Reset 重建策略。
禁止：Ability、GE、Cue 直接操作全局队列；任何 Tick 扫描。
验收：并列排序、连续延后、死亡目标、终局和 Reset 均可由 ActionId/日志解释。

### P8-005：只读 UI/Debug 与收尾

目标：显示 Element、Weakness、Current/Max Toughness、Break 和 Delay 结果；补充最小 Debug/日志观察点。
规则：UI 不计算规则、不持有运行时真源、不使用 Tick；生命周期解绑可验证。
验收：两轮连续 PIE、Editor 重开、主路径与失败路径；UI 缺失不得改变战斗规则。

### P8-006：独立验收与归档

目标：由执行者、教师、审查员和协调者分别提交真实产物，完成阶段门禁。
必须归档：fresh Development Editor Build（UHT/C++/Link）、PIE 主/失败路径、资产/Tag 引用检查、回归结果、未验证项、学习记录、`worklog.md`/`todo_plan.md` 更新。
只有全部证据齐全后，才可将 Phase 8 标记为 Ready 并建议 Phase 9。

## 5. 教学与协作循环

每个工作包按“直觉类比 → UE/GAS 真实机制 → 项目映射 → 学员手算/口述 → 最小实现 → Debug → 教师提问 → learning-journal 记录”执行。核心问题包括：为何 Tag 不等于公式？为何使用 Meta Incoming Toughness Damage？为何 Ability 不能推队列？重复 ActionId 如何保证幂等？UI 如何证明只读无 Tick？

角色边界：协调者维护范围和证据链；执行者提交实际代码/Editor 说明；教师提交原始教学与掌握度；审查员独立检查并保留缺口。当前请求的执行者模型 `gpt-5.5-terra` 在环境中不可用，实际使用兼容的 `gpt-5.6-terra`，不改变职责边界。

## 6. 证据矩阵

| 主题 | 必须证据 | 失败路径 |
|---|---|---|
| Element/Weakness | Tag/资产引用、匹配矩阵、Editor 重开 | 缺失/未知/重复标签零副作用 |
| Toughness | 初始化、Clamp、GE/Execution 日志 | 负值、过量、非法输入 |
| Break | 唯一 BreakResult、HP/Toughness 对账 | 重复 ActionId、重复击破、死亡同帧 |
| Turn Delay | TurnManager 日志、稳定 tie-break | 连续延后、死亡目标、终局、Reset |
| UI/Debug | 委托更新、teardown、无 Tick 静态审查 | UI 缺失仍保持规则正确 |
| 工程回归 | Fresh Build、两轮 PIE、Phase 1–7 回归 | 首次失败日志必须保留 |

## 7. 非目标与风险

明确排除：Phase 9 通用 Buff/Debuff 生命周期、叠层/免疫/驱散、Phase 10 完整战斗 UI、AoE/多段攻击、网络/复制、SaveGame、复杂表现资产、第三方资源、性能优化和大规模重构。

主要风险：当前路线图名称与实际 AttributeSet 不一致；P7 follow-up 可能污染新结论；Toughness 恢复策略、Break 与死亡优先级、Delay 单位尚未被真实证据冻结。任何风险未决时，回退到设计/审查，不跨越门禁实施。

## 8. 阶段完成定义

P8-000～P8-006 已完成 Gate、closeout 与远端交付，证据等级和 inherited follow-ups 原样保留。用户已于 2026-07-21 明确授权进入 Phase 9；该授权只启动相邻的规划/契约工作，不把 Phase 9 Gameplay 记为已实施。
