# HSR 结合项目阶段的 GAS 学习路线

> 状态：学习规划已确认，尚未实施任何 GAS Stage。本文件是 HSR GAS 学习阶段、实践任务、测试与复盘要求的详细真源。

## 0. 推荐学习顺序

GAS Stage 编号按规划保留，但实际项目顺序应与 HSR Phase 对齐：

```text
Stage 0 → Stage 1 → Stage 2 → Stage 3 → Stage 4 → Stage 5
→ Stage 7（Phase 8 弱点击破）
→ Stage 6（Phase 9 状态系统）
→ Stage 8 → Stage 9 → Stage 10
```

| GAS Stage | 对应项目 Phase | 最小成果 |
|---|---|---|
| 0 | Phase 0–1 | 明确 GAS 适用边界 |
| 1 | Phase 2 | ASC、AttributeSet、初始化、UI Delegate |
| 2 | Phase 2/5 | Instant GE、伤害、治疗和 Clamp |
| 3 | Phase 5/6 | 基础攻击、战技、Cost 和 Ability 生命周期 |
| 4 | Phase 6/8 | 统一 Gameplay Tags |
| 5 | Phase 7 | Damage Execution、Crit、Defense、Resistance |
| 7 | Phase 8 | Weakness、Toughness、Break、Turn Delay |
| 6 | Phase 9 | Buff/Debuff、叠层和回合持续 |
| 8 | Phase 10 | UI 查询、监听和命令提交 |
| 9 | Phase 12 | 装备、遗器、属性来源和 Infinite GE |
| 10 | 后期专项 | ASC 所有权、复制模式、预测和服务器权威 |

每个 Stage 必须能够解释、实现、验证和复盘；成功复制代码或让蓝图运行不等于真正学会。

---

## GAS Stage 0：为什么 HSR 使用 GAS

### 阶段信息

- **对应 Phase：** Phase 0–1。
- **学习概念：** ASC、AttributeSet、GameplayAbility、GameplayEffect、Tags、GameplayCue 的职责总览，以及 GAS/TurnSystem/Data/Save/UI 边界。
- **为什么现在学：** 在创建 ASC 前避免“所有系统都不用 GAS”和“所有系统都塞进 GAS”两个极端。
- **项目场景：** 只做架构图和职责判断，不创建 GAS Gameplay 类。

### GAS 适用范围

适合 GAS：

- 战斗属性。
- 普攻、战技、终结技和被动。
- 伤害、治疗、个人 Cost 和部分 Cooldown。
- Buff、Debuff、免疫和状态 Tags。
- 装备、遗器和套装的属性效果。
- GameplayCue 表现通知。
- 未来联机下的技能权限、复制和预测。

不适合直接交给 GAS：

- 回合状态机、当前行动者和行动队列。
- Encounter 和地图旅行。
- 背包物品实例、任务、对话和商店。
- SaveGame、奖励幂等事务和 UI 导航。
- “持续 N 回合”的计数真源。

### 对应 UE 类

本阶段只认识，不创建：`UAbilitySystemComponent`、`UAttributeSet`、`UGameplayAbility`、`UGameplayEffect`、`UGameplayCueNotify_*`、`FGameplayTag`、`FGameplayEffectSpec`、`FActiveGameplayEffectHandle`。

### 最小实践任务

1. 画 GAS 五个核心对象关系图。
2. 画 GAS 与 TurnSystem 职责图。
3. 画 DataAsset、Runtime、GAS 和 SaveGame 边界图。

### 推荐文件、类与 API

- `docs/gas-notes.md`
- `docs/battle-system-design.md`
- `learning-journal.md`
- 不创建 C++ 类、UPROPERTY 或 UFUNCTION。

### Editor、测试与 Debug

- Phase 0 只确认 GameplayAbilities、GameplayTags、GameplayTasks 和 Tags 设置。
- 不创建 Ability、Effect 或 AttributeSet。
- 自检能否说明 ASC/AttributeSet、Ability/Effect 和 GAS/TurnSystem 的区别。

### 常见错误

- 把 GAS 当成整个 RPG 框架。
- 把 GameplayAbility 当作技能配置数据库。
- 把 GameplayEffect 当作存档。
- 尚未理解生命周期就设计预测和复制。
- 第一周就创建完整技能、伤害和状态系统。

### learning-journal、面试与作品集

- **记录：** GAS 解决/不解决的问题、TurnSystem 边界、Definition/Runtime/Save、当前最不理解的三个概念。
- **面试：** GAS 主要组成；Ability 与 Effect；回合制为何仍使用 GAS；哪些系统不适合 GAS。
- **作品集：** 展示写代码前的职责和数据边界设计。

---

## GAS Stage 1：ASC 与 AttributeSet 最小闭环

### 阶段信息

