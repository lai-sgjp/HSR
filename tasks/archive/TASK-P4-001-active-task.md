# TASK-P4-001：稳定 Encounter 合同与空 Battle Map 单次消费

> 创建日期：2026-07-18  
> 最终状态：`PASS WITH FOLLOW-UP / ARCHIVED`  
> 最终 Reviewer 结论：A2 后 Editor/PIE 缺口由用户明确 `USER ACCEPTED`；不等于动态验证通过。  
> 本卡是当前唯一活动任务契约。用户已授权进入本任务，但创建任务卡不等于已实施、已构建、已通过 PIE 或已通过审查。

## Role Lock / 首次执行门禁

低级执行模型当前且仅能担任 `Implementation Agent`，不得切换为 Coordinator、Reviewer、Teacher、资产作者或扩大权限的角色。

首次收到本卡后只能读取本文件，不得调用任何工具。必须复述：任务编号、唯一结果、实现顺序、Pending 状态机、精确允许/只读/禁止文件、用户 Editor 操作、Build/PIE 证据、反射/GC/Tick/GAS/网络风险、失败矩阵与停止条件。末尾必须逐字写：

`等待用户确认执行 TASK-P4-001。`

随后停止。只有用户在复述后另发明确确认，才可调用工具或实施；此前进入 Phase 4 的授权不替代本次二次确认。

## 当前 Phase 与前置 Gate

- Phase 0～2 为 `Ready`。
- Phase 3 已以 Reviewer commit `3b3fbeb` 的 `PASS WITH FOLLOW-UP` 归档为 `Ready`。
- 继承但不阻断本包的边界：Phase 3 最终完整 fresh Build/UHT 控制台缺失；最终 HUD rebuild/Re-Possess、目标销毁/弱失效未补新证据；`OutOfRange` 未动态命中；相关学习和证据等级保持。
- Phase 4 Gameplay 尚未实施；本卡和 `docs/phase-4-execution-plan.md` 仅是规划证据。

## 唯一可验收结果

玩家在 `Map_Phase1_Exploration` 只通过现有 `Character → InteractionComponent → AHSRGrayboxInteractable` 路径触发一次测试 Encounter。灰盒对象依据 Encounter Definition 构造不含旧 World 对象引用的纯数据 Request，统一交给 `UHSRBattleTransitionSubsystem` 校验并进入 `Map_BattleTest`；Battle Test Consumer 第一次取得正确 Request ID、稳定 IDs、地图和返回 Transform，第二次消费结构化失败。缺失 Definition、ID 或 Battle Map 时不旅行、不留下脏 Pending，修正后可重试。

本包不创建 Enemy/AI/NavMesh，不实现返回探索或任何战斗规则。

## 规划审查摘要

- 先验证 DataAsset 静态定义、GameInstance Runtime Context、OpenLevel 生命周期和单次消费，再在 P4-002 加探索 AI。
- 复用 Phase 3 交互链，不修改 InteractionComponent、Character、HUD、输入或 GAS。
- 本包不需新模块；若编译证明需要 `Build.cs`、Config、插件或额外文件，停止请求扩权。
- P4-002：Enemy/AI/感知/追击；P4-003：三种 initiative、旅行失败专项和测试返回；P4-004：最终教学、审查和归档。

## 执行者与职责

- **Implementation Agent：** 仅修改允许 C++，执行获确认后的 fresh Build，记录真实结果；不操作资产或 Git。
- **用户/Editor 资产作者：** 创建/绑定三个允许资产，重开 Editor，执行 PIE 并独立提交资产。
- **Reviewer：** 独立核对 diff、反射、GC、状态机、Tick、Build、PIE 和越权，给出唯一审查结论。
- **Coordinator：** 审查后同步与归档；不替代其他角色。

## 执行前读取边界

- 首次复述：仅 `tasks/active-task.md`。
- 二次确认后：仅按需读取允许修改文件、其直接 include 依赖和下列只读文件；不得读取全局文档扩充需求。

## 精确允许修改文件

### Implementation Agent

- `Source/HSR/Battle/HSREncounterTypes.h`
- `Source/HSR/Data/Definitions/HSREncounterDefinition.h`
- `Source/HSR/Data/Definitions/HSREncounterDefinition.cpp`
- `Source/HSR/Battle/HSRBattleTransitionSubsystem.h`
- `Source/HSR/Battle/HSRBattleTransitionSubsystem.cpp`
- `Source/HSR/Battle/HSRBattleTestConsumer.h`
- `Source/HSR/Battle/HSRBattleTestConsumer.cpp`
- `Source/HSR/Exploration/HSRGrayboxInteractable.h`
- `Source/HSR/Exploration/HSRGrayboxInteractable.cpp`
- `tasks/execution-result.md`

