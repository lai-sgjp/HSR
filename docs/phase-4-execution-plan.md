# HSR Phase 4 详细执行计划

> 创建日期：2026-07-18  
> 当前状态：P4-001～003 已归档；P4-004 Verification 与 Teacher 已完成，Independent Reviewer 当前 `REVISE`。角色提交、Coordinator 归档与最终复审未闭合；Phase 4 为 `Not verified`。
> 阶段主题：敌人探索表现、Encounter 请求与独立 Battle Map 纯数据过渡

## 1. 当前阶段与门禁结论

### 1.1 仓库证据

- Phase 0、Phase 1、Phase 2 已处于 `Ready`。
- Phase 3 已归档为 `Ready`；最终 Independent Reviewer commit `3b3fbeb` 的结论为 `PASS WITH FOLLOW-UP`。
- 当前没有活动任务卡，Phase 4 的 Enemy、Encounter Definition、Battle Transition 和 Battle Map 尚未实施。
- 现有探索地图为 `Content/Maps/Map_Phase1_Exploration.umap`；尚无 `Map_BattleTest`、Enemy 或 Encounter 相关资产。
- `PROJECT_STATE.md` 与 Phase 3 归档记录是当前阶段状态的主要证据；`todo_plan.md` 顶部与 `README.md` 仍有“Phase 3 尚未开始”的陈旧描述，须在下一次协调文档同步时修正。

### 1.2 Gate 状态

**结论：`Ready with inherited follow-ups`。** 可以进入 Phase 4 的规划与第一工作包，但不能把 Phase 3 描述为无缺口完成。

必须继承并在 Phase 4 回归中保留的 Phase 3 边界：

- 最终 fresh Build/UHT 完整控制台日志缺失。
- 最终 Build 后 HUD rebuild / 同会话 Re-Possess 未补新证据。
- 目标销毁/弱失效未补新证据。
- `OutOfRange` 防御分支未动态命中。
- 相关学习 follow-up、资产作者与历史 Git Gate 证据等级保持不变。

这些项目当前不阻断 Phase 4，但不得被改写成已验证；Phase 4 必须取得自己的 fresh UHT/C++/Link 证据，并回归交互入口、无候选和目标销毁路径。

## 2. 阶段目标与唯一最终可见结果

Phase 4 的最终可见结果是：

> 玩家在探索地图中遇到一个数据驱动的灰盒敌人；敌人通过事件驱动的巡逻、发现和追击进入 Encounter；系统只通过统一请求入口构造纯数据 Context，切换到独立空 Battle Map 并恰好消费一次；测试入口可以安全返回探索地图，且整个跨图链没有保留旧 World 的 Actor 指针。

阶段只建立“探索 AI → Encounter → 跨图 Context”边界，不建立战斗规则。Phase 5 才负责根据 Context 重建战斗 Actor/ASC、TurnManager、1v1、攻击、胜负、BattleResult 和战斗结果驱动的正式返回。

## 3. 核心架构与数据所有权

### 3.1 固定职责

| 对象 | 负责 | 不负责 |
|---|---|---|
| Enemy Definition | 敌人稳定 ID、探索表现/配置引用和 Encounter Definition 引用 | 保存当前追击目标或跨图状态 |
| Encounter Definition | Encounter 稳定 ID、玩家/敌人定义 ID、Battle Map 软引用和静态规则 | 保存 Actor、ASC、Widget 或运行时消费状态 |
| Enemy Character | 探索碰撞、表现、当前 World 内的 Avatar/ASC 基线 | 决定跨图 Context 生命周期或战斗技能 |
| Enemy AIController | 感知事件、巡逻/追击状态和 MoveTo 结果处理 | 直接调用 `OpenLevel` 或拥有 Pending Context |
| Battle Transition Subsystem | 校验请求、原子保存/消费/清理 Pending Context、发起关卡旅行 | 保存旧 World Actor、决定回合/伤害/胜负 |
| Battle Test Consumer | 在空 Battle Map 消费并展示/记录 Context，提供测试返回入口 | 生成战斗参与者或运行 TurnSystem |
| UI / Blueprint | 配置、灰盒表现、占位提示和用户测试入口 | 成为 Encounter 或 Battle 状态真源 |

### 3.2 主数据流

