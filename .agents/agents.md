# HSR - Agents Context

## 1. 项目基本信息

| 项目 | 约定 |
|---|---|
| 项目名称 | HSR |
| 项目代号 | 本地学习代号，不代表最终公开名称 |
| 项目类型 | 原创单机 JRPG、探索与回合制战斗游戏 |
| 引擎版本 | Unreal Engine 5.6 |
| 主 IDE | Visual Studio 2026 |
| 主要语言 | C++ |
| C++ 标准 | 跟随 UE5.6 与 UnrealBuildTool 默认设置；生成项目后以实际 Target/Module 配置为准 |
| 首要平台 | Windows |
| 跨平台策略 | Windows 优先，避免无必要的平台绑定，不要求第一阶段验证 Linux |
| 输入系统 | Enhanced Input |
| Gameplay 框架 | Gameplay Ability System（GAS） |
| 当前联网范围 | 单机，不实现会话、RPC 或状态复制 |
| 未来扩展 | 保留职责、数据所有权和权威边界，不承诺可直接升级为联机 |
| 蓝图边界 | 配置、派生类、动画、资源绑定和 UI 表现 |
| 当前目标 | 完成“探索 → 遭遇 → 1v1 回合战斗 → 奖励”的灰盒闭环 |
| 长期目标 | 逐步构建原创 JRPG 的主要系统，不包含限时运营活动 |
| 学习目标 | UE5.6 C++、GAS、UMG、DataAsset、SaveGame、回合制架构和 RPG 数据设计 |
| 作品集目标 | 保留架构说明、验证证据、学习复盘和原创可演示 Demo |

本机已定位 Visual Studio Community 2026 与 UE5.6 安装，但尚未正式创建工程或完成 UBT/Editor/PIE 验证。Phase 0 必须通过实际生成、编译和启动确认兼容性；若不支持，先报告证据，再由用户决定是否切换工具链。

## 2. 项目代号与版权说明

- `HSR` 只是本地学习代号。
- 可以参考商业 JRPG 的系统类别、玩法抽象和工程组织方式，但不能复制受保护的具体表达。
- 禁止复制角色、世界观、剧情、文本、音乐、语音、图标、UI 版式、商标、模型、动作、特效、专有名词和数值表。
- 弱点、韧性、击破等通用机制必须使用原创命名、公式、表现和数据。
- 公开展示、上传 GitHub 或进入作品集前，应改用原创项目名称并执行版权审查。
- 第三方资源必须记录来源、作者、许可证、署名要求、商用范围、修改范围和再分发条件。
- 无法确认授权的资源只能用于本地临时测试，不得提交、打包或公开展示。

## 3. 项目目标

### 长期目标

逐步实现原创单机 JRPG 的探索、交互、遭遇、回合制战斗、技能、属性、状态、角色成长、装备、遗器、背包、奖励、敌人 AI、任务、对话、地图传送、UI、存档和 Debug 工具。

### 中期系统原型

- 数据驱动的玩家角色和敌人定义。
- 可复用的战斗参与者接口。
- 回合顺序、行动选择、技能执行和结算状态机。
- GAS 属性、技能、效果、标签和 UI 数据流。
- 简化且原创的弱点、韧性和击破机制。
- 基础奖励、背包、装备和存档。
- 至少一个任务、一次对话和一个小型灰盒区域。

### 短期 MVP

在探索灰盒地图与独立 Battle Map 之间完成：

1. 玩家通过 Enhanced Input 移动。
2. 玩家接近或交互触发一个敌人遭遇。
3. 系统切换到战斗状态并锁定探索输入。
4. 玩家和敌人按速度生成行动顺序。
5. 玩家可以选择一次基础攻击。
6. 基础攻击通过 GAS 造成伤害。
7. 敌人执行一个确定性的简单行动。
8. 任意一方生命归零后结束战斗。
9. 玩家胜利后获得占位奖励。
10. 系统返回探索状态。

MVP 不包含复杂队伍编成、复杂元素反应、装备随机词条、完整存档、联网和正式美术。独立 Battle Map 的跨关卡 Context 只保存稳定 ID、地图和 Transform，不保存 Actor 指针。

## 4. 技术栈

- Unreal Engine 5.6。
- C++ 为核心实现语言。
- Enhanced Input、Gameplay Ability System、Gameplay Tags、UMG。
- DataAsset、PrimaryDataAsset、DataTable、CurveTable 和 SaveGame。
- UE Automation Tests、PIE 和编辑器人工验证。
- 灰盒、引擎默认资源和原创占位 UI。
- 核心模块优先保持单 Runtime Module；只有形成明确独立边界后才考虑插件化。

## 5. GAS 学习与使用原则

### 为什么使用 GAS

GAS 提供技能激活、属性、效果、标签、消耗、冷却和表现通知等通用能力，适合长期 RPG 项目，也具有较高的 UE 工程和面试学习价值。

### 初学者挑战

- ASC 的 Owner Actor 与 Avatar Actor 容易混淆。
- AttributeSet、GameplayEffect 和 GameplayAbility 的职责容易混用。
- Gameplay Tags 容易缺少统一命名。
- GAS 原生持续时间按现实时间工作，不天然等同于“回合数”。
- 复制、预测和 GameplayCue 会显著增加初期复杂度。
- 编辑器资产、C++ 类和初始化顺序之间存在多处连接点。

### 分阶段学习顺序

1. 初始化 ASC 和 Actor Info。
2. 创建最小 AttributeSet。
3. 使用初始化 GameplayEffect 写入基础属性。
4. 创建基础攻击 GameplayAbility。
5. 使用瞬时 GameplayEffect 造成固定伤害。
6. 通过 Attribute Change Delegate 更新 UI。
7. 引入 Gameplay Tags 和状态限制。
8. 加入 Cost、Cooldown 和 GameplayCue。
9. 再引入 ExecutionCalculation 和完整伤害公式。
10. 最后研究复制、预测和多人所有权。

### 核心对象职责

- `AbilitySystemComponent`：保存可授予技能、活动效果、标签和属性访问入口。
- `AttributeSet`：保存受 GAS 管理的运行时属性，不承担完整战斗流程。
- `GameplayAbility`：表达一个可激活行动的条件和执行过程。
- `GameplayEffect`：表达属性修改、状态、消耗、增益和减益。
- `Gameplay Tags`：表达能力、状态、元素、伤害、事件、冷却和 UI 语义。
- `GameplayCue`：只承担视觉和音频表现，不决定权威战斗结果。
- `Cost`：技能消耗；基础攻击第一版可以无消耗。
- `Cooldown`：回合制冷却优先由回合计数语义管理，不假设 GE 秒数等于回合数。
- `Attribute Change Delegate`：驱动 UI 和非权威表现，避免 Widget Tick 轮询。
- `ExecutionCalculation`：简单 Modifier 验证完成后再引入，用于集中处理复杂伤害公式。

### ASC 放置决策

第一阶段为每个战斗参与角色持有一个 ASC，玩家和敌人采用一致的战斗参与者抽象。暂不因未来联机而把玩家 ASC 强制放入 PlayerState。持久成长数据由数据定义和 SaveGame 管理，不把场景 Actor 当作跨存档真源。

### 回合制中的特殊处理

- GAS 负责“角色能做什么”和“行动产生什么效果”。
- TurnSystem 负责“谁现在可以行动、行动顺序、等待状态和结算阶段”。
- 只有当前行动者可以提交战斗行动。
- 回合状态机不能塞进单个 GameplayAbility。
- 持续 N 回合的状态不得直接依赖真实秒数；由回合系统在边界事件中更新或移除。
- 技能动画和 GameplayCue 完成不能成为唯一的战斗规则真源。
- 第一版敌人行动应确定且可复现，便于 Debug。

### Gameplay Tags 初始命名空间

- `Ability.*`
- `State.*`
- `Status.*`
- `Event.*`
- `Damage.*`
- `Element.*`
- `Weakness.*`
- `Cooldown.*`
- `UI.*`

新增 Tag 前必须检查是否已有等价语义，并记录其生产者、消费者和生命周期。

### 单机阶段可以暂缓的内容

- ASC 和属性的网络复制。
- Server/Client RPC。
- Prediction Key 和客户端预测。
- RepNotify 属性同步。
- 多客户端 GameplayCue 同步。
- PlayerState ASC 所有权模型。
- 多人权威回合状态。

### 未来联机风险

未来联机前必须重新审查 ASC 的 Owner/Avatar 与生命周期、Ability 授予权限、属性复制、EffectContext、TurnSystem 权威性、客户端预测、GameplayCue 重复播放，以及 SaveGame 与 PlayerState 的边界。

## 6. 蓝图使用边界

蓝图可以用于：

- C++ 基类的内容派生。
- Mesh、动画、材质、音频、VFX 和输入资产绑定。
- UMG 布局和视觉表现。
- DataAsset 实例配置。
- Sequencer 或表现性流程。
- 快速验证纯表现方案。

蓝图不得默认承担：

- 回合状态机、伤害和属性公式真源。
- 存档协议、核心奖励结算和全局任务状态。
- GAS 初始化和关键生命周期。
- 只有蓝图可见而 C++ 无法验证的隐藏规则。

如果必须在蓝图实现核心逻辑，需要明确说明原因、边界和后续迁移策略。

## 7. 资产与素材使用原则

- 第一阶段使用灰盒、引擎默认资源和原创占位 UI。
- 第三方资源统一放在 `Content/ThirdParty`，不得混入原创目录。
- `Content/ModResources` 仅用于本地隔离实验，默认不提交、不打包、不公开。
- 每项外部资源维护许可证记录。
- 不直接修改第三方原始资源；需要修改时复制到项目自有目录并记录来源。
- 使用外部平台资源前，必须检查下载授权、署名、商用、二创和再分发条款。
- 不使用来源不明的“提取资源”。
- 公开 Demo 前执行一次资产授权审计。

## 8. 推荐目录结构

目录按需建立，不在第一天创建大量空目录。

```text
Source/HSR/
├─ Character/
├─ Player/
├─ Framework/
├─ Exploration/
├─ Interaction/
├─ Enemy/
├─ Battle/
├─ TurnSystem/
├─ GAS/
│  ├─ Ability/
│  ├─ Attribute/
│  ├─ StatusEffect/
│  └─ Damage/
├─ Quest/
├─ Dialogue/
├─ Inventory/
├─ Equipment/
├─ Relic/
├─ Reward/
├─ Data/
├─ Save/
├─ UI/
├─ Debug/
└─ Developer/

Content/
├─ Blueprints/
├─ Characters/
├─ Enemies/
├─ UI/
├─ Maps/
├─ Data/
├─ Animations/
├─ Materials/
├─ Audio/
├─ VFX/
├─ Icons/
├─ Placeholder/
├─ ThirdParty/
├─ ModResources/
└─ Developer/

docs/
├─ phase-N-xxx.md
├─ gas-notes.md
├─ battle-system-design.md
├─ data-asset-guidelines.md
├─ ui-guidelines.md
├─ debug-tools.md
└─ interview-review/
```

## 9. 核心系统规划

