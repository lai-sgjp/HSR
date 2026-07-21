# Phase 9 详细执行计划：Buff、Debuff 与回合状态生命周期

状态：`P9-000/P9-006 COMPLETE / READY WITH INHERITED FOLLOW-UPS`
日期：2026-07-21
适用版本：UE 5.6 / HSR Runtime / GAS

## 1. 阶段目标与可见结果

Phase 9 建立独立于现实秒数的回合状态生命周期，并交付一个可独立复现的 1v1 战斗闭环：

> 角色能够获得一个属性 Buff、承受一个回合触发 DoT，并受到一个由 Phase 8 Break 结果接入的 Debuff。状态由静态 Definition 描述，由目标角色的 Runtime Status 保存来源、目标、层数、剩余回合和 Active GameplayEffect Handle；TurnManager 只发布明确的回合边界，状态只在规定边界触发或递减，到期、驱散、来源失效、目标死亡、战斗结束和 Reset 时均按 Handle 精确清理；最小 UI/Debug 只读显示状态，不拥有或推进规则。

阶段核心 Gate 问题：**一个状态能否按逻辑回合数而不是 GAS 秒数准确到期，并且不会泄漏或错误移除其他来源的 GameplayEffect？**

计划、代码存在、资产存在和单次编译均不等于阶段完成。每项结论必须标注 `AGENT VERIFIED`、`USER PROVIDED`、`STATIC REVIEW` 或 `NOT VERIFIED`。

## 2. 当前状态与 Gate 0

仓库只读证据显示：

- Phase 8 P8-006 最终结论为 `PASS WITH FOLLOW-UP`，本地角色提交与阶段收尾已经完成。
- Git 当前 `main` 与 `origin/main` 同步于 `2c2abc2`；校准前工作树除本计划这一已有未跟踪文件外无其他变更，旧文档中的 `push pending` 描述需要同步。
- 当前 `UHSRTurnManager` 只有 `OnActionResolved(FName)`；`ResolveAction` 在推进到下一参与者后才广播已完成行动者，尚无语义明确的 `TurnStarted` / `TurnEnded` 公共契约。
- Phase 8 的 Break Debuff 只是最小结果，不等于已经存在通用 Status Runtime。

正式实施前必须完成 Gate 0：

1. 以 Git 证据校准 `PROJECT_STATE.md`、`README.md`、`todo_plan.md`、`worklog.md`、`tasks/active-task.md` 和 Phase 8 文档中的远端状态；不得改写历史 Reviewer 结论或 inherited follow-ups。
2. 创建唯一 Phase 9 活动任务；没有明确授权时只允许规划和审查，不写 Gameplay 代码或 Editor 资产。
3. 冻结回合边界语义：何时广播 TurnStarted/TurnEnded、广播与队列推进的顺序、死亡/跳过/Delay/Finished/Reset 行为，以及迟到事件如何拒绝。
4. 冻结首个状态规则：两回合、单层 Attack Buff、目标自身 TurnEnded 递减、重复施加策略和失败原子性。
5. 将 Phase 8 未动态闭合项加入回归矩阵，但不得冒充已验证：Reset 后再次 Break、Finished/死亡/Reset 后旧 Action 拒绝、3+ 参与者同速排序、正值过量削韧归零和动态 UI 路径。

Gate 0 未满足时，Phase 9 状态实现为 `BLOCKED`；只允许修正文档状态、设计契约和准备任务卡。

## 3. 架构不变量

### 3.1 所有权与数据边界

- `UHSRTurnManager` 是行动顺序与回合边界的唯一所有者，只发布 battle-local 纯值事件；它不保存 StatusInstance，不持有或移除 GE Handle。
- 目标战斗 Actor 上的 `UHSRStatusComponent` 是活动状态的唯一 Runtime 真源，拥有 `FHSRStatusInstance` 与其 `FActiveGameplayEffectHandle`。
- `UHSRStatusDefinition` 是只读静态 DataAsset，只保存规则配置和 GE 软引用；不得保存层数、剩余回合、Actor/ASC 指针或 GE Handle。
- Infinite GE 只承载属性 Modifier 和 Gameplay Tags；逻辑回合、叠层、刷新、免疫、驱散和触发时机由 Status Runtime 决定。
- BattleCoordinator 只负责参与者接线、事件路由和 battle teardown；Ability、GameplayCue、Widget 和 Definition 均不得直接推进状态回合。
- UI/ViewState 只消费状态事件和快照，不自行递减、不移除状态、不依赖 Widget Tick。
- 普通战斗状态不进入 SaveGame。未来明确允许跨战斗的状态也只能保存稳定 Definition ID 和可重建数据，不能保存 UObject、ASC 或 Active GE Handle。

