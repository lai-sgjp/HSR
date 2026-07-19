# TASK-P4-003：三种先手、过渡失败恢复与空图测试返回

> 创建日期：2026-07-19
> 状态：`WAITING FOR FIRST READ-BACK`  
> 本卡是当前唯一活动任务契约。P4-002 已归档为 `PASS WITH FOLLOW-UP`；创建本卡不代表已实施、构建、通过 Editor/PIE 或 Phase 4 已完成。

## Role Lock / 首次只读复述与二次确认

低级执行模型当前且仅能担任 `Implementation Agent`，不得切换为 Coordinator、Reviewer、Teacher、资产作者或自行扩大权限。

首次收到本卡后只能读取本文件，不得调用任何工具。必须复述：任务编号、唯一可见结果、三种 initiative 来源、Encounter/Return 状态与数据流、精确允许/只读/禁止文件、用户 Editor 资产、Build/Editor/PIE、失败/重复/生命周期矩阵、P4-002 组合回归、反射/GC/Tick/GAS/网络与 Phase 5 边界、停止条件。末尾必须逐字写：

`等待用户确认执行 TASK-P4-003。`

随后停止。只有用户在复述后另发明确确认，才可调用工具或实施。用户要求进入下一子任务的授权只允许 Coordinator 建卡，不替代二次执行确认。

## 当前 Phase 与前置 Gate

- Phase 0～3 为 `Ready`，历史 follow-up 保留。
- P4-001 已以 Reviewer `PASS WITH FOLLOW-UP` 归档；A2 后 Editor/PIE 缺失由用户 `USER ACCEPTED`。
- P4-002 已以 Reviewer `PASS WITH FOLLOW-UP` 归档；用户 02:07 PIE 支持 Enemy→Encounter→Battle Map 主路径，A1 Build 完整日志已被用户删除且仅保留报告级证据。
- P4-002 继承 follow-up：目标销毁、重复 Perception 计数、MoveTo Failed/Aborted、独立 UnPossess/Re-Possess 未完整专项验证；BP 路径事后接受、Map_BattleTest 误保存后撤回、mixed commit/同 Git 身份/Git 权限偏差保留。

## 唯一可验收结果

玩家主动交互、敌人追击接触和中性测试对象分别产生 `Player`、`Enemy`、`Neutral` 三种确定且可达的 initiative；每次只通过 P4-001 Transition Subsystem 构造纯值 Context。重复触发、无效地图与可控旅行失败不会覆盖或永久悬挂状态。空 `Map_BattleTest` 在单次消费后通过开发测试入口返回探索地图，并由探索 Return Consumer 恢复原 Transform；连续两轮往返无旧 World 引用。此返回仅验证生命周期，不创建 BattleResult、胜负、奖励或正式战后流程。

## 三种 initiative 的唯一来源

- **Player：** 现有玩家主动 `BP_HSRGrayboxInteractable` 交互路径。
- **Enemy：** P4-002 敌人 `Chasing → EncounterCollision → RequestEncounter` 路径。
- **Neutral：** 新建同一灰盒 C++ 类的中性测试 BP/实例，仅把可编辑 initiative 配为 `Neutral`；不复制请求/OpenLevel 蓝图逻辑。
- 缺失或非法输入安全回落/拒绝规则由 C++ 明确，Blueprint 不推导 initiative。

## 执行角色

- **Implementation Agent：** 只修改精确 C++，执行 fresh Build 并只写执行报告；不得操作资产或 Git。
- **用户/Editor 资产作者：** 修改/创建允许资产，完成两轮往返、失败矩阵和 P4-002 组合回归，独立提交资产。
- **Reviewer：** 独立核对源码、反射/GC/状态机、Build、Editor/PIE、失败与边界后给唯一结论。
- **Coordinator：** 审查后同步/归档并进入 P4-004；不得把本任务放行写成 Phase 4 自动完成。

## 执行前读取边界

- 首次复述：仅 `tasks/active-task.md`。
- 二次确认后：仅按需读取允许修改文件、直接 include 依赖与精确只读文件；不得读取全局文档扩充目标。

## 精确允许修改文件

### Implementation Agent

- `Source/HSR/Battle/HSREncounterTypes.h`
- `Source/HSR/Battle/HSRBattleTransitionSubsystem.h`
- `Source/HSR/Battle/HSRBattleTransitionSubsystem.cpp`
- `Source/HSR/Battle/HSRBattleTestConsumer.h`
- `Source/HSR/Battle/HSRBattleTestConsumer.cpp`
- `Source/HSR/Battle/HSRExplorationReturnConsumer.h`
- `Source/HSR/Battle/HSRExplorationReturnConsumer.cpp`
- `Source/HSR/Exploration/HSRGrayboxInteractable.h`
- `Source/HSR/Exploration/HSRGrayboxInteractable.cpp`
- `Source/HSR/Enemy/HSREnemyAIController.h`
- `Source/HSR/Enemy/HSREnemyAIController.cpp`
- `tasks/execution-result.md`