| 系统 | 第一阶段边界 | 长期方向 |
|---|---|---|
| 探索 | 移动、镜头、战斗输入切换 | 小型区域、传送、场景状态 |
| 交互 | 接近或按键触发敌人 | 通用接口、提示和交互类型 |
| 遭遇 | 单个敌人进入战斗 | 敌群、先手和特殊遭遇 |
| 战斗 | 1v1 状态机 | 多角色、多目标和行动队列 |
| 回合顺序 | 按速度排序 | 行动延迟、插队和状态修正 |
| GAS 技能 | 基础攻击 | 技能、终结技、被动 |
| GAS 属性 | HP、攻击、防御、速度 | 能量、效果命中等原创属性 |
| GameplayEffect 状态 | 瞬时伤害 | Buff、Debuff、持续回合 |
| 弱点/韧性/击破 | MVP 后加入简化原型 | 原创元素和击破规则 |
| 角色成长 | 不进入 MVP | 等级、升级和能力解锁 |
| 装备 | 不进入 MVP | 数据驱动装备与属性汇总 |
| 遗器 | 不进入 MVP | 原创套装和词条规则 |
| 背包 | 只记录占位奖励 | 堆叠、分类、使用和丢弃 |
| 奖励 | 胜利后获得固定占位物品 | 掉落表和结算表现 |
| 敌人 AI | 固定选择基础攻击 | 条件评分和行为配置 |
| 任务 | 不进入 MVP | 状态机、目标和奖励 |
| 对话 | 不进入 MVP | 节点、条件和本地化 |
| 地图与传送 | 单地图 | 多区域和传送点 |
| UI | HP、行动提示、攻击按钮 | 完整战斗和养成界面 |
| 数据驱动 | 最小角色/敌人定义 | Primary Asset 与表格体系 |
| 存档 | MVP 后实现 | 版本化存档和迁移 |
| Debug/GM | 日志和最小状态输出 | 指令、战斗模拟和数据检查 |

## 10. AI 行为规则

1. 优先采用 UE5.6 和 C++ 方案。
2. 面向初学者时按“概念 → UE 职责 → 最小实现 → 数据流 → 验证”解释。
3. 实现前先列出计划修改的文件和每个文件的职责。
4. 改动必须小而明确，避免一次重构大量文件。
5. 不编造 UE5.6 API；不确定时检查实际头文件、官方文档或编译错误。
6. 代码必须说明需要在编辑器中完成的资产绑定和配置。
7. 核心逻辑与表现逻辑分离，优先事件和委托，避免轮询。
8. 先完成最小可验证版本，再引入抽象层。
9. 不把所有逻辑集中到 Character、GameMode 或单个 Subsystem。
10. 删除、覆盖、批量移动、Git 提交或推送前必须取得用户许可。
11. 用户指定 Plan、只读或禁止写入时，不得以日志维护规则为理由修改文件。
12. 每次声明完成前必须说明实际验证了什么、没有验证什么。
13. 使用 Skill 前必须确认当前环境确实存在并已加载，不能假装调用尚未创建的 Skill。

## 11. UE 反射系统检查清单

- `UCLASS`、`USTRUCT`、`UENUM`、`UINTERFACE` 是否正确。
- 每个反射类型是否包含 `GENERATED_BODY()`。
- `*.generated.h` 是否为头文件最后一个 include。
- UObject/Actor/Component 成员引用是否需要 `UPROPERTY()`。
- 是否应使用 `TObjectPtr`、`TWeakObjectPtr` 或 `TSoftObjectPtr`。
- `UFUNCTION` 的 Blueprint、Callable、Pure、Event 和网络标记是否必要。
- `UPROPERTY` 的编辑、可见、蓝图访问和 Category 是否最小化。
- 玩家文本是否使用 `FText`，稳定名称是否使用 `FName`。
- 反射类型是否位于 UHT 可处理的位置。
- Blueprint 暴露是否会破坏 C++ 不变量。
- 枚举和结构体是否具有安全默认值。
- 是否混淆 UObject 构造、CDO 和运行时初始化。

## 12. GAS 检查清单

- ASC 的 Owner Actor 和 Avatar Actor 是谁。
- `InitAbilityActorInfo` 在何时、由谁调用。
- Ability 由谁授予，是否可能重复授予。
- AttributeSet 是否有明确默认值来源。
- GameplayEffect 是 Instant、Duration 还是 Infinite。
- Tag 的生产者、消费者和移除时机。
- 激活条件、阻止标签和取消规则。
- Cost 与 Cooldown 是否符合回合制语义。
- 是否把真实时间错误地当成回合数。
- UI 是否通过 Delegate 更新。
- GameplayCue 是否只负责表现。
- Damage 公式应使用 Modifier 还是 ExecutionCalculation。
- EffectContext 是否保存必要来源信息。
- 失败路径是否有日志或失败标签。
- 当前功能是否真的需要复制或预测。
- 测试是否覆盖激活成功、拒绝、伤害、死亡和状态清理。

## 13. GC 与 UObject 安全规则

- UObject 引用必须明确是强引用、弱引用还是软引用。
- 需要阻止 GC 的 UObject 成员使用 `UPROPERTY`/`TObjectPtr`。
- 可失效的观察引用优先使用 `TWeakObjectPtr`。
- 资源引用按加载需求考虑 `TSoftObjectPtr` 或 `TSoftClassPtr`。
- 非 UObject 数据不使用 GC 语义。
- 不缓存未经生命周期保护的裸 UObject 指针。
- 委托订阅者销毁时确认解绑或使用生命周期安全绑定。
- Timer、异步任务和 Lambda 不得捕获可能已经销毁的对象。
- `IsValid` 不是设计生命周期的替代品。
- `NewObject`、`CreateDefaultSubobject` 和 `SpawnActor` 必须在正确生命周期使用。
- 不在构造函数中访问依赖 World 或运行时状态的对象。

## 14. Tick 与性能规则

- Actor 和 Component 默认关闭 Tick。
- UI 不使用 Tick 轮询属性。
- 优先 Delegate、Gameplay Event、Timer、状态切换和动画通知。
- 使用 Tick 时必须说明事件方式为何不适用、Tick 频率、启停条件、每帧复杂度和对象数量。
- 不在 Tick 中反复查找 Actor、加载资源、创建对象或执行复杂容器遍历。
- 优化必须基于 Unreal Insights、Stat 命令或可复现实测。

## 15. 网络同步检查清单

第一阶段为单机，此清单默认标记为“不适用”，不得为未来假设实现 Steam、会话或复制代码。

未来进入联机设计时必须重新回答：

- 哪个对象拥有状态；哪些逻辑由 Server 决定，哪些只在 Client 表现。
- 哪些属性需要 `Replicated`/`ReplicatedUsing`，是否注册到 `GetLifetimeReplicatedProps`。
- 是否需要 Server、Client、NetMulticast RPC 或 `OnRep`。
- PlayerController、PlayerState、GameState 和 GameMode 的职责是否正确。
- TurnSystem 的权威状态在哪里。
- ASC、AttributeSet、Ability 和 GameplayCue 如何复制。
- 客户端是否可能重复提交行动。
- 断线、重连、延迟和预测是否改变规则。
- UI 是否只读取本地可访问的数据。

## 16. C++ 代码风格要求

- 遵循 UE 前缀：`A`、`U`、`F`、`E`、`I`；布尔值使用 `b` 前缀。
- 使用 `nullptr`，不使用 `NULL`。
- 使用 UE 类型和容器：`FString`、`FName`、`FText`、`TArray`、`TMap`。
- 头文件优先前向声明，`.cpp` include 完整定义；include 保持最小且顺序稳定。
- 可配置默认值优先 `EditDefaultsOnly`，运行时内部状态不随意暴露为 `BlueprintReadWrite`。
- 玩家可见文本使用 `FText`；日志使用项目自定义 Log Category 和 `UE_LOG`。
- 函数参数和成员尽可能表达 `const`；枚举、结构和属性必须有安全默认值。
- 不把战斗、交互、存档、UI 和输入全部放入 Character。
- GameMode 负责当前规则和流程入口。
- GameState 只在确有共享世界状态职责时使用。
- PlayerController 负责玩家输入、选择和本地 UI 协调。
- PlayerState 用于玩家身份或未来共享状态，不替代 SaveGame。
- GameInstance/Subsystem 用于具有明确跨关卡生命周期的服务。
- SaveGame 负责持久化快照，不直接承担运行时业务逻辑。
- 注释解释原因和约束，不重复代码字面含义。

## 17. 数据驱动设计规则

- 角色、敌人、技能、物品和奖励使用稳定 ID，不以显示名称作为主键。
- `PrimaryDataAsset` 用于需要独立身份、资源引用和资产管理的定义。
- `DataTable` 用于大量结构一致的扁平数据；`CurveTable` 用于成长曲线。
- Gameplay Tags 用于可组合语义，不替代所有数据字段。
- 运行时状态与静态定义分离。
- UI 从定义和运行时数据接口读取，不直接硬编码数值。
- 资产之间避免无边界的硬引用链。
- 数据结构变更必须评估旧资产和 SaveGame 兼容性。
- 伤害公式、成长曲线和掉落规则必须有原创设计记录，不复制商业游戏数值表。
- 数据资产规范变更时同步更新 `docs/data-asset-guidelines.md`。

## 18. 文档维护规则

| 文件 | 职责 | 更新时机与格式 | 每轮 | 阶段末 | 学习复盘 | 作品集 |
|---|---|---|---:|---:|---:|---:|
| `worklog.md` | 协作过程、证据、决策和问题 | 日期、目标、检查、决策、改动、验证、待办 | 是 | 是 | 部分 | 是 |
| `todo_plan.md` | 当前阶段任务和验收状态 | 分阶段复选框，完成后立即更新 | 实现轮 | 是 | 否 | 部分 |
| `learning-journal.md` | 可复习的技术知识 | 概念、代码映射、坑、验证、面试问题 | 有学习成果时 | 是 | 是 | 是 |
| `README.md` | 项目入口和运行说明 | 当前功能、依赖、启动、截图、限制 | 重大变化时 | 必须 | 否 | 是 |
| `docs/phase-N-xxx.md` | 阶段目标和总结 | 目标、设计、验收、遗留、下一阶段 | 否 | 必须 | 是 | 是 |
| `docs/gas-notes.md` | GAS 学习记录 | 概念、Actor Info、数据流、错误和示例 | GAS 轮次 | 是 | 是 | 是 |
| `docs/battle-system-design.md` | 战斗真源设计 | 状态机、顺序、行动、伤害、边界 | 设计变化时 | 是 | 是 | 是 |
| `docs/data-asset-guidelines.md` | 数据资产规范 | ID、类型选择、字段、引用和版本 | Schema 变化时 | 是 | 是 | 是 |

`worklog.md` 每条记录至少包含日期与主题、用户目标和需求变化、检查证据、技术决策、实际修改、验证状态、编辑器操作、后续待办和已知风险。

若用户当前回合禁止修改文件，则不得更新日志；应在回答中给出待补录条目，等待之后获得写入许可。

## 19. 阶段结束检查清单

