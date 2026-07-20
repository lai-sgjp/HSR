# PHASE6 详细执行计划：GAS 技能、目标与资源事务

## 1. 阶段定位与当前门禁

PHASE6 的唯一阶段级可见结果是：玩家在 1v1 Battle Map 中可通过最小命令面板选择普攻、战技、终结技占位和治疗，系统依据稳定 ID 重新验证行动者、技能和目标，正确处理 Energy 与队伍战技点，并以结构化 Resolution 区分成功、取消、拒绝和执行失败；任何无效行动都不改变 HP、Energy、战技点或回合状态。

当前 Gate 为 **Ready with inherited follow-ups**：

- P5-001～P5-004 已有 Participant/ASC 重建、确定性 TurnManager、固定伤害普攻、Victory/Defeat、BattleResult exactly-once 与返回 Transform 的提交和用户 PIE 证据。
- `TASK-P5-005` 已完成归档，Phase 5 为 `Ready with inherited follow-ups`；P6-001～P6-004A 已完成各自 Reviewer `PASS WITH FOLLOW-UP` 并归档，P6-004A 已闭合真实 WBP 阻断。
- `README.md`、`todo_plan.md` 与 `docs/phase-5-execution-plan.md` 仍有滞后状态；必须在门禁包内按真实证据校准。
- `git status` 报告 `HSRBattleCoordinator.cpp` 与 `HSRPlayerController.cpp` 修改，但普通 `git diff` 为空；先按 stat/index 噪声或真实修改分类，不回退、不覆盖、不纳入无关提交。

P6-001～P6-004A 已产生 Gameplay 与用户 Editor/PIE 证据，P6-005 已完成 Coordinator/Teacher/Independent Reviewer 收尾。阶段 Reviewer 最终为 `PASS WITH FOLLOW-UP`；动态证据主要为 `USER PROVIDED`。

权威阶段依据为 `docs/phase-roadmap-0-20.md` 的 Phase 6；旧 `.agents/agents.md` 中七阶段概览仅为历史粗粒度分期，不用于替代 0～20 路线图的 Phase 编号。

## 2. 阶段不变量与职责边界

### 2.1 行动与结算

- 每个 Command 必须有 battle-local、唯一且可追踪的 `ActionId`；重复提交和重复回调最多产生一个最终 Resolution。
- 唯一管线为：`SkillDefinition → 可用技能 → 合法目标 → Command → Coordinator/Turn/ASC 校验 → 资源预留或 GAS Cost → Ability/GE → Resolution → 资源提交或回滚 → Turn 推进`。
- 只有 `Succeeded` Resolution 可以提交战技点事务并推进 Turn；`Rejected`、`Cancelled`、`Failed` 均保持资源和 Turn 不变。
- 必须在产生不可逆副作用前完成行动者、回合、目标、Definition、Ability Spec 和资源校验；禁止“GE 已生效但 ResolveAction 失败”的半完成状态。
- Battle 终局优先于下一回合推进；致死行动只能产生一次 BattleResult，不得在终局后再次提交资源或推进 Turn。

### 2.2 所有权

| 数据/行为 | 唯一所有者 | 明确禁止 |
|---|---|---|
| Skill 静态配置 | `UHSRSkillDefinition` DataAsset | 保存当前 Actor、ASC、Target 或战技点运行值 |
| Actor 私有 Energy | ASC + `UHSRCoreAttributeSet`，由 GAS Cost/GE 修改 | Coordinator 或 UI 直接加减属性 |
| 队伍战技点 | Battle-local Coordinator Resource State | 放入 AttributeSet、DataAsset、SaveGame 或 Widget |
| 当前回合与推进 | `UHSRTurnManager` | Ability、GE、UI 直接推进回合 |
| Command/Target 权威复核 | BattleCoordinator | 信任 UI 传入的 Actor/ASC 或候选列表 |
| Ability 规则与 GE 应用 | GAS Ability/ASC | Ability 决定全局回合或跨 World 状态 |
| 展示与输入 | UMG/ViewState | Widget 写属性、Apply GE、持有规则真源或使用 Tick 轮询 |
| 最终 Resolution | Coordinator 统一汇聚 | 每个 Ability 自定义互不兼容的 bool/日志结果 |

