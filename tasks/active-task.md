# TASK-P4-002：数据驱动探索敌人与事件驱动感知追击

> 创建日期：2026-07-18  
> 状态：`WAITING FOR FIRST READ-BACK`  
> 本卡是当前唯一活动任务契约。P4-001 已归档为 `PASS WITH FOLLOW-UP`；创建本卡仅代表规划完成，不代表 P4-002 已实施、构建或通过 Editor/PIE。

## Role Lock / 首次执行与二次确认门禁

低级执行模型当前且仅能担任 `Implementation Agent`，不得切换为 Coordinator、Reviewer、Teacher、资产作者或擅自扩大权限。

首次收到本卡后只能读取本文件，不得调用工具。必须复述：任务编号、唯一可见结果、状态机和事件链、精确允许/只读/禁止文件、Build.cs 依赖边界、用户 Editor 操作、Build/Editor/PIE 证据、反射/GC/Tick/GAS/网络风险、无 NavMesh/丢失目标/目标销毁/重复感知失败矩阵和停止条件。末尾必须逐字写：

`等待用户确认执行 TASK-P4-002。`

随后停止。只有用户在复述后另发明确确认，才可调用工具或实施；用户要求进入下一子任务协调工作不替代这次二次确认。

## 当前 Phase 与前置 Gate

- Phase 0～3 均为 `Ready`，历史 follow-up 保留。
- P4-001 最终 Reviewer 结论为 `PASS WITH FOLLOW-UP`，允许相邻规划；A2 后 Editor 重开/PIE 缺失由用户 `USER ACCEPTED`，不是动态验证通过。
- P4-001 已建立 Encounter Definition、纯值 Request、`UHSRBattleTransitionSubsystem::RequestEncounter`、空 Battle Map Consumer 和单次消费语义。
- P4-002 必须重新覆盖至少一条“探索触发 → RequestEncounter → Battle Map 单次消费”主路径，降低 P4-001 已接受的运行证据风险。

## 唯一可验收结果

在 `Map_Phase1_Exploration` 中，一个由 Enemy Definition 配置的灰盒敌人由 AIController 事件驱动巡逻；AI Perception 发现玩家后进入追击，丢失/销毁目标或 MoveTo 失败时安全恢复；玩家进入敌人的 Encounter 范围后，敌人只复用 P4-001 的 `RequestEncounter` 入口并最多触发一次跨图请求。整个流程无自定义 Gameplay Tick，不由 AI、Blueprint 或 UI 直接 OpenLevel。

## 范围拆分

- 本包只完成单敌人、单玩家、灰盒巡逻/发现/追击与 Encounter 请求复用。
- P4-003 才处理三种 initiative、OpenLevel 失败恢复、重复往返和空图测试返回。
- Phase 5 才处理 Battle Actor/ASC 重建、TurnManager、攻击、胜负、奖励和正式返回。

## 执行角色

- **Implementation Agent：** 精确允许范围内实现 C++/Build.cs，并执行 fresh Build；只写执行报告，不操作资产或 Git。
- **用户/Editor 资产作者：** 创建 Enemy DataAsset/BP，修改探索地图的 NavMesh/敌人摆放，重开 Editor、执行 PIE 并独立提交资产。
- **Reviewer：** 独立审查源码、依赖、反射/GC/Tick、AI 生命周期、Build/PIE 与越权后给结论。
- **Coordinator：** 审查后同步和归档，不替代执行者、用户或 Reviewer。

## 执行前读取边界

- 首次复述：仅 `tasks/active-task.md`。
- 二次确认后：仅按需读取允许修改文件、直接 include 依赖和精确只读文件；不得读取全局文档扩充任务。

## 精确允许修改文件

### Implementation Agent

- `Source/HSR/Enemy/HSREnemyTypes.h`
- `Source/HSR/Enemy/HSREnemyCharacter.h`
- `Source/HSR/Enemy/HSREnemyCharacter.cpp`
- `Source/HSR/Enemy/HSREnemyAIController.h`
- `Source/HSR/Enemy/HSREnemyAIController.cpp`
- `Source/HSR/Data/Definitions/HSREnemyDefinition.h`
- `Source/HSR/Data/Definitions/HSREnemyDefinition.cpp`
- `Source/HSR/HSR.Build.cs`
- `tasks/execution-result.md`

