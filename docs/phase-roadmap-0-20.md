# HSR Phase 0–20 分阶段开发路线图

> 状态：规划已确认，尚未开始创建 UE 工程或实现 Gameplay 功能。每个 Phase 必须在前一阶段验收通过后才能实施。

## 总体决策

GAS 学习与项目 Phase 的详细映射见 [GAS 学习路线](gas-learning-roadmap.md)。实现每个 GAS Phase 前必须完成对应 Stage 的概念、最小实践、测试和 learning-journal 复盘。

- UE5.6 Blank C++ 模板、单 `HSR` Runtime 模块起步。
- 第三人称角色在 Phase 1 从零搭建，不依赖 Third Person 模板代码。
- 普通战斗与 Boss 战均进入独立 Battle Map。
- `UHSRBattleTransitionSubsystem` 携带稳定 ID 形式的 Encounter/Return Context；禁止跨关卡保存 Actor、ASC、Widget 或 Active GameplayEffect Handle。
- `UHSRBattleCoordinatorSubsystem` 管理 Battle Map 生命周期，`UHSRTurnManager` 管理行动队列与回合阶段。
- 每个 Battle Character Actor 持有 ASC；场景切换后根据 Profile、DataAsset 和 SaveGame 重建 Actor、ASC 与 GameplayEffect。
- DataAsset 保存静态定义，Runtime Instance 保存当前状态，SaveGame 保存跨会话状态。
- UI 只显示状态和提交命令；动画、音频、VFX 和 GameplayCue 不作为规则真源。
- Energy 是角色 GAS Attribute；战技点是 BattleCoordinator 管理的队伍 Runtime 资源。
- Phase 6 前使用简单伤害；Phase 7 正式引入 ExecutionCalculation。

## 跨阶段公共边界

- `FHSREncounterRequest`：玩家队伍 ID、敌方编组 ID、先手类型、探索地图和返回 Transform。
- `FHSRBattleActionCommand`：Battle ID、行动者 ID、Ability ID/Spec Handle、目标 ID。
- `FHSRBattleResult`：胜负、奖励来源、存活状态和返回上下文。
- `FHSRStatusInstance`：Status ID、来源、目标、层数、剩余回合、触发时机和 Active GE Handle。
- `FHSRRewardTransaction`：唯一事务 ID、奖励内容、应用结果和幂等状态。
- `FHSRProfileRuntime` 与 Save DTO：只保存稳定 ID、实例、数量、等级和进度。
- `IHSRInteractableInterface`：提示、可交互检查和交互请求。
- `UHSRBattleParticipantComponent`：战斗身份、队伍、ASC 和存活状态入口，不复制一份 Attribute 状态。
- `UHSRSaveSubsystem`：唯一磁盘读写入口。

## MVP 收敛范围

完整定义见 [MVP、第一月、第一周与第一轮计划](mvp-first-month-first-week-plan.md)。

- MVP 覆盖 Phase 0–5，并提前加入固定 Reward 与最小单槽位 Save 子集。
- MVP 只启用 1 名玩家对 1 名敌人，使用静止 Encounter Actor、基础攻击和确定性敌人行动。
- MVP 不包含 Skill、ExecutionCalculation、Break、Status、Growth、Equipment、Inventory、Quest、正式表现和多人。
- 第一月只完成 Phase 0、1、2 和 Phase 3 单对象交互，不开始 Phase 4 Enemy/Encounter 代码。
- 第一周只完成 Phase 0。
- 第一轮只维护 Markdown，不属于 Phase 0 工程实施。
- MVP 不等于 Phase 20；Phase 20 是三角色、Boss、任务和完整 Save/UI 的作品集垂直切片。

---

## Phase 0：项目初始化与规范落地

### 目标

- **阶段目标：** 创建可编译、可启动的 UE5.6 Blank C++ 项目，落实单模块、插件、Tags、目录和文档基线。
- **学习目标：** 理解 `.uproject`、Target、Module、Build.cs、UBT、UHT、Plugin 和 Config。
- **作品集目标：** 保存从空目录建立 UE C++ 项目的完整证据。
- **可见结果：** Editor 可打开空白 HSR 项目，不包含自定义游戏功能。

### 文件和职责

| 文件 | 职责 |
|---|---|
| `HSR.uproject` | 项目、单 Runtime 模块和插件状态 |
| `Source/HSR/HSR.Build.cs` | Core、Engine、EnhancedInput、GameplayAbilities、GameplayTags、GameplayTasks 依赖 |
| `Source/HSR/HSR.h/.cpp` | 模块入口和基础日志类别 |
| `Source/HSR.Target.cs`、`HSREditor.Target.cs` | Game/Editor 构建目标 |
| `Config/DefaultEngine.ini` | 地图、渲染和项目设置 |
| `Config/DefaultInput.ini` | Enhanced Input 类配置入口 |
| `Config/DefaultGameplayTags.ini` | 初始 Tag 命名空间 |
| `.agents/agents.md` | 长期规则，合并现有内容，不覆盖历史 |
| `README.md`、`worklog.md`、`todo_plan.md`、`learning-journal.md` | 项目入口、过程记忆、任务和学习复盘 |
| `docs/phase-0-project-setup.md`、`gas-notes.md`、`battle-system-design.md` | Phase 0、GAS 和战斗架构文档 |

