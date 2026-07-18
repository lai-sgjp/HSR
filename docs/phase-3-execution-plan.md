# Phase 3 详细执行计划：单对象交互系统

> 计划日期：2026-07-17  
> 当前状态：P3-001 已以 Reviewer `PASS WITH FOLLOW-UP` 归档；唯一活动任务为 `TASK-P3-002`，等待 Implementation 首次只读复述与用户确认  
> 阶段门禁：前置条件 `Ready`；Phase 3 本身为 `Not verified`

## 1. 当前阶段、证据与 Gate

- `PROJECT_STATE.md`、`todo_plan.md` 与 `worklog.md` 记录 Phase 0、1、2 已归档，当前无活动任务，允许开始 Phase 3。
- Phase 2 的准确处置是：P2-002 获得 Reviewer `PASS`；P2-003 的 Reviewer 结论仍为 `REVISE`，项目所有者接受剩余学习项为非阻断复习项后，以 `USER ACCEPTED` 将 Phase 2 判定为 `Ready`。本计划不得将其改写为 P2-003 Reviewer `PASS`。
- 现有 `Content/Input/IA_Interact.uasset` 与 `Content/Input/IMC_Exploration.uasset` 已接入探索角色；`AHSRExplorationCharacter::Interact()` 当前仅输出占位日志，是 Phase 3 的自然接入口。
- 本文件只归档计划，不是 Phase 3 的源码、资产、Build、Editor 重开或 PIE 完成证据。只有真实证据齐全后才能把 Phase 3 标记为 `Ready`。

### Gate 0：实施前确认

- 当前没有其他活动任务卡；实施时一次只允许一张活动任务卡。
- 保留 Phase 2 的历史结论和证据来源，不回写为更强结论。
- 实施开始前记录工作树状态，并区分用户已有修改、派生产物和本任务产物。
- 复核探索地图、角色 Blueprint、`IA_Interact`、`IMC_Exploration` 与探索 HUD 的引用仍有效。
- 任一基线异常先记录第一处真实错误，不把 Phase 3 的失败无证据归因于旧阶段。

## 2. 阶段目标与范围决策

### 2.1 唯一阶段可见结果

在探索地图中，玩家接近一个灰盒交互对象时显示提示；按下现有 Interact 输入后，请求只通过共享 `Interface + InteractionComponent` 路径完成并产生可观察的占位反馈；离开范围、对象不可用或对象被销毁时，提示与交互安全失败。整个候选维护和 UI 更新不使用 Gameplay Tick 或 Widget Tick 轮询。

### 2.2 本阶段为何只实现一个对象

`docs/phase-roadmap-0-20.md` 的早期 Phase 3 描述列出 NPC、宝箱和传送点三类占位对象；较新的阶段门禁与第一月边界将验收收敛为“一个灰盒对象通过共享 Interface/Component 工作”。本阶段采用后者作为执行边界：

- 只实现一个灰盒对象的完整闭环。
- 通过“新增第四种对象不需要修改 InteractionComponent”这一设计审查证明协议可扩展。
- NPC、宝箱、传送点只作为未来接口使用示例，不创建三套业务 Actor。
- 真正 NPC 对话、宝箱奖励/持久化和传送均不属于本阶段。

## 3. 学习目标与架构边界

### 3.1 学习目标

- 理解 `UINTERFACE`/`IInterface` 是对象能力合同，不拥有候选或 UI 状态。
- 理解 `UActorComponent` 是玩家侧候选管理、选择、请求提交和事件发布者。
- 理解 `Context` 是请求单、`Result` 是回执单，失败不能只用一个 `bool` 表达。
- 理解临时候选为何使用 `TWeakObjectPtr`，以及弱引用在每次使用前仍需有效性检查。
- 理解 Overlap 与 Delegate 驱动的数据流，以及为何 UI 不应轮询或 Cast 具体业务 Actor。
- 理解当前单机实现如何保留“请求 → 复核 → 执行 → 结果”的未来权威边界，但不提前实现联网。

### 3.2 固定职责

- `IHSRInteractableInterface`：提供提示、可交互校验与执行协议。
- `UHSRInteractionComponent`：注册/注销弱候选、选择当前目标、提交请求、广播提示与结果；不得按具体 Actor 类型分支。
- 灰盒交互 Actor：拥有自身碰撞与可用状态，实现接口并产生占位反馈。
- `AHSRExplorationCharacter`：创建交互组件，并把现有 `IA_Interact` 输入转发给 `TryInteract()`；不保存具体交互对象业务逻辑。
- HUD/Widget：只消费提示快照和 Delegate；不决定候选、不执行交互、不拥有 Gameplay 真源。

