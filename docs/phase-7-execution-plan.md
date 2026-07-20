# Phase 7 详细执行计划：属性、伤害公式与暴击

## 1. 阶段定位与当前门禁

Phase 6 已由 Independent Reviewer 判定为 `PASS WITH FOLLOW-UP`，当前状态是 `Ready with inherited follow-ups`；P6-001～P6-004A 与 P6-005 已归档，当前没有活动工程任务。因此 Phase 7 当前状态为：

- **Gate：`P7-001/P7-002 archived PASS WITH FOLLOW-UP / TASK-P7-003 planned / awaiting independent user confirmation`**。
- **已完成：** P7-001、P7-002 已通过 Reviewer 并归档；统一 Execution 测试入口、确定性矩阵与 P5/P6 smoke 有真实证据。
- **当前可以做：** Implementation Agent 首次只读复述 `TASK-P7-003`。
- **当前不可以做：** 用户独立确认 TASK-P7-003 前实施、修改 Source/Content/Config、运行 Build/Editor/PIE 或进入 P7-004/Phase 8。

Phase 6 的同步 post-GE、真实 Rollback/并发、SingleAlly 动态路径、目标销毁、Ability/GE 失败、终局异步、Save/网络等 inherited follow-ups 继续保留。它们不全部阻断 Phase 7 规划，但与新伤害管线直接相关的失败、目标生命周期、终局和 exactly-once 路径必须进入 Phase 7 回归矩阵，且不得被升级为已验证。

## 2. 唯一阶段结果

Phase 7 的唯一用户可见、可独立验收结果是：

> 普攻、战技和终结技的伤害统一经过一个无状态 `ExecutionCalculation`；系统使用 Attack、Defense、CritRate、CritDamage、原创 Damage Rule 与 battle-local 确定性随机生成一次性 Damage Breakdown。相同 seed、输入和命令序列得到相同结果，最终伤害可以逐层解释并与真实 HP 变化对账；失败或重复 Action 不改变 HP、Energy、战技点、RNG 序列或 Turn。

治疗继续使用 Phase 6 的 Healing 路径，不强行迁入 Damage Execution。Phase 7 结束后才能评估 Phase 8，不自动进入 Phase 8。

## 3. 阶段不变量与职责边界

- `Command → BattleCoordinator 校验 → Ability/GE → Resolution → TurnManager` 的 Phase 6 主链保持不变。
- 每个有效 `ActionId` 最多产生一次随机消费、一次伤害、一个最终 Resolution 和一次 Turn 推进；重复 `ActionId` 返回缓存结果，不重掷、不重伤。
- BattleCoordinator 或专用 battle-local RNG 服务拥有 seed 与随机序列；`ExecutionCalculation` 不保存 `FRandomStream`、Source/Target、上次结果、UObject 指针或 GE Handle。
- Ability 只组装 Context/Spec 并请求应用，不复制公式、不直接扣 Health、不推进 Turn。
- Execution 捕获输入、计算并输出唯一的 meta damage；AttributeSet 在 `PostGameplayEffectExecute` 中唯一消费 meta damage、修改并 Clamp Health。
- Damage Breakdown 是一次结算的纯值快照和只读观察数据，不是 HP、胜负、随机或 Turn 的规则真源，不进入 SaveGame。
- UI 只订阅结构化结果，不写 HP、不决定暴击、不提交随机值、不推进 Turn，无 Widget Tick，委托成对绑定/解绑。
- `Damage.Type.*` 是规则标识而非显示文本；本阶段不实现元素弱点、韧性、击破或抗性系统。
- 当前仍为单机；未来服务器权威 RNG、RPC、复制、PredictionKey、GameplayCue 同步和 PlayerState ASC 只记录边界，不提前实现，也不宣称本地确定性等于网络确定性。

## 4. P7-000：Phase 6→7 门禁校准与伤害协议冻结

### 唯一结果