1. 当前阶段验收条件全部有证据。
2. Development Editor 构建通过，没有 UHT、UBT 或链接错误。
3. 自动化测试通过，或明确记录尚无自动化覆盖。
4. 完成 PIE 主路径和失败路径验证。
5. 检查 Gameplay Tags、输入和数据资产引用。
6. 检查蓝图派生类、C++ 默认值和日志警告。
7. 更新 `worklog.md`、`todo_plan.md`、阶段文档和 `learning-journal.md`。
8. 检查 `README.md` 是否与当前运行方式一致。
9. 将已经稳定的新规则更新到 `agents.md`，不记录短期噪声。
10. 检查第三方资源许可证和未提交的大型二进制。
11. 记录未完成项、技术债和下一阶段入口。
12. Git commit、push 或创建远程仓库前征求用户许可。

## 20. 默认回答结构

### 规划请求

1. 目标和成功标准。
2. 当前仓库证据。
3. 范围内与范围外。
4. 推荐架构和职责。
5. 数据流与接口。
6. 实施阶段。
7. 验证和验收。
8. 风险、假设和后续任务。

### 功能实现请求

1. 功能目标确认。
2. 本轮最小范围。
3. UE 类职责。
4. 文件修改清单。
5. 关键概念和数据流。
6. 实现步骤与代码。
7. 编辑器操作。
8. 编译、测试和 PIE 验证。
9. 常见错误和排查。
10. 文档更新情况。

代码较长时先解释最小骨架，再按文件输出；不得一次倾倒未经说明的大型实现。

## 21. Debug 回答结构

1. 现象与预期行为。
2. 错误类别判断。
3. 第一处有效错误或关键证据。
4. 最可能原因，按概率排序。
5. 最小修复方案。
6. 验证步骤。
7. GAS、反射、GC、Tick、数据或网络专项检查。
8. 如果仍失败，需要用户补充的信息。
9. 最终根因、修复和学习结论。

不得只给“尝试重新编译”“删除缓存”等无证据建议。删除 Intermediate、Binaries、DerivedDataCache 等操作也必须先解释影响并取得许可。

## 22. Skills 索引规划

以下为规划中的 Skills。只有在实际安装、验证并出现在当前环境能力列表后才能调用。

| Skill | 适合任务 | 触发场景 | 输出风格 | 学习价值 | 面试价值 |
|---|---|---|---|---|---|
| UE5 C++ Engineer | Actor、Component、Subsystem、框架代码 | 新增或修改 UE C++ 功能 | 文件职责、最小代码、编辑器步骤、验证 | 生命周期、反射、模块 | UE 工程基础 |
| UE5 GAS Beginner Tutor | ASC、AttributeSet、Ability、Effect 入门 | 首次学习或实现 GAS | 先概念后最小数据流 | 完整 GAS 心智模型 | GAS 对象职责 |
| UE5 GAS Debugger | 激活失败、属性不变、Tag/Effect 异常 | GAS 结果与预期不符 | 证据链和检查清单 | 初始化与数据排错 | GAS Debug 方法 |
| UE5 Turn-Based Battle Designer | 回合状态机、行动顺序、结算 | 新增或调整回合规则 | 状态、事件、边界和示例 | 回合架构 | 状态机设计 |
| UE5 RPG Attribute Designer | 属性、伤害、成长、平衡 | 设计或重构 RPG 数值 | 公式、来源、边界测试 | 数值系统分层 | 数据与公式设计 |
| UE5 RPG DataAsset Architect | DataAsset、DataTable、Asset Manager | 新增数据定义或资产引用 | Schema、ID、引用与迁移 | 数据驱动工程 | 数据架构 |
| UE5 UI Engineer | UMG、HUD、事件绑定 | 开发战斗或菜单 UI | C++ 数据源、蓝图表现、无 Tick | UI 数据流 | UI 架构与性能 |
| UE5 Save System Engineer | SaveGame、版本、迁移 | 保存成长、背包、任务状态 | Schema、版本和失败恢复 | 持久化边界 | 存档兼容设计 |
| UE5 Quest Dialogue Designer | 任务、对话、条件和本地化 | 开发剧情交互原型 | 节点、状态、事件和数据 | 内容系统建模 | 状态与数据设计 |
| UE5 Inventory Equipment Designer | 物品、背包、装备、遗器 | 开发养成和物品系统 | 定义、实例、属性汇总和 UI | 静态/运行时分离 | RPG 系统设计 |
| UE5 Build Error Analyst | UHT、UBT、编译、链接、Live Coding | 构建失败 | 定位第一处真实错误 | UE 构建链 | UHT/UBT 排错 |
| UE5 Code Reviewer | 反射、GC、职责、性能、兼容性 | 提交前或阶段结束 Review | 按严重度列证据和建议 | 工程自检 | 代码审查能力 |
| UE5 Portfolio Coach | Demo、README、截图、技术说明 | 准备公开展示 | 成果、证据、原创性和叙事 | 展示工程价值 | 项目表达能力 |
| UE5 Course Tutor | 课程代码理解和 UE5.6 迁移 | 学完课程章节后复盘 | 概念、代码映射、练习题 | 从跟做转为理解 | 原理表达能力 |

Skill 创建或修改后必须验证 `SKILL.md`、更新本索引、记录适用边界、使用真实任务验证，并避免能力重叠。

## 23. 第一阶段开发建议

### 第一阶段目标

交付原创灰盒闭环：

`探索 → 遭遇 → 回合开始 → 玩家基础攻击 → 敌人行动 → 胜负结算 → 固定奖励 → 返回探索`

### 第一周任务

1. 创建 UE5.6 Blank C++ 项目并验证 Visual Studio 2026 工具链；第三人称系统在 Phase 1 从零搭建。
2. 记录实际 C++/UBT 设置，建立 README、计划和日志基线。
3. 确认 Enhanced Input 移动和镜头。
4. 建立最小 Framework、Character、Exploration 和 Battle 职责。
5. 启用并验证 GAS 所需模块，再建立 ASC 和最小 AttributeSet。
6. 使用初始化 Effect 写入基础属性。
7. 创建基础攻击 Ability 和瞬时伤害 Effect。
8. 建立最小 TurnSystem 和固定敌人行动。
9. 用 UMG 显示双方生命与当前行动者。
10. 完成胜负、奖励和返回探索。
11. 执行编译、PIE、失败路径和文档验收。

### 第一轮最小任务

第一轮只完成工程基线，不同时实现战斗：

1. 创建 Blank C++ 项目，模块名为 `HSR`，关闭 Starter Content。
2. 验证 `.uproject` 可以由 UE5.6 打开。
3. 验证 Visual Studio 2026 可以生成并编译 Development Editor。
4. 运行 PIE，确认默认 Enhanced Input 移动和镜头。
5. 记录引擎路径、工具链版本、实际 C++ 标准和首个编译结果。
6. 建立最少必要文档骨架。

验收条件：项目能从编辑器和 IDE 启动；C++ 模块成功加载；PIE 可移动和控制镜头；没有未解释的编译错误；不启用无关插件；不导入第三方美术；不在首轮实现 GAS、战斗或存档代码。

## 24. 总体架构原则与阶段蓝图

HSR 采用“单 Runtime 模块起步、按领域分目录、静态定义/运行时实例/存档快照分离、GAS 与回合系统分工、UI 只发送命令和显示状态”的总体架构。

```text
DataAsset / DataTable / CurveTable（只读定义）
                    │
                    ▼
Exploration / Battle / TurnSystem / GAS / Quest（Runtime）
                    │
                    ├── Delegate / Domain Event ──► UI / Audio / Animation / VFX
                    │
                    ▼
SaveSubsystem ──► SaveGame（持久快照）
```

| 阶段 | 目标 |
|---|---|
| Phase 0 | UE5.6 C++ 工程、工具链、Enhanced Input 和文档基线 |
| Phase 1 | 探索角色、交互、灰盒敌人和 Encounter 入口 |
| Phase 2 | GAS 基线、数据定义、属性初始化和基础攻击 |
| Phase 3 | 1v1 回合战斗、状态、胜负和奖励闭环 |
| Phase 4 | 成长、背包、装备、遗器和 SaveGame |
| Phase 5 | 任务、对话、地图、传送和商店 |
| Phase 6 | UI、设置、动画、音频和 VFX 完善 |
| Phase 7 | Debug、自动化测试、性能、作品集和联机架构审查 |

任何阶段都必须先完成本阶段最小闭环与验证，再进入下一阶段；不得同时铺开多个未验证的大系统。

## 25. 模块策略与依赖方向

### 单模块起步

第一阶段只使用一个 `HSR` Runtime 模块。GAS、Battle、UI、Save 等通过目录划分，不立即创建 `HSRCore`、`HSRBattle`、`HSRGAS`、`HSRUI`、`HSRDeveloper` 或 `HSREditor`。

单模块对初学者的价值：

- 只维护一个 `Build.cs` 和一个 `HSR_API`。
- 更容易定位 UHT、UBT、include 和模块依赖错误。
- 领域职责尚未稳定时可以低成本调整目录。
- 学习重点保持在 UE 生命周期、GAS 和数据流，而不是编译边界。

### GAS 目录而非独立模块

```text
GAS/
├── HSRAbilitySystemComponent.*
├── HSRAbilitySystemInterface.*
├── Ability/
├── Attribute/
├── StatusEffect/
└── Damage/
```

GAS 目录内管理 ASC、Ability、Attribute、Status 和 Damage；TurnSystem 仍为独立领域，禁止把回合状态机写入 GAS 目录或某个 Ability。

### 模块拆分触发条件

- `HSRUI`：UI 数量较多、公共数据接口稳定，并能保持 `HSRUI → HSR` 单向依赖。
- `HSRDeveloper`：战斗模拟、GM 命令和数据检查工具已经成体系，且必须从 Shipping 排除。
- `HSREditor`：出现 DataAsset Validator、Details 定制、资产工厂或 `UnrealEd/AssetTools` 依赖。
- 其他 Runtime 模块：领域边界稳定、可独立测试，且拆分能解决真实编译时间、复用或团队所有权问题。

拆分前必须检查 Public/Private API、导出宏和循环依赖。Runtime 模块不得依赖 UI 的具体 Widget，也不得依赖 Developer/Editor 模块。

### 早期只保留边界的系统

- Quest：只定义 Quest ID、RuntimeState 和类型化任务事件。
- Dialogue：只定义 Dialogue ID、节点数据和交互入口。
- Map/Teleport：只定义 Map ID、入口 ID 和 Teleport Point ID。
- Shop：只定义交易命令、价格来源和原子事务边界。
- Relic：只复用装备属性来源，不做随机副词条。
- Save：先保留 SaveVersion 和单槽位，不立即实现复杂迁移。
- Multiplayer：只保留请求验证和权威迁移点，不写 RPC。
- Audio/Animation/VFX：只消费表现事件，不决定规则。

## 26. Source 目录与职责矩阵

推荐结构，按需创建，不一次生成全部占位类：

```text
Source/HSR/
├── HSR.h
├── HSR.cpp
├── Character/
├── Player/
├── Framework/
├── Exploration/
├── Interaction/
├── Enemy/
├── Battle/
├── TurnSystem/
├── GAS/
│   ├── Ability/
│   ├── Attribute/
│   ├── StatusEffect/
│   └── Damage/
├── Quest/
├── Dialogue/
├── Inventory/
├── Equipment/
├── Relic/
├── Reward/
├── Data/Definitions/
├── Save/
├── UI/
├── Debug/
└── Developer/
```

