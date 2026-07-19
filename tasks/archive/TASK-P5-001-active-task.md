# TASK-P5-001：战斗运行时骨架与 Encounter Context 重建

## Role Lock / 角色锁定

执行者只能扮演 `Implementation Agent / 低级执行模型`。不得自行切换为协调者、架构师、审查员、教师或项目经理。执行者首次读取本卡后，只能复述任务编号、目标、允许文件、验证方式和风险，并以“等待用户确认执行 TASK-P5-001。”结束；用户明确确认前不得调用工具、修改文件、构建或启动 Editor。

## 任务背景

PHASE4 已以 `Ready with inherited follow-ups` 收尾，收尾 commits 为 `cdb1f00`、`4404e25`，`HEAD == origin/main`。PHASE5 计划已归档，但战斗核心尚未实施。本任务是 PHASE5 的第一个工程垂直切片：从已有纯值 Encounter Context 建立独立 Battle runtime 骨架，进入 Battle Map 后重建 1 名玩家与 1 名敌人及其 ASC，并保存可供后续返回使用的纯值 Return Context。

## 本轮唯一可验收结果

给定有效 Encounter Context，Battle runtime 能在独立 Battle World 中 exactly-once 消费请求，按稳定定义重建玩家/敌人战斗参与者和 ASC，完成有效 ActorInfo 初始化，并在缺失定义、无效 Map 或重复请求时结构化失败；不实现回合、攻击、胜负、BattleResult 或 UI。

## 允许修改的文件

只能修改以下路径；若发现必须扩展范围，立即停止并请求协调者更新任务卡：

- `Source/HSR/Battle/HSRBattleParticipant.h`
- `Source/HSR/Battle/HSRBattleParticipant.cpp`
- `Source/HSR/Battle/HSRBattleCoordinator.h`
- `Source/HSR/Battle/HSRBattleCoordinator.cpp`
- `Source/HSR/Battle/HSRBattleGameMode.h`
- `Source/HSR/Battle/HSRBattleGameMode.cpp`
- `Source/HSR/Battle/HSRBattleTypes.h`
- `Source/HSR/Battle/HSRBattleTypes.cpp`（仅在确有非内联实现时）
- `Content/Maps/Map_Battle.umap`（用户在 UE Editor 中创建的 Battle Map）
- `Content/Blueprints/Framework/BP_HSRBattleGameMode.uasset`（用户在 UE Editor 中创建/绑定的 GameMode 资产）
- `Content/Data/Encounters/DA_Encounter_Phase4Test.uasset`（用户授权将 Battle Map 字段切换为 `Map_Battle`；必须保留可回归的 PHASE4 测试入口）
- `tasks/execution-result.md`

如现有 Transition API 的必要兼容修改超出上述清单，必须停止并报告，不得自行修改 Transition、Content、Config 或 Build.cs。

## 明确禁止修改

禁止修改 `Source/HSR/Player/HSRPlayerController.cpp`（已知 stat/index 噪声）、任何现有 Transition/Encounter 文件、`HSR.Build.cs`、`.uproject`、Config、Content、Blueprint、UI、SaveGame、网络/RPC/Replication、Ability、GameplayEffect、TurnManager、BattleResult、奖励和 AI。

## 数据与生命周期边界

- Encounter 输入、Participant 定义和 Return Context 必须是纯值/稳定 ID；不得跨 World 保存 Actor、ASC、Widget、GE Handle 或裸 UObject 指针。
- Battle runtime 状态由 Coordinator/Participant 持有；DataAsset 仅提供静态定义；本任务不写 SaveGame。
- Actor/ASC 的 Outer、GC、销毁和 World teardown 必须可解释；跨图只保留 Map、Transform、稳定 ID、RequestId 等 DTO 字段。
- 不添加 Tick、Timer 驱动的核心规则、每帧扫描或 Level Blueprint 规则源。

## 实现要求

1. 明确 Coordinator 的状态机至少覆盖 `Idle → Consuming → Spawned/Failed`，并拒绝重复 RequestId。
2. Participant 只表达稳定身份、定义 ID、队伍/阵营和运行时弱引用；不成为跨图持久化容器。
3. GameMode 负责 Battle World 的入口接线，不吞掉 Coordinator 的所有权。
4. 通过已有项目 GAS 约定创建/获取 ASC 并完成 `InitAbilityActorInfo`；不授予正式攻击能力。
5. 所有失败必须有 RequestId/EncounterId/Map 上下文日志，且不得留下不可清理的 Pending 状态。

## 验证与证据

- 静态检查：反射宏、generated include 顺序、UPROPERTY/弱引用、无 Tick、无跨 World UObject 持久化。
- 构建：`HSREditor Win64 Development` fresh UBT/UHT/C++/Link；记录退出码、首个 warning/error 和完整日志路径。
- 最小运行证据：有效 Context 进入 Battle Map 并生成双方 Actor/ASC；重复请求只消费一次；缺失定义/地图失败可结构化返回。
- Editor/PIE：由用户按说明完成必要资产/地图绑定、Editor 重开和至少一轮主路径与一轮失败路径；执行者不得把计划当作 PIE 证据。
- `tasks/execution-result.md` 必须如实区分已验证、用户提供、报告级和未验证内容，不得自判 Reviewer PASS。

## 角色交接

Implementation 完成后先提交执行报告并停止；Coordinator 检查 allowlist 和证据，Teacher 处理所有权/ASC/生命周期学习项，Independent Reviewer 只读审查后决定 PASS、PASS WITH FOLLOW-UP 或 REVISE。未通过 Reviewer 前不得创建 P5-002。

## 本轮非目标

TurnManager、Speed 排序、GameplayAbility、固定伤害、死亡、Victory/Defeat、BattleResult、返回探索地图、战斗 UI、Cost/Cooldown/Energy、复杂 AI、SaveGame、网络和性能优化全部留给后续工作包。

## 用户确认与当前状态

用户已明确确认：上述 Battle Map、Battle GameMode 和 Encounter DataAsset 变更由用户根据执行者建议创建，并确认执行 `TASK-P5-001`。Independent Reviewer 当前结论为 `PASS WITH FOLLOW-UP`。

授权不追溯改写此前执行报告的时间线；已有 Build/PIE 内容仍须由 Coordinator 和 Independent Reviewer 按证据等级重新核对。`Source/HSR/Player/HSRPlayerController.cpp` 的 stat/index 噪声继续保持不动、不纳入任务。

### 修订 Gate

源码修订项已闭合。当前只允许补充 `tasks/final-review.md` 列出的两个 follow-up：修复后的 fresh Build/UHT/C++/Link，以及一条 invalid-but-nonempty DefinitionId 的运行时失败证据。完成前不得进入 P5-002。