- 核心类：只有模块入口。
- USTRUCT/UENUM/UPROPERTY/UFUNCTION：无 Gameplay 反射类型。
- GAS：确认 GameplayAbilities、GameplayTags、GameplayTasks；只规划根 Tags。
- Blueprint/UMG/DataAsset/SaveGame：不需要。
- Enhanced Input：确认模块和插件，不创建移动功能。
- Tick/GC/网络：不新增 Tick，无 Gameplay GC 风险，不启用复制。

### 实施、Editor 与验证

1. 使用 UE5.6 创建 Blank C++/Desktop 项目 `HSR`，关闭 Starter Content。
2. 验证 Visual Studio 2026、Windows SDK、UBT 和实际 C++ 标准。
3. 配置 Enhanced Input 与 GAS 依赖。
4. 建立 `Ability/State/Status/Event/Damage/Element/Cooldown/UI` 根 Tags。
5. 建立按需目录，不创建大量空类。
6. 在 Editor 中确认插件、Tags 和 Project Settings，创建 `Map_ProjectSetup`。
7. 构建 `HSREditor Development Win64`，打开 Editor 并执行空白 PIE。

测试：模块加载、插件加载、Tags 可见、项目重开、空白 PIE、Output Log 无未解释错误。

完成标准：构建和 Editor/PIE 验证通过；没有自定义 Gameplay 类；工具链和配置证据已写入文档。

风险与误区：插件启用不等于 Build.cs 已依赖；编译成功不等于 GAS 已初始化；本阶段不得提前实现 Phase 1。

文档：learning journal 记录 UBT/UHT/Module/Plugin；worklog 记录实际工具链和错误；README 保存运行方式和项目截图。后续进入 Phase 1。

---

## Phase 1：第三人称探索角色

### 目标

- **阶段目标：** 从 Blank 项目建立移动、镜头、Enhanced Input、基础动画参数和探索 HUD。
- **学习目标：** Character、PlayerController、Possession、Input Action、Camera 和 AnimInstance 职责。
- **作品集目标：** 展示从零搭建第三人称控制。
- **可见结果：** 玩家可在灰盒地图移动、观察和跳跃，HUD 显示占位状态。

### 文件、类型和接口

| 文件 | 职责 |
|---|---|
| `Character/HSRCharacterBase.*` | 角色公共基类和未来 ASC/Participant 扩展点 |
| `Character/HSRExplorationCharacter.*` | 移动、CameraBoom、FollowCamera |
| `Player/HSRPlayerController.*` | Mapping Context、ControlMode 和 UI 协调 |
| `Framework/HSRGameModeBase.*` | 默认 Character、Controller、HUD |
| `UI/HSRHUD.*`、`HSRUserWidget.*` | 探索根 Widget |
| `Content/Input/IA_*`、`IMC_Exploration` | Move、Look、Jump、Interact 输入资产 |
| `WBP_ExplorationHUD` | 占位 HUD |
| `docs/phase-1-exploration-character.md` | 输入与职责数据流 |

- `EHSRPlayerControlMode { Exploration, UIOnly, Battle }`。
- 关键 UPROPERTY：CameraBoom、FollowCamera、MappingContext、InputActions。
- 关键 UFUNCTION：`Move`、`Look`、`SetControlMode`、`CreateExplorationHUD`。
- GAS/DataAsset/SaveGame：不接入。
- Blueprint：只派生绑定 Mesh/Anim；UMG：占位 HUD。
- Enhanced Input：核心。
- Tick：禁止自定义 Character Tick；AnimBP 更新允许。
- GC：组件和资产使用 UPROPERTY/TObjectPtr。
- 网络：输入意图与移动职责分离，暂不复制。

### 实施与验收

1. 创建角色、Controller、GameMode 和 HUD。
2. 配置 Capsule、Movement、SpringArm 和 Camera。
3. 创建 IA/IMC 并由 PlayerController 添加/移除 Context。
4. 创建灰盒地图、地面和 PlayerStart。
5. 建立基础 AnimBP 参数与占位 HUD。
6. `IA_Interact` 只预留入口。

测试：移动方向、不同帧率、镜头、UIOnly 输入阻止、Context 不重复、重新 Possess。

完成标准：PIE 可稳定移动和观察；Character、Controller、HUD 职责清晰；无 Tick 输入轮询。

风险/误区：Character/Controller 重复绑定、Context 多次添加、输入模式未恢复；Controller 不是 Character。

文档：learning journal 记录 Possession 和 Enhanced Input；worklog 记录职责决策；README 保存控制说明和灰盒截图。后续 Phase 2 加 ASC。