可按路径创建不存在的目录；不得创建额外 Helper、替代命名或空文件。

### 用户 Editor 资产作者

- `Content/Data/Encounters/DA_Encounter_Phase4Test.uasset`
- `Content/Blueprints/Exploration/BP_HSRGrayboxInteractable.uasset`
- `Content/Maps/Map_BattleTest.umap`

## 精确只读文件

- `Source/HSR/Interaction/HSRInteractionTypes.h`
- `Source/HSR/Interaction/HSRInteractableInterface.h`
- `Source/HSR/Interaction/HSRInteractionComponent.h`
- `Source/HSR/Interaction/HSRInteractionComponent.cpp`
- `Source/HSR/Character/HSRExplorationCharacter.h`
- `Source/HSR/Character/HSRExplorationCharacter.cpp`
- `Source/HSR/HSR.Build.cs`
- `Content/Maps/Map_Phase1_Exploration.umap`
- `Content/Input/IA_Interact.uasset`
- `Content/Input/IMC_Exploration.uasset`
- `Content/Blueprints/Character/Player/BP_HSRExplorationCharacter.uasset`
- `Content/UI/WBP_ExplorationHUD.uasset`

二进制资产只由用户在 Editor 核对，Implementation Agent 不得脚本修改或声称验证内部配置。若现有地图实例必须修改，先请求扩权。

## 禁止修改内容

- `HSR.uproject`、`Source/HSR/HSR.Build.cs`、全部 `Config/`。
- `Source/HSR/Character/`、`Interaction/`、`Player/`、`GAS/`、`UI/`、`Framework/`。
- 除三个用户资产外的全部 `Content/`；低级模型不得创建、修改或提交资产。
- Enemy Character/Definition、AIController、Perception、NavMesh、巡逻、追击、Behavior Tree、EQS。
- Battle Actor/ASC、TurnManager、Ability、攻击、胜负、奖励、BattleResult、正式返回、SaveGame。
- Gameplay Tags 新语义、RPC、Replication、Prediction、新模块/插件、第三方依赖/资产。
- 删除、重命名、批量操作、Git stage/commit/push/reset/clean。

## 固定技术边界

### Definition 与 DTO

- `UHSREncounterDefinition` 为 `UPrimaryDataAsset`，只保存非空 `EncounterId`、非空 `EnemyDefinitionId` 和 `TSoftObjectPtr<UWorld>` Battle Map；安全默认值、最小编辑暴露。
- `EHSREncounterInitiative` 默认 `Neutral`，可声明 `Player/Enemy/Neutral`；本包灰盒路径固定 `Neutral`，不实现三种判定。
- `FHSREncounterRequest` 至少含有效 `FGuid RequestId`、稳定 Encounter/Enemy `FName`、initiative、探索/Battle 地图软路径和返回 `FTransform`，默认构造反射安全。
- 结构化 Operation Result 至少区分 invalid definition/request/map、already pending/traveling、nothing pending/already consumed、travel initiation failure。
- Pending DTO 严禁 Actor、Pawn、Controller、ASC、AttributeSet、Widget、接口/UObject 运行实例、GE/Delegate Handle；DataAsset 指针不能进入 DTO。

### Subsystem 与状态机

- `UHSRBattleTransitionSubsystem` 必须是 `UGameInstanceSubsystem`，是 Pending Payload 的唯一 Runtime 真源。
- 状态至少等价于 `Empty → Pending/Traveling → Consumed → Empty`。
- 唯一请求入口执行校验、构造、原子占位和 travel；第二请求不得覆盖第一请求。
- travel 前可识别失败不得污染 Pending；失败后恢复 `Empty` 并允许重试。
- `ConsumePendingEncounter` 只成功一次，成功后立即移除可再次读取的 Payload；第二次结构化失败。
- Clear 重复调用安全。若 UE5.6 travel failure API 无法在允许范围内确认，不猜 API，记录证据并停止请求决策。
- 日志包含 Request ID、稳定 IDs、状态和失败原因，不以对象地址作业务身份。

### 灰盒适配与 Consumer

- 灰盒只增加 Definition 引用和提交逻辑，继续由现有 Interface 调用；Overlap/Interactor 复核后才请求。
- 成功受理才返回 Interaction Success；失败映射现有 `ExecutionFailed` 并保留结构化诊断。
- 灰盒、Widget、Level Blueprint 不得直接 OpenLevel 或保存 Pending。
- `AHSRBattleTestConsumer` 是无 Tick 测试 Actor：BeginPlay 第一次 Consume 并记录 DTO，再明确验证第二次失败并清理；不生成参与者、不初始化 ASC、不运行战斗。