```text
AI Perception / 灰盒交互
  → Enemy AI 状态复核
  → RequestEncounter（唯一入口）
  → 校验 Definition、稳定 ID、Map 与 Pending 状态
  → 写入纯数据 FHSREncounterRequest / Return Context
  → OpenLevel(Map_BattleTest)
  → Battle Test Consumer 调用 ConsumePendingEncounter
  → 第一次取得完整 Context，第二次消费明确失败
  → 测试返回入口读取 Return Context
  → 回到探索地图并恢复测试 Transform
```

### 3.3 Encounter DTO 不变量

`FHSREncounterRequest` 及其 Return Context 必须是反射安全的纯值 DTO，并具有安全默认值。计划字段至少包括：

- 唯一请求 ID（例如 `FGuid`）。
- 稳定的 Player Definition ID、Enemy Definition ID、Encounter Definition ID。
- `EHSREncounterInitiative`：安全默认值及 `Player`、`Enemy`、`Neutral` 三种可达结果。
- 探索地图软引用或 `FSoftObjectPath`。
- Battle Map 软引用或 `FSoftObjectPath`。
- 返回 `FTransform`。
- 可选 Schema Version，用于未来 DTO 演进；本阶段不做 SaveGame 迁移。

严禁保存：Actor、Pawn、Controller、ASC、AttributeSet、Widget、接口指针、裸 UObject 指针、Active GameplayEffect Handle、Delegate Handle 或旧 World 内的运行实例。

### 3.4 Pending 状态机

Transition Subsystem 至少区分：

```text
Empty → Pending → Traveling → Consumed / Cleared
```

- `SetPendingEncounter` 或等价操作必须原子拒绝第二个未消费请求。
- `ConsumePendingEncounter` 只能成功一次；第二次返回结构化失败并保留可诊断日志。
- `ClearPendingEncounter` 必须覆盖取消、无效输入、旅行失败和测试返回后的清理。
- 旅行失败不得永久卡在 `Traveling`；必须安全恢复为空或可重试状态。
- 日志使用 Request ID 和稳定 Definition ID 定位，不以对象地址作为业务身份。

## 4. 阶段工作包总览

| 工作包 | 单一可验收结果 | 前置 | 初始状态 |
|---|---|---|---|
| P4-001 | 从现有灰盒交互入口构造纯数据 Encounter Request，进入空 Battle Map 并恰好消费一次 | Phase 3 Ready | Reviewer `PASS WITH FOLLOW-UP`，已归档；A2 后 Editor/PIE 为 `USER ACCEPTED` 缺口 |
| P4-002 | 数据驱动敌人在 NavMesh 上事件驱动巡逻、发现和追击，并复用 P4-001 的唯一 Encounter 入口 | P4-001 通过审查 | Reviewer `PASS WITH FOLLOW-UP`，已归档；A1 Build/用户 PIE/范围与生命周期 follow-up 保留 |
| P4-003 | 三种先手、失败恢复、生命周期与空图测试返回形成可重复的跨图矩阵 | P4-002 通过审查 | Reviewer `PASS WITH FOLLOW-UP`，已归档；A4c 一次可定位 Build、1/4 组合动态，其余用户接受/延期 |
| P4-004 | 最终回归、教学、独立审查和阶段文档归档闭合 | P4-001～003 完成 | Verification/Teacher 已完成；Reviewer `REVISE`，等待角色提交、Coordinator 同步与复审；Phase 4 `Not verified` |

每次只创建一张活动任务卡。后一工作包不得因为本计划存在而自动开始；必须在前一包归档后，由 Coordinator 创建并审查新的 `tasks/active-task.md`，再经过执行者首次只读复述和用户对该任务编号的二次确认。

## 5. P4-001：稳定 Encounter 合同与跨图单次消费

### 5.1 唯一可验收结果

玩家通过现有 Phase 3 灰盒交互路径触发一次测试 Encounter：系统构造不含 Actor 指针的请求，进入 `Map_BattleTest`；Battle Map 第一次消费获得正确稳定 ID、地图和 Transform，第二次消费明确失败；无效 Definition 或地图不会污染 Pending 状态，修正后可以重试。

### 5.2 学习目标