### 3.2 生命周期与失败原子性

- 状态使用稳定 StatusId、SourceParticipantId、TargetParticipantId 和 BattleId；不得以显示文本或 Actor 指针充当身份。
- Apply GE 成功并返回有效 Handle 后才能提交 Runtime Instance；失败时不得留下 Tag、属性变化、空实例或半完成层数。
- Remove、Dispel、Expire、Defeat、Reset 和 teardown 必须幂等；重复调用不得移除其他来源的 GE。
- 回合递减必须明确属于 Source、Target 或全局边界；首包只支持目标自身 `TurnEnded`。
- 同一回合事件至多消费一次；Finished、旧 BattleId、无效参与者和 Reset 后迟到事件均为零副作用。
- 不使用 GAS Duration/Period 秒数模拟回合，不新增 Actor/Component/Widget Tick 扫描。

### 3.3 UE 与未来边界

- UObject/ActorComponent 引用使用 UE 反射或弱引用边界，委托必须成对绑定/解绑；EndPlay 和 battle teardown 都能清理。
- 当前只实现单机权威语义；事件和命令保持纯值、可校验，为未来网络化保留边界，但不承诺复制或预测。
- 使用原创名称、数值、图标和表现；不复制商业游戏的专有名称、UI、公式或素材。

## 4. 目标类型与建议路径

最终路径以实施前仓库审查为准；若实际目录或现有类型提供等价边界，应优先复用，不得为匹配计划强行迁移。

建议新增：

- `Source/HSR/Status/HSRStatusTypes.h`
- `Source/HSR/Status/HSRStatusComponent.h`
- `Source/HSR/Status/HSRStatusComponent.cpp`
- `Source/HSR/Data/Definitions/HSRStatusDefinition.h`
- `Source/HSR/Data/Definitions/HSRStatusDefinition.cpp`

建议最小扩展：

- `Source/HSR/Battle/HSRTurnManager.h/.cpp`
- `Source/HSR/Battle/HSRBattleCoordinator.h/.cpp`
- `Source/HSR/Battle/HSRBattleGameMode.h/.cpp`，仅限 Development 验证入口
- `Config/DefaultGameplayTags.ini`

建议类型：

- `EHSRStatusTriggerTiming`：至少区分目标 TurnStarted、目标 TurnEnded；未实现值必须显式拒绝。
- `EHSRStatusRefreshPolicy`：Reject、RefreshDuration、AddStack、Replace 等，按工作包逐项开放。
- `EHSRStatusRemoveReason`：Expired、Dispelled、Replaced、SourceInvalid、TargetDefeated、BattleFinished、Reset。
- `FHSRStatusInstance`：StatusId、Source/Target ParticipantId、BattleId、Stacks、RemainingTurns、TriggerTiming、Active GE Handle。
- 纯值 Add/Remove/Trigger 结果：携带成功状态、失败原因和可审计前后值，不暴露可变 Runtime 引用。

## 5. 工作包与执行顺序

### P9-000：状态校准与回合生命周期契约

状态：`COMPLETE — PASS WITH FOLLOW-UP`（2026-07-21）。修订后 Development Editor Build 成功；用户两轮 PIE 日志经 Reviewer 核对为 `24 PASS / 2 Harness=COMPLETE / 0 FAIL`。本结果只关闭回合边界契约，不代表 Phase 9 状态 Gameplay 已完成。

保留 follow-up：根执行历史的旧 PIE pending 文字属于早期证据；`BattleEpoch` 仅 manager-local，未来 consumer 必须按 TurnManager 生命周期解绑；死亡目标 Break Delay 诊断统一为 `InvalidTarget`，后续 Debug 可恢复更细结构化原因。

目标：关闭 Gate 0，先证明 TurnSystem 能产生可靠、exactly-once 的回合边界，再允许 StatusComponent 消费。

允许范围：阶段状态 Markdown、`tasks/active-task.md`、`HSRTurnManager.*` 及必要的 battle-local 纯值事件类型；测试接缝只允许 Development/Editor 使用。