### 反射、GC、Tick、GAS、网络

- 正确使用反射宏；`*.generated.h` 为最后 include；所有反射字段有安全默认值。
- UObject 引用用 `UPROPERTY/TObjectPtr`，地图用软引用/软路径；不跨 World 保存运行实例。
- 新 Actor `PrimaryActorTick.bCanEverTick=false`；不加 Timer、轮询、世界扫描或 Widget Tick。
- GAS 只回归，不复制/保存 ASC/Attribute/GE/Ability；不实现 RPC、复制、预测或联网承诺。

## 实现步骤

1. 二次确认后检查基线；允许范围存在意外修改即停止，不覆盖。
2. 创建反射安全的 enum、Request、Result 与状态类型。
3. 创建 Encounter PrimaryDataAsset。
4. 创建 Transition Subsystem，实现统一校验、原子请求、travel、单次 Consume、重复拒绝和 Clear。
5. 最小适配现有 Graybox，保持 Phase 3 候选/碰撞/提示链不变。
6. 创建无 Tick Battle Test Consumer，记录第一次成功、第二次预期失败并清理。
7. 关闭 Editor 后执行 fresh `HSREditor Win64 Development`，记录命令、目标、退出码、UHT/C++/Link 和第一处 warning/error。
8. 只更新 `tasks/execution-result.md`，记录实际 diff、构建、未验证项、阻塞/扩权，然后停止交接。

## 用户 UE Editor 操作

1. Build 成功后完整重开 Editor，确认新增 C++ 类型和灰盒属性可加载。
2. 创建 `DA_Encounter_Phase4Test`，填写原创非空 Encounter/Enemy IDs，Battle Map 指向 `Map_BattleTest`。
3. 修改现有 `BP_HSRGrayboxInteractable` 的 Class Default，只绑定该 Definition；Event Graph 不写 travel/Pending/Consume。
4. 创建 `Map_BattleTest`，只放置一个 Battle Test Consumer；不创建 Battle GameMode、角色、UI、敌人或 NavMesh。
5. 保存三个允许资产，重开 Editor 核对 Parent、Definition、软地图与 Consumer 保持。
6. 从探索地图交互：确认只 travel 一次；核对第一次消费成功、第二次失败和清理日志。
7. 依次临时测试 Definition=None、Encounter ID=None、Enemy ID=None、Battle Map=None：均不 travel、不留 Pending，恢复后可重试；不保存故障配置。
8. 回归 Move/Look/Jump、Exploration/UIOnly、提示、无候选、目标销毁和 GAS HUD；回传日志/截图或结构化结果，并独立提交资产。

## 验收与失败矩阵

### Build / 静态

- [ ] fresh Development Editor 退出码 0；UHT、相关 C++、Link 有保存证据，`up to date` 不冒充本轮构建。
- [ ] Pending DTO 静态审查无旧 World/UObject/ASC/Widget/Handle。
- [ ] 无新增 Gameplay/UI Tick、Timer 或世界扫描。

### 成功 / 幂等

- [ ] 重开后所有允许资产及现有 IA/IMC/HUD 引用保持。
- [ ] 一次交互只受理一个 Request、只 travel 一次。
- [ ] 第一次消费取得正确 Request ID、IDs、`Neutral`、地图与 Transform。
- [ ] 第二次消费结构化失败且不返回旧 Payload；清理后为 Empty。
- [ ] 第二个 Pending/Traveling 请求被拒绝，不覆盖首个 Request ID。

### 失败 / 回归

- [ ] Definition/Encounter ID/Enemy ID/Battle Map 各自无效时不 travel、不污染 Pending，恢复后可重试。
- [ ] 无 Pending Consume 和重复 Clear 安全可诊断。
- [ ] travel failure 若本包不能可靠动态制造，明确记为未验证并交 P4-003，不伪造。
- [ ] Move/Look/Jump、UIOnly、提示、无候选、目标销毁与 GAS HUD 正常。
- [ ] 至少两轮 PIE 无旧 Request、重复 Consumer/Delegate、Error/Ensure/GC warning。

## 明确非目标

- P4-002 的 Enemy/AI/Perception/NavMesh/巡逻追击。
- P4-003 的三种 initiative、旅行失败完整专项和返回探索。
- Phase 5 的 Battle Actor/ASC、TurnManager、攻击、胜负、奖励、BattleResult 与正式返回。
- SaveGame、正式 UI/美术/音频/VFX、第三方资产、网络、新模块/Config/Build.cs 或 Phase 1～3 大重构。