### 2.3 目标与生命周期

- Command 只携带稳定的 Battle、Participant、Skill、Target 与 Action ID；Coordinator 在执行时解析 Actor/ASC 并重新校验。
- PHASE6 的最小目标集合仅为 `SingleEnemy`、`SingleAlly`、`Self`；多目标、随机目标、AoE 与复活延期。
- 不把现有 `SetPendingTarget(TWeakObjectPtr<ASC>)` 扩展为长期或异步协议；跨帧需要的上下文必须以受控 Runtime 对象或 GAS TargetData/Event 表达，并在激活前复核有效性。
- Definition、Ability/GE Class 引用必须受反射/GC 保护并在初始化阶段验证；不得在 Ability 激活热路径中反复同步加载软类。
- Reset、Actor 销毁、战斗结束和地图旅行必须清除 reservation、pending command、delegates 与 runtime 引用。
- 全阶段不得新增业务 Tick。

## 3. 公共类型与失败语义草案

最终名称以 P6-001 的实现审查为准，但职责在实施前冻结：

- `EHSRSkillCategory`：BasicAttack、Skill、Ultimate、Heal。
- `EHSRTargetType`：SingleEnemy、SingleAlly、Self。
- `EHSRAbilityResolutionStatus`：Succeeded、Rejected、Cancelled、Failed。
- `EHSRAbilityFailureReason`：至少覆盖 InvalidBattle、DuplicateAction、NotCurrentActor、DefinitionMissing、AbilityUnavailable、InvalidTarget、InsufficientEnergy、InsufficientSkillPoint、CommitFailed、EffectFailed、BattleFinished。
- `FHSRBattleActionCommand`：Battle/Request ID、ActionId、ActorParticipantId、SkillId 或 Spec Handle、TargetParticipantIds。
- `FHSRAbilityResolution`：ActionId、状态、失败原因、Actor/Skill/Targets、资源前后快照、是否推进 Turn，以及仅用于诊断的简短消息。
- `FHSRTeamResourceState`：当前/最大战技点；battle-local，不保存。
- 战技点 reservation：具有唯一 token/ActionId，支持幂等 `Reserve → Commit | Rollback`，且所有 early return 都能安全回滚。

日志至少关联 `RequestId/BattleId`、`ActionId`、Actor、Skill、Targets、Resolution、失败原因、Turn before/after，以及资源 `Before/Reserved/Committed/RolledBack/After`。

## 4. 工作包与严格顺序

### P6-000：Phase 5→6 门禁校准与任务锁定

**唯一结果：** P5-005 得到真实最终结论并归档，项目权威文档一致，P6-001 的 allowlist、非目标和回滚边界被锁定。

**Codex/角色工作：**

- Coordinator 核对 P5-001～P5-004 的 archive 指针、commits、Build/UHT/C++/Link、TurnTest、固定伤害、Victory/Defeat、两轮 PIE、Return 与防重入证据。
- Independent Reviewer 输出并落盘 P5-005 最终结论；Teacher 完成 Phase 5 复盘和 Phase 6 前置概念检查。
- 按真实结论同步 `tasks/`、`PROJECT_STATE.md`、`worklog.md`、`todo_plan.md`、`README.md` 与 `docs/phase-5-execution-plan.md`。
- 分类当前两个 porcelain 修改；若仍是空 diff 噪声，仅记录并保留，不修改、不回退、不提交。
- 归档 P5-005 后创建唯一 `TASK-P6-001` 活动卡；未经单独执行确认，不实施 P6 Gameplay。

**用户 Editor 工作：** 无。

**验收：** Phase 5 状态和证据等级在所有权威文档一致；P5-005 无悬空活动卡；工作树归属明确；P6-001 精确 allowlist 已建立。

### P6-001：通用 Ability/Target/Command/Resolution 合同与普攻迁移

**唯一可见结果：** 玩家仍执行现有单体普攻，但不再依赖 `RequestBasicAttack + PendingTarget ASC + bool` 特例，而是完整通过 `SkillDefinition → TargetPolicy → ActionCommand → Ability → Resolution → Turn`；伪造或失效命令零副作用。