在任何代码实施前，形成一张经过审查、等待用户独立确认的 P7-002 活动任务卡；冻结属性捕获、Execution、随机所有权、测试入口、失败语义、allowlist 和证据要求。

### P7-001 归档校准（2026-07-20）

- Reviewer 结论为 `PASS WITH FOLLOW-UP / Ready for archive`，三件套已归档；fresh `HSREditor Win64 Development -Rebuild` 实际编译/链接成功。
- 用户提供并经 Reviewer 检查的烟雾 PIE 保持旧 Basic/Skill/Ultimate 固定伤害唯一入口；Config 的 metadata/默认字段/数组 `+` 为 UE canonical serialization。
- P7-002 必须使用四项 non-snapshot Capture、BattleCoordinator 独占 battle-local RNG，先做统一 Execution 测试入口，禁止正式 Ability 迁移或旧 GE 双应用。

### 必须作出的设计决策

1. **AttributeSet 现实校准：** 路线图写的是 `HSRCombatAttributeSet`，但当前源码实际使用 `UHSRCoreAttributeSet`。优先评估原位扩展现有类型；若决定新建/拆分 Combat AttributeSet，必须把初始化、ASC 注册、资产引用、迁移和回归作为显式工作，不得暗中重命名或大范围重构。
2. **CritDamage 唯一语义：** 建议定义为“额外倍率”，例如 `0.5` 表示暴击总倍率 `1.5`；类型注释、DataAsset、公式、测试和 UI 必须一致。
3. **捕获策略：** 逐项冻结 Attack、Defense、CritRate、CritDamage 的 Source/Target 与 snapshot/live 语义。同步结算第一版优先在执行时读取当前值；如选择 snapshot，必须说明 Spec 创建时锁值的业务理由。
4. **唯一伤害入口：** 明确从当前固定 Health Modifier GE 迁移到 Execution Damage GE 的切换点；运行时不得让旧固定伤害与新入口同时生效。
5. **随机合同：** 拒绝、重复或 preflight 失败的命令不消费 RNG；同一有效 Action 只消费一次；重放返回缓存 Breakdown。
6. **Resistance 范围：** Phase 7 延期 Resistance，仅保留可扩展合同，不提供未验证字段或假数据；元素抗性与弱点留给 Phase 8。

### P7-000 实际冻结结论（2026-07-20）

- AttributeSet 采用**原位扩展 `UHSRCoreAttributeSet`**；不新增/拆分 `UHSRCombatAttributeSet`。当前 Character 默认子对象和 BattleCoordinator `InitStats` 均注册 CoreSet，拆分会造成无必要迁移。
- Capture 冻结为 Source Attack/CritRate/CritDamage、Target Defense，四者均 `bSnapshot=false`，在同步 Execution 执行时读取当前值。
- `CritDamage` 是额外倍率；`0.5 → 1.5x`。默认 `AbilityMultiplier=1.0`、`DefenseCoefficient=0.5`、`MinDamage=1.0`；NaN/Inf 与非法 Rule/倍率结构化拒绝，最终只在暴击倍率后 `RoundHalfFromZero` 一次。
- BattleCoordinator 将在 P7-002 成为 battle-local RNG 唯一所有者；拒绝/重复/preflight 失败不消费，单个有效 ActionId 最多消费一次并缓存结果。
- 当前旧伤害入口已定位：Basic 的 `BP_GE_P5_BasicAttackDamage`、Skill 的 `BP_GE_P6_SkillAttackDamage`、Ultimate 的 `BP_GE_P6_UltimateDamage` 均直接修改 Health。P7-001 不迁移；P7-002 建测试入口；P7-003 才统一切换，禁止双入口。
- P7-000 审计前 dirty tree 只有未跟踪的本计划文件；未发现 Source/Content/Config 重叠修改。唯一活动卡已创建为 `TASK-P7-001`，仍等待用户独立确认。

### 建议原创最小公式