### 3.3 主数据流

```text
Overlap Begin
  → 灰盒对象向玩家 InteractionComponent 注册自身
  → Component 清理弱引用并选择候选
  → Candidate/Prompt Changed Delegate
  → HUD/Widget 显示 FText 提示

IA_Interact
  → AHSRExplorationCharacter::Interact
  → UHSRInteractionComponent::TryInteract
  → 复核 IsValid / ImplementsInterface / CanInteract
  → IHSRInteractableInterface::Execute_ExecuteInteraction
  → FHSRInteractionResult
  → 日志与结果 Delegate/UI 反馈

Overlap End / Actor Destroyed
  → 注销或惰性清理弱候选
  → 重新选择或清空候选
  → Prompt Changed Delegate
  → HUD/Widget 隐藏提示
```

### 3.4 反射、GC 与 Tick 约束

- `UINTERFACE` 与 `IHSRInteractableInterface` 都包含 `GENERATED_BODY()`；`*.generated.h` 是头文件最后一个 include。
- 若允许 Blueprint 实现，接口使用最小必要的 `BlueprintNativeEvent`，调用统一走 `IHSRInteractableInterface::Execute_*`，不直接调用 `_Implementation`。
- `FHSRInteractionContext`、`FHSRInteractionResult` 与枚举具有安全默认值；玩家可见文本使用 `FText`，稳定标识使用 `FName` 或 `FGuid`。
- 候选保存 `TWeakObjectPtr<AActor>` 或等价弱 UObject 引用，不缓存裸 `IHSRInteractableInterface*`。
- 拥有型 UObject/Component/Widget 引用使用 `UPROPERTY` 与 `TObjectPtr`；Delegate 绑定与解绑成对。
- InteractionComponent 和 Widget 不使用自定义 Tick；不使用 `GetAllActorsOfClass`、每帧 Trace 或每帧距离扫描。
- PlayerController 为 Enhanced Input 正常求值所需的引擎 Tick 不属于本阶段禁止的 Gameplay 轮询，不能误删。

## 4. 建议文件与资产范围

### 4.1 C++ 文件

新增：

- `Source/HSR/Interaction/HSRInteractionTypes.h`
- `Source/HSR/Interaction/HSRInteractableInterface.h`
- `Source/HSR/Interaction/HSRInteractableInterface.cpp`
- `Source/HSR/Interaction/HSRInteractionComponent.h`
- `Source/HSR/Interaction/HSRInteractionComponent.cpp`
- `Source/HSR/Exploration/HSRGrayboxInteractable.h`
- `Source/HSR/Exploration/HSRGrayboxInteractable.cpp`
- 如确有独立观察状态需要：`Source/HSR/UI/HSRInteractionViewModel.h/.cpp`

修改：

- `Source/HSR/Character/HSRExplorationCharacter.h`
- `Source/HSR/Character/HSRExplorationCharacter.cpp`
- 仅在采用现有 HUD 集成且任务卡明确授权时：`Source/HSR/UI/HSRHUD.h/.cpp`、`Source/HSR/UI/HSRUserWidget.h/.cpp`
- 仅在实际编译证明模块依赖不足时：`Source/HSR/HSR.Build.cs`

`HSRInteractionTypes.cpp` 只有在存在非内联实现时才创建，不为目录外观创建空文件。最终允许清单必须由对应活动任务卡逐项锁定；本节不是自动授权。

### 4.2 现有与新增 Editor 资产

复用并核对：

- `Content/Input/IA_Interact.uasset`
- `Content/Input/IMC_Exploration.uasset`
- 当前实际探索角色 Blueprint
- 当前实际探索 HUD/根 Widget
- `Content/Maps/Map_Phase1_Exploration.umap`

建议新增：

- `Content/Blueprints/Exploration/BP_HSRGrayboxInteractable.uasset`
- `Content/UI/WBP_InteractionPrompt.uasset`

若实际目录与建议路径不同，先记录真实 Content Browser 路径，再更新活动任务卡；不要同时创建重复版本。

## 5. 工作包总览

| 工作包 | 单一可验收结果 | 状态 |
|---|---|---|
| P3-001 | 交互协议、弱候选组件与单对象无 UI 闭环可编译、可在 PIE 成功/失败 | Reviewer `PASS WITH FOLLOW-UP`；最终修订 commit `64ac977`，已归档 |
| P3-002 | 提示 UI 事件链与候选/生命周期边界稳定 | 已创建唯一活动任务卡，等待首次复述与确认 |
| P3-003 | 最终回归、教学、审查与阶段归档闭合 | 未开始 |