必须冻结：

1. TurnStarted/TurnEnded 的定义及广播顺序。
2. `ResolveAction` 成功、非法行动、死亡参与者跳过、Break Delay 跳过、FinishBattle 和 Reset 的事件序列。
3. 事件载荷中的 BattleId、ParticipantId 和必要序号/标识；禁止携带 Actor、ASC 或 StatusComponent 指针。
4. 迟到、重复和旧 Battle 事件的拒绝规则。

验收：两轮 Battle 中事件顺序可由日志逐项对账；非法 Action 不广播；死亡/Delay 跳过不会伪造已完成回合；Finished/Reset 后无迟到事件；现有 Turn 排序与 Break Delay 行为不回归。

停止条件：无法给当前 `OnActionResolved` 定义无歧义兼容语义，或事件顺序会破坏 Phase 5～8 行动结算时，停止并重新审查，不接 Status Runtime。

### P9-001：两回合 Attack Buff 生命周期纵切

状态：`CONTRACT ACTIVE / GAMEPLAY NOT STARTED`（2026-07-21）。唯一活动卡为 `tasks/active-task.md`；执行者首次必须只读复述并等待用户单独确认。当前计划不构成代码、Tag、Definition、GE、Editor 或 PIE 完成证据。

唯一可见结果：为存活目标施加一个持续两个目标自身回合的 Attack Buff；属性立即上升，非目标回合不递减，第二次目标 TurnEnded 后到期并按原 Handle 移除，属性恢复。

允许范围：`HSRStatusTypes.*`、`HSRStatusDefinition.*`、`HSRStatusComponent.*`、Turn 事件的最小消费接缝、Coordinator 初始化/teardown、Development 验证入口、状态 GameplayTag，以及用户授权创建的测试 Definition/GE 资产。

首包冻结：MaxStacks=1；重复施加采用单一明确策略（建议 `RefreshDuration`）；只有目标 TurnEnded 递减；不触发伤害；不跨战斗保存。

关键验收：

- Apply 成功后恰好一个 Runtime Instance 和一个有效 GE Handle。
- 敌方回合不改变 RemainingTurns；目标 TurnEnded 每次只减一。
- 重复施加刷新到两回合且不产生第二份 GE；若最终选择 Reject，则必须零副作用并写入契约。
- 到期、目标死亡、Finished、Reset、EndPlay 均清理同一 Handle。
- 空 Definition、非法 StatusId、Duration<=0、缺少 GE、无 ASC、Apply 失败和旧 BattleId 均零副作用。

用户 Editor 资产：

- `Content/Data/Status/DA_Status_AttackUp_P9.uasset`
- `Content/GameplayEffects/Status/GE_Status_AttackUp_P9.uasset`

GE 使用 Infinite Duration，只配置原创 Attack Modifier/状态 Tag，不配置秒数 Duration 或 Period。

### P9-002：叠层、刷新与替换策略

状态：`ASSET GATE A USER PROVIDED PASS / AWAITING IMPLEMENTATION RESTATEMENT / GAMEPLAY NOT STARTED`（2026-07-21）。用户确认实际 DA/GE 路径、Infinite/AggregateByTarget/StackLimit=2/每层 Attack+10、Tag、无 Period/Execution/Cue/额外字段、DA 骨架字段和 Editor 重开保持。此前 BLOCKED 历史保留。RefreshDuration 仍是不可运行 placeholder；执行者重新复述/确认并实现 AddStack+Build 后，Gate B 才切真实 AddStack 并运行两轮 PIE。附件 `a316...` 仍只属于 P9-001，不是本 Gate 证据。

唯一可见结果：同一状态在明确策略下达到层数上限、刷新或替换，属性和 UI/Debug 快照与 Runtime Instance 始终一致。

范围：扩展既有 Status Definition/Component/Types 和 P9 Development harness；不得新增第二套状态容器。

必须冻结：同 StatusId 不同来源是否共享实例、层数与 Modifier 的换算、共享还是逐层剩余回合、刷新全部还是单层、Replace 的旧 Handle 移除与新 Handle 提交顺序。

建议首选简单语义：同目标 + 同 StatusId 为一个实例；层数共享 RemainingTurns；AddStack 达到上限后只刷新；GE 采用一次可审计重应用，不保留失效 Handle。

