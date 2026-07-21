# TASK-P9-001：两回合 Attack Buff 生命周期纵切

状态：`COMPLETE — PASS WITH FOLLOW-UP`

## Role Lock / 角色锁定

执行者当前且仅能担任 `Implementation Agent / 低级执行模型`。不得切换为 Coordinator、Architect、Reviewer、Teacher 或资产作者；不得扩大范围、改变冻结规则、创建 Editor 资产、推进 P9-002，或宣称 Phase 9 已完成。遇到 API、所有权、资产或白名单缺口必须停止并请求扩权。

## 前置门禁

- P9-000 已由 Independent Reviewer 判定 `PASS WITH FOLLOW-UP` 并归档；修订后 Build 成功，USER PROVIDED 两轮 PIE 为 `24 PASS / 2 COMPLETE / 0 FAIL`。
- 用户已明确授权正式开始 P9-001 Coordinator 工作。该授权只允许创建本活动契约，不替代执行者首次复述后的单独实施确认。
- P9-000 已提供纯值 `OnTurnStarted` / `OnTurnEnded`；本任务消费该契约，不修改 TurnManager。

## 唯一可见结果

在正常 1v1 Battle 中，为一个存活目标施加原创 `AttackUp` Buff：Attack 立即增加 `10.0`，持续 `2` 次该目标自己的 `TurnEnded`；其他参与者的回合不递减；第二次目标 `TurnEnded` 后按原 `FActiveGameplayEffectHandle` 精确移除，Attack 恢复。重复施加只刷新到 `2` 回合，不增加第二个 Runtime Instance、不 Apply 第二份 GE，也不叠加 Attack。

本任务只实现 `MaxStacks=1 + RefreshDuration` 的单 Buff 纵切，不实现通用叠层、DoT、Break Debuff、免疫、驱散、UI 或 Save/网络。

## 冻结所有权

- `UHSRStatusDefinition`：只读静态 `UPrimaryDataAsset`。保存稳定 `StatusId`、DurationTurns、MaxStacks、TriggerTiming、RefreshPolicy、Buff GameplayTag 与 Infinite GE 类软引用；不保存 Actor/ASC、RemainingTurns、实例或 Handle。
- `UHSRStatusComponent`：目标参与者的 battle-local Runtime 真源。保存至多一个本任务 `FHSRStatusInstance`，其中包含 StatusId、Source/Target ParticipantId、BattleEpoch、Stacks、RemainingTurns、LastConsumedTurnSequence 与有效 `FActiveGameplayEffectHandle`。
- Infinite GE：只提供 Attack `+10.0` 与 `Status.Buff.AttackUp` Tag。Duration Policy 必须为 Infinite；不得用秒数 Duration/Period 表示回合。
- `UHSRTurnManager`：仅发布 P9-000 纯值事件；不保存状态、不递减回合、不持有/移除 GE Handle。本任务禁止修改 TurnManager。
- `UHSRBattleCoordinator`：为本轮参与者创建/持有 StatusComponent，注入 ParticipantId/ASC，绑定当前 TurnManager，路由测试 Add 请求，并在 defeat、Finished、Reset、重建和 teardown 时调用清理/解绑。Coordinator 不保存 StatusInstance 或 GE Handle。
- `AHSRBattleGameMode`：只提供默认关闭的 `Development|P9` opt-in P9-001 harness 与用户资产引用；不成为 Runtime 状态真源。
- UI、Ability、GameplayCue、DataAsset、TurnManager 均不得直接修改 RemainingTurns 或移除 Handle。

## 冻结数据与事务

### Definition 字段

- `StatusId = Status.Buff.AttackUp`
- `DurationTurns = 2`
- `MaxStacks = 1`
- `TriggerTiming = TargetTurnEnded`
- `RefreshPolicy = RefreshDuration`
- `GrantedStatusTag = Status.Buff.AttackUp`
- `InfiniteGameplayEffectClass`：软类引用，指向用户创建的 `GE_Status_AttackUp_P9`

未实现的 Timing/Policy 值必须结构化拒绝，不能静默降级。

### Add 原子性

1. 先校验 Definition、StatusId、Duration、MaxStacks、Timing/Policy、Source/Target、BattleEpoch、ASC、GE 类已加载且目标存活。
2. 新增时先构造并 Apply Infinite GE；只有返回有效 Handle 后才提交 Runtime Instance。
3. Apply 失败必须保持 `InstanceCount=0`、Attack/Tag/RemainingTurns 不变，无空实例、无失效 Handle。

### Refresh 原子性