```text
NormalizedAttack = max(0, Attack)
NormalizedDefense = max(0, Defense)
RawDamage = max(MinDamage,
                NormalizedAttack * AbilityMultiplier
                - NormalizedDefense * DefenseCoefficient)
CritMultiplier = Critical ? 1 + max(0, CritDamage) : 1
FinalDamage = max(MinDamage, RoundHalfFromZero(RawDamage * CritMultiplier))
AppliedDamage = HealthBefore - HealthAfter
```

`CritRate` Clamp 到 `[0, 1]`。`AbilityMultiplier`、`DefenseCoefficient`、`MinDamage`、合法区间和缺失资产策略来自原创 Rule/Skill 静态配置，不复制商业游戏公式或数值。P7-001 任务卡必须冻结具体默认值、非法值规范化顺序、NaN/Inf 策略和舍入位置。

### 允许文档与证据

- 只读核对：`PROJECT_STATE.md`、`todo_plan.md`、`worklog.md`、Phase 6 归档、当前 AttributeSet、伤害 GE 引用和 BattleCoordinator/Ability 数据流。
- 产物：唯一 `tasks/active-task.md`，状态为等待用户确认；精确列出允许文件和禁止文件。
- 证据：dirty tree 分类、旧伤害入口引用清单、P6 follow-up 映射、Reviewer 对合同与范围的结论。

### 停止条件

- 属性类型迁移范围不清；固定 GE 与新入口的切换无法保证单入口；需要未授权 Config/新模块/批量资产修改；或 P6 工作树存在无法归属的重叠变更。

## 5. P7-001：战斗属性、原创规则与初始化基线

### 唯一结果

所有 Battle Character 的 ASC 可稳定读取 Attack、Defense、CritRate、CritDamage，原创 Damage Rule 与 Damage Type 合同具有唯一语义；本包尚不迁移正式技能，也不启用随机伤害。

### 预期文件边界

实际路径必须在任务卡创建前由源码审计确认。候选范围：

- 修改现有 `Source/HSR/GAS/Attribute/HSRCoreAttributeSet.*`，或按 P7-000 已批准决策显式新增 `HSRCombatAttributeSet.*`；两者不得未经迁移设计并存。
- 新增 `Source/HSR/GAS/Damage/HSRDamageTypes.*`：`FHSRDamageContext`、`FHSRDamageResult`、`FHSRDamageBreakdown` 与结构化失败枚举。
- 新增 `Source/HSR/GAS/Damage/HSRDamageRuleDefinition.*`：原创系数、默认值、合法区间和校验。
- 最小修改现有 Skill Definition，仅加入 `DamageType`、`AbilityMultiplier` 等真正需要的静态字段。
- 最小修改现有角色/战斗生成初始化入口，保证属性集注册和初始化只发生一次。
- 用户 Editor 资产：初始化 GE、`DA_DamageRule_Default`，以及经批准的 `Damage.Type.*` Tags。

### 数据合同

- Context：BattleId、ActionId、Source/Target ParticipantId、DamageType、AbilityMultiplier，以及可复现随机输入或标识。
- Breakdown：捕获值、规则系数、CritRoll、`bCritical`、RawDamage、FinalDamage、AppliedDamage 与舍入信息。
- Result：是否应用、失败原因、Health before/after 与 Breakdown。
- 所有字段为纯值；不得保存 Actor、ASC、AttributeSet、Widget、捕获定义指针或 GE Handle。

### Editor 操作

1. 更新玩家与敌人的初始化 GE，填写四项属性的明确默认值。
2. 创建默认 Damage Rule DataAsset，填写原创系数与合法范围。
3. 注册最小 `Damage.Type.*` Tags；重开 Editor 后确认仍存在且无重定向/重复语义。
4. 不创建 Phase 8 的 Element/Weakness/Resistance/Toughness 资产。

### 验收与失败路径