- **对应 Phase：** Phase 2。
- **学习概念：** ASC、`IAbilitySystemInterface`、Owner/Avatar、AttributeSet、初始化、Health/MaxHealth、Energy/MaxEnergy、Speed、Attribute Delegate。
- **为什么现在学：** Character/Controller 生命周期已建立，可以集中验证 GAS 初始化，不受技能和伤害公式干扰。

### ASC 放置决策

当前单机：玩家和敌人的 ASC 都放 Character/BattleCharacter；Owner Actor 与 Avatar Actor 暂时都是 Character。它简单、一致，适合独立 Battle Map 重建。

未来联机：玩家 ASC 可迁移到 PlayerState，Owner Actor 为 PlayerState，Avatar Actor 为当前 Character；敌人 ASC 通常仍在 Enemy Character。当前不为联机提前引入 PlayerState ASC。

### 对应 UE 类与 HSR 场景

- `AHSRCharacterBase`
- `UHSRAbilitySystemComponent`
- `UHSRCoreAttributeSet`
- `IAbilitySystemInterface`
- `UHSRAttributeViewModel`

Exploration Character 可显示 Debug 属性；Battle Map 重建 Battle Character 后走同一 ASC 初始化入口；Speed 后续供 TurnManager 排序。

### 最小实践任务

1. CharacterBase 创建 ASC 和 AttributeSet。
2. 实现 `IAbilitySystemInterface`。
3. 正确调用 `InitAbilityActorInfo`。
4. 使用 Instant 初始化 GE 设置五个属性。
5. Delegate 更新 Debug UI。
6. 重复初始化保持幂等。

### 推荐文件与类

```text
Source/HSR/GAS/HSRAbilitySystemComponent.h/.cpp
Source/HSR/GAS/Attribute/HSRCoreAttributeSet.h/.cpp
Source/HSR/Character/HSRCharacterBase.h/.cpp
Source/HSR/UI/HSRAttributeViewModel.h/.cpp
Content/Data/Effects/GE_InitializeCoreAttributes
Content/UI/Debug/WBP_AttributeDebug
```

### 关键 UPROPERTY / UFUNCTION

- `TObjectPtr<UHSRAbilitySystemComponent> AbilitySystemComponent`
- `TObjectPtr<UHSRCoreAttributeSet> CoreAttributeSet`
- `TSubclassOf<UGameplayEffect> InitialAttributesEffect`
- Health、MaxHealth、Energy、MaxEnergy、Speed 使用 `FGameplayAttributeData` 和 Accessor 宏。
- `GetAbilitySystemComponent`
- `InitializeAbilityActorInfo`
- `ApplyInitialAttributes`
- `BindAttributeDelegates` / `UnbindAttributeDelegates`

### Editor 配置

1. 创建 Instant `GE_InitializeCoreAttributes`。
2. 配置五个初值。
3. 在 Character 派生蓝图绑定 Effect。
4. 创建只读属性 Debug Widget。
5. 不创建 Ability。

### 测试与 Debug

- ASC 可获取，Owner/Avatar 正确。
- 属性只初始化一次且处于合法范围。
- Character 重建后重新初始化。
- Widget 销毁后 Delegate 解绑。
- 输出 Owner、Avatar、Actor Info、初始化次数和当前 Attribute。

### 常见错误

- 忘记 `IAbilitySystemInterface` 或 `InitAbilityActorInfo`。
- 在构造函数应用 GE。
- BeginPlay 重复初始化。
- UI 使用 Tick。
- 初值硬编码在 Widget/Character。

### learning-journal、面试与作品集

- **记录：** Owner/Avatar、Character vs PlayerState ASC、Base/Current Value、初始化 GE、Delegate、Battle Map 重建。
- **面试：** ASC 放 Character/PlayerState 的差异；Actor Info 调用时机；AttributeSet 如何与 ASC 关联；UI 如何响应属性。
- **作品集：** `Character → ASC → Init GE → AttributeSet → Delegate → UI` 数据流。

---

## GAS Stage 2：GameplayEffect 修改属性

### 阶段信息

- **对应 Phase：** Phase 2/5。
- **学习概念：** Instant/Duration/Infinite GE、Modifier、SetByCaller、Meta Attribute、Damage/Healing、Clamp、`PreAttributeChange`、`PostGameplayEffectExecute`。
- **为什么现在学：** Ability 最终依赖 GE；先证明属性修改正确，再引入 Ability 生命周期。

### Effect 类型

- **Instant：** 应用一次并修改 Base Value；适合初始化、伤害、治疗、Energy。
- **Duration：** 按真实秒数存在；适合探索实时效果，不等于回合数。
- **Infinite：** 主动移除前一直存在；适合装备、被动和回合 Status 的 GAS 表现层。

### HSR 伤害入口