## 6. P3-001：协议、组件与单对象垂直闭环

### 6.1 唯一可验收结果

玩家进入一个灰盒对象范围后，该对象成为弱候选；按下 Interact 只经 Character → InteractionComponent → Interface 路径执行一次并产生占位反馈；离开、无候选或目标销毁后再次按键安全失败。提示 UI 可暂以日志/组件事件验证，不在本包完成视觉布局。

### 6.2 类型与协议

计划定义最小类型，最终名称以任务卡与源码审查为准：

- `EHSRInteractionFailureReason`：至少覆盖 `None`、`NoCandidate`、`InvalidTarget`、`OutOfRange`、`Unavailable`、`ExecutionFailed`。
- `FHSRInteractionContext`：本次请求者、发起位置或其他当下校验所需的最小上下文；不保存跨地图 Actor 指针或 UI 对象。
- `FHSRInteractionResult`：成功标志、失败原因和可选用户消息。
- `GetInteractionPrompt`、`CanInteract`、`ExecuteInteraction`。
- `RegisterCandidate`、`UnregisterCandidate`、`TryInteract`。

本阶段只有一个对象，不提前加入宝箱专用 `AlreadyConsumed` 业务；如灰盒对象采用一次性状态，可将其作为通用 `Unavailable` 验证。只有真实需求出现后才扩充枚举。

### 6.3 Codex/C++ 工作

1. 创建反射安全的 Interaction Types 与 Interface。
2. 创建默认关闭 Tick 的 InteractionComponent。
3. 使用弱引用保存候选；注册去重、注销幂等，并在选择/执行前清理失效项。
4. 当前只有单候选也要避免依赖容器迭代顺序；为未来多候选明确稳定选择契约，但不实现实时每帧最近目标重排。
5. `TryInteract()` 依次校验候选有效、实现接口、当前可交互，再执行接口并返回/广播结构化结果。
6. 在 ExplorationCharacter 构造函数创建组件，把现有 `Interact()` 占位日志替换为唯一的 `TryInteract()` 调用。
7. 创建一个轻量灰盒交互 Actor；由其碰撞 Begin/End Overlap 注册/注销自身，不让组件扫描世界。
8. 对重复 Begin/End、错误 Pawn、目标在校验前失效、组件 EndPlay 等路径安全处理并输出可定位日志。
9. 不修改 Phase 2 ASC、AttributeSet、GameplayEffect 或属性 HUD 行为。

### 6.4 用户 Editor 工作

1. 编译后关闭并重开 Editor，确认新增 C++ 类可用于 Blueprint 派生。
2. 创建 `BP_HSRGrayboxInteractable`，使用引擎基础 Cube/灰盒 Mesh，不导入第三方资产。
3. 配置 Box/Sphere Collision 为 Query Only、Generate Overlap Events 开启，并只与 Pawn 产生所需 Overlap；记录实际 Collision Profile/Channel。
4. 将对象放入 `Map_Phase1_Exploration`，确保不会阻挡探索移动。
5. 核对角色 Blueprint 中 `IA_Interact` 与 `IMC_Exploration` 引用没有丢失。
6. 保存、重开 Editor，再次核对类、组件、碰撞和地图引用。

### 6.5 必需验证证据

- 新鲜 `HSREditor Development Win64` 触发相关 UHT/C++/Link 并成功；保存目标、退出码和第一处 warning/error。
- PIE：进入范围注册一次，按键执行一次，离开后返回 `NoCandidate` 或等价失败。
- PIE：重复注册不导致一次输入执行多次。
- PIE：候选 Actor 被销毁后按键不崩溃、不触发 ensure、不访问失效对象。
- 静态/运行证据：InteractionComponent 与灰盒对象没有自定义 Gameplay Tick。
- 回归：Move、Look、Jump、现有 GAS 属性 HUD 与 Exploration/UIOnly 切换仍正常。

### 6.6 常见错误与复盘

- 错误：Character Cast 成灰盒对象后直接调用业务函数。
- 错误：交互对象直接监听 `IA_Interact`。
- 错误：保存裸接口指针或在弱引用失效后解引用。
- 错误：BeginOverlap 重复注册，EndOverlap 漏注销。
- 错误：直接调用 BlueprintNativeEvent 的 `_Implementation`。

复盘问题：Interface 为什么不保存状态？Component 为什么不应知道对象具体类型？Context 与 Result 分别由谁创建和消费？

## 7. P3-002：提示观察层与生命周期专项

### 7.1 前置门禁