---

## Phase 2：GAS 基础接入

### 目标

- **阶段目标：** 完成 ASC、AttributeSet、Health/MaxHealth、Energy/MaxEnergy、Speed、初始化 GE 和事件驱动 UI。
- **学习目标：** Owner/Avatar、Actor Info、AttributeSet、GE 和 Attribute Delegate。
- **作品集目标：** 展示最小 GAS 运行闭环。
- **可见结果：** HUD 显示属性，测试 Effect 能改变属性并自动刷新 UI。

### 文件与反射接口

| 文件 | 职责 |
|---|---|
| `GAS/HSRAbilitySystemComponent.*` | ASC 扩展和初始化保护 |
| `GAS/Attribute/HSRCoreAttributeSet.*` | Health、MaxHealth、Energy、MaxEnergy、Speed |
| `Character/HSRCharacterBase.*` | 持有 ASC、实现 `IAbilitySystemInterface` |
| `UI/HSRAttributeViewModel.*` | 绑定 Delegate 并提供 UI 数据 |
| `GE_InitializeCoreAttributes` | 初始化属性 |
| `WBP_AttributeDebug` | 属性调试 UI |

- `FHSRAttributeInitializationValues`。
- 关键 UPROPERTY：ASC、AttributeSet、InitialAttributesEffect、五个 `FGameplayAttributeData`。
- 关键 UFUNCTION：`InitializeAbilityActorInfo`、`ApplyInitialAttributes`、`BindAttributeDelegates`、`GetAbilitySystemComponent`。
- GAS：ASC、AttributeSet、初始化 GE、Delegate。
- Blueprint：绑定 GE；UMG：属性 Debug。
- DataAsset/SaveGame：暂不需要。
- Input：沿用 Phase 1；Tick：无。
- GC：ASC/AttributeSet 受 UPROPERTY 保护，Delegate 必须解绑。
- 网络：不复制，但集中封装初始化入口。

### 实施与验收

1. 加入 GAS 模块依赖。
2. CharacterBase 创建 ASC/AttributeSet 子对象。
3. 在正确生命周期执行 `InitAbilityActorInfo`。
4. 只应用一次初始化 GE。
5. AttributeSet 处理 Health/Energy 上下限。
6. ViewModel 绑定 Attribute Delegate，不使用 Widget Tick。

测试：重复初始化、Health/Energy 边界、Character 重建、Widget 销毁、无 ASC/GE 的失败路径。

完成标准：Actor Info 正确、属性只初始化一次、UI 随 Delegate 更新；不实现复杂技能。

风险/误区：忘记 Actor Info、AttributeSet 未注册、GE 与 SaveGame 混淆、UI Tick 轮询。

文档：gas-notes 记录完整数据流；learning journal 解释 Owner/Avatar；worklog 记录 ASC 放置决策；README 展示属性变化。

---

## Phase 3：交互系统与探索对象

### 目标

- **阶段目标：** 建立 NPC、宝箱和传送点共用的交互接口、组件和提示 UI。
- **学习目标：** Interface、ActorComponent、候选弱引用和命令边界。
- **作品集目标：** 展示可扩展交互，而不是按 Actor 类型硬编码。
- **可见结果：** 接近对象显示提示，按键触发三类占位反馈。

### 文件与接口

- `Interaction/HSRInteractableInterface.*`：提示、校验、执行协议。
- `Interaction/HSRInteractionComponent.*`：Overlap 候选、选择和提交。
- `Interaction/HSRInteractionTypes.*`：`FHSRInteractionContext/Result`、失败原因。
- `Exploration/HSRNPC.*`、`HSRTreasureChest.*`、`HSRTeleportPoint.*`。
- `WBP_InteractionPrompt`。

核心枚举：`EHSRInteractionType`、`EHSRInteractionFailureReason`。关键函数：`CanInteract`、`GetInteractionPrompt`、`ExecuteInteraction`、`RegisterCandidate`、`TryInteract`。

- GAS：默认不用，未来可读 Tags。
- Blueprint：三种对象派生；UMG：Prompt。
- DataAsset：MVP 可选 InteractionDefinition。
- SaveGame：只预留宝箱 Stable ID。
- Enhanced Input：使用 IA_Interact。
- Tick：禁止；Overlap 事件驱动。
- GC：候选使用 `TWeakObjectPtr`。
- 网络：交互请求未来可服务器验证。

### 实施与验收

创建接口/组件 → Character 加组件 → 配 Collision Channel → 创建三类 BP → Prompt UI → 测试地图。

测试：进入/离开、多候选稳定、目标销毁、宝箱重复交互、无 Tick、UI 不 Cast 具体业务类。

完成标准：三类对象使用同一协议，失败原因可记录。

文档：learning journal 记录 Interface/Component；worklog 记录 Overlap 方案；README 记录交互扩展方法。后续敌人 Encounter 接入。