可创建不存在的上述目录。`HSR.Build.cs` 只允许增加实际编译所需的 `AIModule` 与 `NavigationSystem`；现有 `GameplayTasks` 已存在，不得重排或增加其他模块。若 UE5.6 编译证明还需其他依赖，停止请求扩权。

### 用户 Editor 资产作者

- `Content/Data/Enemies/DA_Enemy_Phase4Test.uasset`
- `Content/Blueprints/Enemy/BP_HSREnemy_Phase4Test.uasset`
- `Content/Maps/Map_Phase1_Exploration.umap`

地图修改只允许：放置一个 Enemy BP、`NavMeshBoundsVolume` 和本任务所需灰盒导航范围；不得删除或重构现有玩家、交互物、HUD、地图规则。

## 精确只读文件与资产

- `Source/HSR/Character/HSRCharacterBase.h/.cpp`
- `Source/HSR/Character/HSRExplorationCharacter.h/.cpp`
- `Source/HSR/Battle/HSREncounterTypes.h`
- `Source/HSR/Battle/HSRBattleTransitionSubsystem.h/.cpp`
- `Source/HSR/Data/Definitions/HSREncounterDefinition.h/.cpp`
- `Source/HSR/GAS/HSRAbilitySystemComponent.h/.cpp`
- `Source/HSR/Exploration/HSRGrayboxInteractable.h/.cpp`
- `Content/Data/Encounters/DA_Encounter_Phase4Test.uasset`
- `Content/Maps/Map_BattleTest.umap`
- `Content/Blueprints/Exploration/BP_HSRGrayboxInteractable.uasset`
- `Content/Input/IA_Interact.uasset`
- `Content/Input/IMC_Exploration.uasset`
- `Content/UI/WBP_ExplorationHUD.uasset`

Implementation Agent 不得脚本修改二进制资产或声称验证其内部配置。

## 禁止修改内容

- `HSR.uproject`、全部 `Config/`，以及允许列表外的 Source/Content/Markdown。
- P4-001 Encounter/Transition/Consumer/Graybox 源码；本包只能调用其公开 `RequestEncounter`，发现接口不足必须停止请求扩权。
- Player、Interaction、UI、GAS、Framework、Battle、TurnSystem、SaveGame 源码。
- Behavior Tree、Blackboard、EQS、StateTree、Mass、复杂队列、多敌人选择、多目标或战斗 AI。
- 自定义 Gameplay Tick、每帧距离扫描、`GetAllActorsOfClass`、每帧 MoveTo、Widget Tick。
- Ability、伤害、胜负、奖励、BattleResult、返回探索、三种 initiative、旅行失败恢复。
- RPC、Replication、Prediction、PlayerState ASC、新模块/插件、第三方资产。
- 删除、重命名、批量移动/格式化、Git stage/commit/push/reset/clean。

## 固定架构与状态机

### DataAsset / Runtime 边界

- `UHSREnemyDefinition` 为 `UPrimaryDataAsset`，至少保存稳定非空 `EnemyDefinitionId`、P4-001 Encounter Definition 引用，以及原创的巡逻半径、巡逻等待、追击接受半径等最小静态配置；所有数值有安全默认值和非负 Clamp 元数据。
- Definition 只保存静态数据，不保存 World Actor、Controller、感知目标、MoveRequest、Timer Handle、ASC 或运行状态。
- `AHSREnemyCharacter` 持有受 GC 保护的 Definition 引用、当前 World 的 Encounter 触发碰撞和生成点快照；不得成为 AI 决策或跨图 Pending 真源。

### 探索状态与事件链

`EHSREnemyExplorationState` 至少表达：

```text
Idle → PatrolWaiting → MovingToPatrol
  → Alert/Chasing → EncounterPending
  ↘ MoveFailed / LostTarget / TargetInvalid → Idle 或 PatrolWaiting
```

- AIController 拥有探索状态、感知组件和当前 World 的目标弱引用；Enemy Character 不复制 Controller 状态。
- 巡逻由 MoveCompleted 和有限的一次性 Timer/事件触发下一目标；Timer 只用于巡逻等待，必须在 UnPossess/EndPlay 清理，禁止轮询。
- 感知只由 `OnTargetPerceptionUpdated` 或 UE5.6 等价委托驱动。重复感知同一目标不得重复绑定、重复创建 Timer、每帧下发 MoveTo 或重复请求 Encounter。
- 当前目标使用 `TWeakObjectPtr<AActor>`；每次追击、MoveCompleted、丢失感知和 Encounter 前复核有效性与玩家身份。
- 丢失目标、目标销毁、UnPossess、EndPlay、MoveTo Failed/Aborted 均清弱引用并回到安全状态；不得忙循环重试。