- P3-001 的 Build、Editor 重开、PIE 主路径、无候选和目标销毁证据均已记录。
- Reviewer 已确认 P3-001 没有类型分支、裸候选指针、Gameplay Tick 或越权文件。
- P3-001 follow-up 必须继续保留：修复后完整 Build/UHT 控制台未保存、最终 PIE 主要为用户证据、`OutOfRange` 未动态命中、历史 `9356ad0` Git Gate 偏差和同一 Git 身份证据等级。

### 7.2 唯一可验收结果

接近灰盒对象时单实例提示 UI 通过事件显示正确 `FText`；离开、对象不可用或销毁时立即隐藏；HUD/Widget 重建、Re-Possess 和连续 PIE 后仍只有一条观察链，不重复回调、不访问已销毁 Widget。

### 7.3 Codex/C++ 工作

1. InteractionComponent 在候选或提示状态真正变化时发布 Delegate，并提供一次当前快照。
2. 若现有 HUD/Widget 边界不足，再引入最小 `UHSRInteractionViewModel`；不得让交互 UI 依赖 GAS Attribute ViewModel。
3. 建立初始化快照、绑定、解绑和 teardown 的明确顺序。
4. Widget 只读取提示 ViewState/事件，不 Cast 灰盒 Actor，不调用 ExecuteInteraction。
5. 保持 HUD 创建入口幂等；不得破坏 Phase 2 属性 Widget 的生命周期。
6. 记录成功与失败 Result，使底层行为在 UI 隐藏时仍可由日志解释。

### 7.4 用户 Editor 工作

1. 创建最小 `WBP_InteractionPrompt`，只包含提示文本和必要的显示/隐藏表现。
2. 将其集成进当前探索 HUD；记录是作为子 Widget 还是由 HUD 创建单独实例。
3. 在 Construct/Destruct 或 C++ 对应生命周期中成对连接/断开事件。
4. 保存并重开 Editor，核对 WidgetClass、Character、HUD 与地图引用。
5. 执行进入、按键、离开、禁用/销毁对象、HUD 重建、Re-Possess 和连续 PIE 矩阵。

### 7.5 必需验证证据

- 进入范围显示提示，离开/销毁隐藏；不存在 Widget Tick 或每帧 Cast/距离查询。
- 初次绑定能收到唯一当前快照；候选未变化时不重复广播。
- HUD/Widget 重建后旧 Widget 回调零增长，新 Widget 只接收一条有效观察链。
- Re-Possess 后一次输入只执行一次；候选与提示状态正确恢复或安全清空。
- 至少两轮连续 PIE 无重复 Delegate、销毁对象访问、Error 或 Ensure。
- Editor 重开后所有引用保持。

### 7.6 候选确定性边界

本阶段只验收一个对象，不实施多候选玩法。组件仍需写明未来规则：候选变化事件发生时，可按可交互性、显式优先级、距离快照和稳定 ID 破平；不得依赖 `TSet` 迭代或 Overlap 到达顺序。玩家移动期间实时重排最近目标需要 Tick、Timer 或额外事件策略，明确列为本阶段非目标。

## 8. P3-003：最终回归、教学、审查与阶段收尾

### 8.1 唯一可验收结果

Phase 3 的工程证据、Editor/PIE 证据、源码教学、独立审查和文档状态全部闭合；阶段结论只依据真实材料判定为 `Ready`、`Not verified` 或 `Blocked`。

### 8.2 强制顺序

1. Coordinator 核对 P3-001/P3-002 的允许范围、源码/资产作者、提交、Build、Editor 重开和 PIE 证据。
2. Teacher 讲解最终源码与数据流并出题；用户保留原始作答。
3. Teacher 纠正答案，记录掌握项与复习项，并提交自己的真实教学产物。
4. Reviewer 独立只读审查实际 diff、反射/GC/Tick、生命周期、Build、Editor/PIE 与失败路径，给出独立结论。
5. 只有 Reviewer `PASS` 或用户明确接受真实剩余风险后，Coordinator 才更新阶段状态并完成收尾；两种处置不得混写。

### 8.3 教学与 Debug 练习

用户应能复述：

```text
Overlap → RegisterCandidate(weak) → CandidateChanged → Prompt observer
IA_Interact → Character → InteractionComponent::TryInteract
→ Interface Can/Execute → InteractionResult
```

至少完成一次真实故障定位练习：“提示存在但按键无效果”，按以下边界逐层排查：

1. IA/IMC 与输入事件是否触发。
2. Character 是否只转发一次。
3. InteractionComponent 是否持有有效弱候选。
4. 对象是否实现 Interface，`CanInteract` 是否返回可交互。
5. Execute 是否执行，Result/日志为何失败。
6. UI 是否只是结果消费者，而不是 Gameplay 真源。