## 执行后职责与 Git

- Implementation 只更新 `tasks/execution-result.md`，不得更新本卡或长期状态，不得自行判 `PASS`。
- Reviewer 后续只写 `tasks/final-review.md`；Coordinator 审查后归档和同步状态。
- 各角色只提交自己的真实产物；本卡不授权 Implementation 执行 Git。任务中途不 push，Phase 4 收尾后才按项目规则 push。

## 当前唯一下一步

Implementation Agent 在原 C++ 白名单内完成下方 Coordinator 锁定的最小修订，重新取得 fresh Build 证据并只更新执行报告。修订审计通过前不得进入用户 Editor/PIE，也不得自动进入 P4-002。

## 2026-07-18 Coordinator 首次交付审计

### 提交、白名单与证据事实

- 用户资产 commit `7056f72cb39db574caafd4b7c3e3c147b287086d` 精确包含三个允许资产；Implementation commit `a4fd762bb34e2d7c841d002dbd45ed71c3337f71` 精确包含九个允许 C++ 文件与 `tasks/execution-result.md`，无 Source/Content/Config 白名单越界。
- 实际顺序为资产 commit 在前、Implementation commit 在后；两者 Git author/committer 均为同一身份 `lai_sgjp`。角色/作者归属只能依据提交信息与用户声明，不能由 Git 身份独立区分。
- 执行报告写明“不执行 Git”，但实际已有 Implementation commit；commit message 还写“构建/PIE 证据”，而报告明确 PIE 全部未验证。两项流程/描述偏差保留，不改写历史。
- 根目录 Build logs 保留。首轮日志含 `Total of 11 written` 后失败；`Build-Log-P4-001-fix.txt` 含 `Total of 2 written`、相关编译/Link 与 `Result: Succeeded`；最终 `Build-Log-P4-001.txt` 显示 generated code up to date、实际编译 Subsystem、Link 与 `Result: Succeeded`。因此可确认 UHT 曾在修订链运行且最终 C++/Link 成功，但这些日志未提交，Coordinator 未重跑，退出码 0 来自执行报告。
- 三个资产已存在不等于 Editor 重开、内部绑定或 PIE 已验证；当前没有真实 Editor/PIE 运行证据。

### 阻断性实现偏差

1. `ConsumePendingEncounter()` 成功后只把状态改为 `Consumed`，没有立即移除 `PendingRequest`；公开 `GetPendingRequest()` 仍可读取旧 Payload，Consumer 正是消费后再从 Subsystem 读取它。这违反本卡“消费成功后立即移除可再次读取 Payload”的不变量。
2. `FHSREncounterRequest` 没有探索地图软路径，只保存 Battle Map 与 Return Transform，无法满足本卡要求的完整跨图纯数据 Context。
3. `BattleMap.IsNull()` 只拒绝空引用，不证明非空软路径对应可旅行地图；OpenLevel 返回 void，旅行失败不会自动从 `Traveling` 恢复。后者可继续如实留给 P4-003，但 P4-001 不得声称已验证旅行失败恢复。
4. 报告中“校验失败、单次消费、Clear 已验证”主要是源码静态判断，不是 Automation/PIE 动态证据，必须改标为静态审查或未动态验证。

### 锁定的最小修订

- 只在原允许的 `HSREncounterTypes.h`、Transition Subsystem、Battle Test Consumer 与执行报告内修订；不得扩大到 Source/Content/Config/Build.cs。
- Request 增加探索地图软路径/稳定路径值，并在请求构建时从当前 World 记录；保持 DTO 无 UObject/Actor 引用。
- Consume API 必须以输出参数或结构化消费结果把 DTO 值交给 Consumer，并在成功时立即清空 Subsystem Payload；删除或收窄可在 Consumed 后读取旧 Payload 的公开入口。第二次 Consume 仍需结构化失败，但不能返回旧 DTO。
- Consumer 使用第一次 Consume 返回的 DTO 日志，不得消费后回读 Subsystem Pending；再验证第二次失败并 Clear。
- 保留 OpenLevel void/真实 travel failure 未动态覆盖边界，不猜测 UE API。
- 修订后 fresh Build 必须保存 UHT/C++/Link/退出码和首个错误；执行报告同步实际 Git 偏差、静态/动态证据等级。Coordinator 再审通过后才交用户 Editor/PIE。

**Coordinator Gate：`IMPLEMENTATION REVISION REQUIRED`。**