- Fresh `HSREditor Win64 Development` Build，真实覆盖 UHT、C++、Link。
- 属性初始化一次；重建、Re-Possess、连续两轮 PIE 不重复应用初始化效果。
- Attack/Defense 非负，CritRate/CritDamage 的越界、NaN/Inf 和缺失规则均有明确拒绝或规范化结果。
- 原有 Health、MaxHealth、Speed、Energy、Phase 6 命令面板无回归。
- 用户能手算正常、防御较高、必暴击三组真值，并说明 Base/Current 与这些属性为何不是 meta attribute。

## 6. P7-002：Attribute Capture、统一 Execution 与确定性暴击

### 唯一结果

一个 Development/Editor 测试伤害入口通过唯一 Execution Damage GE 计算并应用伤害，输出可对账 Breakdown；同 seed、输入与命令序列可复现。

### 预期文件与资产

- 新增 `Source/HSR/GAS/Damage/HSRDamageExecutionCalculation.*`。
- 最小扩展现有 BattleCoordinator 或已批准的 Damage service，作为 battle-local RNG 和一次性 Damage Trace 的所有者。
- 扩展已批准的 AttributeSet，加入 `IncomingDamage` meta attribute，并在 `PostGameplayEffectExecute` 中唯一修改 Health。
- 用户创建一个统一 Instant Damage GE 并绑定 ExecutionCalculation。
- 可新增或扩展 Development-only Harness；不得引入 Shipping 副作用。

### 执行顺序

1. 先以固定非暴击输入验证纯公式和 Capture。
2. 再由 battle-local RNG 生成 CritRoll，并通过 Spec/Context 传入；Execution 不自行持有长期 RNG。
3. Execution 捕获 Source Attack/Crit 与 Target Defense，读取 Rule/SetByCaller，输出 `IncomingDamage`。
4. AttributeSet 消费 meta damage、修改并 Clamp Health；Coordinator 根据结果发布 Resolution，TurnManager 只在成功后推进。

若使用自定义 `FGameplayEffectContext` 承载 Breakdown，任务卡必须显式覆盖 `Duplicate`、`NetSerialize`、`GetScriptStruct`、`UAbilitySystemGlobals::AllocGameplayEffectContext` 和精确 Config 变更；未经专项批准，不用临时 UObject 或裸指针绕过纯值合同。

### 验收矩阵

- 公式：Attack=0、Defense=0/极高、最低伤害、非法倍率、NaN/Inf；FinalDamage 有限、非负，防御增长不会反向增加伤害。
- Crit：CritRate=0 永不暴击，CritRate=1 必暴击，CritDamage=0/高值语义一致。
- 确定性：相同 seed 与命令序列两轮输出一致；不同 seed 可变化；拒绝或重复命令不消费 RNG。
- GAS：无效 Source/Target/ActorInfo、捕获失败、Rule/Tag 缺失、Spec 创建失败、GE 应用失败均返回结构化失败。
- 零副作用：失败时 HP、Energy、战技点、RNG 序列和 Turn 不变。
- 状态：Execution CDO 无单次可变成员；一次结果的 Breakdown 与 IncomingDamage、FinalDamage、AppliedDamage 可对账。

## 7. P7-003：普攻、战技、终结技迁移到统一伤害管线

> 状态：已创建唯一 `TASK-P7-003` 活动卡，等待用户独立确认；尚未实施。

> 资源延期边界：当前 Basic 成功 +1 SP、Skill 成功 -1 SP，无 Wait/Pass；Energy 仅作 Ultimate Cost，无 Basic/Skill/受击回能；当前单一 TeamResourceState 的敌我共池风险另行规划为“每 Team 共享池、敌方独立或无池”，不是每角色独立。本包不得顺带实现这些需求。

### 唯一结果

Basic Attack、Skill、Ultimate 的伤害全部只构建统一 Damage Spec/Context 并应用同一个 Execution；旧直接 Health Modifier 伤害资产不再被运行时引用。Heal 保持 Phase 6 路径。