### 8.4 Phase 3 最终 Gate

- 同一个灰盒对象只能通过共享 Interface/Component 路径交互。
- 进入显示提示、按键产生占位反馈、离开隐藏提示。
- Overlap 事件维护弱候选；重复注册/注销幂等。
- 无候选、不可用、失效/销毁对象有结构化失败原因且零崩溃。
- InteractionComponent、灰盒对象与 Widget 无自定义 Gameplay/UI Tick 轮询。
- Widget 不 Cast 具体业务 Actor，不推进 Gameplay。
- 新鲜 UHT/C++/Link 成功；Editor 重开引用保持。
- PIE 主路径、至少一条失败路径、目标销毁、重复注册、Re-Possess 和连续 PIE 有真实证据。
- Move/Look/Jump、控制模式和 Phase 2 GAS 属性/HUD 无回归。
- 新增另一种交互对象只需实现相同接口/派生配置，不需要修改组件选择与提交逻辑。
- Teacher 教学与用户练习有真实记录；Reviewer 结论与用户处置边界明确。
- `PROJECT_STATE.md`、`todo_plan.md`、`worklog.md`、`learning-journal.md`、`README.md` 与实际证据一致。

## 9. 文档归档要求

### 每个工程工作包

- `tasks/active-task.md`：唯一活动契约、允许文件/资产、成功标准、明确非目标和首次执行门禁。
- `tasks/execution-result.md`：实际改动、构建命令/结果、Editor/PIE 证据、失败和未验证项。
- `tasks/archive/`：完成或取消后归档活动卡、执行结果和审查结论，保留历史 `REVISE` 链。
- `worklog.md`：只记录真实决策、命令、证据来源、错误与处置。
- `todo_plan.md`：只在真实 checkpoint 完成后勾选。

### 阶段收尾

- `learning-journal.md`：Interface、ActorComponent、弱引用、Overlap 生命周期、Delegate 与命令边界。
- `README.md`：只有闭环实际成立后才记录交互扩展方式。
- `PROJECT_STATE.md`：当前阶段、真实 Gate、阻塞/非阻断风险和唯一下一步。
- 本文件：将工作包状态和最终证据边界更新为真实结果，不删除历史计划。

## 10. 风险与控制措施

| 风险 | 控制措施 |
|---|---|
| Blueprint 接口调用绕过 UE 反射分发 | 统一使用 `Execute_*` 包装调用 |
| 候选 Actor 被 GC/销毁后仍被访问 | 弱引用、每次使用前复核、EndPlay 清理 |
| Overlap 双边维护导致重复注册 | 固定灰盒对象产生事件、Component 作为唯一候选真源；注册/注销幂等 |
| Widget 成为 Gameplay 真源 | UI 只消费快照/Delegate，不 Cast、不执行交互 |
| 无 Tick 与候选排序需求冲突 | 本阶段单对象；多候选仅记录事件时排序方案，实时重排列为非目标 |
| 交互改动破坏现有输入或 GAS HUD | Character 只替换 Interact 占位入口；执行专项回归 |
| 为未来联机过度设计 | 保留请求/复核边界，不添加 RPC、Replication 或 Prediction |
| 计划被误记为完成 | 所有状态初始为未开始；Gate 只接受真实 Build/Editor/PIE/审查证据 |

## 11. 全阶段明确非目标

- NPC、宝箱、传送点三套业务实现。
- Enemy、AI、EncounterDefinition、Battle Map、OpenLevel 与 Phase 4 稳定跨图 Context。
- 对话、任务、奖励、背包、宝箱持久化与 SaveGame。
- GAS Ability、GameplayEffect、Attribute 或 Gameplay Tag 交互门禁。
- Interaction DataAsset、世界级 Interaction Subsystem 或新 Runtime Module。
- 多候选实时最近目标扫描、视线/镜头锥锁定和每帧 Trace。
- RPC、复制、预测、服务器实现或任何“已支持联网”的承诺。
- 正式模型、动画、音频、VFX、复杂 UI 与第三方资产。
- 大规模重构 Character、Controller、HUD 或 Phase 2 GAS 架构。

## 12. 计划归档后的唯一下一步

创建并审查 `TASK-P3-001` 活动任务卡：只授权交互 Types、Interface、InteractionComponent、ExplorationCharacter 接入、一个灰盒 Actor 及其最小 Editor 资产/验证证据。任务卡经 Reviewer 审查且用户确认执行后，再开始源码与资产实施；不得自动进入 P3-002 或 Phase 4。