- 同 Target + 同 StatusId 已存在时，只将 RemainingTurns 原子刷新为 `2`，更新必要 SourceId；复用原 Handle，不再次 Apply GE，不改变 Attack/Tag/Stacks。
- Refresh 前若实例/Handle/ASC 已失效，结构化拒绝并保持旧可观察状态；不得先移除旧 GE 或创建第二份 GE。

### Expire 与 Clear 原子性

- 仅 TargetParticipantId 匹配、BattleEpoch 匹配、EventType 为 TurnEnded 且 TurnSequence 大于 LastConsumed 时消费一次；非目标、重复、旧 Epoch、TurnStarted 均零副作用。
- 每个合法目标 TurnEnded 先将 RemainingTurns 减一；归零时按保存 Handle 精确 Remove，然后移除 Runtime Instance。重复 Expire/Clear 幂等。
- Remove 返回异常或 ASC 已失效时仍必须清空本组件的 Runtime Instance/Handle，记录结构化失败；不得移除任何其他 GE。
- Target death、Battle Finished、Coordinator Reset/rebuild、Component EndPlay 与 TurnManager 替换均执行 Clear + delegate unbind；迟到事件零副作用。

## P9-000 follow-up 继承

1. 根执行历史旧 PIE pending：已由 P9-000 归档校准为 USER PROVIDED。本任务不再修改历史，只按新证据独立报告。
2. BattleEpoch manager-local：本任务必须通过 TurnManager 替换/Reset 的成对解绑、重新绑定和旧事件拒绝动态关闭该消费风险；不得把 Epoch 当全局 ID。
3. 死亡目标 Break Delay 诊断为 `InvalidTarget`：本任务不修改 TurnManager/Delay，保留为非阻断 follow-up，不宣称关闭。

## 执行前必须读取

- `tasks/active-task.md`
- `Source/HSR/Battle/HSRBattleCoordinator.h`
- `Source/HSR/Battle/HSRBattleCoordinator.cpp`
- `Source/HSR/Battle/HSRBattleGameMode.h`
- `Source/HSR/Battle/HSRBattleGameMode.cpp`
- `Source/HSR/Battle/HSRTurnManager.h`（只读）
- `Source/HSR/Battle/HSRBattleParticipant.h`（只读）
- `Source/HSR/GAS/Attribute/HSRCoreAttributeSet.h`（只读）
- `Source/HSR/Data/Definitions/HSREnemyDefinition.h`（只读样式参考）

## Implementation 允许修改/新增路径

- `Source/HSR/Status/HSRStatusTypes.h`（新增）
- `Source/HSR/Status/HSRStatusComponent.h`（新增）
- `Source/HSR/Status/HSRStatusComponent.cpp`（新增）
- `Source/HSR/Data/Definitions/HSRStatusDefinition.h`（新增）
- `Source/HSR/Data/Definitions/HSRStatusDefinition.cpp`（新增）
- `Source/HSR/Battle/HSRBattleCoordinator.h`
- `Source/HSR/Battle/HSRBattleCoordinator.cpp`
- `Source/HSR/Battle/HSRBattleGameMode.h`
- `Source/HSR/Battle/HSRBattleGameMode.cpp`
- `tasks/execution-result.md`

未列出的文件默认禁止修改。不得修改 TurnManager；如真实编译证明必须修改模块依赖、CharacterBase 或其他路径，立即停止请求扩权。

## User Editor/Config 资产作者范围

用户负责以下操作；Implementation 不得代创建、代保存或冒充验证：

1. 在 Project Settings > Gameplay Tags 新增唯一 Tag `Status.Buff.AttackUp`，保存到 `Config/DefaultGameplayTags.ini`。
2. 创建 `Content/Data/Status/DA_Status_AttackUp_P9.uasset`，父类为 `UHSRStatusDefinition`，字段严格使用本卡 Definition 值。
3. 创建 `Content/GameplayEffects/Status/GE_Status_AttackUp_P9.uasset`，父类为 `UGameplayEffect`：Duration Policy=`Infinite`；Modifier 仅 `UHSRCoreAttributeSet.Attack`、Operation=`Add`、Magnitude=`10.0`；Granted/Target Tag 仅 `Status.Buff.AttackUp`；无 Period、无其他 Modifier、无 Cost/Cue/Execution。
4. 在 `BP_HSRBattleGameMode` 的 `Development|P9` 区域绑定 Definition 并只开启 P9-001 harness；保存、重开 Editor，确认 Tag、Definition、GE 类和字段引用保持。

用户需回传资产路径/字段截图或文字确认、Editor 重开结果和两轮 PIE 原始日志。Config/Content 归 User 作者，必须与 Implementation 代码分开归属。

## 明确禁止范围