推荐增加 `IncomingDamage`、`IncomingHealing` Meta Attribute：Instant GE 写入 Meta Attribute，`PostGameplayEffectExecute` 统一修改并 Clamp Health，然后清零 Meta Attribute。这为 Stage 5 ExecutionCalculation 保留稳定入口。

### 对应类、任务与文件

- `UHSRCoreAttributeSet`
- `FHSRDamageTypes`
- `GE_TestDamage`、`GE_TestHealing`
- 任务：SetByCaller Damage/Healing、过量伤害/治疗和统一 Health 边界。

### 关键 UPROPERTY / UFUNCTION

- IncomingDamage、IncomingHealing。
- `Data.Damage`、`Data.Healing` SetByCaller Tags。
- `PreAttributeChange`
- `PostGameplayEffectExecute`
- `MakeOutgoingSpec`
- `AssignSetByCallerMagnitude`
- `ApplyGameplayEffectSpecToTarget`

### Editor 配置

1. 创建 Instant Damage/Healing GE。
2. Modifier 指向 Meta Attribute。
3. Magnitude 使用 SetByCaller。
4. Debug UI 按钮只请求测试服务，不直接改属性。

### 测试与 Debug

- 测试 0、负数、极大值、恰好归零、过量治疗、MaxHealth 变化、无效 Source/Target、重复 GE。
- 检查 IncomingDamage 是否清零。
- 输出 GE、Source/Target、SetByCaller、Health Before/After 和 EvaluatedData。

### 常见错误

- GE 和 AttributeSet 各扣一次 Health。
- 忘记清零 Meta Attribute。
- 用 PreAttributeChange 处理全部伤害规则。
- Duration 秒数当回合数。
- UI 直接 SetHealth。

### learning-journal、面试与作品集

- **记录：** 三种 GE、SetByCaller、Meta Attribute、Pre/Post、Clamp 和一次伤害数据流。
- **面试：** Instant/Infinite 区别；Meta Attribute 价值；Pre/Post 区别；SetByCaller 场景。
- **作品集：** 可扩展伤害入口而非直接 `Health -= Damage`。

---

## GAS Stage 3：GameplayAbility 最小技能

### 阶段信息

- **对应 Phase：** Phase 5/6。
- **学习概念：** Ability Class/Spec/Instance、激活、输入/UI 触发、Cost、Commit、Cooldown、结束/取消/失败、AbilityTask、目标和 ActionResolved。
- **为什么现在学：** ASC、Attribute 和 GE 已独立验证，Ability 只需组织一次行动。

### HSR 使用场景与资源边界

- 普攻：无个人 Cost，成功后增加共享战技点。
- 战技：消费共享战技点。
- 终结技：检查并消费 Energy。
- 治疗：应用 Healing GE。
- Energy 是 Attribute/GAS Cost；战技点由 BattleCoordinator 原子预留、提交和回滚，不放每个角色 ASC。

### 激活与目标数据流

```text
Enhanced Input 或 Skill Button
→ PlayerController/ViewModel
→ FHSRBattleActionCommand
→ BattleCoordinator 验证当前行动者/目标/共享资源
→ ASC TryActivateAbility
→ Ability Commit
→ GE
→ FHSRAbilityResolution
→ Event.Battle.ActionResolved
→ TurnManager
```

TargetingPolicy 生成合法目标，UI 只选择候选，Coordinator 与 Ability 都必须重新校验。

### AbilityTask

用于 Ability 内等待 Gameplay Event、目标数据、Montage 或异步状态。同步基础攻击不使用 AbilityTask；动画/目标异步化后再引入。AbilityTask 不管理整个回合。

### 回合 Cooldown

GAS Cooldown GE 默认使用秒数。回合冷却由 Turn Runtime 保存 RemainingTurns，并使用 `Cooldown.Ability.*` Tag 阻止激活；归零后移除 Infinite GE/Tag。不要用 `Duration=2` 表示 2 回合。

### 推荐文件与类

```text
GAS/Ability/HSRGameplayAbilityBase.*
GAS/Ability/HSRBasicAttackAbility.*
GAS/Ability/HSRSkillAbility.*
GAS/Ability/HSRUltimateAbility.*
GAS/Ability/HSRHealAbility.*
Battle/HSRTargetingPolicy.*
Battle/HSRBattleResourceState.*
Data/Definitions/HSRSkillDefinition.*
```

### 关键 UPROPERTY / UFUNCTION

- SkillDefinition、Ability/Required/Blocked Tags、TargetType、EnergyCost、SkillPointDelta、Effect Classes、SetByCaller Tags。
- `CanActivateAbility`、`ActivateAbility`、`CommitAbility`、`EndAbility`、`CancelAbility`。
- `BuildLegalTargets`、`ValidateActionCommand`、`ReportAbilityResolution`、`TryActivateAbility`。

### Editor 配置