- 理解 `UGameInstanceSubsystem` 为何能跨 `OpenLevel` 生存，而 World Actor 不能。
- 区分 DataAsset 静态定义、Pending Runtime Context 与未来 SaveGame DTO。
- 理解“请求一次、消费一次、失败清理”比一个全局 bool 更可靠。
- 理解软地图引用、稳定 ID 和旧 World 生命周期的关系。

### 5.3 计划 C++ 范围

最终精确允许清单由活动任务卡锁定；计划候选如下：

- `Source/HSR/Enemy/HSREnemyTypes.h`
- `Source/HSR/Battle/HSREncounterTypes.h`
- `Source/HSR/Data/Definitions/HSREnemyDefinition.h/.cpp`
- `Source/HSR/Data/Definitions/HSREncounterDefinition.h/.cpp`
- `Source/HSR/Battle/HSRBattleTransitionSubsystem.h/.cpp`
- 一个最小 Battle Test Consumer；优先放在 `Source/HSR/Battle/`，具体 Actor、Component 或 GameMode 形式由活动卡确定。
- 仅在静态交互触发需要最小适配且现有接口不能配置时，修改精确列出的 `Exploration` 灰盒派生 C++；不得重写 Phase 3 InteractionComponent。
- 只有真实编译证明需要模块依赖时，才申请修改 `Source/HSR/HSR.Build.cs`。

P4-001 不创建 Enemy AIController，不做巡逻、感知或追击。

### 5.4 用户 UE Editor 操作

1. 创建一个原创灰盒 `DA_Enemy_*` 与 `DA_Encounter_*`，配置稳定 ID、软地图引用和占位显示数据。
2. 创建 `Content/Maps/Map_BattleTest.umap`。
3. 在 Battle Map 配置最小 Consumer/GameMode 或放置测试 Consumer Actor。
4. 在现有探索灰盒对象或其派生 BP 上绑定 Encounter Definition；不得在 Level Blueprint 中复制核心请求逻辑。
5. 保存并完整重开 Editor，核对 DataAsset、地图、父类和软引用没有丢失。

### 5.5 验证矩阵

- fresh `HSREditor Win64 Development`：保存命令、退出码、UHT/C++/Link 和第一处 warning/error。
- 成功：交互一次进入 Battle Map，消费日志中的 Request ID、Definition IDs、initiative、地图和 Transform 正确。
- 幂等：重复触发只产生一次旅行；同一 Pending 请求第二次设置被拒绝；第二次消费失败。
- 失败：缺 Enemy Definition、Encounter Definition、Battle Map 或无效稳定 ID 时不旅行、不留下脏 Pending 状态。
- 生命周期：切图后探索 Actor 已销毁，Context 仍可读取且不包含旧 Actor/ASC/Widget 引用。
- 回归：Move/Look/Jump、交互提示、单次交互、无候选、目标销毁和 GAS 属性 HUD 正常。
- 静态审查：无新增 Gameplay/UI Tick、无直接 Actor 跨图引用、无 SaveGame 写入、无 RPC/Replication。

### 5.6 常见错误与复盘问题

- 错误：把触发敌人 Actor 直接保存到 GameInstance。
- 错误：AI、Widget 或 Level Blueprint 直接 `OpenLevel`，绕过统一入口。
- 错误：失败后仍保留 Pending，下一次合法请求永远被拒绝。
- 复盘：为什么稳定 ID 可以跨图，而 UObject 实例不能？DataAsset 与 Pending Context 谁能被修改？第二次消费为什么必须失败？

## 6. P4-002：数据驱动探索敌人、感知与追击

### 6.1 唯一可验收结果

一个灰盒 Enemy Character 在有效 NavMesh 上按配置巡逻；AI Perception 发现玩家后进入 Alert/Chase，丢失目标或 MoveTo 失败时安全回落；接近触发条件后只调用 P4-001 的 `RequestEncounter`，不会重复旅行。

### 6.2 学习目标

- 理解 Pawn/Character 与 AIController 的职责分离。
- 理解 AI Perception 更新、`MoveTo` 完成/失败与状态转换的事件链。
- 理解 NavMesh 是移动前置条件，不是 AI 规则真源。
- 理解当前 World 内短期目标弱引用与跨 World 纯数据 Context 的差异。

### 6.3 计划 C++ 范围

