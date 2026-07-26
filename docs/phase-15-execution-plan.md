# Phase 15 详细执行计划：地图、传送与关卡流程

> 基线：2026-07-26；Phase 14=`Ready with inherited follow-ups`。
> Gate 0：`PASS`。Coordinator、Independent Reviewer、Teacher、Implementation 四角色已完成只读规划与复核；本计划冻结后才允许开始 P15-001。

## 1. 当前事实与证据等级

- `AGENT/BUILD VERIFIED`：Phase 14 的 `HSREditor Win64 Development`、`HSR.QuestDialogue` 与 `HSR.Save` 8/8 已通过。
- `USER PROVIDED`：Dialogue NPC 候选注册、`Talk`、F 键交互和 `StartNode=Start` 已在 PIE 观察。
- Phase 14 最终状态为 `PASS WITH FOLLOW-UP / Ready with inherited follow-ups`；分支选择后的可视奖励结果继续继承，但不阻塞 Phase 15。
- 当前已有 `UHSRBattleTransitionSubsystem`、纯值 Encounter/Return DTO、旅行失败入口、BattleResult/Return 单次消费和探索位置恢复基础。
- 当前没有 Map/Teleport Definition、MapSubsystem、地图状态 Save DTO 或直接覆盖 Battle travel/return/failure 的 Automation。
- 工作树中的 `.claude/settings.local.json`、`learn/AI.md`、`learn/CppEngineDepth.md` 和 `Content/UI/WBP_Reward_Summary_P13.uasset` 属于用户现有改动，Phase 15 排除且不得认领。

## 2. 阶段唯一最终结果

玩家可从探索地图 A 的已解锁传送点进入探索地图 B，在 B 进入独立 Battle Map，并在胜利、失败或中断的明确策略下只返回一次；返回后恢复稳定 MapId 和合法位置。当前地图、位置、最小区域/传送解锁与探索状态可通过 Save v5 保存和恢复。无效 ID、缺失地图/落点、重复请求、加载失败、坏存档和重复消费均不得污染已提交地图状态。

## 3. 所有权与数据流

- Definition：Map/Teleport DataAsset 只保存稳定 ID、软地图引用、目标落点 ID、显示数据和静态约束，不保存 Runtime 状态。
- Map authority：`UHSRMapSubsystem`（GameInstanceSubsystem）唯一拥有普通地图导航、解锁集合、当前位置和普通旅行 Pending Transaction。
- Battle authority：现有 `UHSRBattleTransitionSubsystem` 继续唯一拥有 Encounter、Battle Return、resolved rollback 和 BattleResult 生命周期；MapSubsystem 只提供稳定地图/落点桥接，不建立第二套 Battle 状态机。
- Arrival：目的 World 加载且 Pawn 可用后，Consumer 解析稳定 ArrivalId/Transform，成功放置后才提交消费；失败必须可诊断、可清理或按冻结策略重试。
- Save：只捕获/校验/提交 Map DTO；不得直接 `OpenLevel`，不得保存 Actor、World、Subsystem、ASC、GE Handle、Widget 或 UObject 指针。
- UI：仅允许可选 Debug/只读地图入口；正式 Screen Stack 和地图 UI 属于 Phase 17。

普通旅行时序：`Preflight -> 创建唯一 RequestId/Pending -> OpenLevel -> 目的图确认 MapId -> Pawn/Arrival 就绪 -> 放置成功 -> Commit/Consume -> 单次通知`。Preflight 失败不得写 Pending；加载失败、错误地图或中断必须清理或回滚到可重试状态。

## 4. 串行工作包

### P15-001：Map 领域契约、Definition 与纯值 Authority

唯一验收结果：注册两个 Map Definition 与 Teleport Definition；稳定 ID、所属地图、解锁和请求预检可被纯值 Automation 验证；空/重复/冲突/未知/锁定输入零副作用。不发起旅行，不接 Battle，不改 Save。

Codex allowlist：

- `Source/HSR/Map/HSRMapTypes.h`
- `Source/HSR/Map/HSRMapSubsystem.h/.cpp`
- `Source/HSR/Data/Definitions/HSRMapDefinition.h`
- `Source/HSR/Data/Definitions/HSRTeleportDefinition.h`
- `Source/HSR/Tests/HSRMapSubsystemTests.cpp`
- `tasks/active-task.md`、`tasks/execution-result.md`、`tasks/final-review.md`

用户 Editor Gate：C++/Automation 后创建两个原创灰盒探索 Map、两个 Map DataAsset 和至少一项 Teleport DataAsset；保存并重开核对稳定 ID、软地图引用和 ArrivalId。资产由用户创建和提交。

### P15-002（原计划）：双探索图旅行与 Battle Return 适配

唯一验收结果：A -> B -> A 普通传送及 B -> Battle -> B 返回均使用稳定 ID；成功放置后单次消费；重复请求/消费、无效目标和旅行失败不污染状态，BattleResult/Return 保持 exactly-once。

预期 allowlist：P15-001 Map 文件与测试；`HSRBattleTypes.h`、`HSREncounterTypes.h`、`HSRBattleTransitionSubsystem.h/.cpp`、`HSRExplorationReturnConsumer.h/.cpp`；若新增 `HSRMapArrivalConsumer.h/.cpp` 必须在任务卡单独列明。禁止修改 Coordinator、TurnManager、EncounterDefinition、Config 和 Content。