| 目录 | 职责与主要类型 | 早期 | GAS | DataAsset | UI | SaveGame | 联机扩展 |
|---|---|---:|---:|---:|---:|---:|---:|
| 根目录 | Module、Build、公共日志入口 | 是 | 间接 | 否 | 否 | 否 | 模块级 |
| Character | Character 基类、探索角色、组件容器 | 是 | 持有 ASC | 读取角色定义 | 间接 | 不直接写 | Actor 复制 |
| Player | PlayerController、玩家命令和本地 UI 协调 | 是 | 提交激活命令 | 读取 Profile 定义 | 是 | 经 SaveSubsystem | 玩家所有权 |
| Framework | GameInstance、GameMode、GameState、Subsystem | 是 | 间接 | 可解析 | 间接 | 协调 | 权威迁移点 |
| Exploration | 探索模式、输入状态、区域活动 | 是 | 通常否 | 区域配置 | HUD | 检查点可选 | 移动权威 |
| Interaction | 交互接口、候选目标和请求 | 是 | 通常否 | 提示配置可选 | 提示 | 一次性状态 | 请求验证 |
| Enemy | 敌人角色、探索表现和行动决策 | 是 | 持有 ASC | EnemyDefinition | 状态显示 | 持久敌人可选 | AI 权威 |
| Battle | Encounter、Coordinator、Participant、Result | 是 | 调度 GAS | 战斗/敌人定义 | 战斗 UI | 只写结果 | 战斗权威 |
| TurnSystem | 状态机、行动队列、当前行动者 | 是 | 与 Ability 协作 | 规则配置可选 | 行动条 | 通常不存 | 核心权威 |
| GAS | ASC 基类和公共 GAS 接口 | Phase 2 | 核心 | 间接 | 事件输出 | 不存 Handle | 高风险区 |
| GAS/Ability | 基础攻击、技能、被动入口 | Phase 2 | 核心 | AbilityDefinition | 技能按钮 | 解锁/等级 | 激活权限 |
| GAS/Attribute | HP、Attack、Defense、Speed 等 | Phase 2 | 核心 | 初值/曲线 | 属性 UI | 保存来源 | 属性复制 |
| GAS/StatusEffect | Runtime Status、叠层、回合生命周期 | Phase 3 | 核心 | StatusDefinition | 图标 | 跨战斗可选 | 状态同步 |
| GAS/Damage | 伤害 Context、Result、Execution | Phase 3 | 核心 | DamageRule | 浮字 | 不直接存 | 服务器计算 |
| Quest | QuestSubsystem、目标和领域事件 | Phase 5 | 可选 Tag | QuestDefinition | 任务 UI | 核心 | 权威进度 |
| Dialogue | 节点、条件、选择和触发 | Phase 5 | 通常否 | DialogueDefinition | 核心 | 重要选择 | NPC 同步 |
| Inventory | Item Instance、堆叠、增减事务 | Phase 4 | 消耗品可用 | ItemDefinition | 背包 | 核心 | 所有权/交易 |
| Equipment | 槽位、校验、StatAggregator | Phase 4 | Infinite GE | EquipmentDefinition | 装备 UI | 核心 | 变更权威 |
| Relic | 遗器实例、词条和套装解析 | Phase 4 后 | Infinite GE | Relic/Set Definition | 遗器 UI | 核心 | 随机权威 |
| Reward | 幂等奖励事务和结果 | Phase 3 | 间接 | RewardDefinition | 结算 UI | 核心 | 防重复 |
| Data | Definition 类型、Row Struct、稳定 ID | 是 | 间接 | 核心 | 不直接 | 只存 ID | 数据一致性 |
| Save | Profile、SaveGame、版本和加载 | Phase 4 | 重建 GAS | 只引用 ID | 存档 UI | 核心 | 与 PlayerState 分离 |
| UI | HUD、Widget 基类、ViewModel、UI 事件 | 少量早建 | 只读/发命令 | 读取显示数据 | 核心 | 不直接写 | 本地表现 |
| Debug | 日志、快照和状态查询 | 最小早建 | 检查 | 显示 ID | Overlay | 不污染 | 权威诊断 |
| Developer | Validator、模拟器和 GM 命令 | 后期 | 可检查 | 大量读取 | 开发 UI | 测试存档 | 不进 Shipping |

## 27. Content、授权与 Docs 结构

### Content

```text
Content/
├── Blueprints/{Framework,Characters,Enemies,Interaction,Battle}/
├── Characters/{Player,Shared}/
├── Enemies/{Common,Boss}/
├── UI/{Common,Exploration,Battle,Inventory,Quest,Settings}/
├── Maps/{Development,Exploration,Test}/
├── Data/{Characters,Enemies,Abilities,Effects,Status,Items,Equipment,Relics,Rewards,Quests,Dialogue,Curves,Tables}/
├── Animations/
├── Materials/
├── Audio/{Music,SFX,UI}/
├── VFX/{GameplayCues,Common}/
├── Icons/
├── Placeholder/{Meshes,Materials,Textures,Icons,Audio,UI}/
├── ThirdParty/<PackageName>/
├── ModResources/LocalOnly/
└── Developer/<UserName>/
```

- `Placeholder`：自制灰盒、纯色材质、文字图标、占位 UI/音效/VFX；不得成为来源不明资源的藏匿目录。
- `ThirdParty`：授权允许进入项目的第三方包，按来源隔离并维护授权记录。
- `ModResources`：模之屋等本地研究资源或授权未确认资源；默认不提交、不打包、不展示，也不能成为构建必要依赖。
- “免费下载”不代表允许商用、二创、再分发或公开仓库包含源文件。

第三方授权记录至少包含包名、作者、来源 URL、获取日期、许可证、商用、修改、再分发、署名和作品集展示权限。

禁止公开提交商业游戏提取资源、来源不明资源、禁止再分发资产、仅限个人使用资产、受限字体/音频和包含密钥或本机绝对路径的配置。

第一阶段玩家、敌人、地图、UI、动画、VFX、图标和音频全部使用灰盒、引擎默认或原创占位。公开作品集使用原创项目名、原创 UI/命名/数值，并突出架构、Debug 和个人贡献。

### Docs

```text
docs/
├── phase-0-project-setup.md
├── phase-1-exploration-character.md
├── gas-notes.md
├── battle-system-design.md
├── data-asset-guidelines.md
├── ui-guidelines.md
├── save-system-design.md
├── portfolio-notes.md
└── interview-notes.md
```

| 文档 | 职责 | 更新时机/阶段 | 参考者 | 学习与作品集价值 |
|---|---|---|---|---|
| phase-0 | 工具链、模块、插件、构建 | Phase 0 必须；配置变化时 | 工程/Build 排错 | 独立建项证据 |
| phase-1 | 探索、输入、镜头、交互、遇敌 | Phase 1 必须 | Character/Interaction/UI | Enhanced Input 与职责拆分 |
| gas-notes | ASC、Attribute、Ability、Effect、Tags、Cue | Phase 2–4 持续 | GAS 学习与 Debug | GAS 心智模型和面试 |
| battle-system-design | 状态机、行动顺序、结算、不变量 | Phase 3 必须；规则变化时 | Battle/Turn/UI/AI | 核心系统设计展示 |
| data-asset-guidelines | Definition/Instance/ID/引用/Schema | 新数据类型和字段变化时 | 所有数据系统 | 数据架构能力 |
| ui-guidelines | UI 分层、事件、输入、样式、可访问性 | Phase 3、4、6 | UI 与业务开发 | 无 Tick UI 设计 |
| save-system-design | Schema、版本、事务、迁移、失败恢复 | Phase 4 必须 | Save/Inventory/Quest | 持久化兼容设计 |
| portfolio-notes | Demo 路径、截图点、贡献、授权、限制 | 每个里程碑；Phase 7 汇总 | Portfolio | 直接用于作品集 |
| interview-notes | 原理、真实 Bug、取舍和模拟题 | 每阶段结束 | 学习复盘 | 项目经验转面试表达 |

## 28. UE 核心类型职责补充

| 类型 | 在 HSR 负责 | 不负责/常见误解 | 推荐系统 | GAS | 联机扩展 |
|---|---|---|---|---:|---:|
| GameInstance | 应用生命周期和跨地图服务入口 | 不保存当前战斗和具体 Widget；不是万能单例 | Save/Profile 初始化 | 间接 | 本地生命周期 |
| GameInstanceSubsystem | Save、Profile、Quest 等跨地图服务 | 不负责 Actor 行为或每帧战斗 | Save/全局进度 | 可间接 | 与服务器状态分离 |
| GameMode | 当前地图规则、默认类和流程入口 | 客户端不能依赖其存在；不管 UI/存档 | Exploration/Battle 模式 | 间接 | 未来服务器专有 |
| GameState | 共享世界/战斗快照 | 单机初期不强行塞所有状态 | 未来共享战斗状态 | 可涉及 | 高 |
| PlayerController | 输入、ActionCommand、本地 UI | 不作为属性、任务或背包真源 | Player/UI | 提交激活请求 | 所有权边界 |
| PlayerState | 玩家身份和未来共享玩家状态 | 不等于 SaveGame；初期不强放 ASC | 未来玩家共享数据 | 未来可托管 ASC | 高 |
| Pawn/Character | 场景实体、移动、动画入口、组件容器 | 不管理全局战斗、任务数据库或存档 | Character/Enemy | 持有 ASC | Actor 复制 |
| ActorComponent | Actor 级可复用行为/状态 | 不承担跨世界全局服务 | Interaction/Participant/Inventory | 适合 GAS 周边 | 可复制组件 |
| UObject | 非场景策略和 Runtime 对象 | 不自动拥有 World 或复制；注意 Outer/GC | TurnManager/Policy/Instance | 可引用 | 通常不直接复制 |
| UUserWidget | 显示状态、收集意图 | 不扣血、发奖励、写存档或推进任务 | 所有 UI | 读 Delegate/发命令 | 本地表现 |
| HUD | 根 Widget 入口和 Debug 绘制 | 不应承载所有 Widget 业务 | Root HUD/Overlay | 间接 | 每本地玩家 |
| DataAsset | 只读定义和资源引用 | 不保存当前 HP、数量或任务进度 | 所有定义 | 可引用 GA/GE | 各端一致 |
| SaveGame | 可序列化持久快照 | 不保存 Actor/Widget/GE Handle；不替代网络状态 | Profile/Inventory/Quest | 保存来源，加载后重建 | 与 PlayerState 分离 |
| ASC | Ability、Effect、Tag、Attribute 入口 | 不负责完整回合循环或背包 | 每个战斗参与者 | 核心 | 所有权/复制风险高 |
| AttributeSet | GAS 数值和边界处理 | 不保存角色静态定义或完整成长来源 | Combat/Progression 属性 | 核心 | 属性复制 |
| GameplayAbility | 一次可激活行动 | 不掌管全局战斗状态机 | 攻击/技能/被动 | 核心 | 激活权限/预测 |
| GameplayEffect | 属性修改、Tag、状态表现层 | 秒数不等于回合数；不负责完整状态生命周期 | 伤害/治疗/Buff/装备 | 核心 | Effect 复制 |
| GameplayCue | VFX、Audio、表现通知 | 不决定伤害、死亡和胜负 | 技能/命中/状态表现 | 核心表现 | 防重复 |
| Gameplay Tags | 可组合状态和语义查询 | 不替代所有 Enum、ID 和结构字段 | Ability/State/Status/Event | 核心 | 各端一致 |