- `Source/HSR/Enemy/HSREnemyCharacter.h/.cpp`
- `Source/HSR/Enemy/HSREnemyAIController.h/.cpp`
- 扩展 `Source/HSR/Enemy/HSREnemyTypes.h`
- 必要时最小扩展 Enemy Definition 与 Battle Transition 请求入口。
- 仅当 UBT 证明缺失时，申请在 `Source/HSR/HSR.Build.cs` 增加 `AIModule`、`GameplayTasks` 或 `NavigationSystem` 等实际所需依赖。

`EHSREnemyExplorationState` 至少具有安全默认状态，并覆盖 `Idle`、`Patrol`、`Alert`、`Chase` 和 `EncounterPending` 或等价最小集合。不要为目录外观创建无实现文件。

### 6.4 事件、GC 与 Tick 约束

- Enemy Character 与 AIController 默认不启用自定义 Gameplay Tick。
- 使用 AI Perception 目标更新、AI MoveTo 完成/失败和必要的低频 Timer 驱动巡逻等待。
- 禁止每帧 `GetAllActorsOfClass`、距离扫描、资源加载或反复下发 MoveTo。
- 当前感知目标只能作为当前 World 的弱观察引用；目标失效、UnPossess、EndPlay 时清理。
- Timer/Delegate/Lambda 必须具备生命周期安全的解绑或失效检查。
- Enemy 可复用 CharacterBase 的 ASC/属性初始化边界，但不授予 Ability、不造成伤害，不让 GAS 决定 AI 状态、initiative 或切图。

### 6.5 用户 UE Editor 操作

1. 创建 Enemy Character 派生 BP，绑定灰盒 Mesh、碰撞、Enemy Definition、AIControllerClass 与 Auto Possess AI。
2. 配置 AI Perception Sight 的距离、遗忘/丢失目标参数和玩家识别方式；记录实际配置。
3. 在探索地图放置 `NavMeshBoundsVolume` 并显示导航区域确认覆盖。
4. 配置最小巡逻点或返回点；不创建 Behavior Tree/EQS。
5. 将 Enemy BP 放入 `Map_Phase1_Exploration`，保存并重开 Editor 核对引用。

### 6.6 验证矩阵

- 有 NavMesh：Idle/Patrol → 发现 → Alert/Chase → Encounter 请求只发生一次。
- 丢失目标：状态安全回到 Idle/Patrol，不持续追逐无效 Actor。
- 无 NavMesh：MoveTo 失败/中止可诊断，不崩溃、不忙循环、不触发错误 Encounter。
- 重复感知：同一目标多次更新不会重复注册、重复 MoveTo 或重复 OpenLevel。
- 目标销毁：弱观察清理，无 ensure、GC warning 或失效访问。
- 定义缺失：敌人进入安全状态并输出稳定 ID/配置相关错误，不从蓝图偷偷补规则。
- 回归：P4-001 的静态交互跨图入口仍可用；Phase 1～3 输入、HUD 和交互链无回归。

### 6.7 常见错误与复盘问题

- 错误：在 Character Tick 中每帧找玩家并计算距离。
- 错误：AIController 自己保存 Pending Context 或直接 OpenLevel。
- 错误：MoveTo failed 后立即无限重试。
- 复盘：Perception、AIController、Enemy Character 和 TransitionSubsystem 分别拥有哪种状态？为什么目标 Actor 只能停留在当前 World？

## 7. P4-003：先手、失败恢复、生命周期与空图测试返回

### 7.1 唯一可验收结果

`Player`、`Enemy`、`Neutral` 三种 initiative 都有原创、确定、可达的输入路径，并在 Battle Map 得到正确纯数据结果；重复触发、无效定义/地图、旅行失败和连续往返不会悬挂 Pending 状态。空 Battle Map 可通过明确的测试入口返回探索地图与测试 Transform，但不生成 BattleResult、不执行战斗规则。

### 7.2 Initiative 计划边界

- 先手只作为 Encounter 输入事实写入 Request；Phase 4 不根据它创建回合队列。
- 每种结果必须有清晰且可测试的来源，不只声明 enum：例如玩家主动交互、敌人追击接触、无明显优势的中性触发。
- 具体判定规则由活动任务卡锁定，使用原创命名与阈值，不复制商业游戏表现或数值。
- 缺失或矛盾输入回落到安全默认 `Neutral`，并记录原因。