1. 创建 C++ Ability 基类与 GA 派生资产。
2. 创建 Cost/Damage/Healing GE。
3. 创建 SkillDefinition。
4. 配置 Tags、目标和角色 Ability 授予。
5. 创建 Skill Button/Target 提示。

### 测试与 Debug

- 未授予、非当前行动者、无效/死亡目标、Cost 不足、Commit 失败、取消、重复点击和 Resolution 单次发送。
- 日志按 Requested、Activated、Committed、Resolved、Ended 分阶段，并输出 Spec Handle、Actor Info、Tags、Cost 和 Trace ID。

### 常见错误

- UI `NewObject` Ability 或直接 Apply GE。
- Commit 前造成伤害。
- Cost 扣两次。
- Ability 自己推进 TurnManager。
- EndAbility 被当作成功。
- Montage 完成被当作规则完成。

### learning-journal、面试与作品集

- **记录：** Class/Spec/Instance、激活链、Commit、Cost/战技点、Targeting、Resolution/End、AbilityTask。
- **面试：** Ability 如何授予/激活；Commit 做什么；UI 如何触发；AbilityTask；为何不能靠 EndAbility 推进回合。
- **作品集：** 展示完整命令、校验和失败恢复链。

---

## GAS Stage 4：Gameplay Tags

### 阶段信息

- **对应 Phase：** Phase 6/8。
- **学习概念：** Ability、State、Element、Weakness、Buff、Debuff、Cooldown、Required/Blocked/Cancel/Granted Tags 和 Tag Event。
- **为什么现在学：** Ability 和状态数量增加后，Tags 可以避免大量 Bool、Enum、Cast 和硬依赖。

### 命名规范

```text
Ability.Attack.Basic
Ability.Skill
Ability.Ultimate
State.Battle.Active
State.Battle.CurrentTurn
State.Dead
State.CannotAct
Element.*
Weakness.*
Status.Buff.*
Status.Debuff.*
Cooldown.*
Event.Battle.ActionResolved
Event.Turn.Started
Event.Turn.Ended
```

元素、弱点、状态和显示名称必须是 HSR 原创定义。

### 对应文件、类与任务

- `GAS/HSRGameplayTags.h/.cpp`
- `Config/DefaultGameplayTags.ini`
- AbilityBase、BattleCoordinator。
- 建立 Tag 生产者/消费者表；长期稳定 Tag 优先 Native 注册。

### 关键 UPROPERTY / UFUNCTION

- `FGameplayTag`、`FGameplayTagContainer`
- `HasMatchingGameplayTag`、`HasAll/AnyMatchingGameplayTags`
- `RegisterGameplayTagEvent`
- `Add/RemoveLooseGameplayTag`：只在生命周期所有者明确时使用。

### Editor、测试与 Debug

1. 创建命名空间。
2. Ability 配 Asset/Activation Tags。
3. GE 配 Granted Tags。
4. UI 配 Tag 到图标/文本映射。
5. 测试 Required 缺失、Blocked、Cancel、GE 移除、Element/Weakness 匹配和 Tag Event。
6. Debug 输出 Owned Tags 和生产者/消费者。

### 常见错误

- 同一语义多个 Tag。
- Tag 包含显示文本。
- 到处添加 Loose Tag 但无人移除。
- 多系统生产同一状态 Tag。
- 用 Tag 代替 Asset ID。

### learning-journal、面试与作品集

- **记录：** Tag 与 Enum/Bool/ID、Owned/Asset/Granted/Required/Blocked/Cancel、生命周期和命名树。
- **面试：** Tags 优势；Required/Blocked/Cancel；Loose Tag 风险；Tag Event 驱动 UI。
- **作品集：** Tag 生产者—消费者矩阵。

---

## GAS Stage 5：伤害公式与 ExecutionCalculation

### 阶段信息

- **对应 Phase：** Phase 7。
- **学习概念：** ExecutionCalculation、Attribute Capture、Source/Target、Snapshot、Damage Context、Crit、Defense、Resistance、MMC、Meta Attribute 和可复现随机。
- **为什么现在学：** Attack、Defense、Crit 和 Resistance 同时进入公式后，普通单 Modifier 难以表达多输入、多输出和完整 Debug Breakdown。

### ExecutionCalculation 与 MMC

- `ModifierMagnitudeCalculation` 适合计算一个 Modifier Magnitude，例如基于 MaxEnergy 的 Cost 或基于等级的治疗。
- `ExecutionCalculation` 适合 Source/Target 多属性、Crit/Defense/Resistance、多输出和 Damage Breakdown。
- 不因 ExecCalc 更复杂就用它替代所有 MMC。

### HSR 数据流

```text
Skill Multiplier + Source Attack
→ Crit Roll
→ Target Defense
→ Resistance
→ Final Damage
→ IncomingDamage
→ PostGameplayEffectExecute
→ Health
```