---

## Phase 4：敌人探索表现与遇敌触发

### 目标

- **阶段目标：** 数据驱动敌人、巡逻占位、发现/追击和独立 Battle Map Encounter。
- **学习目标：** AIController、AI Perception、NavMesh、DataAsset、OpenLevel 生命周期。
- **作品集目标：** 展示探索 AI 与战斗系统解耦。
- **可见结果：** 敌人巡逻、发现玩家、追击并切换到空 Battle Map。

### 文件与类型

- `Enemy/HSREnemyCharacter.*`、`HSREnemyAIController.*`、`HSREnemyTypes.*`。
- `Data/Definitions/HSREnemyDefinition.*`、`HSREncounterDefinition.*`。
- `Battle/HSRBattleTransitionSubsystem.*`。
- `Map_BattleTest`。
- `EHSREnemyExplorationState`、`EHSREncounterInitiative`、`FHSREncounterRequest`。
- 关键函数：感知更新、巡逻/追击、`RequestEncounter`、`Set/ConsumePendingEncounter`。

- GAS：EnemyCharacter 复用 ASC，不实现技能。
- Blueprint：敌人表现、感知和 DataAsset 绑定。
- UMG：先手占位提示。
- DataAsset：必须；SaveGame：不落盘。
- Tick：不自定义；使用 AI MoveTo/感知事件。
- GC：Pending Context 只保存稳定 ID、软引用和 Transform，禁止 Actor 指针。
- 网络：EncounterRequest 是未来权威入口。

### 实施与验收

定义敌人/Encounter → AIController/感知 → NavMesh/巡逻 → 发现/追击 → 构建 Request → TransitionSubsystem → OpenLevel Battle Test → 验证 Context 和返回。

测试：丢失目标、重复触发、无 NavMesh/Definition/Map、三种先手、关卡切换后无旧 Actor 引用。

完成标准：探索 AI 与 Battle Map 解耦；Context 能跨关卡消费一次。

文档：learning journal 记录 AI/OpenLevel；worklog 记录独立 Battle Map Schema；README 记录 Encounter 演示。

---

## Phase 5：回合制战斗最小闭环

### 目标

- **阶段目标：** 独立 Battle Map 内完成队伍生成、速度排序、普攻、胜负和返回探索。
- **学习目标：** WorldSubsystem、UObject Outer、状态机、队列和 GAS 属性边界。
- **作品集目标：** 第一个端到端“探索 → 战斗 → 返回”闭环。
- **可见结果：** 1v1 轮流普攻，分出胜负并返回原位置。

### 文件与接口

- `Battle/HSRBattleCoordinatorSubsystem.*`：BattleManager 概念实现。
- `Battle/HSRBattleParticipantComponent.*`、`HSRBattleGameMode.*`、`HSRBattleTypes.*`。
- `TurnSystem/HSRTurnManager.*`、`HSRTurnTypes.*`。
- `Character/HSRBattleCharacter.*`。
- `GAS/Ability/HSRBasicAttackAbility.*` 和固定伤害 GE。
- `EHSRBattlePhase/Team/Outcome`。
- `FHSRBattleParticipantHandle`、`FHSRTurnEntry`、`FHSRBattleActionCommand`、`FHSRBattleResult`。
- 关键函数：初始化、注册、排序、提交命令、ActionResolved、胜负和返回。

- GAS：Participant 通过 ASC 读取 Attribute，不复制 HP；Battle Actor 重建 ASC。
- Blueprint：BattleCharacter/GameMode 配置。
- UMG：攻击按钮、HP、当前行动者。
- DataAsset：Character/Enemy/Encounter。
- SaveGame：暂不正式存盘。
- Input：Battle UI 命令；Tick：无。
- GC：Coordinator UPROPERTY 持有 TurnManager，TurnEntry 使用稳定 ID/弱引用。
- 网络：Coordinator 是未来服务器权威迁移点。

### 实施与验收

消费 Encounter → 生成双方 → 初始化 ASC → 注册 Participant → Speed 排序/稳定同速裁决 → 玩家/敌人普攻 → ActionResolved → 死亡/胜负 → Result → 返回探索。

测试：不同/相同 Speed、非当前行动者、死亡者、Ability 失败、Victory/Defeat、重复 Finish、Result 单次消费。

完成标准：独立 Battle Map 可完成可复现 1v1 闭环。

风险/误区：Participant 不复制 Attribute；TurnManager 不持有裸 Actor；动画/Ability 不直接掌管全局阶段。

文档：battle design、learning journal、worklog 和 README 架构图。后续扩展正式 Skill/Cost/Target。

---

## Phase 6：GAS 技能系统基础

### 目标

- **阶段目标：** 普攻、战技、终结技占位、治疗、Cost、Energy、战技点和目标类型。
- **学习目标：** Ability Spec、Commit、Cost GE、Target、Gameplay Event 和 Resolution。
- **作品集目标：** 展示 GAS 与队伍共享资源协作。
- **可见结果：** 玩家可选择四类 Ability，资源和目标变化可见。