### 预期文件边界

- `HSRGameplayAbilityBase.*`：仅抽取公共 Build/Apply Damage 接口。
- `HSRBasicAttackAbility.*`、`HSRSkillAbility.*`、`HSRUltimateAbility.*`：迁移伤害请求，不复制公式。
- Skill Definition 与必要的 BattleCoordinator Resolution 接线。
- 用户 Editor 更新三类伤害 Skill Definition/GE/Rule 引用。
- 不重写 TargetPolicy、TurnManager、Energy/SP 所有权、Heal 或战斗 UI 架构。

### 事务顺序与失败语义

- 在产生不可逆副作用前完成 Target、Rule、Tag、Spec、Context 和终局状态 preflight。
- 重复 ActionId 返回缓存 Resolution/Breakdown，不重掷、不重复扣资源或伤害。
- Apply 失败不得伪报成功；若现有同步事务无法保证 Cost 已提交后 Apply 失败的恢复，本包必须停止并把事务顺序/回滚作为阻断修订，不能用 `ensure` 代替正确性。
- 致死伤害只产生一个 BattleResult；终局后拒绝新伤害，不再推进 Turn、发奖或返回第二次。

### Editor 与引用审计

1. 将三种伤害 Skill Definition 指向统一 Damage GE、Rule、DamageType 与各自倍率。
2. 使用 Reference Viewer/引用扫描证明旧固定伤害资产不再被运行时引用。
3. 本阶段只解除引用，不贸然删除历史资产；删除需用户另行授权。

### 验收

- 三技能各验证非暴击与必暴击；普攻/战技至少各一条动态统一入口是阶段 Gate 必需项。
- 资源不足、伪造目标、目标销毁、Rule/GE 缺失、重复 ActionId、致死与非致死均有轨迹。
- 记录 `ActionId → roll → Breakdown → Health delta → Resolution → Turn/BattleResult` 全链。
- HP Clamp 导致的实际减少必须记录为 AppliedDamage，不能把 FinalDamage 与 AppliedDamage 淹没为同一字段。

## 8. P7-004：Damage Breakdown 观察层与回归 Harness

### 唯一结果

最小 Debug 日志/只读 UI 能显示最近一次伤害的 Source/Target、DamageType、Attack、Defense、倍率、roll、Crit、Raw/Final/AppliedDamage；连续两轮 PIE、Widget 重建和 teardown 不重复绑定、不显示旧 World 数据。

### 预期边界

- 优先扩展现有只读 Battle ViewState/ViewModel/Widget 数据结构，由 Coordinator 发布不可变快照。
- 可新增 Development-only Damage Harness；必须受 `WITH_EDITOR` 或等价构建边界保护。
- 用户可在现有 `WBP_BattleCommandPanel` 增加最小 Breakdown 区域，或创建独立 Debug Widget；这不是 Phase 10 完整 UI。
- 无 Tick，无 UI 业务写入，无 Shipping 调试副作用。

### 验收

- Fresh Build、Editor 重开、至少连续两轮 PIE。
- 重跑 P7-002 公式/seed 矩阵和 P7-003 三技能、重复 ActionId、目标销毁、终局矩阵。
- Widget `NativeDestruct`/Unbind、重建、Re-Possess、地图退出后无重复委托、悬挂引用、旧 Trace 或 GC/Blueprint Runtime Error。
- Debug 输出与 HP 属性真源一致；UI 显示错误时从 Command/Spec/Capture/Execution/Attribute/Resolution 链定位，不在 Widget 中补算公式。

## 9. P7-005：阶段收尾、教学、独立审查与归档

### 唯一结果

P7-000～P7-004 的任务卡、执行报告、审查、作者/资产归属和证据等级一致；Independent Reviewer 给出真实 Gate，Phase 7 才可标记为 `Ready` 或 `Ready with inherited follow-ups`。

### Coordinator