## 29. DataAsset、Runtime 与 SaveGame 边界

| 层 | 典型内容 | 规则 |
|---|---|---|
| 静态定义 | Character/Enemy/Ability/Status/Item/Equipment/Reward/Quest Definition、DataTable、CurveTable | 运行时只读；使用稳定 ID；不写玩家进度 |
| Runtime Instance | EncounterContext、ParticipantState、TurnQueue、StatusInstance、属性来源、GE Handle、当前 UI 状态 | 当前会话可修改；Actor 生命周期内有效；只挑选必要字段持久化 |
| Save DTO | SavedCharacter、SavedItemInstance、装备槽位、QuestState、解锁区域 | 可序列化；不含 Actor、Widget、ASC、TurnQueue、GE Handle |

DataAsset 保存“是什么”，Runtime 保存“现在怎样”，SaveGame 保存“下次启动必须恢复什么”。

装备和成长重算必须追踪属性来源。推荐 StatAggregator 汇总角色基础、等级曲线、装备、遗器和被动，然后用可移除的 Infinite GE 应用到 ASC。每个来源对应的 Active GE Handle 只保存在 Runtime。

## 30. 六条核心数据流规范

### 30.1 探索遇敌到战斗结束

Interaction/探索攻击产生 EncounterRequest；BattleCoordinator 验证并读取 CharacterDefinition、EnemyDefinition、RewardDefinition；创建 Runtime EncounterContext/Participant；初始化 ASC 和属性；TurnManager 按 Speed 生成稳定队列；执行玩家命令、敌人决策、Ability、Effect、死亡与胜负；生成 BattleResult；RewardSubsystem 创建幂等事务；Profile Runtime 更新后由 SaveSubsystem 写盘；UI 结算并返回探索。

SaveGame 不保存 EncounterContext、Actor、TurnQueue 或 GE Handle。未来联机时 EncounterRequest、ActionCommand 和 BattleResult 的验证迁移到服务器权威对象。

### 30.2 玩家释放技能

UI 从可用 Ability 构建列表并选择合法目标；PlayerController 提交包含 Battle ID、Actor ID、Ability ID/Spec Handle、Target ID 的 ActionCommand；TurnManager 验证行动者/阶段/目标；ASC 检查 Ability、Tags、Cost；Ability Commit Cost 并创建 GameplayEffectSpec；Effect 应用目标 ASC；AttributeSet 处理边界；Delegate 更新 UI；Cue/动画/音频/VFX 消费表现；规则成功后发送 `ActionResolved` 供 TurnManager 推进。

UI 不直接改状态。不能仅监听 `EndAbility` 推进回合，因为取消、失败和中断也可能结束 Ability。

### 30.3 Buff/Debuff 生命周期

`StatusDefinition → FHSRStatusInstance → GameplayEffect/Tags/Attributes`。StatusInstance 保存 Status ID、Source、Target、RemainingTurns、StackCount、触发时机和 Active GE Handle。Infinite GE 提供属性和 Tag；TurnManager 的行动开始/结束事件递减回合并移除 GE。只有探索中的真实时间效果才使用 GAS 秒数 Duration。

UI 显示图标、层数、剩余回合和可驱散状态。Debug 记录 Battle/Status ID、Source/Target、层数、回合、GE Handle 及刷新/覆盖/免疫/移除原因。

### 30.4 角色成长

Reward 增加 Profile Runtime 经验；Progression Service 根据 CharacterDefinition 和 CurveTable 计算等级；StatAggregator 汇总基础、等级、晋阶、装备、遗器和被动；刷新来源 GE；Attribute Delegate 更新 UI；SaveGame 保存 Level、EXP、晋阶和解锁。

不得用大量 `switch(Level)` 写死成长。learning-journal 记录曲线选择、属性来源顺序、MaxHP 策略、防重复叠加和真实 Debug 证据。

### 30.5 装备更换

UI 提交 EquipCommand；Equipment 系统验证实例所有权、槽位和角色条件；更新内存 Profile；StatAggregator 替换属性来源；移除旧 Infinite GE、应用新 GE；重新计算套装；刷新 UI；全部成功后 SaveSubsystem 写盘。

Definition 保存静态数据，Instance 保存实例字段，SaveGame 保存 ID/等级/词条/槽位，Runtime 保存属性来源和 GE Handle。禁止从最终 Attribute 反推来源。

### 30.6 任务完成

QuestSubsystem 根据 QuestDefinition 创建 QuestRuntimeState，并监听类型化事件：EnemyDefeated、ItemAcquired、InteractionCompleted、LocationEntered、DialogueChoiceSelected。完成后通过幂等 RewardTransaction 发奖，标记 RewardClaimed 并保存。

Debug 必须按 Quest ID 查询状态、目标进度、最近事件、忽略原因和奖励事务，并允许在开发构建推进或重置测试任务。

## 31. 长期功能系统蓝图

所有系统实现前必须逐项回答：目标、子功能、实现方式、GAS、依赖、DataAsset、Runtime、SaveGame、UI、Debug、学习难度、作品集价值、阶段、MVP、可推迟内容和常见坑。

### 31.1 探索系统

- **目标/子功能：** 第三人称移动、镜头、区域状态、探索/战斗输入切换；后续冲刺和场景检测。
- **实现/GAS/依赖：** Character Movement + Enhanced Input + ExplorationComponent；通常不用 GAS，移动状态可用 Tag；依赖 Character、Interaction、Map。
- **Data/Runtime/Save：** 区域配置 DataAsset；Runtime 当前模式；Save 区域/检查点，不保存 Actor。
- **UI/Debug：** 探索 HUD、交互提示；显示区域、模式、输入和碰撞。
- **难度/价值/阶段：** 低到中；学习输入与组件；作品集价值中；Phase 1。
- **MVP/推迟/坑：** MVP 移动、镜头、模式切换；推迟攀爬等；注意 Input Context 重复和战斗后输入恢复。

### 31.2 交互系统

- **目标/子功能：** 统一 NPC、敌人、宝箱、传送点和调查点交互。
- **实现/GAS/依赖：** Interface + InteractionComponent + 查询策略；通常不用 GAS；依赖 Exploration、Quest、Dialogue、Encounter。
- **Data/Runtime/Save：** 交互定义/提示；Runtime 候选目标；一次性交互可存档。
- **UI/Debug：** 提示与高亮；显示候选排序、距离和拒绝原因。
- **难度/价值/阶段：** 中；学习 Interface/事件；作品集价值中高；Phase 1。
- **MVP/推迟/坑：** MVP 最近目标；推迟多目标轮选；禁止 Widget 直接修改目标业务和每帧全世界搜索。

### 31.3 遇敌系统

- **目标/子功能：** 将探索敌人转换为合法 Encounter 和 Battle Context。
- **实现/GAS/依赖：** EncounterRequest + BattleCoordinator；参与者初始化后使用 GAS；依赖 Exploration、Enemy、Battle、Data。
- **Data/Runtime/Save：** Encounter/Enemy Definition；Runtime Encounter ID/参与者；Save 仅持久敌人状态。
- **UI/Debug：** 进入战斗提示；记录触发者、参与者和拒绝原因。
- **难度/价值/阶段：** 中；跨系统边界学习；作品集价值高；Phase 1/3。
- **MVP/推迟/坑：** MVP 单敌人进入独立 Battle Test Map；推迟敌群和复杂先手；防止碰撞重复创建并发战斗，禁止跨地图保存 Actor 指针。

### 31.4 回合制战斗系统

- **目标/子功能：** 参与者、行动顺序、输入等待、解析、死亡、胜负和退出。
- **实现/GAS/依赖：** BattleCoordinator + TurnManager 状态机；GAS 执行动作但不管理完整流程；依赖 Encounter、GAS、AI、Reward、UI。
- **Data/Runtime/Save：** BattleRuleDefinition 可选；Runtime 队列/阶段；默认不存中途战斗。
- **UI/Debug：** 行动条、命令、目标和结算；记录状态迁移和队列。
- **难度/价值/阶段：** 高；状态机/不变量；作品集价值极高；Phase 3。
- **MVP/推迟/坑：** MVP 1v1 速度循环；推迟插队/多波；禁止动画驱动规则，Ability 失败必须可恢复。

### 31.5 GAS 技能系统

- **目标/子功能：** 基础攻击、主动技能、终结技、被动和可用性。
- **实现/GAS/依赖：** ASC、GameplayAbility、Spec、Tags、Cost；GAS 核心；依赖 Attribute、Damage、Status、Turn。
- **Data/Runtime/Save：** AbilityDefinition；Runtime Spec/激活状态；Save 解锁、等级和配置。
- **UI/Debug：** 技能按钮、Cost、目标、失败原因；输出 Spec、Tags、激活/取消。
- **难度/价值/阶段：** 高；GAS 核心；作品集价值极高；Phase 2/3。
- **MVP/推迟/坑：** MVP 基础攻击；推迟复杂被动/预测；注意重复授予、Actor Info 和 UI 直接实例化 Ability。

### 31.6 GAS 属性系统

- **目标/子功能：** HP、MaxHP、Attack、Defense、Speed，后续 Energy 等原创属性。
- **实现/GAS/依赖：** AttributeSet、初始化 GE、Delegate；GAS 核心；依赖 Growth、Equipment、Damage。
- **Data/Runtime/Save：** 初值/曲线在定义；当前值在 ASC；Save 保存成长来源。
- **UI/Debug：** HP 和属性来源；显示基础、加成、最终值和变化来源。
- **难度/价值/阶段：** 中高；Attribute 生命周期；作品集价值高；Phase 2。
- **MVP/推迟/坑：** MVP 五属性；推迟命中/抗性；避免初始化重复和静态/当前值混用。

### 31.7 伤害系统

- **目标/子功能：** 伤害 Context、公式、减伤、暴击、元素和结果。
- **实现/GAS/依赖：** GE + ExecutionCalculation；GAS 核心；依赖 Attribute、Ability、Status。
- **Data/Runtime/Save：** DamageRule/Element；Runtime Context/Result；不保存单次伤害。
- **UI/Debug：** 伤害数字；输出公式输入、分层修正和最终结果。
- **难度/价值/阶段：** 高；捕获属性/EffectContext；作品集价值极高；Phase 3。
- **MVP/推迟/坑：** MVP 简单原创公式；推迟暴击/乘区；避免公式散落和顺序不透明。

### 31.8 弱点、韧性与击破

- **目标/子功能：** 原创弱点匹配、防护值削减、击破状态和附加效果。
- **实现/GAS/依赖：** Tags + Attribute/Status + Damage；使用 GAS；依赖 Damage、Status、Turn。
- **Data/Runtime/Save：** EnemyDefinition 弱点；Runtime 当前防护/击破；通常不存档。
- **UI/Debug：** 弱点图标、防护条；输出匹配、削减和触发条件。
- **难度/价值/阶段：** 高；规则组合；作品集价值高；Phase 3 后。
- **MVP/推迟/坑：** MVP 可推迟或单弱点；不得复制商业游戏命名、公式和 UI。