### 巡逻、NavMesh 与 Encounter

- 巡逻点在生成点周围按 Definition 半径通过 NavigationSystem 选择；无 NavMesh/找点失败时保持 Idle/PatrolWaiting、输出可诊断日志，并以有限等待重试或停止，不触发 Encounter。
- AI MoveTo 只由状态事件下发；Character/Controller 均不直接 OpenLevel。
- Enemy Character 的事件驱动 Encounter 碰撞只接受有效 `AHSRExplorationCharacter`；触发时调用 AIController/Character 的单一 `TryRequestEncounter` 边界，最终只调用 P4-001 `RequestEncounter(EncounterDefinition)`。
- Request 成功后进入 `EncounterPending`，停止移动、清理巡逻 Timer 并拒绝重复触发。失败时记录 P4-001 Result；配置失败可安全留在非 Pending 状态，不覆盖 Subsystem 请求。
- 本包固定 `Neutral` initiative，不另建 OpenLevel、Pending 或 Consume 逻辑。

### 反射、GC、Tick、GAS、网络

- `UENUM/UCLASS` 宏正确，`*.generated.h` 是最后 include；反射字段有安全默认值。
- UObject/Component 引用用 `UPROPERTY/TObjectPtr`；当前目标用弱引用；动态委托、Timer 在生命周期边界成对清理。
- Character 与 AIController 明确关闭自定义 Gameplay Tick；允许引擎内部 CharacterMovement、AI MoveTo、Perception 工作，不把“无自定义 Tick”误写成引擎完全无 Tick。
- Enemy 可继承现有 CharacterBase/ASC 基线，但本包不授予 Ability、不应用 GE、不修改 Attribute、不让 GAS 决定 AI 状态或 Encounter。
- 未来权威边界只保留 `RequestEncounter` 与目标复核点；禁止实现或声称 RPC/复制/预测。

## 实现步骤

1. 二次确认后核对工作树与允许范围；发现意外修改不覆盖，停止报告。
2. 仅给 Build.cs 增加实际必需的 AI/Navigation 依赖。
3. 创建探索状态 enum 与 Enemy PrimaryDataAsset，锁定安全默认值和静态/Runtime 边界。
4. 创建无自定义 Tick 的 Enemy Character：Definition、Encounter overlap、生成点快照和唯一请求转发。
5. 创建无自定义 Tick 的 AIController：Perception、目标弱引用、巡逻 Timer、MoveTo/MoveCompleted 与安全状态转换。
6. 静态检查无世界扫描、无 P4-001 源码修改、无 GAS/网络/Phase 5 越界。
7. 关闭 Editor 后 fresh 构建 `HSREditor Win64 Development`；保存完整命令、目标、退出码、UHT/C++/Link 和第一处 warning/error。
8. 只更新 `tasks/execution-result.md`，按静态/Build/动态 PIE 分层记录证据与未验证项，然后停止交接用户。

## 用户 UE Editor 手动操作

1. fresh Build 成功后完整重开 Editor，确认 Enemy Definition、Character、AIController 类型可加载。
2. 创建 `DA_Enemy_Phase4Test`：填写原创稳定 ID，绑定只读 `DA_Encounter_Phase4Test`，配置最小巡逻/等待/追击数值。
3. 创建 `BP_HSREnemy_Phase4Test`：绑定 Definition、灰盒 Mesh/材质、AIControllerClass、Auto Possess AI、Encounter 碰撞；不在 Event Graph 写感知、状态机、MoveTo、OpenLevel 或 RequestEncounter。
4. 修改 `Map_Phase1_Exploration`：放置一个 Enemy BP 和覆盖玩家/敌人活动范围的 `NavMeshBoundsVolume`；按 `P` 可视化确认绿色导航区域。
5. 保存三个允许资产，完整重开 Editor，核对 Parent、Definition、Encounter 引用、AIControllerClass、Auto Possess AI、碰撞和 NavMesh 保持。
6. 执行下方 PIE 主路径、失败矩阵和 P4-001 跨图回归；提供 Output Log、截图/录屏或结构化逐项结果。资产由用户独立提交并回传 hash。