- 核对各包 allowlist、dirty tree、角色提交、用户资产、派生产物、Build 日志和 PIE 证据。
- 扫描并记录旧固定伤害引用；确认 ActionId、seed、Breakdown、HP、Resolution 和 Turn 可对应。
- 同步真实进度到 `PROJECT_STATE.md`、`todo_plan.md`、`worklog.md`、本计划与 README；规划项不得提前标完成。

### Teacher

- 教学顺序：meta attribute → Source/Target Capture → snapshot/live → Spec/SetByCaller → Execution 输出 → AttributeSet 消费 → MMC/Execution 选择 → 原创公式/舍入 → RNG 所有权 → Breakdown 对账。
- 保存用户原始回答、纠正、手算题、Debug 练习和掌握/复习项到 `learning-journal.md`。
- 更新 `docs/gas-notes.md`、`docs/battle-system-design.md` 和 `docs/interview-notes.md` 中已经真实验证的 Capture、Execution、随机与伤害所有权知识。

### Independent Reviewer

- 审查 Capture Source/Target 与 snapshot 策略、Execution 无状态、反射/GC/序列化、RNG 所有权、单入口、Cost/伤害/Turn/终局顺序、UI 只读、无 Tick、连续 PIE 和证据等级。
- 将完整结论先写入 `tasks/final-review.md`；Reviewer 未落盘前不得归档或进入下一包。
- 结论为 `REVISE`/`BLOCKED` 时只修订明确缺口，不扩大 Phase。

## 10. 阶段级验证矩阵

| 维度 | 必须验证的不变量 |
|---|---|
| 公式 | FinalDamage 有限、非负、满足最低伤害规则；防御增长不会反向增加伤害 |
| Crit | Rate=0 永不暴击，Rate=1 必暴击；同 seed 同序列一致；失败动作不消费随机 |
| GAS | ActorInfo/捕获定义正确；Execution 无持久状态；GE 失败不报告成功 |
| ActionId | 有效动作最多一次 roll、伤害、Breakdown、Resolution 和 Turn 推进 |
| 资源 | 失败或重复动作不改变 HP、Energy、战技点或 RNG 序列 |
| TurnSystem | 只在成功 Resolution 后推进；Ability/Execution 不拥有全局回合状态 |
| 终局 | 致死只产生一次 Result；终局后拒绝新伤害；奖励/返回不重复 |
| 生命周期 | 目标销毁、ASC 失效、Reset、连续 PIE、跨图 teardown 后无悬挂引用/委托 |
| UI/Debug | 只读、事件驱动、无 Tick；不写 HP、不决定暴击、不推进 Turn |
| 反射/GC | 反射宏、generated include、`UPROPERTY/TObjectPtr` 与纯值结构边界正确 |
| 未来网络 | 不实现 RPC/复制/预测；仅保留服务器权威 RNG/伤害/ActionId/Turn 的迁移边界 |

最低阶段 Gate 证据：

1. Fresh Development Editor Build，含真实 UHT/C++/Link 动作。
2. 纯公式自动化或确定性 Harness，覆盖正常与边界向量。
3. 普攻和战技各一条动态统一入口。
4. 至少一条目标/GE/Rule 失败路径且零副作用。
5. CritRate 0/1 与同 seed 同序列一致。
6. 重复 ActionId exactly-once。
7. 致死/终局 exactly-once。
8. 连续两轮 PIE 或等价生命周期验证。
9. Damage Breakdown 与 Health before/after 对账。
10. 所有证据保留真实等级，不以报告替代原始输出。

## 11. 证据等级

- `AGENT VERIFIED`：Agent 实际运行并保存完整命令、首个真实错误与 exit code。
- `USER PROVIDED`：用户在 Editor/PIE 中执行并提供日志、截图或可复核观察。
- `STATIC REVIEW`：源码、反射、引用或资产配置的只读审查。
- `REPORT ONLY`：只有报告陈述，没有可复核原始输出。
- `NOT VERIFIED`：明确没有执行。