### 31.9 Buff/Debuff

- **目标/子功能：** 添加、免疫、叠层、刷新、驱散、回合触发和到期。
- **实现/GAS/依赖：** StatusInstance + Infinite GE + Turn 事件；GAS 核心；依赖 Ability、Turn、UI。
- **Data/Runtime/Save：** StatusDefinition；Runtime 回合/层数/Handle；跨战斗状态才考虑保存。
- **UI/Debug：** 图标、层数、剩余回合；完整生命周期日志。
- **难度/价值/阶段：** 高；学习 GAS Duration/回合分离；作品集价值极高；Phase 3。
- **MVP/推迟/坑：** MVP 一个 Buff 或 DoT；推迟复杂触发；禁止秒数当回合，必须保存移除 Handle。

### 31.10 角色成长

- **目标/子功能：** 经验、等级、曲线、晋阶和能力解锁。
- **实现/GAS/依赖：** Progression + StatAggregator + 初始化 GE；使用 GAS；依赖 Reward、Save、Data。
- **Data/Runtime/Save：** CharacterDefinition/CurveTable；Runtime 最终缓存；Save 等级/经验/解锁。
- **UI/Debug：** 经验和属性变化；输出曲线输入与来源。
- **难度/价值/阶段：** 中高；数据重算；作品集价值高；Phase 4。
- **MVP/推迟/坑：** MVP 固定升级；推迟复杂晋阶；防止最终值重复保存和 GE 重复叠加。

### 31.11 装备系统

- **目标/子功能：** 槽位、校验、属性加成、卸下和对比。
- **实现/GAS/依赖：** EquipmentComponent + StatAggregator + Infinite GE；使用 GAS；依赖 Inventory、Attribute、Save。
- **Data/Runtime/Save：** EquipmentDefinition；Runtime 来源/Handle；Save 实例和槽位。
- **UI/Debug：** 装备列表、对比和来源；输出槽位校验/Effect 映射。
- **难度/价值/阶段：** 中高；事务与聚合；作品集价值高；Phase 4。
- **MVP/推迟/坑：** MVP 固定装备；推迟强化；先内存成功后存盘，必须可追踪属性来源。

### 31.12 遗器系统

- **目标/子功能：** 多槽位、主副词条、套装和强化。
- **实现/GAS/依赖：** 复用 Equipment/StatAggregator/GE；使用 GAS；依赖 Inventory、Reward、Save。
- **Data/Runtime/Save：** Relic/Set Definition；Runtime 聚合/Handle；Save 实例词条/等级。
- **UI/Debug：** 筛选、强化、套装；输出随机种子和来源。
- **难度/价值/阶段：** 高；实例/随机；作品集价值高；Phase 4 后。
- **MVP/推迟/坑：** MVP 不做或固定套装；推迟随机副词条；保持原创和随机可复现。

### 31.13 背包系统

- **目标/子功能：** 物品实例、堆叠、分类、查询和使用。
- **实现/GAS/依赖：** InventoryComponent/Subsystem + 事务；消耗品可触发 GAS；依赖 Item、Reward、Save。
- **Data/Runtime/Save：** ItemDefinition；Runtime 索引缓存；Save 实例/数量。
- **UI/Debug：** 分类、排序、详情；输出增减事务和来源。
- **难度/价值/阶段：** 中；Definition/Instance；作品集价值中高；Phase 4。
- **MVP/推迟/坑：** MVP 固定奖励入包；推迟容量；禁止显示名作主键，区分堆叠与实例物品。

### 31.14 奖励系统

- **目标/子功能：** 战斗、任务、宝箱和商店奖励统一发放。
- **实现/GAS/依赖：** RewardDefinition + 幂等 Transaction；通常不直接使用 GAS；依赖 Inventory、Growth、Save、Quest。
- **Data/Runtime/Save：** Reward/Drop Definition；Runtime 事务结果；Save 获得结果/已领取。
- **UI/Debug：** 结算与来源；输出 Transaction ID 和明细。
- **难度/价值/阶段：** 中；事务与防重复；作品集价值高；Phase 3。
- **MVP/推迟/坑：** MVP 固定奖励；推迟随机掉落；防止战斗结束重复发奖。

### 31.15 敌人 AI

- **目标/子功能：** 目标、技能、阶段和可复现决策。
- **实现/GAS/依赖：** DecisionComponent + 规则/评分；通过 GAS 用技能；依赖 Battle、Ability、Data。
- **Data/Runtime/Save：** AIProfile；Runtime 阶段/冷却；通常不存。
- **UI/Debug：** 意图提示可选；输出候选、评分和选择。
- **难度/价值/阶段：** 中高；策略/确定性；作品集价值高；Phase 3/5。
- **MVP/推迟/坑：** MVP 固定基础攻击；推迟 BT/复杂评分；随机需可复现，AI 不直接改 Turn 状态。

### 31.16 任务系统

- **目标/子功能：** 接受、目标、完成、交付、奖励和前置条件。
- **实现/GAS/依赖：** QuestSubsystem + 类型化事件；GAS 可供 Tag/战斗事件；依赖 Interaction、Battle、Inventory、Reward、Save。
- **Data/Runtime/Save：** QuestDefinition；Runtime 活动监听；Save 状态/进度/奖励领取。
- **UI/Debug：** 列表、追踪、提示；按 ID 查询/推进/重置。
- **难度/价值/阶段：** 高；事件驱动/状态机；作品集价值极高；Phase 5。
- **MVP/推迟/坑：** MVP 单目标任务；推迟复杂链；避免无类型 Event Bus 和重复领奖。

### 31.17 对话系统

- **目标/子功能：** 节点、选择、条件、事件和本地化。
- **实现/GAS/依赖：** DialogueDefinition + Subsystem；通常不用 GAS；依赖 Interaction、Quest、UI。
- **Data/Runtime/Save：** 对话定义/节点；Runtime 当前节点；Save 重要选择/完成。
- **UI/Debug：** 对话框、选项、历史；跳节点和条件检查。
- **难度/价值/阶段：** 中高；数据图/本地化；作品集价值高；Phase 5。
- **MVP/推迟/坑：** MVP 线性短对话；推迟复杂分支/配音；禁止文本硬编码 Widget。

### 31.18 地图系统

- **目标/子功能：** 区域、入口、场景状态和地图元数据。
- **实现/GAS/依赖：** MapDefinition + Level 服务；不用 GAS；依赖 Exploration、Save、Teleport。
- **Data/Runtime/Save：** MapDefinition；Runtime 当前区域；Save 解锁区域/检查点。
- **UI/Debug：** 区域名/地图；显示 Map ID、入口和状态。
- **难度/价值/阶段：** 中；关卡生命周期；作品集价值中；Phase 5。
- **MVP/推迟/坑：** MVP 单地图；推迟流送；禁止用显示名/Level 字符串作为全部逻辑主键。

### 31.19 传送系统

- **目标/子功能：** 传送点解锁、选择、加载和安全落点。
- **实现/GAS/依赖：** TeleportDefinition + Map Service；不用 GAS；依赖 Map、Save、UI。
- **Data/Runtime/Save：** Point Definition；Runtime 当前请求；Save 解锁 ID。
- **UI/Debug：** 列表/确认；强制传送和落点检查。
- **难度/价值/阶段：** 中；跨关卡状态；作品集价值中；Phase 5。
- **MVP/推迟/坑：** MVP 同地图；推迟异步加载；不保存 Actor 指针，验证落点加载。

### 31.20 商店系统

- **目标/子功能：** 商品、购买、出售、价格、库存和刷新。
- **实现/GAS/依赖：** ShopDefinition + 原子 Transaction；通常不用 GAS；依赖 Inventory、Currency、Save、UI。
- **Data/Runtime/Save：** Shop/Price Definition；Runtime 会话；Save 库存/限购。
- **UI/Debug：** 商品、确认、余额；输出校验和结果。
- **难度/价值/阶段：** 中；事务一致性；作品集价值中高；Phase 5。
- **MVP/推迟/坑：** MVP 固定商品/单货币；推迟刷新；扣钱与加物必须原子，防重复购买。

### 31.21 存档系统

- **目标/子功能：** Profile、槽位、自动保存、版本、迁移、备份和恢复。
- **实现/GAS/依赖：** SaveSubsystem 统一序列化，加载后重建 GAS；依赖所有持久系统。
- **Data/Runtime/Save：** DataAsset 只按 ID 引用；Runtime Profile 工作副本；SaveGame 版本快照。
- **UI/Debug：** 槽位/时间/错误；Schema 检查和迁移测试。
- **难度/价值/阶段：** 高；持久化边界；作品集价值极高；Phase 4。
- **MVP/推迟/坑：** MVP 单槽位/检查点；推迟云存档；禁止保存 UObject 指针，必须有版本和失败策略。

### 31.22 UI 系统

- **目标/子功能：** 探索、战斗、背包、角色、任务、地图、设置和提示。
- **实现/GAS/依赖：** C++ 数据接口/ViewModel + UMG；通过 GAS Delegate 读属性；依赖所有系统。
- **Data/Runtime/Save：** UI Style/Icon Definition；Runtime 界面状态；Save 仅必要偏好。
- **UI/Debug：** 本身为 UI；显示 Widget 栈、输入模式和绑定。
- **难度/价值/阶段：** 高；事件驱动/焦点/生命周期；作品集价值极高；Phase 3 起持续。
- **MVP/推迟/坑：** MVP HP、行动、攻击、结算；推迟正式主题；禁止 Tick 轮询和 Widget 写业务状态。

### 31.23 设置系统

- **目标/子功能：** 图形、音频、语言、输入和可访问性。
- **实现/GAS/依赖：** GameUserSettings + Settings Service；不用 GAS；依赖 UI、Audio、Input。
- **Data/Runtime/Save：** 默认选项；Runtime 待应用值；使用用户设置存储，不混入角色 SaveGame。
- **UI/Debug：** 设置/确认；输出有效值与平台限制。
- **难度/价值/阶段：** 中；平台配置；作品集价值中；Phase 6。
- **MVP/推迟/坑：** MVP 音量/基础画质；推迟完整重绑定；设置应用需有恢复策略。

### 31.24 音频系统

- **目标/子功能：** 音乐、环境、战斗、技能和 UI 声音。
- **实现/GAS/依赖：** Sound Class/Mix + 表现事件；GameplayCue 可触发；依赖 Battle、UI、Settings。
- **Data/Runtime/Save：** Audio Definition 可选；Runtime 当前上下文；音量进用户设置。
- **UI/Debug：** 音量；显示音乐状态和触发事件。
- **难度/价值/阶段：** 中；表现解耦；作品集价值中高；Phase 6。
- **MVP/推迟/坑：** MVP 静音或合法占位；推迟动态音乐；授权必须明确，避免硬引用网络。

### 31.25 动画表现系统