**主要文件候选：**

- 新增 `Source/HSR/GAS/Ability/HSRGameplayAbilityBase.*`。
- 新增 `Source/HSR/Data/HSRSkillDefinition.*`。
- 新增或扩展 `Source/HSR/GAS/Ability/HSRAbilityTypes.h`、`Source/HSR/Battle/HSRBattleTypes.h`。
- 新增 `Source/HSR/Battle/HSRTargetingPolicy.*`，或在审查后采用职责等价且更小的纯值策略实现。
- 最小修改 `HSRBattleCoordinator.*`、`HSRBasicAttackAbility.*` 和必要的测试入口。
- 如需要 Ability/Event Tags，先列出精确 `Config/DefaultGameplayTags.ini` 变更并单独审查；不得顺手扩展无关 Tags。

**Editor 工作：**

1. 创建 BasicAttack SkillDefinition DataAsset，配置稳定 SkillId、Category、SingleEnemy、Ability Class 与占位 GE。
2. 将 Definition 绑定到当前测试参与者的可用技能列表。
3. 重开 Editor，确认软/类引用与资产路径持久。

**必须验证：** 合法敌方单体；空目标；己方伪装为敌方；已销毁/死亡目标；非当前行动者；未知 SkillId；重复 ActionId；Ability/GE 缺失。失败时 HP、Energy、战技点、TurnIndex 全部不变；成功只产生一次 Resolution 与一次 Turn 推进。完成 fresh Build 和至少两轮 PIE。

**教学检查：** 能说明 Definition 与 Runtime Command 的区别、候选生成与权威复核的区别、Ability 激活成功与战斗行动成功为何不是同一层概念。

### P6-002：角色 Energy 与终结技 GAS Cost 垂直切片

**唯一可见结果：** Ultimate 占位技能在 Energy 足够时只扣一次并对合法敌方目标生效；Energy 不足、目标失效、Commit/Effect 失败或取消时不扣 Energy、不改变 HP/战技点、不推进 Turn。

**Codex 文件工作：**

- 新增 `HSRUltimateAbility.*` 并复用 P6-001 基类、Command、Target 与 Resolution。
- 复用 `UHSRCoreAttributeSet::Energy/MaxEnergy`；通过 GAS Cost GE 与选定的 `CheckCost/CommitAbility` 语义建立唯一扣费入口。
- 增加 Ultimate Definition、Cost/Effect Class 的受保护引用和初始化校验；禁止 Coordinator 手减 Energy。
- 明确 Cost 已提交后 Effect 失败的项目语义；优先让所有可预检失败发生在 Commit 前，不以“失败后再手工加回”作为常规补偿。

**Editor 工作：** 创建 Ultimate SkillDefinition、Energy Cost GE 与简单伤害占位 GE，配置测试初始 Energy/MaxEnergy。

**必须验证：** Energy 为 0、差 1、刚好、满值；重复命令/回调；目标激活前失效；Cost/Effect Class 缺失；终局竞态。每条路径记录 Energy before/after 与 Resolution；成功只扣一次。

**教学检查：** 能说明 Energy 为什么属于 ASC、Commit 的边界、为什么不能依靠失败后加回资源模拟原子性。

### P6-003：队伍战技点事务与普攻/战技垂直切片

**唯一可见结果：** 普攻成功后战技点 +1，战技成功时 -1；点数为 0、达到上限、目标失效、Ability/GE 失败、取消或重复 Resolution 时均满足上下限与零错误副作用。

**Codex 文件工作：**

- 在 battle-local Coordinator 侧实现 `FHSRTeamResourceState` 与 reservation token/ActionId 账本。
- 新增 `HSRSkillAbility.*`；战技在验证后预留，成功 Resolution 提交，所有失败/取消/teardown 路径回滚。
- BasicAttack 的成功 Resolution 增加点数；失败或上限时遵循已文档化语义，禁止因加点失败使伤害处于半结算。
- 所有 Commit/Rollback 和重复回调幂等；Reset、FinishBattle、Actor 销毁与旅行清空未完成 reservation。