### 文件、类型与决策

- `HSRGameplayAbilityBase`、`HSRBasicAttackAbility`、`HSRSkillAbility`、`HSRUltimateAbility`、`HSRHealAbility`。
- `HSRSkillDefinition`、`HSRBattleResourceState`、`HSRTargetingPolicy`、`WBP_BattleCommandPanel`。
- `EHSRSkillCategory`、`EHSRTargetType`。
- `FHSRAbilityResolution`、`FHSRTeamResourceState`。
- 关键函数：目标生成、Command 校验、Ability 激活/Commit、Damage/Healing GE、战技点事务和 Resolution。

Energy 是 GAS Attribute，由 GE 增减；终结技使用 GAS Cost。战技点是队伍 Runtime 资源，由 Coordinator 原子预留/提交/回滚；普攻成功增加，战技成功消费。

- GAS：Ability、Spec、Commit、Cost、GE、Tags、Event。
- Blueprint：可由 C++ GA 基类派生配置。
- UMG：技能命令面板和目标选择。
- DataAsset：SkillDefinition 必须。
- SaveGame：以后保存解锁/等级；战技点不保存。
- Input：UI 为主，可预留快捷键；Tick：无。
- GC：Definition/Ability 类引用受保护，异步表现不得悬挂。
- 网络：Command、目标和资源事务未来由服务器验证。

### 实施与验收

SkillDefinition → 可用技能列表 → 合法目标 → Command → Turn/ASC 校验 → Cost/战技点事务 → Damage/Heal GE → Resolution → UI/Turn 推进。

测试：Cost 不足、目标失效、资源上下限、普攻成功才加点、终结技只扣一次、治疗上限、取消/失败/成功区分、UI 伪造目标。

完成标准：四类 Ability 路径可运行，GAS Cost 与共享战技点边界清晰。

文档：gas-notes、learning journal、worklog 记录 Energy/战技点所有权；README 展示技能数据流。后续 Phase 7 统一伤害计算。

---

## Phase 7：属性、伤害公式与暴击

- **目标/学习/作品集/结果：** 增加 Attack、Defense、CritRate、CritDamage、DamageType 和可追踪公式；学习 Attribute Capture、ExecutionCalculation 和可复现随机；展示 Damage Breakdown。
- **文件/职责：** `HSRCombatAttributeSet.*` 扩属性；`HSRDamageExecutionCalculation.*` 统一计算；`HSRDamageTypes.*` 定义 Context/Result/Breakdown；`HSRDamageRuleDefinition.*` 保存原创系数。
- **类型/API：** `Damage.Type.*` Tags，`FHSRDamageContext/Result/Breakdown`；`BuildDamageContext`、`RollCritical`、Execution `Execute_Implementation`。
- **技术：** 正式使用 ExecutionCalculation；UI 显示 Crit/Breakdown；公式资产静态、单次结果 Runtime、不存单次伤害；无新输入/Tick；未来随机种子由服务器权威。
- **步骤/Editor：** 扩 Attribute → 建 Execution → 迁移普攻/战技 → Battle RandomStream → Damage GE/Rule DataAsset → Debug UI。
- **测试/完成：** 最低伤害、零/高防御、CritRate 0/1、无效 Source/Target、同种子一致；所有伤害只走统一入口且能解释每层。
- **风险/误区：** 不复制商业公式；CritDamage 语义唯一；ExecutionCalculation 不保存状态。
- **记录/扩展：** learning journal 记录 Capture；worklog 记录原创公式；README 展示 Breakdown；后续接弱点/抗性。

## Phase 8：弱点、韧性与击破

- **目标/学习/作品集/结果：** 原创元素、弱点、Toughness、Break、击破伤害/延后/Debuff；学习 Tags、Attribute 和 Turn Delay；UI 显示弱点与韧性条。
- **文件/类型：** CombatAttributeSet 增 Toughness/MaxToughness；`HSRWeaknessSystem.*`、`HSRBreakExecution.*`、`HSRElementDefinition.*`；`FGameplayTagContainer WeaknessTags`、`FHSRBreakResult`。
- **技术：** GE 负责削韧/击破伤害/Debuff，TurnManager 负责行动延后；Definition 静态，当前 Toughness/Delay Runtime，不存普通战斗状态；无 Tick。
- **步骤/Editor：** Element Tags → 弱点配置 → 属性/GE → Break Resolution → Turn Delay → 韧性 UI。
- **测试/完成：** 匹配/不匹配、恰好归零、重复击破、死亡同帧、延后排序；规则、UI 和 Debug 一致且原创。
- **风险/误区：** 不照搬参考数值/UI；Tag 不是显示文本；Ability 不直接重排 Turn 队列。
- **记录/扩展：** 记录 Tag 生产/消费和原创规则；Phase 9 接状态生命周期。