不得修改 `HSR.Build.cs`；现有 `Core/CoreUObject/Engine/AIModule/NavigationSystem` 应满足本包。若实际编译需要额外模块或文件，停止请求扩权。

### 用户 Editor 资产作者

- `Content/Blueprints/Exploration/BP_HSRGrayboxInteractable.uasset`
- `Content/Blueprints/Exploration/BP_HSRNeutralEncounterTest.uasset`
- `Content/Maps/Map_Phase1_Exploration.umap`
- `Content/Maps/Map_BattleTest.umap`

地图仅允许放置/配置 Neutral 测试对象、Exploration Return Consumer，以及 Battle Test Consumer 的测试返回属性；不得添加正式 Battle GameMode、战斗角色、UI、奖励或结果流程。

## 精确只读文件与资产

- `Source/HSR/Data/Definitions/HSREncounterDefinition.h/.cpp`
- `Source/HSR/Data/Definitions/HSREnemyDefinition.h/.cpp`
- `Source/HSR/Enemy/HSREnemyCharacter.h/.cpp`
- `Source/HSR/Enemy/HSREnemyTypes.h`
- `Source/HSR/Character/HSRExplorationCharacter.h/.cpp`
- `Source/HSR/Interaction/HSRInteractionComponent.h/.cpp`
- `Source/HSR/UI/HSRHUD.h/.cpp`
- `Source/HSR/GAS/HSRAbilitySystemComponent.h/.cpp`
- `Source/HSR/HSR.Build.cs`
- `Content/Data/Encounters/DA_Encounter_Phase4Test.uasset`
- `Content/Data/Enemies/DA_Enemy_Phase4Test.uasset`
- `Content/Blueprints/Character/Enemy/BP_HSREnemy_Phase4Test.uasset`
- `Content/Input/IA_Interact.uasset`
- `Content/Input/IMC_Exploration.uasset`
- `Content/UI/WBP_ExplorationHUD.uasset`

Implementation Agent 不得脚本修改二进制资产或声称验证内部配置。

## 禁止修改内容

- `HSR.uproject`、`Source/HSR/HSR.Build.cs`、全部 `Config/`，以及白名单外 Source/Content/Markdown。
- Encounter/Enemy Definition schema、Enemy Character、Character/Interaction/UI/GAS/Framework/Player/TurnSystem/SaveGame。
- Battle Actor/ASC 重建、TurnManager、Ability、攻击、伤害、胜负、奖励、BattleResult、正式战后返回。
- Blueprint/Widget/Level Blueprint 复制 RequestEncounter、Pending、OpenLevel 或 Return 状态真源。
- 自定义 Gameplay/UI Tick、世界扫描、每帧 MoveTo、无界 Timer 重试。
- RPC、Replication、Prediction、PlayerState ASC、新模块/插件、第三方资产。
- 删除、重命名、批量操作、Git stage/commit/push/reset/clean。

## 固定类型与数据所有权

### Encounter Request 与 initiative

- `RequestEncounter` 增加显式 `EHSREncounterInitiative` 输入；所有调用者必须传入，不允许 Subsystem 根据 Actor 类型或 World 隐式猜测。
- Request 继续只含纯值 DTO；initiative 写入后跨图消费，不进入 TurnSystem 或决定行动队列。
- Graybox 暴露安全默认 `Player` 的 initiative 配置；正式玩家交互 BP 保持 Player，中性测试 BP 配 Neutral。
- P4-002 AIController 固定传 Enemy；不得新增第二套请求或 OpenLevel。

### Return Context

- 新增最小 `FHSRExplorationReturnContext`（或等价类型）：原 Encounter Request ID、探索地图稳定软路径、返回 Transform；安全默认值，不含 Actor/World/ASC/Widget/Handle。
- Transition Subsystem 是 Pending Return Context 的唯一跨图 Runtime 真源；Battle Consumer 只提交测试返回请求，Exploration Return Consumer 只消费并应用一次。
- Return Context 必须单次消费；第二次返回结构化失败，不重复 Teleport。
- Exploration Return Consumer 可使用一次性 next-tick/短 Timer 等待玩家 Pawn，但必须有有界尝试、成员 Timer Handle 和 EndPlay 清理；无 Tick、无无限重试。

## Transition 状态、重复触发与失败恢复