**Editor 工作：** 创建 Skill Definition 与简单伤害 GE；配置初始/最大战技点测试值，运行值不得写回 DataAsset。

**必须验证：** 0 点战技；上限时普攻；正常 Reserve/Commit；Reserve 后目标销毁；Ability CanActivate/Commit/GE 失败；Cancel；重复 Commit/Rollback/Resolution；Reset 与地图旅行。资源日志必须能解释每笔事务。

**教学检查：** 能对比 Energy 与战技点的所有权，并解释 Reserve/Commit/Rollback 为什么优于“先扣后补”。

### P6-004：治疗与最小 Battle Command Panel

**唯一可见结果：** 命令面板显示四类 Ability、合法目标、Energy/战技点与禁用原因；玩家可完成四条占位技能路径，治疗不超过 MaxHealth；UI 只展示状态和提交稳定 ID Command。

**Codex 文件工作：**

- 新增 `HSRHealAbility.*`，复用 SingleAlly/Self TargetPolicy 和 Healing GE；本阶段不支持复活。
- 提供最小事件驱动 Battle Command ViewState/委托和 Command 提交入口；不使用 Widget Tick。
- 暴露只读技能列表、候选目标、资源快照与最近 Resolution；Widget 不接触权威 ASC/Actor 引用。
- 规定满血治疗的统一语义（建议 `Rejected/NoEffectiveChange`，不消耗行动）；最终选择必须写入合同与测试。

**Editor 工作：**

1. 创建 Heal SkillDefinition 与 Healing GE。
2. 创建/扩展 `WBP_BattleCommandPanel`，绑定四个技能入口、目标列表、Energy/战技点、禁用原因和 Resolution 文本。
3. 绑定 Widget 创建/销毁与委托解绑；验证键鼠基本点击、按钮连点和 Widget 重建。

**必须验证：** 四类 Ability 成功路径；己方/敌方/自身目标规则；受伤治疗、满血治疗、治疗 Clamp；UI 伪造/过期目标；按钮连点；Widget 重建/解绑；连续 PIE 无重复 delegate。所有拒绝仍由 C++ 权威复核，按钮禁用不能替代验证。

**教学检查：** 能解释 UI 为什么不是权限边界、Widget 销毁后谁拥有资源、为何规则稳定后再接 UI。

### P6-005：阶段回归、教学、独立审查与归档

**唯一结果：** 四类 Ability、目标、Energy、战技点、Resolution 与 UI 的证据完整且可复查；PHASE6 获得真实 Gate 结论。

**收尾工作：**

- fresh `HSREditor Win64 Development` Build，记录 UHT/C++/Link、退出码与第一个真实 warning/error。
- Editor 重开、至少连续两轮 PIE、一次完整成功序列和完整失败矩阵；记录日志、截图/录像及资产引用。
- Reviewer 只读检查反射、GC、软类加载、delegate 解绑、无 Tick、Action 幂等、资源事务、终局顺序、跨 World 清理和证据等级。
- Teacher 复盘 Ability Spec、Commit、Target、GAS Cost、共享资源事务和 Resolution；保存用户原始回答、纠正与复习项。
- 更新 `docs/gas-notes.md`（若不存在则创建）、`docs/battle-system-design.md`、`learning-journal.md`、`worklog.md`、`todo_plan.md`、`README.md`、`PROJECT_STATE.md` 和本计划的真实状态。
- 全部子任务归档、Reviewer 放行且证据链完整后，才可由 Coordinator 标记 PHASE6 Ready 并执行经授权的阶段收尾 Git 操作。

## 5. 固定验收矩阵