- **目标/子功能：** 移动、攻击、受击、死亡、状态和镜头配合。
- **实现/GAS/依赖：** AnimBP、Montage、Notify、表现事件；Ability 可触发但规则不依赖动画；依赖 Character、Battle、GAS。
- **Data/Runtime/Save：** AnimationSet Definition；Runtime 表现状态；不存档。
- **UI/Debug：** 无主要业务 UI；显示 Montage、状态和事件时序。
- **难度/价值/阶段：** 中高；动画/Gameplay 解耦；作品集价值高；Phase 6。
- **MVP/推迟/坑：** MVP 默认/占位动画；推迟复杂连携；动画 Notify 不能是唯一伤害真源。

### 31.26 VFX 表现系统

- **目标/子功能：** 技能、命中、状态、击破、奖励和环境特效。
- **实现/GAS/依赖：** Niagara + GameplayCue；使用 GAS 表现接口；依赖 Ability、Damage、Status。
- **Data/Runtime/Save：** Cue/VFX 映射；Runtime 临时实例；不存档。
- **UI/Debug：** 画质选项；显示 Cue Tag、次数和来源。
- **难度/价值/阶段：** 中高；GameplayCue；作品集价值高；Phase 6。
- **MVP/推迟/坑：** MVP 纯色占位；推迟复杂特效；防重复、泄漏，VFX 不决定规则。

### 31.27 Debug/GM 工具

- **目标/子功能：** 状态快照、日志、战斗模拟、任务推进、物品发放和数据验证。
- **实现/GAS/依赖：** Debug API/命令/开发 Widget；读取 ASC、GE 和 Tags；依赖所有核心系统。
- **Data/Runtime/Save：** Debug 配置；Runtime 调试会话；默认不写正式存档。
- **UI/Debug：** Overlay/命令面板；系统自身即 Debug。
- **难度/价值/阶段：** 中高；可观测性/测试；作品集价值极高；Phase 3 起持续。
- **MVP/推迟/坑：** MVP 日志/战斗快照；推迟完整 GM；Shipping 禁用，测试命令不得污染正式存档。

### 31.28 作品集展示系统

- **目标/子功能：** 可重复 Demo、架构图、Debug 展示、技术说明、截图和授权清单。
- **实现/GAS/依赖：** Showcase Map、演示脚本和文档；不直接使用 GAS，但展示 GAS 数据流；依赖已完成系统。
- **Data/Runtime/Save：** Showcase 配置可用 DataAsset；Runtime 演示步骤；使用独立演示存档。
- **UI/Debug：** 技术信息面板和干净 HUD；Debug 可视化是展示重点。
- **难度/价值/阶段：** 中；技术表达；作品集价值最高；每阶段记录，Phase 7 汇总。
- **MVP/推迟/坑：** MVP 录制探索到奖励闭环；推迟宣传级剪辑；必须说明个人贡献并排除未授权资源。

## 32. 推荐实现顺序与依赖禁令

实际实现顺序：工程与工具链 → 探索移动 → Interaction/Encounter → BattleParticipant → ASC/Attribute → 基础攻击/伤害 → TurnManager → 敌人行动/胜负 → UI Delegate → Reward → Save/成长/背包/装备 → Quest/Dialogue/Map → 表现与作品集。

禁止形成以下依赖：

- DataAsset 定义依赖 Widget。
- Battle Runtime 依赖具体 Widget Blueprint。
- SaveGame 依赖 Actor、ASC、Widget 或 Active GE Handle。
- GameplayCue、Montage、Audio 或 VFX 决定规则结果。
- UI 直接更新任务、背包、装备、Attribute 或 SaveGame。
- TurnSystem 依赖具体技能类名称。
- Runtime 模块依赖 Developer/Editor 模块。
- 为未来联机提前引入 Steam、会话或复制代码。

第一阶段最重要的成果是清楚证明 GAS/TurnSystem 分工、Definition/Runtime/Save 分层、UI 命令边界、端到端战斗数据流、Debug 证据和版权合规，而不是追求系统数量。

## 33. Phase 0–20 路线图执行规则

完整、可执行的 Phase 0–20 路线图以 [`docs/phase-roadmap-0-20.md`](../docs/phase-roadmap-0-20.md) 为真源。`todo_plan.md` 保存可勾选状态，`worklog.md` 保存真实执行证据；三者不得互相替代。

### 已锁定决策

- Phase 0 使用 UE5.6 Blank C++ 模板和单 `HSR` Runtime 模块。
- Phase 1 从零搭建第三人称 Character、Controller、Enhanced Input、Camera 和 HUD。
- 探索战斗进入独立 Battle Map，不采用同地图原地战斗。
- `UHSRBattleTransitionSubsystem` 只携带稳定 ID、Map、Transform、Encounter 和 Result 数据。
- Battle Map 中重建 Battle Actor、ASC 和 GameplayEffect；禁止持久化 Actor、ASC、Widget、TurnQueue 或 GE Handle。
- `UHSRBattleCoordinatorSubsystem` 管理 Battle 生命周期，`UHSRTurnManager` 管理回合阶段和行动队列。
- Energy 是 GAS Attribute；战技点是 BattleCoordinator 管理的队伍 Runtime 资源。
- Phase 6 前使用轻量伤害；Phase 7 引入 ExecutionCalculation。
- Phase 11 引入最小 Save 基线，Phase 16 完成版本、迁移和兼容。
- Phase 20 交付 3 名完整角色，架构支持第 4 名。

### 阶段门禁

1. 每次只实施一个 Phase 或其中一个可独立验收的小节。
2. 实施前列出文件、职责、公共类型和编辑器操作。
3. 实施后完成编译、PIE、专项测试和文档更新。
4. 没有真实验证证据不得勾选完成。
5. Phase 0–6 必须逐阶段验收，不允许批量生成后统一排错。
6. Phase 7–20 只有在依赖阶段完成后才能开始。
7. 路线图发生变化时，同步更新 roadmap、todo、worklog、agents 和受影响的 phase 文档。

### Phase 摘要

| Phase | 内容 |
|---|---|
| 0 | Blank C++ 工程、工具链、插件、Tags 和规范 |
| 1 | 第三人称探索角色、Enhanced Input、动画参数、HUD |
| 2 | ASC、Core AttributeSet、初始化 GE、属性 UI |
| 3 | Interaction Interface/Component、NPC、宝箱、传送占位 |
| 4 | 敌人探索 AI、Encounter、独立 Battle Map Context |
| 5 | BattleCoordinator、Participant、TurnManager、1v1 闭环 |
| 6 | 普攻、战技、终结技、治疗、Cost、Energy、战技点、目标 |
| 7 | Attack/Defense/Crit、Damage Execution 和 Breakdown |
| 8 | 原创弱点、韧性、击破和 Turn Delay |
| 9 | Buff/Debuff、叠层、刷新和回合生命周期 |
| 10 | 完整战斗 UI 与事件驱动 ViewModel |
| 11 | CharacterData、成长、队伍和最小 Save |
| 12 | 装备、遗器、StatAggregator 和来源 GE |
| 13 | 背包、幂等奖励、掉落和持久化 |
| 14 | 任务、对话、类型化事件和奖励 |
| 15 | 地图、传送、位置和跨关卡流程 |
| 16 | 完整版本化 Save、迁移、备份和恢复 |
| 17 | 主菜单与全 UI 导航整合 |
| 18 | 授权模型、动画、镜头、音频、VFX 和动效 |
| 19 | Debug/GM、战斗日志和自动化测试 |
| 20 | 三角色原创垂直切片 Demo 与作品集材料 |

## 34. MVP 与早期执行边界

完整定义以 [`docs/mvp-first-month-first-week-plan.md`](../docs/mvp-first-month-first-week-plan.md) 为真源。本章节只保存长期不易变化的范围门禁。

### 层级区分

| 层级 | 定义 |
|---|---|
| 第一轮 | 只维护 Markdown，不创建 UE 工程 |
| Phase 0 | 创建和验证 Blank C++ 工程，不写 Gameplay |
| 第一周 | 只实施 Phase 0 |
| 第一月 | Phase 0、1、2 和 Phase 3 单对象交互 |
| MVP | Phase 0–5 加固定 Reward 与最小 Save 子集，形成 1v1 可玩闭环 |
| Phase 20 | 三角色、Boss、任务、成长和完整展示的垂直切片 |

MVP、第一月成果和 Phase 20 是三个不同范围，不得混用完成状态。

### MVP 固定边界

- 灰盒探索地图、第三人称移动、镜头和 Enhanced Input。
- 一个静止 Encounter Actor；完整巡逻、感知和追击推迟。
- 独立 Battle Map，Context 只保存稳定 ID、Map 和 Transform。
- 1 名玩家对 1 名敌人；数据结构支持队伍数组。
- ASC、Health、MaxHealth、Speed、初始化 GE、基础攻击 GA 和固定伤害 GE。
- Speed 排序、稳定同速裁决、确定性敌人行动、死亡和胜负。
- 固定幂等奖励、返回探索原位置和单槽位最小 Save。
- Save 只保存版本、Map、Transform、Encounter ID、奖励数量和演示角色 ID。

### MVP 必需类型

- CharacterBase、ExplorationCharacter、PlayerController、GameMode、HUD。
- InteractionComponent、InteractableInterface、EncounterActor。
- BattleTransitionSubsystem、BattleGameMode、BattleCoordinatorSubsystem、TurnManager、BattleParticipantComponent、BattleCharacter。
- AbilitySystemComponent、CoreAttributeSet、BasicAttackAbility。
- RewardSubsystem、SaveSubsystem、SaveGame。
- CharacterDefinition、EnemyDefinition、EncounterDefinition、RewardDefinition。

### MVP 明确推迟

- 战技、终结技、被动、Cost、Cooldown 和 GameplayCue。
- Attack、Defense、Crit 和 ExecutionCalculation。
- 元素、弱点、韧性、击破和 Buff/Debuff。
- 多角色实际战斗、复杂敌人 AI、成长、装备、遗器、背包、任务和商店。
- 正式 UI、模型、动画、音频、VFX 和多人。

### 第一月和第一周门禁

- 第一周只做 Phase 0；禁止 Character、Controller、ASC、Blueprint、UMG 和 Gameplay。
- Week 2 只做第三人称探索角色。
- Week 3 只做 ASC、Core AttributeSet、初始化 GE 和属性 UI。
- Week 4 只做一个灰盒交互对象。
- 第一月不开始 Enemy/Encounter 代码，只允许形成 `FHSREncounterRequest` 草案。

### 早期禁止规则

- 不做抽卡、复杂 Boss、完整剧情、完整遗器随机词条和复杂 UI。
- 不导入大量模型或使用授权不清资源。
- 不做完整 GAS 架构、预测、复制、多人和 Steam。
- 不把 TurnSystem 塞进 Ability，不把核心逻辑写进 Widget。
- 不把所有系统塞进 Character 或 GameInstanceSubsystem。
- 不使用 Tick 轮询输入、Attribute、UI 或战斗。
- 不把 DataAsset 当 Runtime，不在 SaveGame/GI Subsystem 保存跨地图对象指针。
- 不一次实施多个 Phase，不把文档规划完成标为 Gameplay 完成。

## 35. 结合项目阶段的 GAS 学习门禁