- Encounter 的 `Empty/Pending/Traveling/Consumed` 语义继续保持；Return 使用独立明确状态/Presence，不能覆盖未完成 Encounter。
- 同帧/连续两个请求时，首个 Request ID 和 initiative 保持，第二个返回 `AlreadyPending`，不得触发第二次 OpenLevel。
- 请求前验证 Definition、稳定 ID、软地图非空及可由 UE5.6 安全检查的 package/path；所有前置失败不污染 Pending。
- 使用 UE5.6 实际头文件确认的本地 travel failure delegate/回调，在 `Traveling` 失败时清理对应 Encounter/Return Pending 并记录 Request ID、地图和失败类型。不得猜 API；若可靠回调需要越权或无法确认，停止报告。
- 失败清理不能清除无关的新请求；delegate 必须在 Subsystem Initialize/Deinitialize 对称绑定/解绑，避免重复回调。
- Battle Consumer 的自动测试返回必须可配置且默认关闭；开启后第一次 Consume 成功、第二次失败，再按有界延迟调用统一 Return API。

## 空 Battle Map 测试返回

```text
Exploration RequestEncounter(initiative)
→ Battle Map Consume Encounter once
→ Battle Test Consumer RequestTestReturn(consumed DTO)
→ Transition Subsystem 保存纯值 Return Context / OpenLevel Exploration
→ Exploration Return Consumer ConsumeReturn once
→ 恢复 Player Pawn Transform
→ 第二次 ConsumeReturn 失败，状态清理
```

- 测试返回不是 BattleResult，不判断胜负、不给奖励、不影响 SaveGame。
- 返回 Transform 应在合理位置/旋转容差内恢复；若碰撞导致调整，记录实际值与原因，不静默宣称完全相等。
- Battle Map 直接启动时没有 Encounter/Return Context，Consumer 只输出可诊断失败，不制造 Error 风暴或错误旅行。

## P4-002 继承组合回归

本包至少保存一条组合生命周期证据，覆盖下列四项中的至少三项，并明确未覆盖项：

1. 追击中目标销毁/失效，弱引用清理且无悬挂访问。
2. 同一目标重复 Perception 更新，不产生 MoveTo/Timer/Request 风暴。
3. MoveTo Failed 或 Aborted 后有界恢复，无同步递归或忙循环。
4. 独立 UnPossess→Re-Possess，旧 Timer/Delegate 零增长，新实例/新 Possession 只保留一条观察链。

组合回归后敌人仍能恢复为合法状态并完成一次 Enemy initiative Encounter→Battle→Return；不得为测试添加永久作弊逻辑或扩大资产范围。

## 反射、GC、Tick、GAS 与网络边界

- 新/改 `UENUM/USTRUCT/UCLASS/UFUNCTION` 宏正确，`*.generated.h` 最后 include，字段有安全默认值。
- UObject 引用使用 `UPROPERTY/TObjectPtr`；跨图只传 `FGuid/FName/enum/FTransform` 等值；Timer/delegate 生命周期对称清理。
- Battle/Return Consumers、Graybox、AIController 不新增自定义 Gameplay Tick；只允许有界一次性 Timer，禁止轮询。
- GAS 只回归，不保存/修改 ASC、Attribute、GE、Ability；Return 不把旧 ASC 跨图带回。
- 网络只保留 Request/Consume/Return 验证边界，不实现 RPC、复制、预测或声称服务器权威。

## 实现步骤

1. 二次确认后核对允许范围；发现意外修改不覆盖，停止报告。
2. 扩展纯值类型：显式 initiative 输入、结构化 Return Context/Result/状态，安全默认值。
3. 扩展 Transition Subsystem：三种 initiative、重复拒绝、前置地图校验、travel failure 对称回调与 Return 单次事务。
4. Graybox 传可配置 Player/Neutral；AIController 固定传 Enemy，不改变 P4-002 状态机职责。
5. 扩展 Battle Test Consumer：记录 initiative/Context、可选有界延迟测试返回；新建无 Tick Exploration Return Consumer 单次恢复 Transform。
6. 静态审查 DTO、delegate/Timer、无 Tick、GAS/网络/Phase 5 边界和全部调用点。
7. 关闭 Editor 后 fresh 构建 `HSREditor Win64 Development`，保存命令、目标、退出码、UHT/C++/Link 和第一处 warning/error。
8. 只更新 `tasks/execution-result.md`，按 S/B/D 证据分层，停止交接用户。

## 用户 UE Editor 操作