| 类别 | 必测场景 | 成功/失败不变量 | 证据 |
|---|---|---|---|
| Command | 非当前行动者、旧 Battle/ActionId、重复点击/回调 | 最多一个最终 Resolution | 关联 ID 的日志与断言 |
| Target | 空、同队伪造、错误类型、死亡/销毁、过期目标 | Coordinator 重验；失败零副作用 | 候选与复核结果 |
| Energy | 0、差 1、刚好、满值、重复激活 | Ultimate 成功只扣一次 | before/after 与 Cost 日志 |
| 战技点 | 0、cap、Reserve 后失败、重复 Commit/Rollback | 普攻成功才加；战技成功才扣 | 事务状态链 |
| Effect | Ability/Cost/Effect 缺失，Commit/Apply 失败，Cancel | HP/资源/Turn 不出现半完成 | Resolution 原因 |
| Heal | 受伤、满血、越界、敌方伪造、死亡目标 | 不超过 MaxHealth；不复活 | Attribute delegate/日志 |
| Terminal | 致死 Skill/Ultimate、重复终局回调 | 一次 Result；终局后不推进 Turn | Outcome/Result/Turn 日志 |
| 生命周期 | Actor 销毁、Reset、Widget 重建、连续 PIE、旅行 | 无悬挂 reservation/delegate/runtime 引用 | teardown 与第二轮日志 |
| 构建 | fresh Development Editor | UHT/C++/Link 成功 | 命令、退出码、日志 |

每条失败用例至少记录：ActionId、失败原因、HP/Energy/战技点 before/after、Turn before/after。阶段总门禁问题是：**一次无效行动是否让所有资源与回合状态保持完全不变？**

## 6. 明确非目标

- Phase 7 的 Attack/Defense/Crit、ExecutionCalculation、正式伤害公式与可复现随机。
- Phase 8/9 的弱点、韧性、击破、Buff/Debuff 和回合状态生命周期。
- Phase 10 的完整战斗 UI、完整行动条、正式结果页与完整手柄焦点体验。
- AoE/随机/弹射目标、复活、复杂 AI 选技、连携、追击、被动和复杂 Cooldown。
- 正式动画、音频、VFX、GameplayCue 和第三方表现资产。
- SaveGame 持久化战斗资源、跨战斗保留战技点、多队伍、多人网络、RPC、Replication、Prediction。
- 新模块、第三方依赖、危险 Config 扩张、广泛重构或 Git 历史改写；如确有需要必须停下并单独授权。

## 7. 每个工作包的固定执行节奏

1. 先由 Coordinator 锁定唯一可见结果、状态机、不变量、精确 allowlist、Editor 资产和非目标。
2. 执行者先只读复述合同；按项目当前流程取得该任务的独立执行确认后再修改。
3. 实现最小完整垂直切片，不按类或文件机械拆分。
4. 用户在 UE Editor 创建/绑定资产并提供实际观察证据。
5. 运行 fresh Build、主路径、至少一条失败路径和生命周期回归。
6. 执行报告保存第一个真实错误、修复、未验证项和证据等级。
7. Independent Reviewer 结论必须先写入 `tasks/final-review.md`；通过后再归档并创建下一张活动卡。
8. 每包更新 `worklog.md`；仅在真实进度变化时更新 `todo_plan.md`，可复用知识写入 learning/design 文档。

## 8. 风险与停止条件

- 最高风险是当前 P5 同步特例“先应用 GE，后 ResolveAction”被直接泛化，造成 HP/Cost 已变化但 Turn 未推进。P6-001 必须先消除此协议风险。
- 第二风险是 Energy 与战技点形成双真源；任何 Coordinator 手改 Energy、GE 修改战技点或 UI 写资源都应直接 Reviewer 拒绝。
- 第三风险是把 UI 目标或弱 ASC 指针当成权威；所有命令必须用稳定 ID，并在执行时解析和重验。
- 如 P5-005 最终审查未放行、源码 dirty diff 出现真实未归属内容、需要新模块/第三方资产/危险 Config/广泛重构，立即停止对应实施并请求授权。
- 规划、资产存在、编译成功和单次 PIE 均不能单独证明阶段完成；必须保留失败路径、连续 PIE、审查和文档证据。

## 9. 现在唯一的下一步

Phase 6 已收尾并归档。当前无活动工程任务；下一步只建议先规划 Phase 7，不自动创建或实施 Gameplay，也不自动执行 Git。

P6-005 已把 P6-001～P6-004A 的 Build/PIE、ActionId/Target/Resolution、Energy、SP、Heal/ViewState、真实 WBP、Editor 资产与 follow-up 如实归档。最终 Gate 为 `PASS WITH FOLLOW-UP`，Phase 6 为 `Ready with inherited follow-ups`。