### 7.3 测试返回边界

- Return Context 仅保存探索地图软引用与返回 Transform 等纯值数据。
- 空 Battle Map 的返回只是 Phase 4 生命周期验证，可由开发用按键/Consumer 明确触发。
- Phase 5 的胜负、BattleResult、奖励、正式战后返回和“结果只消费一次”不在本包实现。
- 返回失败必须保留可诊断状态并允许人工重试或安全清理，不得跳到错误地图。

### 7.4 验证矩阵

- 三种 initiative 分别可达，Battle Map 消费值正确且第二次消费失败。
- 同帧或连续触发多个 Encounter 时只有一个合法 Pending 请求。
- 缺 Definition、稳定 ID、探索地图、Battle Map 或无效 Transform 时明确失败，不污染后续合法请求。
- 模拟/制造可控旅行失败后 Pending 状态可恢复；不得靠删除历史日志掩盖失败。
- 空 Battle Map 测试返回探索地图，Transform 在合理容差内恢复；连续两轮往返无旧 World 引用。
- 完整重开 Editor 后引用保持；至少两轮连续 PIE 无重复 Delegate、重复旅行、Error、Ensure 或 GC warning。
- 静态审查 Transition 状态机、软引用、反射默认值、Timer/Delegate 清理和无自定义 Gameplay/UI Tick。

### 7.5 教学复盘

- `OpenLevel` 前后哪些对象必然销毁，哪些对象继续存在？
- Pending、Traveling、Consumed、Cleared 各由谁改变？
- 如果第二个敌人在第一请求旅行前也触发，系统为什么不能覆盖第一个请求？
- 空图测试返回与 Phase 5 战斗结果返回的差别是什么？

## 8. P4-004：最终回归、教学、审查与阶段收尾

### 8.1 唯一可验收结果

Phase 4 的工程、Editor、PIE、失败路径、教学、独立审查和文档证据全部闭合；阶段结论只根据真实材料判定为 `Ready`、`Not verified` 或 `Blocked`。

### 8.2 强制顺序

1. Coordinator 核对 P4-001～P4-003 的允许范围、作者、提交、资产、构建和运行证据。
2. Teacher 讲解 DataAsset/Runtime/World/Save 边界、AI 事件链、OpenLevel 生命周期和单次消费，并保留用户原始作答。
3. Teacher 记录掌握项、纠正项和复习项，只提交自己的教学产物。
4. Independent Reviewer 审查实际源码/资产清单、反射、GC、Tick、GAS、未来联网边界、Build、Editor/PIE 与失败矩阵。
5. 只有 Reviewer 放行，或用户明确接受被如实记录的剩余风险，Coordinator 才能更新阶段状态；两种处置不得混写。

### 8.3 Phase 4 完成判据

- fresh `HSREditor Win64 Development` 的 UHT、相关 C++、Link 与退出码 0 有保存证据。
- Editor 重开后 Enemy BP、AIController、Perception、NavMesh、DataAssets、地图和软引用保持。
- Enemy 在有效 NavMesh 上事件驱动巡逻、发现和追击，无自定义 Gameplay Tick。
- 丢失目标、目标销毁、无 NavMesh、MoveTo failed/aborted 均能安全恢复。
- 三种 initiative 真实可达，并只作为 Request 数据，不越界进入 TurnSystem。
- 缺 Definition/ID/Map、重复触发、重复消费和旅行失败均不崩溃、不悬挂、可重试或可清理。
- Battle Map 只按稳定 ID、软地图引用和 Transform 消费 Context 一次；旧 World Actor 不可达。
- 空图测试返回可以重复验证；未实现 Battle Actor、TurnManager、Ability、胜负、奖励或正式 BattleResult。
- Phase 1～3 的 Move/Look/Jump、控制模式、GAS 属性 HUD、交互提示、单次执行、无候选和目标销毁回归通过。
- 无 Actor/ASC/Widget/GE Handle 跨图引用；无 SaveGame、RPC、Replication 或 Prediction。
- `PROJECT_STATE.md`、`todo_plan.md`、`worklog.md`、`learning-journal.md`、`README.md` 与本计划状态一致。

## 9. Codex、用户与证据分工

### 9.1 Codex 可负责