完整学习路线以 [`docs/gas-learning-roadmap.md`](../docs/gas-learning-roadmap.md) 为真源；`docs/gas-notes.md` 记录实际学习和 Debug 证据，二者不得混用。

### 学习顺序

`Stage 0 → 1 → 2 → 3 → 4 → 5 → 7 → 6 → 8 → 9 → 10`。Stage 7 对应 Phase 8，必须先于对应 Phase 9 的 Stage 6 实施。

### 长期放置决策

- 当前单机玩家和敌人的 ASC 放 Character/BattleCharacter，Owner 和 Avatar 暂为 Character。
- 独立 Battle Map 重建 Battle Actor、ASC、Attribute 和 GE。
- 当前不为联机提前使用 PlayerState ASC、RepNotify、RPC 或 Prediction。
- 未来联机玩家 ASC 可迁移 PlayerState，Owner 为 PlayerState，Avatar 为当前 Character；敌人通常保持 Character ASC。

### GAS 与回合职责

- GAS 负责 Ability、Attribute、Effect、Tags、个人 Cost、伤害、治疗、状态 Modifier 和 GameplayCue。
- TurnSystem 负责当前行动者、行动队列、回合阶段、共享战技点、持续回合计数、行动延后和胜负流程。
- Ability 成功后报告 `FHSRAbilityResolution`/`Event.Battle.ActionResolved`，不直接推进下一回合。
- `EndAbility`、Montage 结束和 GameplayCue 播放都不能单独作为行动成功真源。

### Effect 与回合时间

- Instant GE：初始化、伤害、治疗和资源变化。
- Duration/Periodic GE：世界秒数效果，不直接表示回合。
- Infinite GE：装备、被动和回合状态的 GAS 表现层。
- “持续 N 回合”由 Runtime Status + Turn 事件计数，到期按 Active GE Handle 移除。

### 高级概念引入门禁

- Stage 2 前不写 Ability。
- Stage 3 前不做 Cost/Target/AbilityTask。
- Stage 5 前不引入 ExecutionCalculation；简单伤害先使用 SetByCaller/Meta Attribute。
- Stage 7 前不做 Weakness/Toughness/Break。
- Stage 6 前不做通用 Buff/Debuff、Stacking、Immunity 和命中抵抗。
- Stage 9 前不将装备/遗器转为 Infinite GE。
- Stage 10 前不实施复制、预测和服务器权威。

### Debug 与学习完成标准

每个 Stage 必须说明概念本质、为何现在引入、创建者/所有者/移除者、Data/Runtime/Save 边界、正常/失败路径、地图旅行和销毁行为、UI 观察方式、测试与 Debug 证据。只复制代码、编译通过或蓝图运行不算真正完成。

### 复制模式速查

- Full：完整 GameplayEffects 复制给所有相关客户端，信息和带宽最高。
- Mixed：完整 Effects 主要给 Owner，Tags/Cues 给相关客户端，常用于玩家；Owner 链必须正确。
- Minimal：不复制完整 Effects，只复制必要 Tags/Cues，常用于大量 AI。
- Attribute 复制仍需 ReplicatedUsing、GetLifetimeReplicatedProps 和 GAS RepNotify，不能仅依赖 ASC Replication Mode。

## 36. 低级模型执行任务模板

完整可复制模板以 [`docs/low-level-model-task-templates.md`](../docs/low-level-model-task-templates.md) 为唯一真源。本文件只保存长期使用规则，不复制模板全文。

### 模板选择

- 模板 A：只用于 Markdown 规划、归档和记录。
- 模板 B：只用于 1–5 个明确文件范围内、可独立编译和测试的小型 C++ 功能。
- 模板 C：只用于已有明确现象、错误证据和复现路径的 Debug/修复。
- 同时包含大型功能、重构、资源导入和 Debug 的请求必须先拆分，不能直接交给低级模型。

### 硬边界

- 每轮只允许一个可独立验收的小任务。
- `允许修改的文件` 必须列出明确路径；未列入允许清单的文件默认禁止修改。
- 发现必须增加文件或扩大范围时立即停止并请求用户授权。
- 高级模型在生成任务卡前必须读取 agents、todo、worklog、对应 Phase 文档和任务专项设计文档；低级模型只以 `tasks/active-task.md` 为上下文入口。
- 执行前必须说明目标、文件职责、修改范围和明确不修改的内容。
- 禁止无关重构、删除文件、覆盖历史、批量资源导入、批量 Config 修改和把核心逻辑迁入蓝图。
- 禁止 `git reset`、`git clean` 和自动 add、commit、push。
- 反射、GAS、GC、Tick、UI、DataAsset 和 SaveGame 检查按所选模板逐项执行。
- 代码任务必须按实际结果报告编译、测试、PIE 和 Editor 验证；未验证内容不得描述为完成。
- Debug 任务必须先定位第一处真实错误，以证据确认根因，再实施最小修复；不得用删除缓存或随机试错代替诊断。
- 完成代码或修复后追加 worklog；只有真实进度变化才更新 todo；产生可复用知识时更新 learning journal 和对应专项文档。

## 37. Codex、用户与可选阶段 Skill 协作

完整流程见 [`docs/phase-execution-workflow.md`](../docs/phase-execution-workflow.md)、[`docs/loop-engineering-workflow.md`](../docs/loop-engineering-workflow.md) 和 [`docs/codex-ue-editor-collaboration.md`](../docs/codex-ue-editor-collaboration.md)。这些文档是流程细节真源；本节只保留长期规则。

### UE Editor 与 C++ 职责边界

- Codex 负责经授权的 C++、Build.cs、文档、数据结构和可审查的 Debug 修复。
- 用户负责 UE Editor 中需要视觉确认或资产操作的内容：创建/配置 Blueprint、DataAsset、GameplayEffect、InputAction、UMG、Gameplay Tags、动画、地图摆放、资源绑定和 PIE 观察。
- Codex 必须说明 Editor 页面、操作顺序、配置目的、典型错误和验证方式；不能把未在 Editor 中确认的内容声称为完成。
- 用户负责审查代码、编译项目、完成 Editor 手动配置、运行 PIE 并反馈日志、截图和现象。

### Loop Engineering 协作模式

复杂任务可采用以下可选闭环：

`Prompt Planner → Prompt Reviewer → Architect → Implementation Agent → Code Reviewer → Learning Reviewer/Safety Reviewer`

- Planner 拆解目标、范围、输入、输出和验收；
- Prompt Reviewer 检查越权、遗漏上下文、测试和文档要求；
- Architect 设计 UE 职责、数据流和最小文件边界；
- Implementation Agent 只执行通过审查的任务；
- Code Reviewer 检查反射、GAS、GC、Tick、职责和 UE5.6 兼容性；
- Learning Reviewer 归档知识、面试题和真实踩坑；
- Safety Reviewer 检查删除、覆盖、批量操作、版权、Git 和大型资产风险。

复杂任务未通过审查时不得交给低级模型。小型文档或 C++ 任务可以简化角色，但不能省略范围、验证和真实状态汇报。

### 可选阶段 Skill

- 项目级 Skill 位于 [`.agents/skills/phase-next-steps/`](skills/phase-next-steps/)，名称为 `phase-next-steps`。
- 它只在用户询问“下一步应该做什么”、阶段门禁或阶段切换建议时提供参考，不是强制流程。
- Skill 必须先读取当前 agents、todo、worklog、README、roadmap 和 Phase 文档，依据证据推荐一个相邻小任务。
- Skill 不自动修改文件、不自动推进 Phase、不替代用户授权，也不把规划建议标记为实现完成。
- Skill 的详细行为和 Phase 0–20 矩阵见 [`SKILL.md`](skills/phase-next-steps/SKILL.md) 与其 references。

### 工作流与阶段记忆

- `docs/phase-execution-workflow.md` 规定通用阶段循环、门禁、验证和“下一步”回答格式。
- `docs/loop-engineering-workflow.md` 规定复杂任务的角色审查闭环。
- `docs/codex-ue-editor-collaboration.md` 规定 Editor 手动操作与 Codex C++ 修改的边界。
- `docs/low-model-execution-guide.md` 与 `docs/low-level-model-task-templates.md` 约束低级模型的小任务执行。
- 本 Skill 和这些文档都是可选指导；用户当前回合的明确要求、文件安全限制和实际 UE5.6 证据优先。

## 38. CC-SWITCH 多模型上下文传递规则

CC-SWITCH 切换 API 源或模型后，不假设新模型继承原对话。仓库内文件是跨模型共享上下文的主要真源，聊天历史只作为临时辅助。

### 文件职责

- `PROJECT_STATE.md`：当前 Phase、任务、阻塞、代码状态、设计决策、限制和下一任务的快照，由高级模型维护。
- `tasks/active-task.md`：唯一活动任务契约；同一时间只能有一张，未列入允许清单的文件默认禁止修改。
- `tasks/task-template.md`：高级模型生成活动卡时使用的结构真源。
- `tasks/review-template.md`：审查模型的检查与结论格式。
- `tasks/archive/`：保存完成、取消或被替代的任务卡；不得覆盖历史。
- `docs/model-switch-prompts.md`：高级、低级、审查模型及结果回传的启动 Prompt 真源。
- `worklog.md`、`todo_plan.md`、`learning-journal.md`：分别保存真实证据、真实进度和可复用知识，不能被快照替代。

### 模型职责

- 高级模型必须先读 `PROJECT_STATE.md`、本文件、活动任务卡和阶段证据，负责拆解、架构、授权边界、任务卡与快照维护。
- 低级模型只以 `tasks/active-task.md` 作为上下文入口，不自行读取全局文档补充隐含需求；实现时也只能接触任务卡明确列出的目标文件。
- 审查模型读取活动任务卡、审查模板、实际改动和验证证据，独立判断是否越界及是否满足验收，不能只采信执行模型自述。
- 用户当前明确要求始终优先；任务卡存在不等于获得执行授权。

### 任务流

1. 高级模型从项目文件恢复状态，并生成一张自包含活动任务卡。
2. Prompt Reviewer/Safety Reviewer 在执行前检查 Phase、文件范围、授权、测试与 Editor 步骤。
3. 低级模型只执行审查通过且明确授权的活动卡；遇到缺失信息、模型不匹配或范围外文件立即停止。
4. 用户完成 UE Editor 手动操作并提供日志、截图或现象。
5. 审查模型使用 `tasks/review-template.md` 给出 `PASS`、`PASS WITH FOLLOW-UP`、`REVISE` 或 `BLOCKED`。
6. 高级模型根据审查结果更新 `PROJECT_STATE.md`、worklog、真实变化对应的 todo 和学习文档。
7. 任务完成或取消后归档活动卡，再创建下一张；不得同时推进多个活动任务。

### 一致性与安全

- 状态冲突时按“用户当前要求 → agents 长期规则 → active task 本轮契约 → project state 快照 → 专项文档 → todo → worklog”处理，并以真实证据纠正文件。
- 计划、代码、编译、PIE 和人工验证必须分别记录；无证据不得标记完成。
- 切换模型不会扩大授权；禁止因此跨 Phase、批量修改 Config、创建未授权 Blueprint/资产、删除历史或执行 Git。