## Build / Editor / PIE 验收

### Build 与静态

- [ ] fresh Development Editor 退出码 0；UHT、全部新增 C++、Link 有保存证据，`up to date` 不冒充本轮构建。
- [ ] Build.cs 只增加实际需要的 `AIModule`/`NavigationSystem`，无其他依赖或模块变化。
- [ ] 无自定义 Gameplay Tick、世界扫描、每帧 MoveTo；Timer 仅用于一次性巡逻等待并能清理。
- [ ] Definition 无 Runtime/World 状态；目标为弱引用；委托/Timer 生命周期安全。

### Editor 重开与主路径

- [ ] 重开后 Enemy DA/BP、AIController、Auto Possess AI、Perception、Encounter Definition、碰撞、NavMesh 与地图引用保持。
- [ ] 有 NavMesh 时敌人 `Idle/PatrolWaiting → MovingToPatrol`，不会忙循环或每帧重发 MoveTo。
- [ ] 发现玩家只建立一个有效目标并进入 `Alert/Chasing`；接近后只提交一次 P4-001 Request，进入 `EncounterPending`。
- [ ] 至少一条完整 `探索敌人 → RequestEncounter → Map_BattleTest → First Consume SUCCESS / Second AlreadyConsumed` 主路径成立。

### 失败与生命周期矩阵

- [ ] **无 NavMesh：** 找点/MoveTo 可诊断失败，保持安全状态，不崩溃、不忙循环、不触发 Encounter。
- [ ] **丢失目标：** 感知丢失后停止追击、清弱引用，安全返回 Idle/PatrolWaiting。
- [ ] **目标销毁：** 追击中销毁/失效目标不访问悬挂对象，无 crash/ensure/GC warning。
- [ ] **重复感知：** 同一目标重复 Updated 不产生重复 Timer、重复 MoveTo 风暴、重复 Encounter/OpenLevel。
- [ ] **MoveTo Failed/Aborted：** 有结构化状态恢复，不立即无限重试。
- [ ] **Definition/Encounter 缺失：** 不请求、不旅行，日志可定位稳定 Enemy ID/失败原因，修正后可重试。
- [ ] **UnPossess/EndPlay：** Timer/委托/目标清理，第二轮 PIE 无残留回调。

### 回归

- [ ] P4-001 A2 后 ExplorationMapPath、Consume 清 Payload、第二次空 DTO 的运行现象有至少一条新证据。
- [ ] Move/Look/Jump、Exploration/UIOnly、Prompt、NoCandidate、目标销毁与 GAS HUD 正常。
- [ ] 至少两轮 PIE 无旧 Request、重复回调、Error/Ensure/GC warning/Blueprint Runtime Error。

## 停止条件

- 需要修改 P4-001 Battle/Encounter/Graybox、Character/Interaction/UI/GAS/Config 或白名单外文件。
- UE5.6 AI/Perception/Navigation API 无法由实际头文件或编译确认。
- 需要 Behavior Tree/EQS/新插件/模块、网络、正式战斗或返回逻辑。
- Build 非 0 或 Editor 资产实际路径与白名单不同。

出现任一条件立即保留第一处证据并请求 Coordinator/用户扩权，不得自行绕过。

## 明确非目标

- 多敌人、多候选、队伍、复杂巡逻路线、视野 UI、攻击动画或战斗 AI。
- 三种 initiative、旅行失败恢复、测试返回与连续往返（P4-003）。
- Battle Actor/ASC、TurnManager、Ability、伤害、胜负、奖励、BattleResult、正式返回（Phase 5）。
- SaveGame、正式美术/音频/VFX、第三方资产、RPC/Replication/Prediction。

## 执行后职责与 Git

- Implementation 只更新 `tasks/execution-result.md`，不得更新本卡/长期状态或自行判 `PASS`。
- Reviewer 后续只更新 `tasks/final-review.md`；Coordinator 审查后归档和同步。
- 各角色只提交自己的真实产物；本卡不授权 Implementation 执行 Git。任务中途不 push。

## 当前唯一下一步

Implementation Agent 首次只读复述本卡并逐字等待用户二次确认。确认前不得调用工具或实施；不得自动进入 P4-003 或 Phase 5。