公式必须原创；系数保存在 DamageRuleDefinition 或统一规则入口。

### 推荐文件、类与任务

```text
GAS/Attribute/HSRCombatAttributeSet.*
GAS/Damage/HSRDamageExecutionCalculation.*
GAS/Damage/HSRDamageTypes.*
Data/Definitions/HSRDamageRuleDefinition.*
Debug/HSRDamageTrace.*
```

- `FHSRDamageContext/Result/Breakdown`
- 可选 `UHSRCostMagnitudeCalculation`
- 任务：扩展属性、迁移基础攻击、Battle RandomStream、公式层级 Debug。

### 关键 UPROPERTY / UFUNCTION

- Attack、Defense、CritRate、CritDamage、Resistance。
- `FGameplayEffectAttributeCaptureDefinition`
- `Execute_Implementation`
- `AttemptCalculateCapturedAttributeMagnitude`
- `GetSetByCallerMagnitude`
- `AddOutputModifier`
- `BuildDamageBreakdown`

### Editor、测试与 Debug

1. 创建 DamageRule DataAsset、Execution Damage GE、Crit 测试角色和 Debug 面板。
2. 测试 Attack/Defense 0、高防御、Crit 0/1、Resistance 边界、缺少 SetByCaller、无效 Source/Target、相同种子。
3. 输出 Trace ID、捕获值、倍率、Crit Roll、Defense/Resistance 修正、Final Damage 和最终 Health。

### 常见错误

- Source/Target 捕获配反。
- Snapshot 选择错误。
- 公式散落在 Ability、ExecCalc 和 AttributeSet。
- ExecCalc 直接扣 Health 又输出 IncomingDamage。
- 全局随机不可复现。
- CritDamage 语义不一致。
- ExecCalc 保存成员状态。

### learning-journal、面试与作品集

- **记录：** Capture、Snapshot、MMC/ExecCalc、Meta Attribute、随机和原创公式。
- **面试：** Modifier/MMC/ExecCalc 如何选择；如何捕获属性；Snapshot；为什么输出 Meta Attribute。
- **作品集：** Damage Breakdown 同时展示 GAS、数值、测试和 Debug。

---

## GAS Stage 6：Buff、Debuff 与持续回合

### 阶段信息

- **对应 Phase：** Phase 9；实际学习顺序在 Stage 7 之后。
- **学习概念：** Duration/Infinite/Periodic GE、Stacking、Refresh、Immunity、命中/抵抗、自定义 TurnBasedStatusRuntime、回合触发和状态 UI。
- **为什么现在学：** Phase 8 已产生 Break Debuff 的真实用例，此时抽象通用 Status Runtime，避免过早设计。

### 回合状态双层模型

```text
StatusDefinition
→ FHSRTurnBasedStatusInstance
→ Infinite GameplayEffect
→ Modifier/Granted Tags
→ TurnManager 更新 RemainingTurns
→ 到期 RemoveActiveGameplayEffect
```

Duration/Periodic GE 使用世界秒数，不直接表示回合。回合 DoT 在 TurnStarted/Ended 时应用一次 Instant Damage GE。

### Stacking、Immunity 与命中抵抗

- Definition 明确 StackLimit、刷新/增加、Source 合并、到期移除策略。
- 初版只支持“共享剩余回合，重新应用时整体刷新”。
- Immunity 可由 Owned Tag、GE Application Immunity Query 或 StatusComponent 校验实现。
- Debuff 命中由 Base Chance、Source EffectHit、Target EffectResistance 和可复现 RandomStream 决定；成功后才应用 GE。

### 推荐文件、类与任务

```text
GAS/StatusEffect/HSRStatusComponent.*
GAS/StatusEffect/HSRStatusTypes.*
GAS/StatusEffect/HSRStatusResolver.*
Data/Definitions/HSRStatusDefinition.*
UI/HSRStatusViewModel.*
```

- `FHSRTurnBasedStatusInstance`：ID、Definition、Source/Target、RemainingTurns、StackCount、TriggerTiming、GE Handle、Battle ID。
- 最小任务：Attack Up、DoT、Break Debuff 和统一状态 UI。

### 关键 UPROPERTY / UFUNCTION

- Active Status Instances、Definition、Stack、RemainingTurns、GE Handle。
- `AddStatus`、`RefreshOrStackStatus`、`HandleTurnStarted/Ended`、`RemoveStatus`、`CheckImmunity`、`RollStatusApplication`。

### Editor、测试与 Debug

1. 创建 StatusDefinition、Infinite Modifier/Tag GE、Instant Trigger GE、图标和详情。
2. 测试首次应用、不同 Source、叠层上限、刷新、免疫、命中 0/100%、Turn Start/End、来源/目标死亡、战斗清理和驱散。
3. Debug 输出 StatusInstance ID、Source/Target、Stack、Turns、GE Handle、命中 Roll 和移除原因。