## Phase 9：Buff、Debuff 与状态效果

- **目标/学习/作品集/结果：** 状态添加、叠层、刷新、免疫、驱散、回合触发和 UI；学习 Infinite GE 与逻辑回合分离。
- **文件/类型：** `HSRStatusComponent.*`、`HSRStatusTypes.*`、`HSRStatusDefinition.*`；`EHSRStatusTriggerTiming/RefreshPolicy`、`FHSRStatusInstance`。
- **技术：** Infinite GE 提供属性/Tags；Runtime Status 保存回合、层数、Source/Target、GE Handle；只有明确跨战斗状态才保存 Definition ID/剩余回合；无 Tick。
- **步骤/Editor：** Definition → Add/Stack/Refresh → TurnStarted/Ended → Remove → Buff/DoT/Break Debuff → 图标/详情。
- **测试/完成：** 叠层上限、刷新、覆盖、免疫、驱散、来源死亡、战斗清理；至少一个 Buff、DoT 和击破 Debuff。
- **风险/误区：** GAS 秒数不等于回合；UI 不自行递减；Handle 不能丢失。
- **记录/扩展：** gas-notes、battle design、learning/worklog；后续命中/抵抗。

## Phase 10：战斗 UI 完整化

- **目标/学习/作品集/结果：** 行动条、技能按钮/描述、目标选择、双方状态、Buff、伤害数字和胜负页；学习事件驱动 Widget 生命周期。
- **文件/类型：** `HSRBattleHUD.*`、`HSRBattleViewModel.*`、`HSRActionBarViewModel.*` 和 `WBP_Battle*`；`FHSRBattleViewState/SkillButtonState/TargetViewState`。
- **技术：** UI 监听 Attribute/Ability/Status 事件，只提交 Command；Definition 提供图标/文本；ViewState Runtime，不保存；无 Widget Tick；Delegate 成对解绑。
- **步骤/Editor：** ViewModel → Widget 分层 → 行动条/命令/目标 → 状态/伤害 → 结果页 → 键鼠/手柄焦点。
- **测试/完成：** Actor 销毁、目标变化、Ability 禁用、状态增删、分辨率、输入模式；全部 UI 由事件更新。
- **风险/误区：** 重复绑定、动画期间重复命令、Widget 写业务状态。
- **记录/扩展：** 更新 ui-guidelines、worklog 和作品集截图；后续复用 UI 栈。

## Phase 11：角色数据与成长

- **目标/学习/作品集/结果：** CharacterDataAsset、等级、经验、突破、技能等级、被动、队伍和角色详情。
- **文件/类型：** `HSRCharacterDefinition.*`、`HSRProgressionSubsystem.*`、`HSRPartySubsystem.*`、最小 `HSRSaveGame/SaveSubsystem`；`FHSRCharacterRuntimeState/SavedCharacter/PartySlot`。
- **技术：** CharacterDefinition/CurveTable 静态；Runtime 最终缓存；Save 等级/EXP/突破/技能/队伍；StatAggregator 用 GE 重建属性；无 Tick，资源用软引用。
- **步骤/Editor：** 角色定义/曲线 → Profile Runtime → 经验/升级 → 属性重算 → 队伍 → 角色 UI → 最小存档。
- **测试/完成：** 跨多级、曲线缺失、重复重算、队伍重复、加载重建 ASC；至少两个占位角色可编队。
- **风险/误区：** 不保存临时最终值、不修改 DataAsset、防重复 GE。
- **记录/扩展：** 成长曲线、来源顺序和 Save Schema；后续装备进入同一 Aggregator。

## Phase 12：装备与遗器系统

- **目标/学习/作品集/结果：** Equipment/Relic DataAsset、实例、槽位、主副词条、套装、强化、更换和重算。
- **文件/类型：** `HSREquipmentComponent.*`、`HSRStatAggregator.*`、`HSRRelicSetResolver.*`；`FHSREquipmentInstance/RelicInstance/StatModifier`、`EHSREquipmentSlot`。
- **技术：** 每个来源对应可移除 Infinite GE；Definition 静态、Instance/Save 持久、Handle Runtime；UI 显示比较/套装/来源；无 Tick。
- **步骤/Editor：** Definition → 实例/槽位 → Equip 事务 → GE 重建 → 套装 → 强化 → UI。
- **测试/完成：** 错误槽位、重复实例、卸下、失败回滚、套装阈值、加载重建；固定装备和一套固定遗器可用。
- **风险/误区：** 内存事务成功后再写盘；禁止从最终 Attribute 反推来源。
- **记录/扩展：** data/save 文档和 worklog；随机副词条推迟。

## Phase 13：背包、奖励与掉落