验收：1→上限、超上限、刷新、Replace、Apply 新 GE 失败回滚、重复命令、不同来源和移除后重加；任何失败不得改变旧实例或旧 GE。

### P9-003：回合触发 DoT 与 Phase 8 Break Debuff 接入

状态：`ASSET GATE USER PROVIDED PASS / IMPLEMENTATION MATRIX REVISION ACTIVE / PIE NOT AUTHORIZED`（2026-07-22）。用户确认三 Tags、DoT/Break marker GE、Definitions、BP 双绑定和 Editor 保存重开持久；证据等级保持 USER PROVIDED。所有 harness 必须关闭，矩阵与 Build Gate 闭合后才可另行授权 PIE；不进入 P9-004。

唯一可见结果：一个原创 DoT 在规定回合边界通过现有 Phase 7 统一伤害入口触发并到期；Phase 8 Break 结果通过同一 Status Runtime 添加一个最小 Break Debuff，而不是维护旁路状态。

范围：现有 Status Runtime、Damage/Resolution 公共入口、Break 结果到 Status Add 请求的最小映射、经授权的 DoT/Break Debuff Definition 与 GE 资产。

不变量：

- DoT 不直接写 Health，必须走现有可审计伤害入口并产生可区分的 Damage Context/Breakdown。
- Trigger 与 RemainingTurns 递减的先后顺序必须冻结；死亡同帧、到期同帧和终局优先级必须有唯一结果。
- Break/Ability/GE 只请求添加状态，不持有状态实例或直接修改 TurnManager。
- DoT 不能在 Finished、目标死亡、旧 BattleId 或重复事件上触发。

用户 Editor 资产建议：

- `DA_Status_DamageOverTime_P9` 与对应 Infinite/触发配置资产。
- `DA_Status_BreakDebuff_P9` 与对应 Infinite GE。

验收：规定次数触发、每次伤害可解释、到期不多触发一次、重复 Break 不重复添加、死亡同帧与 Reset 无迟到伤害。

### P9-004：免疫、驱散、来源失效与战斗清理

状态：`ASSET GATE USER PROVIDED PASS / PIE AUTHORIZED / AWAITING REVIEW`（2026-07-22）。Immunity GE Infinite/tag-only、无 Modifier/Execution/Cue/Period，GameMode immunity GE 与 Status 绑定经 Editor 重开保持；等待 Reviewer，不进入 P9-005。

唯一可见结果：免疫目标拒绝指定 Debuff；驱散只移除符合规则的状态；来源死亡、目标死亡和战斗 teardown 均按策略清理且不影响无关 GE。

范围：Status Definition/Runtime 的规则过滤、GameplayTags、Coordinator teardown 和 Development 验证；不实现命中率/效果抵抗数值公式。

必须冻结：正面/负面/不可驱散分类、免疫 Tag 查询、驱散选择顺序、来源死亡时 Keep/Remove 策略、目标死亡和 Battle Finished/Reset 的清理顺序。

验收：免疫拒绝、合法驱散、不可驱散、空候选、重复驱散、多来源状态、来源死亡、目标死亡、两轮 Battle 和 Reset；按保存 Handle 移除，其他来源 Infinite GE 保持。

### P9-005：只读状态 UI/Debug 与阶段回归

状态：`CONTRACT ACTIVE / GAMEPLAY NOT STARTED`（2026-07-22）。唯一活动卡为 `tasks/active-task.md`；等待复述与用户确认。

唯一可见结果：最小战斗状态视图能事件驱动显示 StatusId/显示名、层数、剩余回合和分类；状态增删、刷新和触发后更新，隐藏 UI 不影响任何规则。

范围：扩展现有 Battle ViewState/ViewModel/Widget 的最小状态字段和事件绑定，或采用 Development Debug 视图；具体路径在实施前按现有 UI 架构确认。不得提前实施 Phase 10 完整战斗 UI。

规则：UI 不保存 GE Handle、不自行倒计时、不轮询、不使用 Widget Tick；Definition 提供显示配置，Runtime 提供当前快照；委托成对解绑。

验收：Add/Stack/Refresh/Trigger/Dispel/Expire/Reset 的事件更新；Actor/Widget 销毁后无回调；无图标或 UI 缺失时规则仍完整；两轮连续 PIE 无重复绑定。

### P9-006：独立验收、教学与归档

