# HSR 开发指南 / 资产配置手册

给我和后续开发 agent 参阅：各功能在后端如何实现，以及在编辑器里要怎么建、怎么配
资产才能真正接上后端跑起来。

基线 commit `60673a7`。配套文档：收尾规划见
[`multi-member-battle-completion-plan.md`](multi-member-battle-completion-plan.md)；
架构设计意图见 [`battle-system-design.md`](battle-system-design.md)；
数据资产的分层**原则**（本文讲的是具体字段）见
[`data-asset-guidelines.md`](data-asset-guidelines.md)。

## 怎么用这份文档

按你要做的事查：

| 我想… | 看哪节 |
|---|---|
| 加一个技能 | [技能定义](#技能定义) |
| 加一个敌人 / 一场遭遇 | [敌人与遭遇](#敌人与遭遇) |
| 让某个角色进队伍并有自己的技能 | [角色与技能 loadout](#角色与技能-loadout) |
| 加 Buff / Debuff | [状态定义](#状态定义) |
| 加元素与弱点 | [元素与弱点标签](#元素与弱点标签) |
| 配伤害公式 | [伤害规则](#伤害规则) |
| 加装备 / 遗器套装 | [装备与遗器](#装备与遗器) |
| 建战斗地图的 GameMode | [战斗世界装配](#战斗世界装配) |
| 建 UI 控件接后端数据 | [前端接入契约](#前端接入契约) |
| 改动涉及存档 | [存档与 schema](#存档与-schema) |
| 跑构建与测试 | [构建与验证](#构建与验证) |

## 总体架构：数据往哪个方向流

四层单向流动，反向依赖是缺陷而不是选项：

```
授权资产 (Content/Data 下的 DataAsset)
    ↓  只读，运行时从不写回
运行时权威 (Coordinator / TurnManager / 各 Subsystem)
    ↓  只发布纯值 DTO
ViewState (无任何指针)
    ↓  ViewModel 广播
UMG 控件
```

三条支撑这个结构的不变量，改代码时必须保持：

1. **跨子系统边界只传纯值结构**（`FHSRBattleParticipant`、
   `FHSRBattleCommandViewState`、`FHSRAbilityResolution`、`FHSRDamageResult`）。
   不传 Actor、不传 ASC、不传 UObject。这让快照、回放和测试都无需构造世界。
2. **UI 永不持有 Actor 或 ASC 引用**。UI 要什么数值，由权威侧读好写进 ViewState。
   曾出现过 ViewModel 自行订阅 ASC 属性委托的实现，那是对权威侧已有读取的重复，
   已移除。
3. **Coordinator 是战斗状态的唯一真源**。TurnManager、StatusComponent、
   participants 全部由它拥有与协调，不要在旁路新建第二条权威路径。

## 战斗世界装配

战斗地图的 GameMode 蓝图（`AHSRBattleGameMode` 子类）是资产与后端的接线板。
编辑器里要填的槽（属性定义见 `Source/HSR/Battle/HSRBattleGameMode.h`）：

| 分类 | 槽位 | 作用 |
|---|---|---|
| `Battle\|Initialization` | `CharacterCatalog` | 队伍成员的角色定义总表，见下节 |
| | `PlayerCharacterId` | 默认主控角色 id，默认 `Character.A` |
| | `ParticipantInitializationGameplayEffect` | 给参与者铺初始属性的 GE |
| | `CharacterProgressionGameplayEffect` | 把等级/成长投射成属性的 GE |
| `Battle\|Definitions` | `EnemyDefinition` | 敌方定义 |
| `Battle\|Skills` | 各技能定义槽 + `HealSkillDefinition` | 默认 loadout 来源 |
| `Battle\|Status` | 状态定义槽 | 状态、持续伤害、击破状态等 |
| `Battle\|UI` | UI 类槽 | 命令面板控件类 |
| `Development\|*` | 各 harness 布尔 | 仅开发期契约自检，见下文 |

`Development|P7`…`P11` 那批布尔是**开发期测试开关**，用于不进 PIE 就跑契约自检。
正式内容不要依赖它们，打包配置也不应开启。

## 角色与技能 loadout

这里有个最容易配错的地方。`UHSRCharacterCatalog::Characters` 的类型是
`TArray<TSubclassOf<UHSRCharacterDefinition>>`（`Source/HSR/Data/Definitions/HSRCharacterCatalog.h`）
—— 它收的是**类**，不是 DataAsset 实例，授权数据从 CDO 读取。

所以正确做法是：以 `HSRCharacterDefinition` 为父类建 **data-only Blueprint** 子类，
在子类默认值里填角色数据，再把这个**类**加进 catalog 的 `Characters` 数组。
如果建成了 DataAsset 实例，它无法被填进这个数组；如果建成普通 Blueprint 而非
data-only，数据仍从 CDO 读，行为上能跑但偏离约定。

catalog 由 `RegisterLoadedCatalog` 一次性注册进 `UHSRCharacterProfileSubsystem`
（`HSRBattleGameMode.cpp:1565`）。注意 `:1560` 的前置守卫：Profiles 子系统缺失、
catalog 未填、或 `PlayerCharacterId` 为 None 时，整段注册**直接跳过且不报错**。
症状是队伍为空或角色属性全默认值，却看不到明显错误 —— 排查时先确认这三项都填了。

技能 loadout 的解析顺序：`UHSRBattleCoordinator::GetSkillLoadoutFor` 先找该参与者的
per-participant override，没有则回退到 default loadout。要做到「每人技能不同」，
就为每个参与者设置 override；不设的成员会共用 GameMode 上那套默认技能，
这也正是「组队后看起来只有一个人在打」时期的遗留表现之一。

## 前端接入契约

UI 只读 `FHSRBattleCommandViewState`（`Source/HSR/UI/HSRBattleCommandTypes.h`），
由 `UHSRBattleCommandViewModel` 广播 `OnChanged()`。控件订阅 ViewModel，
不要反向去抓 GameMode 或 Coordinator。

几个字段的语义容易误读，它们都用「已知/未知」标志与「零值」区分开：

- `bEnergyCostIsKnown` 为 false 表示 `EnergyCost` **未知**，不表示免费。
- `bHasAttributes` 为 false 表示该参与者的数值从未从 ASC 读到过，不表示数值为零。
  UI 在这种情况下应留空而不是渲染 `0 / 0`。
- `SkillPointCost` 恒为非负的消耗量；要判断增减看带符号的 `SkillPointDelta`。
- 查技能优先用 `FindSkill(SkillId)`。`FindSkillByCategory` 是首个匹配即返回，
  同类别有多个技能时无法定位第二个，只为遗留四按钮 UI 保留。

命令提交走 `IHSRBattleCommandSink`（`Source/HSR/UI/HSRBattleCommandSink.h`），
这是 UI 被允许看到的全部权威面 —— 只有「当前战斗 id」和「提交命令」两个方法。

拉条数据见 `FHSRTurnForecastEntry`（`Source/HSR/Data/HSRBreakTypes.h`），
契约与建控件注意事项写在收尾规划的 P0-2 节。

## 构建与验证

命令以 CLAUDE.md 为准，这里只强调三条最容易出假结论的：

- **在 worktree 里工作时，必须用本工作树自己的 `.uproject`**，否则会静默编译并
  验证主检出的代码。破绽是刚改完多个文件却立刻得到 `Target is up to date`。
- **测试进程即使有失败也返回 0**。必须 grep 日志里的
  `Result={Success}` / `Result={Fail}`，不能相信退出码。
- **单次 `-ExecCmds` 只接受第一个 `RunTests`**，多个套件要分多次跑。

Live Coding 开启时构建会失败（`Unable to build while Live Coding is active`），
先关编辑器或按 `Ctrl+Alt+F11`。

## 字段表怎么读

下面每节列的是**必填项与容易踩的坑**，不是字段全集（全集看头文件）。
重点标注三类失败模式，因为它们的排查难度完全不同：

- **静默失效** —— 不报错，功能就是不发生。最难查，本文优先标注这类。
- **静默跳过** —— 该条目被丢弃并留一行 log，其他条目正常。
- **构建中止** —— 整场战斗建不起来。最吵但最好查。

## 技能定义

`Source/HSR/Data/HSRSkillDefinition.h`。这是最容易配错的资产。

**必填，否则技能不会出现在命令面板（静默跳过）：**

| 字段 | 规则 |
|---|---|
| `SkillId` | 缺失则整条被丢弃，log `SkillLoadout SKIP InvalidDefinition`（`HSRBattleGameMode.cpp:80-85`） |
| `AbilityClass` | 同上，两者由 `IsValidDefinition()` 一起校验（`HSRSkillDefinition.h:97-100`） |
| `Category` + `TargetType` | 必须是合法配对，见下 |

`Category` 与 `TargetType` 的合法配对（`HSRSkillDefinition.h:133-148`）：
BasicAttack / Skill / Ultimate 都要求 `SingleEnemy`；Heal 要求 `SingleAlly` 或 `Self`。
**配错的症状是技能整个从面板消失**，不是报错。

**必填，否则按钮出现但一按就失败：**

`EffectGameplayEffectClass`、`DamageRule`、`DamageType`、`AbilityMultiplier > 0`
（伤害预检 `HSRBattleCoordinator.cpp:685-694`）。

这里有一个**唯一会「按钮可见但永远无效」**的组合：BasicAttack 在面板过滤时容忍
`EffectGameplayEffectClass` 为空，但执行时不容忍（`UsesPreparedDamage()` 覆盖
BasicAttack，`HSRSkillDefinition.h:260-265`），于是按钮正常显示、点下去报
`DefinitionMissing`。配 BasicAttack 时务必填 Effect GE。

**Ultimate 额外要求，失败会中止整场战斗构建：**

`CostGameplayEffectClass` 与 `EnergyRefundGameplayEffectClass` 都必填，且结构被严格
校验（`HSRUltimateAbility.cpp:37-59`）：cost 必须是**负的** Energy 加法修正；refund
必须是 Instant 且**正的** Energy 加法修正，**数值恰好等于 cost**。不符会让
`GrantSingleSkill` 失败并连带 `BuildParticipants` 拆掉整场战斗
（`HSRBattleCoordinator.cpp:2594-2599`、`:434-445`）。这是全系统最严厉的失败模式 ——
好在它吵，不会静默。

**`SkillPointDelta` 与 `SkillPointCost` 的关系（易误解）：**

只有 `SkillPointDelta` 是授权字段，`SkillPointCost` 是派生的
（`HSRSkillDefinition.h:176-180`）。`Delta` 为 `0` 时**不表示不消耗**，而是回退到
按类别的历史默认：BasicAttack `+1`、Skill `-1`、其余 `0`（`:158-173`）。
推论：**你无法把一个 `Skill` 类别的技能显式配成零消耗** —— 填 0 会重新启用 `-1`。
真要零消耗，得换类别或改这段回退规则。

**能量显示（`DisplayEnergyCost`）：**

资产上**没有** `EnergyCost` 字段，授权的是 `DisplayEnergyCost`（`:58`），纯显示用，
实际扣费权威仍在 GAS。`TryGetDisplayEnergyCost`（`:192-239`）优先用授权值，否则去
反射 cost GE，且只在「恰好一个静态负值 Energy 加法修正」时才给出答案 ——
曲线驱动、SetByCaller、多修正、软类未加载都返回 false，UI 随即整行隐藏而不是显示 0
（对应 ViewState 的 `bEnergyCostIsKnown`）。
**症状**：cost GE 用了 SetByCaller 而 `DisplayEnergyCost` 留 0 时，终结技按钮**完全
不显示能量消耗**，但扣费正常。

**`EnergyGain` 有个反直觉点：** 默认 `20.0`，且只在**已准备伤害**那条路径提交
（`HSRBattleCoordinator.cpp:754`）；Heal 路径从不调用（`:938-948`）。所以一个 Heal
技能即便 `EnergyGain` 是默认 20 也**不产生能量**，很容易误判成 bug。

**元素与韧性（`ElementTag` + `ToughnessDamage`）：** 校验上都可选，但要产生任何
韧性/击破效果就必须**成对**填。`ElementTag` 必须是真实的 `Element.*` 标签，
`ToughnessDamage` 必须有限且 `> 0`，且 `ToughnessDamageGameplayEffectClass` 必填
（否则 `MissingGameplayEffect`）。失败都只体现在 `P8-002 Toughness ... FailureReason=N`
日志里（`HSRBattleCoordinator.cpp:829-833`）—— **伤害照常结算，韧性不动，击破不触发**。

**安全可省：** `DisplayName` 回退成 `SkillId`，`Description` 回退成生成的占位文本并置
`bDescriptionIsPlaceholder = true`（`HSRBattleCoordinator.cpp:1261-1265`）。
`AppliedStatuses` 为空正常，无法加载的条目告警后继续，被拒绝的施加（免疫/满层）
不会回滚伤害。

## 元素与弱点标签

推导规则是纯字符串约定（`FHSRToughnessConfiguration::GetWeaknessTagFor`,
`Source/HSR/Data/HSRBreakTypes.h:134-152`）：校验输入是 `Element.*`，砍掉前缀，
再请求 `Weakness.<Leaf>`。

**最重要的一条坑：** 只授权 `Element.X` 而漏了 `Weakness.X`，请求会返回无效标签 ——
**没有报错、没有 ensure、没有日志**。调用方当作「无弱点匹配」处理，只在
`FailureReason=3` 与 `ExpectedWeakness=<invalid>` 的日志里能看出来。伤害照常，
韧性不动，击破不触发，**看起来跟「这个敌人本来就不吃这个元素」一模一样**。

所以**加一个元素要改两行 ini，不是一行**。

匹配用 `HasTagExact`，标签层级不起作用 —— 敌人身上挂父级 `Weakness` 不会匹配
`Weakness.Arc`。

`Config/DefaultGameplayTags.ini` 当前授权（三对，完全配对）：

| 元素 | 弱点 |
|---|---|
| `Element.Arc` | `Weakness.Arc` |
| `Element.Gale` | `Weakness.Gale` |
| `Element.Tide` | `Weakness.Tide` |

## 敌人与遭遇

**`UHSREnemyDefinition`**（`Source/HSR/Data/Definitions/HSREnemyDefinition.h`）

- `EnemyDefinitionId`（`:26`）必填，且必须与遭遇资产的 `EnemyDefinitionId`
  **一致**，否则战斗构建直接失败 `DefinitionNotFound`（`HSRBattleCoordinator.cpp:363-366`）。
  这是「战斗地图加载出来但什么都没发生」的经典原因。
- `WeaknessTags`（`:42`）为空 → 每次攻击都报 `EmptyWeaknesses`，韧性永不下降，
  除 P8-002 日志外无任何提示。
- `InitialToughness` / `InitialMaxToughness`（`:45`、`:48`）**这条路径没有校验**，
  直接写成属性基值。两者都默认 `1.0`，意味着一次弱点命中就击破 —— 做切片够用，
  做正式内容会意外。若把 `InitialToughness` 配得大于上限，会在 GE 应用审计处
  夹断并报 `EffectApplicationFailed`（`:813-822`）。
- `EncounterDefinition`（`:39`）探索触发战斗必填；为空只留一条告警，遭遇**永不触发**
  （`HSREnemyAIController.cpp:353-359`）。
- `DisplayName`（`:33`）可省，回退到参与者 id。roster 授权的名字优先于定义
  （`HSRBattleCoordinator.cpp:421-424`），`Portrait` 同理。
- `LoseSightRadius` 会被强制抬到 `>= SightRadius`（`HSREnemyAIController.cpp:230-231`），
  配得更小会被**静默忽略**而不是报错。
- **`PatrolWaitTime` 与 `ChaseAcceptanceRadius` 全仓库没有任何消费者** —— 填了不起作用。
- `BehaviorTreeAsset` 与 `BlackboardAsset` 必须一致：BT 自己的 Blackboard 必须等于
  定义里的，否则树根本不启动（`HSREnemyAIController.cpp:396-399`）。

**`UHSREncounterDefinition`**（`Source/HSR/Data/Definitions/HSREncounterDefinition.h`）

- `EncounterId`、`EnemyDefinitionId` 必填；`BattleMap` 必填**且包必须真实存在于磁盘**
  （`FPackageName::DoesPackageExist` 校验，失败为 `InvalidMap`，
  `HSRBattleTransitionSubsystem.cpp:154-164`）。
- **奖励字段是全有或全无，且以 `VictoryRewardDefinition` 为闸门。** 它填了，
  `RewardDropTable` 也必须填（否则 `reward bundle is incomplete`，`:273-280`）；
  它为空时，`RewardSeed`、`VictoryExperience`、`RewardItemDefinitions` **根本不会被读**
  （`:188-193`）。只配种子和经验却不配奖励定义是**静默空操作** —— 奖励类最容易犯的错。
- 掉落表与奖励 `FixedItems` 里引用的每个物品都必须是已知物品，否则
  `UnknownDropItem`（`HSRRewardSubsystem.cpp:208-226`）。

## 状态定义

`Source/HSR/Data/Definitions/HSRStatusDefinition.h`。好消息：`Validate()` 在每次施加时
都跑（`HSRStatusComponent.cpp:69`），所以坏资产会带着**具体枚举**在施加时失败，
不是静默。

- `StatusId` 必须以 `"Status."` 开头且长度大于 7，**并且 `GrantedStatusTag` 的标签名
  必须与 `StatusId` 完全相等**（`HSRStatusDefinition.cpp:10-13`）。两个字段一个真值，
  不一致就是 `InvalidDefinition`。这条最不直观，也最常踩。
- **叠层是硬性跨字段规则**（`:29-31`）：`RefreshDuration` 要求 `MaxStacks == 1`；
  `AddStack` 要求 `MaxStacks >= 2`。其他组合一律 `InvalidPolicy`。
  把 `MaxStacks` 调成 3 却忘了改策略，状态会彻底失效。
- `TriggerTiming` 只支持 `TargetTurnEnded`；枚举里的 `Unsupported` 存在的意义就是被拒绝。
- **Buff / Debuff 不对称**（`:26-27`）：Buff 必须保持 `bDispellable` 为 false **且**
  不设 `ImmunityTag`；Debuff **必须**有合法 `ImmunityTag`。编辑器的 `EditCondition`
  会对 Buff 隐藏这两个字段 —— 逻辑一致，但意味着从 Buff 复制出 Debuff 的作者
  得记住一个自己从没被提示过的字段。
- `InfiniteGameplayEffectClass` 必填且 `DurationPolicy` 必须是 `Infinite`
  （否则 `GameplayEffectNotInfinite`）。持续回合由组件的回合计数器跟踪，不是 GAS。
- DoT：`EffectKind == DamageOverTime` 要求 `DamageType`、`DamageRule`、
  `DamageGameplayEffectClass`、`DamageAbilityMultiplier > 0` **且 `Classification`
  必须是 Debuff**（`:37-39`）。反过来 `TagOnly` 要求这些**一个都不能设**（`:40`）——
  从别的资产复制时残留的伤害字段会让 tag-only 状态失效。
- `DurationTurns` 默认 2，必须 `>= 1`。`DisplayName` 可省，回退到 `StatusId`。

## 角色与技能 loadout（字段部分）

`Source/HSR/Data/Definitions/HSRCharacterDefinition.h`。建资产的方式见上文
[角色与技能 loadout](#角色与技能-loadout)（要用 data-only Blueprint 子类）。

- `CharacterId` 必填且在 catalog 内唯一。
- `CharacterClass` 必填且必须派生自 `AHSRCharacterBase`；不符会跳过该成员，
  若是队长则整个 setup 中止（`HSRBattleGameMode.cpp:1712-1717`、`:1688-1691`）。
- **`CumulativeExperienceCurve` 必填且约束很紧**：必须能加载，且**从 2 到 `MaxLevel`
  的每一个整数等级都要有显式 key**，每个值是非负整数、严格递增、小于 2^31
  （`HSRCharacterProgressionLibrary.cpp:40-51`）。插值出来的值不算 —— 校验用
  零容差 `FindKey`。曲线稀疏的症状是 catalog 注册失败、角色引导以 `CatalogConflict`
  中止，于是**整支队伍不可用**，而不是单个角色出问题。
- `MaxLevel` 默认 80，必须 `>= 1`。
- `SkillDefinitions`（`:34`）是每角色 loadout 的授权面。`SkillMaxLevels`（`:38`）是
  另一套仅供成长系统用的映射，**两者没有任何交叉校验**，可以静默漂移。

**loadout 解析优先级**（`HSRBattleCoordinator.cpp:2514-2527`）：per-participant 覆盖
优先，**但仅当它解析出至少一条**；解析成空会回退到默认列表，而不是留一个空面板。

默认列表来自 GameMode 的 `DefaultSkillLoadout`；若该数组没有任何非空条目，则回退到
四个历史命名槽（BasicAttack / Skill / Ultimate / Heal，按此展示顺序，
`HSRBattleGameMode.cpp:1526-1538`）。默认列表去重按 `SkillId`**后写覆盖**，
覆盖列表内去重则**先写保留**。

**敌人永远不会有覆盖**，始终跑默认列表。

**参与者 loadout 解析为空是致命的**，不是外观问题：`GrantSkillLoadout` 返回 false
并让 `BuildParticipants` 拆掉整场战斗（`:2566-2570`、`:434-445`）。

## 伤害规则

`Source/HSR/GAS/Damage/HSRDamageRuleDefinition.h`。两个字段都有可用默认值
（`DefenseCoefficient 0.5`、`MinDamage 1.0`），编辑器夹断与冻结契约一致
（系数 `0..100`，最小伤害 `0..1000000`），所以唯一的失败方式是 NaN。
无效规则或加载失败的 `DamageRule` 引用会让行动被拒为 `DefinitionMissing`，
DoT 路径上则是 `MissingDamageRule`。

## 装备与遗器

- `UHSRRelicDefinition.DefinitionId` 必填且不可重复。`EnhancementCap` 允许为 `0`
  （默认值），只有负数被拒 —— **cap 为 0 意味着这件遗器根本不能强化**。
- `Slot`（默认 `Head`）决定装备是否合法，配错表现为 `InvalidSlot` / `SlotOccupied`。
- 遗器上的 `SetId` 是与套装的连接键。`SetId` 匹配不到任何已注册套装**不是错误**，
  它只是永远不计入任何套装效果（`HSRRelicSetResolver.cpp:29-33`）。
- `UHSRRelicSetDefinition.Threshold` 默认 2，夹断 1..6，必须 `> 0`。
  **`SetGameplayEffectClass` 是解析器的硬要求** —— GE 为空的套装会被整个跳过，
  永远无法激活（`HSRRelicSetResolver.cpp:23`）。

## 物品

`Source/HSR/Data/Definitions/HSRItemDefinition.h`

- `ItemId` 必填。`MaxStack` 必须 `>= 1`，且 **`Unique` 要求 `MaxStack` 恰好为 1** ——
  `Unique` 配默认的 99 会被拒为 `InvalidDefinition`。这是要盯的配对。
- `StorageKind` 决定物品能走哪套 API，错配返回 `StorageKindMismatch`。
- 同一 `ItemId` 用**不同**的 `StorageKind`/`MaxStack` 重复注册是 `DuplicateDefinitionId`；
  完全相同则是无害的 `NoOp`。两个遭遇把同一物品配成不同堆叠上限会让 bundle 注册失败。
- 这个资产上的 `DisplayName` 没有运行时消费者；物品显示文本来自
  `UHSRInventoryCatalog`，而那个资产**要求 `DisplayName` 非空**。

## 存档与 schema

当前 schema 版本 **9**（`HSRSaveVersion.h:21`、`:31`），最低兼容 1。

**队伍宽度是磁盘格式的一部分**，按 schema 钉死为历史字面量而不跟随运行时容量：
`LegacyPartySlotCount = 2`、`PartySlotCount = 4`、
`PartySlotCountForSchema(Schema) { return Schema >= 9 ? 4 : 2; }`（`:39-41`）。
注释写得很直白：schema 9 写出的 payload **永远**是四槽宽。

唯一的关卡是 `HSRSaveVersion.h:48` 的 `static_assert`：
`PartySlotCountForSchema(CurrentSchema) == HSRPartyCapacity`。它的失败信息本身就写明了
扩容流程。所以把队伍从 4 扩到 5 的成本是明确的三件套：改 `HSRPartyTypes.h:11`、
加 schema 10 分支、写 9→10 迁移。

**不要为了让 assert 通过而删掉 assert** —— 它是有意的防线，绕过它会让磁盘格式产生
歧义（旧档按新宽度解析）。

任何改动只要动到存档结构，都要走 `data-asset-guidelines.md` 的 Schema 变更清单。