### 常见错误

- Duration 秒数或 Periodic 秒数当回合。
- Runtime/GE 只移除一个。
- Handle 丢失。
- Stack 与 RemainingTurns 不一致。
- UI 自己递减。
- 随机不可复现。

### learning-journal、面试与作品集

- **记录：** 三种 GE、世界/回合时间、Status+GE 双层模型、Stack/Refresh/Immunity、命中抵抗。
- **面试：** 如何实现持续 3 回合；为何不用 Duration=3；Infinite GE 移除；Stack 策略。
- **作品集：** 状态生命周期 Debug 面板。

---

## GAS Stage 7：弱点、韧性与击破

### 阶段信息

- **对应 Phase：** Phase 8。
- **学习概念：** Toughness/MaxToughness、Element/Weakness Tags、削韧 GE、Break Damage/Effect/Debuff、Turn Delay 和 GAS/TurnSystem 协作。
- **为什么现在学：** Damage Execution 已稳定，可增加 Toughness Damage 通道，并明确行动条不属于 GAS。

### 职责边界与流程

GAS 负责 Toughness Attribute、Weakness Tags、削韧/击破 GE 和 Break Debuff；TurnSystem 负责行动条、当前行动者、Turn Delay 和队列重排。

```text
Ability Element + Target Weakness
→ Toughness Damage GE
→ Toughness 归零
→ Break Resolution
├── Break Damage GE
├── Break Debuff Status
└── TurnManager ApplyTurnDelay
```

行动条使用 `FHSRTurnEntry` 和 `FHSRTurnDelayRequest`，不是 AttributeSet，也不每帧更新。

### 推荐文件、类型与 API

```text
GAS/Attribute/HSRCombatAttributeSet.*
GAS/Damage/HSRBreakExecutionCalculation.*
Battle/HSRWeaknessSystem.*
TurnSystem/HSRTurnManager.*
Data/Definitions/HSRElementDefinition.*
```

- `FGameplayTag ElementTag`、`FGameplayTagContainer WeaknessTags`
- `FHSRBreakResult`、`FHSRTurnDelayRequest`
- `ApplyToughnessDamage`、`ResolveBreak`、`ApplyTurnDelay`、`RestoreToughness`

### Editor、测试与 Debug

1. 创建原创 Element Tags、Weakness 配置、Toughness/Break GE、Break Debuff 和 UI。
2. 测试匹配/不匹配、恰好归零、过量削韧、重复 Break、Health 死亡同帧、Delay 排序、Debuff 免疫/抵抗。
3. Debug 输出 Element/Weakness、Toughness Before/After、Break 首次触发、Damage Breakdown 和 Turn Delay Before/After。

### 常见错误

- Ability 直接重排 TurnQueue。
- Weakness 使用显示字符串。
- Toughness/Health 共用错误 GE。
- Break 多次触发。
- 恢复时机不明确。
- 复制参考作品公式、命名和 UI。

### learning-journal、面试与作品集

- **记录：** Health/Toughness Damage、Tag 匹配、Break Resolution、行动条边界和原创规则。
- **面试：** 弱点为何适合 Tags；Delay 为何属于 TurnSystem；如何避免重复 Break；Toughness 是否应为 Attribute。
- **作品集：** 展示 GAS 与自定义回合框架协作。

---

## GAS Stage 8：UI 与 GAS

### 阶段信息

- **对应 Phase：** Phase 10。
- **学习概念：** Attribute Delegate、Tag Event、Active Effect Query、Ability Spec、Cost/Cooldown、状态 UI、ViewModel 和 UI Command。
- **为什么现在学：** Ability、Tags、Damage、Break 和 Status 已形成稳定事件源，可以建立统一 UI 数据层。

### UI 数据流

- Attribute：`GetGameplayAttributeValueChangeDelegate`。
- Tags：`RegisterGameplayTagEvent`。
- Active Effects：GameplayEffect Query；回合剩余仍从 Status Runtime 获取。
- Ability：ASC 查询 Spec，SkillDefinition 提供显示数据。
- 命令：`Widget → ViewModel → PlayerController → BattleActionCommand → Coordinator → ASC`。

UI 禁止 Apply GE、SetHealth、AddLooseTag、AdvanceTurn 和写 SaveGame。

### Cost/Cooldown 显示

- Energy Cost：SkillDefinition + 当前 Attribute。
- 战技点：Battle Resource Snapshot。
- 真实时间 Cooldown：Active GE/Tag。
- 回合 Cooldown：Turn Ability Runtime。
- 最终可用性以 Coordinator/ASC 验证为准。

### 推荐文件、类与 API