目标：执行者、教师、审查员和协调者分别提交真实产物，完成 Phase 9 Gate，而不是由计划替代证据。

必须归档：

- Fresh `HSREditor Win64 Development` Build，明确记录 UHT、C++、Link 与首个真实错误。
- P9-000～P9-005 的主路径、失败路径、两轮 PIE 和必要静态审查。
- Definition/GE/GameplayTag/Blueprint 引用及 Editor 重开检查。
- Phase 1～8 回归结果和 inherited follow-ups 的最新证据等级。
- Teacher 原始教学、用户真实回答、纠正与掌握度；不得代写用户回答。
- Independent Reviewer 的 `PASS`、`PASS WITH FOLLOW-UP` 或 `REVISE` 原始结论。
- `worklog.md`、`todo_plan.md`、`learning-journal.md`、`docs/gas-notes.md`、`docs/battle-system-design.md` 和必要 README 状态更新。

只有 Gate 全部关闭并完成角色归属、归档与交付后，才可将 Phase 9 标记为 Ready，并建议 Phase 10。

P9-006 Coordinator 首轮审计（2026-07-22）：P9-000～005 三件套归档 18/18；最终 `USER PROVIDED` 五套矩阵为 `28/38/36/16/28 PASS`、各 2 COMPLETE、零失败标记；P9-005 fresh Build 的 UHT/C++/Link/metadata/exit0 已对账，并保留 reflected `uint64` 首错。当前仍缺 Teacher 真实问答、Independent Reviewer 最终 Gate、dirty tree 精确 provenance、`.agents/CLAUDE.md` 分类、Config EOF 处理、角色 commits、P9-006 归档和远端交付，因此状态保持 `CLOSEOUT AUDIT IN PROGRESS / PHASE NOT READY`。

最终收尾（2026-07-22）：Teacher 六题全部掌握；Independent Reviewer 为 `PASS WITH FOLLOW-UP`；`.agents/CLAUDE.md` 已按用户确认纳入项目 provenance；Config EOF 已修复且全局 diff-check 通过。User `2a2eb3d`、Implementation `a996475`、Teacher `39e0449`、Reviewer `db383b3` 已分别提交，P9-006 三件套已归档。Phase 9 因此为 `Ready with inherited follow-ups`；不自动开始 Phase 10。

## 6. Editor 协作清单

每个资产创建前，Codex 必须给出父类、路径、字段、引用和验证方法；用户负责在 UE Editor 中实际创建和保存：

1. GameplayTags：原创 `Status.Buff.*`、`Status.Debuff.*`、`Status.Immunity.*` 等最小集合。
2. Status Definition DataAssets：Attack Buff、DoT、Break Debuff。
3. Infinite GameplayEffects：属性/Tag 载荷；不得使用秒数 Duration/Period 模拟回合。
4. 最小状态显示资产：原创占位图标、显示名和描述；无授权第三方资产不得提交。
5. Battle Character/Definition 或 Development harness 中必要引用绑定。
6. 保存、Editor 重开并逐项确认软引用、Tag 和 GE 类仍有效。

蓝图只承担配置和表现，不实现叠层、回合递减、免疫、驱散、伤害或清理规则。

## 7. 验证与证据矩阵

| 主题 | 主路径证据 | 必须失败/边界路径 |
|---|---|---|
| Turn lifecycle | TurnStarted/Ended 顺序、ParticipantId、BattleId、exactly-once 日志 | 非当前 Action、死亡/Delay 跳过、Finished、Reset、旧 Battle |
| Add/Remove | Instance、层数、剩余回合、GE Handle、属性/Tag 对账 | 空 Definition、无 ASC、GE Apply 失败、重复 Remove |
| 回合到期 | 非目标回合不减、目标边界恰减一、归零移除 | 重复事件、迟到事件、到期与死亡同帧 |
| Stack/Refresh | 1→上限、刷新、Replace、属性与快照一致 | 超上限、不同来源、重应用失败回滚 |
| DoT | 统一伤害入口、规定触发次数、Damage Breakdown | 目标死亡、Finished、到期不多触发、重复事件 |
| Break Debuff | BreakResult→Status 请求→单实例链路 | 重复 Break、无效目标、旧 ActionId |
| Immunity/Dispel | 分类、Tag 查询、精确 Handle 移除 | 不可驱散、空候选、多来源、重复驱散 |
| Teardown | Defeat/Finish/Reset/EndPlay 后零活动实例与 Handle | 两轮 Battle、来源/目标销毁、迟到委托 |
| UI/Debug | 事件驱动快照、无 Tick、解绑证据 | UI 缺失、Widget 销毁、两轮无重复绑定 |
| 工程回归 | Fresh Build、Editor 重开、两轮 PIE、Phase 1～8 关键路径 | 保留第一处真实失败，不以旧日志冒充本轮证据 |