- 禁止 P9-002 叠层/Replace、多来源策略，P9-003 DoT/Break Debuff，P9-004 免疫/驱散，P9-005 UI。
- 禁止修改 TurnManager、CharacterBase、AttributeSet、Build.cs、`.uproject`、Save、网络、伤害/Break/Delay、技能/资源/胜负逻辑。
- 禁止 GAS Duration/Period 秒数驱动回合、直接写 Attack、直接 AddLooseGameplayTag、按类/Tag 批量移除 GE、第二份状态容器。
- 禁止 Actor/Component/Widget Tick、Timer、world scan、CDO/静态跨 PIE 可变状态。
- 禁止未授权 Config/Content、第三方资产、无关重构、删除/覆盖历史、批量格式化、stage/commit/push、`git reset` 或 `git clean`。

## Development 验证矩阵

Harness 必须默认关闭、隔离使用临时战斗状态，并为每项输出结构化 Case/Result、Epoch、Participant、Sequence、Remaining、InstanceCount、HandleValid、Attack、Tag、ApplyCount/RemoveCount；不能只输出总 PASS。

| Case | 必须结果 |
|---|---|
| AddSuccess | 1 Instance、1 有效 Handle、Remaining=2、Stacks=1、Attack 基线+10、Tag 有且 ApplyCount=1 |
| OtherParticipantTurnEnded | Remaining/Attack/Tag/Handle/计数完全不变 |
| FirstTargetTurnEnded | Remaining 2→1，GE 与 Attack/Tag 保持 |
| RefreshAtOneTurn | Remaining 1→2，InstanceCount=1、同 Handle、ApplyCount 仍为 1、Attack 不再增加 |
| TwoTargetTurnEndedAfterRefresh | 第一次 2→1；第二次到 0 后精确 Remove，InstanceCount=0、Attack 恢复、Tag 消失、RemoveCount=1 |
| DuplicateTurnEvent | 同 Epoch+Sequence 第二次消费零副作用 |
| InvalidDefinition/StatusId/Duration/GE | 结构化拒绝；零 Instance/GE/Attack/Tag 副作用 |
| MissingASC/InvalidTarget/DefeatedTarget | 结构化拒绝；零副作用 |
| ForcedApplyFailure | 零 Instance，Attack/Tag/Remaining 不变，无 Handle 泄漏 |
| TargetDeathClear | 只移除本 Handle，Instance=0；重复 Clear 幂等 |
| FinishedClear | 无迟到递减；Instance/Handle 清空，其他非状态 GE 不受影响 |
| ResetAndSecondBattle | 第一轮解绑/清理；第二轮新 Manager/Epoch 可 Add/Expire，旧事件零副作用 |
| EndPlayClear | delegate 成对解绑，Handle/Instance 清空，之后无回调 |
| ManagerReplacement | 旧 Manager 解绑，新 Manager 绑定一次；旧事件不消费，新事件正常消费 |

强制 ApplyFailure 只能通过 `WITH_EDITOR`/Development 注入 seam 或安全的无效 GE 配置实现，不得污染 Shipping API 或直接伪造成功。

## Build、静态与 PIE Gate

- Fresh `HSREditor Win64 Development -NoHotReload -WaitMutex` 必须真实运行 UHT、HSR C++、`UnrealEditor-HSR.lib/.dll` Link 与 metadata；保留首个真实错误和 warning。
- 静态审查：UHT/GC、Definition 静态性、Handle 唯一所有权、Apply/Refresh/Remove 原子性、delegate 成对解绑、无 Tick/Timer、无批量 GE 移除、allowlist/diff。
- 用户完成资产和 Editor 重开后运行两轮 PIE。两轮都必须矩阵全 PASS、总 COMPLETE、零 FAIL；第二轮必须证明无重复绑定、旧 Epoch/Manager 不消费、Attack/Tag/Handle 恢复基线。
- Build、Agent 静态检查与 USER PROVIDED Editor/PIE 分级记录；缺任一证据不得标记任务完成。

## 执行者首次响应门禁

执行者首次读取后只能只读复述：任务编号、唯一结果、所有权、Add/Refresh/Expire/Clear 原子性、精确代码白名单、用户资产步骤、失败矩阵、Build/PIE Gate、P9-000 follow-up 和停止条件，然后停止。最后必须原样写：

`等待用户确认执行 TASK-P9-001。`

用户随后单独明确确认前不得调用实现或写入工具；确认只适用于 P9-001 Implementation，不授权 P9-002、Editor 资产代办或 Git 写操作。

## Git 交付

- 当前不授权 stage、commit 或 push。
- 后续如单独授权角色提交，必须先核对 status、diff、allowlist、用户资产归属与 ignored 派生产物；Phase 中途不得 push。