证据等级只按事实记录；不得把 Phase 6 的 `USER PROVIDED` 或 `REPORT ONLY` 自动升级为 Phase 7 的直接验证。

## 12. 结构化失败语义

至少覆盖以下结果；最终枚举名以任务卡和代码审查为准：

- `InvalidSource`
- `InvalidTarget`
- `MissingDamageRule`
- `InvalidDamageType`
- `CaptureFailed`
- `InvalidCapturedValue`
- `SpecCreationFailed`
- `EffectApplicationFailed`
- `DuplicateAction`
- `BattleTerminal`
- `DamageResolved`

所有失败都必须声明 HP、Energy、战技点、RNG 序列和 Turn 是否变化；默认合同为零副作用。

## 13. 总依赖链与协作方式

```text
P7-000 门禁/协议冻结
  → P7-001 属性/规则/初始化
  → P7-002 Capture/Execution/确定性 Crit
  → P7-003 三类伤害 Ability 迁移
  → P7-004 Breakdown/回归 Harness
  → P7-005 教学/独立审查/归档
```

这些工作包严格串行。共享 AttributeSet、SkillDefinition、BattleCoordinator、Ability 和 GE 资产不得跨包并行修改。每包遵循：Coordinator 创建精确 allowlist → Implementation Agent 只读复述 → 用户对该任务编号独立确认 → 实施 → Build/Editor/PIE → Reviewer 落盘 → Coordinator 归档。

## 14. 明确非目标

- Phase 8 的 Element、Weakness、Resistance、Toughness、Break、Turn Delay。
- Phase 9 的 Buff、Debuff、DoT、叠层、免疫、驱散和回合持续时间。
- Phase 10 的完整行动条、目标/状态/结果 UI 和正式表现。
- AoE、弹射、多段、追击、复杂伤害队列。
- 动画、VFX、音频、GameplayCue 作为伤害完成真源。
- 保存单次伤害、Runtime Actor/ASC/GE Handle 或 RNG 状态到 SaveGame。
- RPC、复制、Prediction、PlayerState ASC 或“已网络安全”的承诺。
- 新模块、第三方依赖、商业公式复刻、大范围 Config/架构重写或历史资产删除。

## 15. 主要风险与停止条件

1. 新旧伤害入口并存导致重复扣血。
2. Execution CDO 保存 RandomStream/LastResult，造成跨行动或跨 World 污染。
3. Source/Target Capture 配反或 snapshot 时机不明。
4. CritDamage 的“额外倍率/总倍率”语义混用。
5. Cost 已提交但 Damage Apply 失败，产生半完成事务。
6. 重复 ActionId、回调重入或拒绝命令再次消费 RNG。
7. Health Clamp 后把 FinalDamage 与 AppliedDamage 混为一谈。
8. Breakdown 与实际伤害使用两套计算，形成伪日志。
9. 自定义 EffectContext 未正确 Duplicate/序列化/分配，导致切片或数据丢失。
10. UI 变成规则真源、重复绑定或保留旧 World 引用。
11. 旧固定 GE 仍被软引用；删除资产前未做 Reference Viewer 审计。
12. P6 或用户资产/历史 dirty 变更被夹带进角色提交。

遇到上述任一无法在当前 allowlist 内安全闭合的情况，应停止当前工作包、保留第一处真实错误和工作树状态，交由 Coordinator/用户决定修订或扩权，不使用 reset、clean、覆盖或无证据绕过。

## 16. 规划后的唯一下一步

归档本计划后，唯一建议的下一任务是：

> 由 Coordinator 创建 `P7-000 Phase 6→7 门禁校准与伤害协议冻结` 的活动任务卡，先完成源码引用审计、AttributeSet 决策、原创公式/随机/失败合同和精确 allowlist；等待用户对 P7-000 单独确认后再执行。此步骤不修改 Gameplay 代码或 Editor 资产。