## 8. 教学与协作循环

每个工作包采用：直觉模型 → UE/GAS 真实机制 → 项目所有权映射 → 学员口述/手算 → 最小实现 → Debug → 独立审查 → learning journal。

Phase 9 核心学习问题：

1. 为什么 Infinite GE 适合承载属性/Tag，却不能天然表示“持续两个逻辑回合”？
2. 为什么 Active GE Handle 必须由 Runtime Status 保存，而不能放在 DataAsset、SaveGame 或 UI？
3. TurnManager、StatusComponent、ASC 和 Coordinator 各自拥有什么，不拥有什么？
4. Apply/Refresh/Replace 如何保证失败原子性？
5. DoT 为什么仍需通过统一伤害入口，而不是直接修改 Health？
6. 如何证明 Reset、teardown 和第二轮 Battle 没有委托或 GE 泄漏？

角色边界：协调者维护范围、任务卡、归属和证据链；执行者提交授权范围内的代码与 Editor 操作说明；用户创建并验证 Editor 资产；教师保留真实教学与回答；审查员独立检查实现、证据和未验证项。

## 9. 风险、停止条件与非目标

主要风险：

- 当前 `OnActionResolved` 的广播发生在队列推进之后；若直接当作 TurnEnded 使用而不冻结语义，未来 TurnStarted、死亡跳过和 Delay 可能产生顺序歧义。
- `PendingBreakDelayActionIds` 当前按目标保存单个请求；多个状态/延后同时作用时可能覆盖。Phase 9 不得默认为队列或聚合，必须保留现有契约或另行授权设计。
- GE 重应用可能产生新 Handle；若先移除旧效果再 Apply 新效果，失败会丢失旧状态。Replace/Refresh 必须设计可回滚顺序。
- 委托解绑、Actor/ASC 失效、Reset 与 EndPlay 顺序不清会造成第二轮 Battle 残留。
- Phase 8 的部分动态路径仍为 inherited follow-up，不能用 Phase 9 计划或静态审查升级为已验证。

任何以下情况立即停止并请求审查：需要新模块、广泛重构 TurnSystem/Coordinator、修改危险 Config、引入第三方资产/外部依赖、改变 Save Schema、实现网络复制，或需要超出活动任务白名单的文件。

明确非目标：

- Phase 10 完整战斗 UI、行动条、正式状态面板和输入焦点体系。
- 效果命中、效果抵抗、复杂控制条、Boss 多阶段免疫和大规模状态组合。
- GAS 秒数 Duration/Period 驱动回合、Actor/Widget Tick 扫描。
- 网络权威、复制、预测、回滚和跨会话状态保存。
- AoE、多段攻击、复杂 AI、正式动画/VFX/音频和第三方表现资产。
- 新 Runtime 模块、插件化、大规模目录迁移或无关重构。

## 10. Phase 9 完成定义

只有同时满足以下条件，Phase 9 才能标记为 `Ready` 或 `Ready with inherited follow-ups`：

1. P9-000～P9-006 均有归档产物和独立结论。
2. 至少一个 Buff、一个 DoT 和一个 Break Debuff 通过真实 PIE。
3. 叠层上限、刷新/替换、免疫、驱散、来源死亡、目标死亡和战斗清理均有证据。
4. 状态按逻辑回合而非秒数触发/到期；无 Tick。
5. 每个活动状态的 GE Handle 可追踪、精确移除且不会影响其他来源。
6. Fresh Build、Editor 重开、两轮 PIE、主路径、失败路径和 Phase 1～8 回归均已记录。
7. UI/Debug 只读、事件驱动、生命周期解绑通过审查。
8. 文档、教学、Reviewer、provenance、角色提交、阶段归档与远端交付全部闭合。

在此之前不得进入 Phase 10。P9-002 当前先等待 User 资产 Gate；Gate 通过前不得实施，且不得进入 P9-003。