- 在活动任务卡精确授权后修改对应 C++ 和 Markdown。
- 设计反射安全 DTO、DataAsset 类型、AI 状态和 Transition 状态机。
- 运行并记录允许的 UBT/Automation/静态检查。
- 提供逐项 Editor 配置、PIE 路径和失败矩阵。
- 审查源码中 Actor 指针、Tick、Delegate、Timer、GAS 和未来联网边界。

### 9.2 用户负责 UE Editor

- 创建/修改 `.uasset`、`.umap`、DataAsset 实例、Enemy BP、AI Perception 配置与 NavMesh。
- 绑定软地图引用、Definition、AIControllerClass、Auto Possess AI、Mesh 和碰撞。
- 保存并重开 Editor，执行 PIE、观察行为并提供日志/截图或结构化结果。
- 由真实资产作者提交资产；不得把 Codex 的文字说明当作资产已创建证据。

### 9.3 每个工作包必须保存的证据

- 活动任务卡、精确允许/禁止清单、唯一成功结果和明确非目标。
- 实际 diff、文件作者/资产作者与 commit hash。
- Build 命令、目标、退出码、UHT/C++/Link、第一处 warning/error。
- Editor 重开后的引用核对。
- PIE 主路径、至少一条失败路径和回归路径。
- `Saved/Logs` 片段或用户结构化观察，明确证据来源。
- 未验证项、失败项和 Reviewer 结论，不得用“应该可以”代替。

## 10. 风险与控制措施

| 风险 | 控制措施 |
|---|---|
| GameInstance 保存旧 World Actor | DTO 只允许稳定 ID、软引用和值类型；静态审查成员类型 |
| 重复触发造成多次 OpenLevel | 原子 Pending 状态、Request ID、重复 Set 拒绝和日志 |
| 消费后仍可重复读取 | 明确 Consume 语义，第二次消费专项测试 |
| 旅行失败使状态永久悬挂 | 失败清理/恢复策略和可重试测试 |
| AI Perception/MoveTo 形成忙循环 | 事件驱动状态机、完成/失败回落、无自定义 Tick |
| 目标销毁后访问失效对象 | 当前 World 弱引用、EndPlay/UnPossess 清理、生命周期安全绑定 |
| Blueprint 成为规则真源 | Blueprint 只配置/表现；请求校验、状态和消费留在 C++ |
| GAS 越界承担 AI/旅行规则 | Enemy 只复用现有 ASC 基线，不授能、不伤害、不决定切图 |
| 为未来联网过度设计 | 只保留单一验证入口；不实现或声称 Replication/RPC/Prediction |
| Phase 4 偷跑 Phase 5 | 空图只消费/展示 Context；禁止重建参与者、TurnManager、攻击和胜负 |
| 文档状态漂移 | P4-004 对 PROJECT_STATE/todo/worklog/README/learning/plan 做证据一致性核对 |

## 11. 全阶段明确非目标

- Battle Actor/ASC 重建、BattleCoordinator、BattleParticipant、BattleGameMode 和 TurnManager。
- 1v1 回合顺序、普攻、伤害、死亡、胜负、奖励、BattleResult 和正式战后返回。
- 正式 Ability、GameplayEffect、Cost、Cooldown、Energy 或技能 UI。
- Behavior Tree、Blackboard、EQS、复杂巡逻、群体 AI、多敌群或队伍。
- SaveGame、跨会话 Encounter 恢复或完整地图系统。
- RPC、Replication、Prediction、会话或任何“已支持联网”的承诺。
- 正式模型、动画、音频、VFX、第三方资产和复杂 UMG。
- 新 Runtime Module/Plugin、批量 Config 修改或 Phase 1～3 大规模重构。

## 12. 当前唯一下一步

`TASK-P4-001` 的 A1/A2 修订与最终 Reviewer `PASS WITH FOLLOW-UP` 已完成归档。用户接受 A2 后 Editor/PIE 缺失为非阻断，但 P4-002 回归必须重新覆盖至少一条 Encounter 跨图与单次消费主路径。

P4-002/003 已归档。P4-004 Verification 与 Teacher 产物已产生，Independent Reviewer 当前结论为 `REVISE`；必须依次闭合独立角色提交、Coordinator 自有协调提交和 Reviewer 复审。Phase 4 保持 `Not verified`，不得自动进入 Phase 5。