- **目标/学习/作品集/结果：** ItemDataAsset、物品实例、奖励表、敌人/宝箱/战斗奖励和背包 UI。
- **文件/类型：** `HSRInventorySubsystem.*`、`HSRItemTypes.*`、`HSRRewardSubsystem.*`、`HSRRewardTransaction.*`；Item/Reward/Drop DataAssets。
- **技术：** 消耗品可触发 GAS Ability/GE，奖励本身不依赖 GAS；Runtime 索引/事务；Save 数量/实例/已领取 ID；UI 显示来源。
- **步骤/Editor：** Item Definition → Inventory → 幂等奖励事务 → Enemy/Chest/Battle 接入 → 背包/结算 UI。
- **测试/完成：** 堆叠、实例、容量策略、重复事务、随机种子、存档往返；所有奖励统一入口。
- **风险/误区：** 显示名不是 ID；避免重复发奖；随机可复现。
- **记录/扩展：** 奖励事务和背包 Schema；后续商店复用事务。

## Phase 14：任务与对话系统

- **目标/学习/作品集/结果：** QuestDataAsset/RuntimeState、DialogueDataAsset、NPC 对话、分支、目标、奖励和保存。
- **文件/类型：** `HSRQuestSubsystem.*`、`HSRQuestTypes.*`、`HSRDialogueSubsystem.*`；`EHSRQuestState/ObjectiveType`、`FHSRQuestRuntimeState/DomainEvent/DialogueNode`。
- **技术：** 类型化事件推进任务；GAS 可提供 Battle/Tag 事件但不是任务数据库；Definition 静态、Runtime 活动状态、Save 进度/选择/领奖。
- **步骤/Editor：** Quest/Dialogue 定义 → NPC 接入 → 事件监听 → 分支 → 任务奖励 → UI/Debug。
- **测试/完成：** 重复接受、事件过滤、分支条件、幂等奖励、存档恢复；完成一个带分支的单任务闭环。
- **风险/误区：** 避免无类型 Event Bus、Widget 硬编码文本和重复领奖。
- **记录/扩展：** 任务事件表、对话规范、作品集任务流程。

## Phase 15：地图、传送与关卡流程

- **目标/学习/作品集/结果：** MapDataAsset、传送点、区域解锁、场景切换、位置/探索状态和地图 UI。
- **文件/类型：** `HSRMapSubsystem.*`、`HSRMapDefinition.*`、`HSRTeleportDefinition.*`、TransitionSubsystem 扩展；`FHSRMapLocation/ReturnContext/TeleportRequest`。
- **策略：** 普通/Boss 战都进入独立 Battle Map；GameInstanceSubsystem 携带稳定 ID；Battle Actor 重建；返回恢复 Map/Transform/Result。
- **技术：** 旅行前清理 Battle Runtime，加载后重建 ASC；Save 当前地图、位置、区域、传送点、探索状态；无 Actor 跨图引用。
- **测试/完成：** 两探索地图和一 Battle Map 稳定往返；无效 ID、加载失败、战斗中断、位置恢复、Result 单次消费。
- **风险/误区：** OpenLevel 销毁 Actor；GI 不保存 Actor；Pending Context 必须可清理。
- **记录/扩展：** 关卡生命周期图、返回上下文和恢复策略。

## Phase 16：存档系统总整合

- **目标/学习/作品集/结果：** 完整 SaveGame、版本、兼容、备份和失败恢复。
- **文件/类型：** `HSRSaveGame.*`、`HSRSaveSubsystem.*`、`HSRSaveTypes.*`、`HSRSaveVersion.*`、`save-system-design.md`；`FHSRSaveHeader/ProfileSaveData` 和系统 DTO。
- **内容：** 地图/位置、角色/队伍、背包、装备/遗器、任务、宝箱、持久敌人和区域；设置使用 GameUserSettings/独立配置。
- **GAS/Data：** 不保存 ASC/AttributeSet/GE Handle；Definition ID → Runtime Profile → 版本化 DTO；加载后重建。
- **测试/完成：** 空/损坏/旧版本、写入中断、重复加载、Definition 缺失、完整往返；可恢复垂直切片状态。
- **风险/误区：** 不保存 UObject 指针；逐版本迁移；成功前保留旧文件。
- **记录/扩展：** Schema、版本历史、迁移和测试证据。

## Phase 17：UI 总整合

- **目标/学习/作品集/结果：** 主菜单、暂停、角色、队伍、背包、任务、地图、设置和统一导航。
- **文件/类型：** `HSRUIManagerSubsystem.*`、`HSRScreenStack.*`、`HSRInputModeCoordinator.*` 和 WBP；`EHSRUIScreenLayer`、`FHSRScreenRequest`。
- **技术：** 角色/战斗页只读 GAS；Style/Icon Data 静态；Screen Stack Runtime；键鼠/手柄导航；无 Tick；Delegate 生命周期严格管理。
- **测试/完成：** 层级返回、暂停、焦点、分辨率、InputMode、地图旅行后重建；所有主界面可导航。
- **风险/误区：** 多系统争抢 InputMode；Widget 不拥有业务数据。
- **记录/扩展：** UI 导航图、可访问性和演示路径。