用户 Editor Gate：在 A/B 放唯一 ArrivalId 与传送交互物；配置反向传送；运行 A -> B -> A、B -> Battle -> B，并提供 RequestId、Pending/Consume 和最终 Transform 日志。至少验证一次无效 ID 或缺落点后仍可重试。

### P15-003（原计划）：Save v5 地图状态投影

唯一验收结果：当前 MapId、稳定位置、区域/传送解锁和最小探索 Flag 可 candidate-first 保存/恢复；v1-v4 保守迁移为空 Map 状态；坏 Map/Teleport/Transform/重复 ID 拒绝且旧 Runtime 零污染；重复 Load 无重复通知。

预期 allowlist：Map types/subsystem、`HSRSaveTypes.h`、`HSRSaveSubsystem.h/.cpp`、Map/Save tests，以及仅为 schema 断言所需的既有 Save tests。Phase 16 的备份、损坏文件恢复和通用迁移框架不在本包。

用户 Editor Gate：在 B 非默认位置解锁一个传送点，Save，关闭并重开 Editor，Load 后恢复 Map/位置/解锁；重复 Load 不重复改变状态。

### P15-004（原计划）：阶段验证、教学、审查与归档

唯一验收结果：P15-001～003 的 Build、Automation、Editor 资产、PIE、失败路径、Save 冷恢复、Teacher、Independent Review、provenance 和三件套均可独立复核；不新增 Gameplay。

验证顺序：`git diff --check` -> fresh Development Editor Build -> `HSR.Map`/`HSR.Save` Automation -> 资产保存重开 -> 两轮 A/B/Battle 往返 -> 无效旅行失败路径 -> Save/Editor 重开 -> Teacher -> Independent Review。

## 5. 失败矩阵与事务政策

- Definition：空/重复/冲突 ID、空软引用、Teleport 不属于源 Map、未知/锁定目标。P15-001 静态注册只验证软引用非空；package 是否真实存在延迟到 P15-002 的旅行 preflight，不把静态注册误当作可加载证据。
- Request：同图非法请求、已有 Pending、RequestId 不匹配、重复请求、World 无效。
- Travel：`OpenLevel` 仅表示已发起；覆盖加载失败、错误目标图和中断。TravelFailure 不依赖脆弱的 World 字符串后缀猜测。
- Arrival：缺失/重复/阻塞/越界落点、Pawn 未生成、非法 Transform；不得在真实放置成功前不可逆清空 Context。
- Battle：胜利、失败、中断、重复 Result/Return；resolved rollback 必须与请求事务对应。
- Save：旧 schema、未知 Definition、重复解锁、非法 Transform/Flag、candidate failure、重复 Load；失败不广播、不旅行、不部分提交。

## 6. UE5.6 风险

- `UGameplayStatics::OpenLevel` 无返回值；成功只能由目的 World 生命周期/Consumer 与 PIE 证据确认。
- `TSoftObjectPtr<UWorld>` 只用于软引用和 package 预检，不同步加载来伪造 travel 成功。
- Region 是旅行授权门禁：即使 Teleport 已解锁，目标 Map 所属 Region 未解锁时仍拒绝请求；Region ledger 不是仅供 UI 显示的孤立状态。
- P15-001 的 ArrivalId 是非空稳定标识但不解析场景 Actor；P15-002 由目的图 Arrival registry 校验其存在性、唯一性和可放置性。
- MapTypes 与 BattleTypes 按值 USTRUCT 互含会导致 UHT include cycle；稳定 Map DTO 独立定义，Battle 单向依赖。
- GI Subsystem 跨 World 存活不等于 Actor/ASC 存活；目的图必须重建 Actor Info/ASC。
- 历史风险（已关闭）：旧 Return Consumer 曾在放置前消费；最终由实际 P15-003 改为 placement-before-consume、有限 retry/rollback 与旅行互斥。
- Definition registry 的强/软持有和 GC 策略必须明确并测试。

## 7. 教学 Gate

用户在收尾前应能解释：Definition 与 Runtime State；稳定 ID；Actor 与 GI Subsystem 生命周期；Preflight/Pending/Commit/Consume；Map 与 Battle 权威边界；BattleResult/ReturnContext/持久 Map State 的不同生命周期；Save DTO/candidate-first；Automation、PIE 与 Editor 重开各自能证明什么。

## 8. 风险、停止条件与非目标

删除/移动资产、Config、新模块、第三方依赖、跨系统大重构、扩大到 Quest/Reward/Battle 规则或需要 Codex 创建 Content 时停止并请求授权。

非目标：World Partition/Level Streaming、正式地图 UI、复杂任务锁、动态 Spawn Registry、持久敌人通用系统、多人 travel、Phase 16 备份恢复、Phase 17 Screen Stack、正式美术。Phase 14 可视奖励 follow-up 只继承。

## 9. 完成定义与当前唯一下一步

P15-001～004 全部归档；两探索地图和一 Battle Map 稳定往返；无效旅行不破坏状态；Return/Result 单次消费；Save v5 可冷恢复；Build/Automation/PIE/Editor/教学/审查证据齐全。满足前不得进入 Phase 16。

最终执行拓扑（实施中经独立失败边界拆分）：P15-001 Map/Teleport 契约；P15-002 A↔B 普通旅行；P15-003 Battle Return；P15-004 Save v5；P15-005 阶段收尾。五个工作包均最终 `PASS` 并归档。

最终状态：Phase 15=`Ready with inherited follow-ups`。未外推 Standalone、packaged、真实注入式 travel failure、包损坏/强杀进程恢复或多人旅行；这些不阻塞进入 Phase 16 Gate 0。