```text
UI/HSRBattleViewModel.*
UI/HSRAttributeViewModel.*
UI/HSRStatusViewModel.*
UI/HSRActionBarViewModel.*
UI/HSRUIEvents.*
```

- ASC/Coordinator 弱引用、Delegate Handles、Cached View State。
- `Initialize/Deinitialize`、`Bind/UnbindASC`、Attribute/Tag/Status Handlers、`SubmitAbilityCommand`、`BuildSkillButtonState`。

### Editor、测试与 Debug

1. 创建战斗 HUD、属性条、技能按钮、状态图标和行动条；蓝图只绑定 ViewModel 与动画。
2. 测试 ASC 未初始化、Actor 销毁、地图旅行、重复绑定、状态增删、Cost/Cooldown、Ability 阻止、重复点击和焦点。
3. Debug 输出绑定 ASC Owner/Avatar、Delegate Handle、View State 和 Battle/Participant ID。

### 常见错误

- Widget Tick 和业务真源。
- Widget 直接 Apply GE。
- 忘记解绑。
- 地图旅行后持有旧 ASC。
- UI 显示可用但拒绝时无反馈。
- 用 Active GE 秒数显示回合状态。

### learning-journal、面试与作品集

- **记录：** Delegate UI、Tag Event、Effect Query、ViewModel 生命周期、Command 边界和两种 Cooldown。
- **面试：** UMG 如何监听 Attribute/Tag/Buff；为何不直接改状态；地图旅行后的 ASC/UI 生命周期。
- **作品集：** 无 Tick、可解绑、跨地图安全的 GAS UI。

---

## GAS Stage 9：装备、遗器与属性来源

### 阶段信息

- **对应 Phase：** Phase 12。
- **学习概念：** Infinite GE、Effect Source、SetByCaller、Active Handle、属性来源、套装、Granted Ability 和 Save 后重建。
- **为什么现在学：** 基础 Attribute、Damage、Status 和 UI 已稳定，装备可以作为统一属性来源接入。

### HSR 使用流程

```text
Equipment/Relic Definition + Instance
→ StatAggregator 计算 Modifier
→ Infinite GE Spec + SetByCaller
→ 应用到 ASC
→ 保存 Instance ID → GE Handle Runtime 映射
→ 卸下按 Handle 精确移除
```

套装达到阈值时应用独立 Infinite GE；失去阈值时移除。被动 Ability 必须记录授予来源和 Spec Handle。

### Data/Runtime/Save 边界

- Definition：静态属性规则。
- Instance/Save：Definition ID、等级、词条和槽位。
- Runtime：属性来源、Active GE Handle、Granted Ability Handle。
- 不保存 ASC、GE Handle、Spec Handle 和临时最终 Attribute。
- 加载后重新计算并应用，必须幂等。

### 推荐文件、类与 API

```text
Equipment/HSREquipmentComponent.*
Equipment/HSRStatAggregator.*
Equipment/HSREquipmentTypes.*
Relic/HSRRelicSetResolver.*
Data/Definitions/HSREquipmentDefinition.*
Data/Definitions/HSRRelicDefinition.*
```

- Equipped Instance IDs、Runtime Effect Map、Set Handles。
- `Equip`、`Unequip`、`ValidateSlot`、`RebuildEquipmentEffects`、`Apply/RemoveEquipmentEffect`、`ResolveSetBonuses`、`BuildStatBreakdown`。

### Editor、测试与 Debug

1. 创建固定装备、固定遗器、通用属性 Infinite GE、套装 GE、SetByCaller Tags 和来源 UI。
2. 测试槽位、重复实例、换装/卸下、套装阈值、地图旅行、Save/Load、重复重建和 Definition 缺失。
3. Debug 输出 Instance ID → GE Handle、属性 Before/After、套装计数和 Active GE。

### 常见错误

- 打开 UI 时重复应用 GE。
- 按 Class 批量删掉其他装备 Effect。
- 丢失或保存 Handle。
- 最终 Attribute 与来源同时持久化。
- 套装 GE 重复。
- 装备蓝图直接修改 Attribute。

### learning-journal、面试与作品集

- **记录：** Infinite GE、SetByCaller、Handle、StatAggregator、Save 重建和套装来源。
- **面试：** 装备如何使用 GAS；如何安全移除；为何不保存 Handle；如何避免加载重复叠加。
- **作品集：** 属性来源 Breakdown、换装对比和 Save 重建。

---

## GAS Stage 10：未来联机扩展

### 阶段信息

- **对应 Phase：** Phase 19/20 后专项学习，不进入单机 MVP。
- **学习概念：** Character/PlayerState ASC、Owner/Avatar、Ability 权限、Attribute Replication、RepNotify、Full/Mixed/Minimal、Server Authority、Prediction、GameplayCue 复制和 Ability 同步。
- **为什么最后学：** 联机会同时放大所有权、生命周期、RPC、Ability、Attribute、Cue、Turn 权威和 UI 延迟问题。