## Phase 18：表现层打磨与模型导入

- **目标/学习/作品集/结果：** 合法模型、骨骼/动画适配、Montage、技能镜头、音效、VFX、UI 动效、受击反馈和简单过场。
- **授权流程：** 保存授权证据 → 检查商用/修改/署名/再分发 → `ModResources` 隔离测试 → 骨骼/材质/动画验证 → 授权通过后迁移 `ThirdParty/<Package>` → 更新台账。
- **技术：** AnimationSet/Presentation DataAsset、GameplayCue、Camera Sequence、Sound/VFX 映射；只消费表现事件，不创建规则真源；不保存 Montage/Cue 实例。
- **测试/完成：** 重定向、Root Motion、Montage 中断、Cue 重复、授权、低画质、缺失资产回退；占位可逐项替换且不影响规则。
- **风险/误区：** 免费不等于可公开；Notify/Cue 不决定伤害；第三方源文件可能不可提交。
- **记录/扩展：** 授权台账、替换清单、署名和资产来源。

## Phase 19：Debug、GM 工具与测试

- **目标/学习/作品集/结果：** 快速战斗、经验/角色/物品/装备、任务重置、地图传送和 GAS/伤害/行动条可视化。
- **文件/类型：** `HSRDebugCommands.*`、`HSRDebugSnapshot.*`、`HSRBattleLog.*`、Automation Tests；`FHSRDebugBattleSnapshot/DamageTraceEntry`。
- **命令：** StartBattle、GrantExperience/Character/Item/Equipment、ResetQuest、TravelToMap、DumpASC/Effects/TurnOrder/Damage。
- **技术：** Debug Preset DataAsset；测试会话 Runtime；默认不写正式存档；Development/Editor 可用，Shipping 禁用。
- **测试覆盖：** Turn、Damage、Status、Reward、Inventory/Equipment、Quest、Save、Encounter/Return Context。
- **完成标准：** 核心问题可由快照和日志定位，不依赖猜测。
- **风险/误区：** Debug 不进入 Shipping、不污染正式存档、不泄露敏感路径。
- **记录/扩展：** Debug 文档、覆盖表、真实 Bug 和作品集演示。

## Phase 20：垂直切片 Demo

### 内容

- 1 个完整探索地图、1 个 NPC、1 条带分支任务、1 个宝箱、1 个传送点。
- 1 组普通敌人、1 个 Boss。
- 3 名完整角色，架构支持第 4 名；每名至少有普攻、战技和终结技。
- 弱点/韧性/击破、Buff/Debuff、普通装备、一个遗器套装。
- 背包、任务奖励、Save/Load 和从主菜单到任务完成的完整流程。

### 技术与文件

- 内容集中在 `Content/Maps/VerticalSlice`、`Content/Data/VerticalSlice` 和原创/合法授权资产。
- Phase 20 不引入新架构，只修复既有系统缺口。
- Showcase Definition 保存演示起点、角色、任务和检查点。
- 使用独立 Demo Save。
- GAS 承载角色/Boss 技能、状态和装备效果；所有定义 DataAsset 化；Runtime 与 Save 边界保持不变。
- UI 覆盖主菜单、探索、战斗、角色、背包、任务、地图、设置和结果页。
- 不因打磨引入无依据 Tick；执行 Widget、软引用、Cue、动画和异步加载 GC 审查。
- 只做未来联机架构审查，不实现联机。

### 验收流程

新存档 → NPC/接任务 → 宝箱 → 普通战 → 奖励/成长/换装 → 传送 → Boss 战 → 多角色/击破/状态 → 完成任务 → Save/Exit/Load → 验证进度 → Debug Overlay 展示。

完成标准：干净存档可重复完成；无阻断错误、重复奖励、任务死锁和存档损坏；Development Editor、Development 包和目标 Shipping 配置通过；授权清单、README、架构图、视频、截图、技术亮点、个人贡献和限制完整。

### 作品集清单

- 30–90 秒核心视频。
- Encounter/独立 Battle Map/返回数据流图。
- GAS/TurnSystem 职责图。
- DataAsset/Runtime/SaveGame 分层图。
- Ability/Damage/Status/UI Delegate 数据流。
- Debug Snapshot 和 Damage Breakdown。
- 自动化测试与性能摘要。
- 第三方授权/署名、个人贡献、真实 Bug 和后续计划。

---

## 全局阶段门禁

- Phase 0–6 每阶段必须独立编译、PIE 验证并更新文档。
- Phase 7–20 只有在依赖阶段验收后开始。
- Phase 5 数组结构支持队伍，但 MVP 只启用 1v1。
- Phase 11 引入最小 Save 基线，Phase 16 完成完整版本化和兼容。
- 第一阶段全部使用灰盒、UE 默认或原创占位资源。
- 每阶段结束更新 `todo_plan.md`、`worklog.md`、`learning-journal.md`、README 和对应 phase 文档。