1. fresh Build 成功后完整重开 Editor，确认新增/变更类型与属性可加载。
2. 现有 `BP_HSRGrayboxInteractable` 明确设为 Player；创建 `BP_HSRNeutralEncounterTest` 并只把 initiative 设 Neutral、复用同一 Encounter Definition。
3. 在探索地图放置一个 Neutral 测试对象和一个 Exploration Return Consumer；避免与 Enemy/玩家出生点重叠。
4. 在 Battle Map 保留恰好一个 Battle Test Consumer，开启本包的测试自动返回并配置合理延迟；不添加战斗规则。
5. 保存四个允许资产，重开 Editor 核对 Parent、initiative、Definition、两个 Consumer、地图引用保持。
6. 分别执行 Player、Enemy、Neutral 三条完整往返；每条核对 Request ID、initiative、单次 Encounter/Return Consume、地图和 Transform。
7. 执行重复触发、无效/不存在地图、直接启动 Battle Map、两轮连续往返及 P4-002 组合生命周期回归；回传日志/截图/录屏或结构化结果。
8. 用户独立提交资产并回传 hash，不与 Implementation/Reviewer 文档混合。

## Build / Editor / PIE 验收矩阵

### Build 与静态

- [ ] fresh Development Editor 退出码 0；UHT、全部相关 C++、Link 有保留日志，`up to date` 不冒充 fresh。
- [ ] DTO 无 Actor/World/UObject/ASC/Widget/Handle；Return payload 单次消费。
- [ ] travel delegate Initialize/Deinitialize 对称；Timer 有界并在 EndPlay 清理；无自定义 Tick。
- [ ] 全部 RequestEncounter 调用点显式传 initiative，且只有 Transition Subsystem OpenLevel。

### 三种 initiative 与往返

- [ ] Player/Enemy/Neutral 三条路径分别消费正确 initiative，来源可解释且不由 Blueprint 核心逻辑推导。
- [ ] 每条在 Battle Map Encounter 只消费一次；Return Context 只消费一次并恢复探索 Transform。
- [ ] 至少两轮连续往返无旧 Request/Return、重复 Consumer/delegate/Timer 或旧 World 引用。

### 重复与失败恢复

- [ ] 同帧/连续请求只有首个成功，第二个 `AlreadyPending`，首个 ID/initiative 不被覆盖且只有一次 travel。
- [ ] Definition/ID/空地图/不存在软地图在旅行前失败且状态干净，修正后可重试。
- [ ] 可控 travel failure 触发回调后从 Traveling 安全恢复，之后合法请求可成功；若无法在允许范围安全制造，保留真实边界并由 Reviewer决定 Gate，不伪造。
- [ ] Battle Map 直接启动：Consume/Return 安全失败，不产生错误往返。
- [ ] 返回地图/Transform 无效：不覆盖合法 Context，失败可诊断并可清理/重试。

### P4-002 组合与全局回归

- [ ] 保存一条覆盖至少三项的目标销毁/重复 Perception/MoveTo Failed-Aborted/UnPossess-RePossess 组合证据，未覆盖项明确列出。
- [ ] Enemy 恢复后完成 Enemy initiative 往返，无重复 Request/OpenLevel。
- [ ] Move/Look/Jump、Exploration/UIOnly、Prompt、NoCandidate、GAS HUD 正常。
- [ ] 无未解释 Error/Ensure/Fatal/GC warning/Blueprint Runtime Error。

## 停止条件

- 需要修改 Definition schema、Enemy Character、Character/Interaction/UI/GAS/Config/Build.cs 或白名单外文件。
- UE5.6 travel failure delegate/package validation API 无法由实际头文件或编译证据确认。
- 需要 BattleResult、胜负、奖励、正式返回、TurnManager、Battle Actor/ASC、新模块/插件或网络。
- Build 非 0、资产真实路径与白名单不一致或无法安全复现失败。

出现任一条件立即保留第一处真实证据并请求 Coordinator/用户授权，不得绕过或伪造。

## 明确非目标

- Phase 5 BattleResult 驱动的正式战后返回、Battle Actor/ASC、TurnManager、攻击、胜负、奖励。
- 多敌群、多队伍、复杂 initiative 公式、中途战斗存档、SaveGame。
- 正式 UI/动画/音频/VFX、第三方资产、联网。
- P4-004 的最终教学、独立全阶段审查和 Phase 4 Ready 归档。

## 执行后职责与 Git

- Implementation 只更新 `tasks/execution-result.md`，不得更新本卡/长期状态或自行判 `PASS`。
- Reviewer 后续只更新 `tasks/final-review.md`；Coordinator 审查后归档并创建 P4-004，而不是 Phase 5。
- 本卡不授权 Implementation 执行 Git；任务中途不 push，各角色只提交自己的真实产物。

## 当前唯一下一步

Implementation Agent 首次只读复述本卡并逐字等待用户二次确认。确认前不得调用工具、实施或执行 Git；不得自动进入 P4-004 或 Phase 5。