### ASC 放置建议

| 放置 | Owner | Avatar | 适用 |
|---|---|---|---|
| Character | Character | Character | 当前单机玩家、敌人；简单直观 |
| PlayerState | PlayerState | 当前 Character | 未来玩家联机，跨死亡/重生/换 Pawn |
| Enemy Character | Enemy Character | Enemy Character | 联机敌人；按数量选择复制模式 |

当前：玩家/敌人 Character ASC，Battle Map 重建，不配置预测/RPC。未来：玩家迁移 PlayerState ASC，敌人保持 Character ASC，Ability 仅 Server 授予。

### Replication Mode

- **Full：** GameplayEffects 复制给所有客户端；信息最多，带宽最高。
- **Mixed：** 完整 Effects 主要给 Owner，Tags/Cues 给相关客户端；常用于玩家，Owner 链必须正确指向 Controller。
- **Minimal：** 不复制完整 Effects，只复制必要 Tags/Cues；常用于大量 AI。

AttributeSet 属性复制由 `UPROPERTY(ReplicatedUsing)`、`GetLifetimeReplicatedProps` 和 `GAMEPLAYATTRIBUTE_REPNOTIFY` 控制，不由 ASC Replication Mode 完全替代。

### 单机可暂缓

- ASC 网络复制验证。
- Attribute RepNotify。
- Server/Client RPC。
- Net Execution Policy。
- Prediction Key、Client Target Data。
- GameplayCue 网络去重。
- PlayerState ASC、断线重连和网络战斗状态。

### 当前必须预留

- UI 只提交 Command。
- Coordinator 统一验证。
- Ability 不信任 UI Target。
- Stable Participant/Battle/Action ID。
- SaveGame 与 PlayerState 分离。
- Ability 授予和 Actor Info 初始化集中管理。
- Attribute 修改只走 GAS。
- TurnManager 不由客户端推进。
- 表现与规则分离。

### 未来重构清单

1. 玩家 ASC 从 Character 移到 PlayerState。
2. 在 `PossessedBy`、`OnRep_PlayerState` 重建 Actor Info。
3. Attribute 加 RepNotify。
4. Ability 授予 Server Only。
5. ActionCommand 经 Server RPC 验证。
6. Battle/Turn 状态迁移到服务器权威可复制对象。
7. 选择玩家/敌人的 Replication Mode。
8. 加入 Prediction、确认和拒绝。
9. GameplayCue 防预测/确认重复。
10. UI 处理延迟、OnRep 和回滚。

### 推荐文件、API 与测试

未来文件：`HSRPlayerState.*`、`HSRReplicatedBattleState.*`、`HSRBattlePlayerCommand.*`；当前不创建。

未来 API：Replicated 属性、`GetLifetimeReplicatedProps`、`PossessedBy`、`OnRep_PlayerState`、`OnRep_Attribute`、`ServerSubmitBattleAction`、`ClientActionRejected`。

测试：PIE 多玩家、Dedicated Server、Latency/Packet Loss、重生/换 Pawn/地图旅行、Ability/Cost/Cooldown/Cue/Attribute 同步。

### 常见错误与 Debug

- Client 决定伤害。
- Mixed 模式 Owner 链错误。
- Attribute 忘记 RepNotify。
- Cue 重复。
- Client/Server 都授予 Ability。
- Avatar 更换后 Actor Info 未更新。
- SaveGame 与 PlayerState 混用。
- 每个客户端独立推进 Turn。

Debug 输出 Net Role、Owner、Avatar、Controller、Replication Mode、执行端、Prediction Key、Server/Client Attributes/Tags 和 Command ID。

### learning-journal、面试与作品集

- **记录：** Character/PlayerState ASC、三种模式、Attribute 复制、Server Authority、Prediction、Cue 复制和迁移清单。
- **面试：** 三种模式；为何 PlayerState ASC；重生时 Owner/Avatar；Attribute 复制；Prediction；Cue 重复；回合服务器权威。
- **作品集：** 展示单机到联机的迁移审查，同时说明为何没有过早实现。

---

## 统一 Stage 验收模板

每个 GAS Stage 完成时必须回答：

1. 这个概念解决了什么问题？
2. 为什么此阶段才引入？
3. HSR 中谁创建、拥有、使用和移除它？
4. 它属于 DataAsset、Runtime 还是 SaveGame？
5. 正常路径和失败路径是什么？
6. 对象销毁和地图旅行时会怎样？
7. UI 如何观察，是否存在 Tick？
8. 如何用日志、测试或 Debug Snapshot 验证？
9. 哪些复杂配置仍被推迟？
10. 能否脱离代码解释完整数据流？

只有能够解释并验证，才算学会；成功复制代码或让蓝图运行不能作为唯一完成标准。
