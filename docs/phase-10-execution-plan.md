# Phase 10 详细执行计划：事件驱动战斗 UI 完整化

> 状态：`P10-005 ARCHIVED / PASS WITH FOLLOW-UP；Phase 10 READY WITH INHERITED FOLLOW-UPS`
> 基线：2026-07-22，Git `HEAD=dca9267`；Phase 9 为 `Ready with inherited follow-ups`。1920x1080 为 `USER PROVIDED / VISUALLY OBSERVED`；1280x720 为 `USER ACCEPTED / NOT VERIFIED`；物理手柄、完整导航、双分辨率验证和注入式 preflight 拒绝/重试保留为 follow-ups，不自动开始 Phase 11。
> 本文只建立执行契约，不证明任何 Phase 10 C++、UMG、Editor、Build 或 PIE 已完成。

## 1. 阶段目标与 Gate 0

Phase 10 将 Phase 6～9 已存在的 Command、Target、Resolution、Attribute、Weakness/Toughness/Break 和 Status 事件，收敛为可实际操作的完整灰盒战斗界面：行动者/行动条、技能说明与可用性、显式目标选择、双方属性和状态、伤害数字、胜负页，以及键鼠/手柄焦点。UI 只读消费纯值 ViewState，只能提交纯值 Command；它不是战斗规则、属性、状态、目标合法性或胜负的真源。

Gate 0 在任何写代码前必须满足：

1. 执行者只读审计 `HSRBattleCommandTypes/ViewModel/Widget`、`HSRBattleCoordinator`、`HSRTurnManager`、Ability Resolution、Status public snapshot/operation 与现有 BP 绑定，逐项列出生产者、消费者、生命周期和现有委托句柄。
2. 复述确认 Phase 9 的证据边界和 inherited follow-ups：PIE/资产字段仍为 `USER PROVIDED`；`BattleEpoch` 仅 manager-local；旧 manager/Coordinator/ASC/Widget 切换必须靠成对解绑，不能依赖 Epoch 全局唯一。
3. 复述精确文件白名单、唯一可见结果、测试矩阵、Editor 作者边界、停止条件与非目标，并以任务卡指定句子结束；用户或 Coordinator 明确确认前零写入、零 Build、零 Editor/PIE。
4. 不修改或认领回退后保留的 `.agents/CLAUDE.md`、`.claude/`、`Tools/`、`Plugins/` dirty 状态；不运行 reset/clean，不把计划当运行证据。

## 2. 全阶段架构与所有权不变量

- `UHSRBattleCoordinator`/既有 Battle Runtime 是规则与参与者接线入口；TurnManager 决定当前行动者，Ability/Resolution 决定命令是否合法和结果，StatusComponent 是活动状态真源。
- ViewModel 拥有可丢弃的 Runtime 展示快照、选中 SkillId/TargetId 和委托句柄；不得拥有 Actor/ASC/Status 实例、GE Handle、回合推进权或胜负真源。
- Widget 只呈现 ViewModel 并提交 `BattleId/ActionId/ActorParticipantId/SkillId/TargetParticipantId` 等纯值 Command；不得直接改属性、SP、Energy、Status、TurnManager 或 Coordinator 内部状态。
- Definition 提供只读图标、名称和说明；稳定 ID 不使用显示文本。缺失图标/文本/Widget 时 Gameplay 必须仍可完整运行。
- `GetCommandViewState`、availability builder 与任何 ViewModel refresh 必须是 observationally pure：禁止调用 `SetPendingTarget`、`TryActivateAbility`、Reserve/Commit/Rollback，禁止写 Ability instance 或消费 RNG。现有查询若只能通过可变 pending 状态完成，P10-001 必须先建立无副作用 query seam；若因此需要修改活动卡白名单外的 Ability 文件，立即停止并由 Coordinator 修订白名单、重新走确认 Gate。
- 禁止 Widget Tick、Actor Tick 或轮询扫描；所有更新来自既有或最小新增的纯值事件。绑定前先解绑，Coordinator/ASC/TurnManager 替换、`NativeDestruct`、`BeginDestroy`、Battle Finished/Reset/EndPlay 均成对清理。
- Command pending 与 presentation lock 是两个独立锁，提交条件恒为 `!bCommandPending && !bPresentationLocked`。pending 只能由匹配 `BattleId + ActionId` 的权威 Resolution/Cancel/teardown 清除；presentation lock 只能由匹配的动画完成/取消/teardown 清除，P10-001 至少匹配 `BattleId + ActionId`，P10-003 引入后还须匹配 `EventId`。动画通知只能释放表现锁，不能决定伤害、回合或胜负；若 P10-001 尚无可靠动画完成事件，本包不得启用表现锁，也不得用 Timer/Delay 伪造。
- 纯值快照必须有安全默认值；Blueprint 暴露只读或最小 Callable。UObject 引用使用 `UPROPERTY/TObjectPtr/TWeakObjectPtr`；Lambda、Timer、动画回调不得悬挂捕获。
- 当前为单机，不添加 RPC、复制、预测、SaveGame、新模块、插件或外部依赖。

## 3. 工作包与串行交接

当前因 P10-001 PIE 暴露“Enemy 首回合无行动”的 Runtime 缺口，插入补充工作包 `P10-001A`。当前唯一活动顺序为 `P10-001A → 完成/归档 P10-001 → P10-002 → P10-003 → P10-004 → P10-005`。P10-001A 只闭合敌方确定性 BasicAttack 与 authored 技能文本边界，不吞并后续详情 UI。每包均先由 Coordinator 建立唯一活动卡；Implementation 首次只读复述；Implementation 与 User Editor 资产分别交接、分别归属；随后 Teacher 与 Independent Reviewer；Reviewer 未放行不得进入下一包。

### P10-001A：敌方确定性基础攻击与技能展示文本边界

**唯一可见结果：** 任一合法 Enemy 回合由 Coordinator 经既有 `RequestAction` 自动且恰好一次提交固定 BasicAttack，随后按既有结算推进到下一回合；技能按钮只显示 authored 短 `DisplayName` 与成本，长 `Description` 保存于 Definition 并复制到纯值 ViewState，留给未来详情页。

**关键边界：** 无 Tick/Timer/Delay，不经 Widget，不直接激活 Ability 或推进 TurnManager。唯一 subsequent seam 为 `TurnStarted handler 只记录 pending key -> 最外层 RequestAction 全部收尾后的非递归迭代 drain`；handler 禁止直接提交。规范键为 `bound manager identity + BattleEpoch + TurnSequence + ParticipantId`，并有全局 `bRequestActionInProgress`/最外层 depth guard。唯一 drain 安全点必须晚于校验、Commit/Rollback、Resolution、Turn advance、terminal、cache/LastResolution 和 ViewState publication；initial Spawned-complete 也只记录 key 并启动同一 drain。同步失败不递归、不推进；Finished/Reset/旧 manager/旧 epoch 零副作用。若现有 RequestAction 无法形成无递归单管线 dispatch/drain，则停止重新设计；TurnManager 只读。

**允许范围：** `HSRBattleCoordinator.h/.cpp`；`HSRSkillDefinition.h` 增加 `FText DisplayName/Description`（当前无 `.cpp`，不新建）；`HSRBattleCommandWidget.h/.cpp` 仅隐藏并停止写入四个 Optional Description 控件；`HSRBattleGameMode.h/.cpp` 仅默认关闭 Development harness；`tasks/execution-result.md`。User 单独配置四个实际 Skill DataAsset 并可删除 Designer 中四个 Description TextBlock。

**Development seam 修订：** 在既有 Coordinator h/cpp 白名单内允许严格 `#if WITH_EDITOR`、非 UFUNCTION/UPROPERTY/Blueprint 的测试接缝：(1) 将指定 `SourceManager + FHSRTurnLifecycleEvent` 只传入生产 `RecordEnemyTurnIfCurrent`；(2) 只读 pending/consumed/depth/queue/dispatch/reject/bound identity；(3) 临时绑定隔离测试 manager，并在所有出口成对恢复生产 manager与唯一 delegate。GameMode 可创建隔离的 3+ participant manager/真实生命周期与 Delay 序列。Seam 禁止 drain、ActionId、RequestAction/Core、Turn推进或 Gameplay mutation；flag 默认 false，Shipping 零符号/零对象/零行为；真实行动仍必须走生产 pending/drain/统一请求路径。不得修改 TurnManager。

**验证：** initial/subsequent enemy turn；同步 TurnStarted 在玩家 Commit/Finalize 前只排队且外层 LastResolution 先完整；拒绝玩家请求不错误生成/消费 pending；3 个以上连续 Enemy/Delay 跳过逐 Sequence 恰好一次；同一回合重复/重入；旧 manager 与新 manager Epoch 数字相同仍因 identity 拒绝；缺 Definition/Ability/target/ASC；死亡/Finished/Reset/manager replacement/EndPlay；两轮 battle key/handle/count 不继承或倍增；P10-001 与 Phase 6～9 关键回归。详细契约以 `tasks/active-task.md` 为准。

**当前 Build 证据：** 首次 fresh Build 已通过 UHT（2 files generated），随后因 Development harness 把条件表达式用作 `UE_LOG` verbosity 而发生真实 C++ 编译失败；已改为显式 Log/Error 分支但尚未重跑。必须保留失败链并执行 fresh retry，成功前状态仍为 Build pending。

**归档附注（2026-07-23）：** P10-001A 的后续 fresh Build、Development harness、post-turn ViewState 修订与 harness=false 两局正常 PIE 已闭合；Independent Reviewer 为 `PASS WITH FOLLOW-UP`，Teacher 为 `3/3 mastered`。三件套位于 `tasks/archive/TASK-P10-001A-*`。当前活动返回 P10-001 主任务收尾；本附注不授权 P10-002，历史首错与修订链继续保留在归档 execution result。

**P10-001 收尾矩阵修订（2026-07-23）：** 用户既有 P10-001 实施授权与继续授权允许补充默认 false、严格 `WITH_EDITOR` 的最小 Development harness。白名单限既有 GameMode、Coordinator、BattleCommand ViewModel/Widget、GameplayAbilityBase 与执行报告；只允许非反射只读/audit seam，禁止生产 API、TurnManager、Config/Content、Gameplay 语义或第二命令管线。矩阵必须闭合 availability 纯读与结构化原因、Target/单提交、pending 匹配解锁、Widget/Coordinator/ASC 生命周期及两轮不倍增；随后 fresh Build、User opt-in harness、flag=false 两局、Teacher 与 Reviewer。详细契约以根 `tasks/active-task.md` 为准；P10-002 仍未授权。

**Reviewer 固定槽修订：** 唯一新增生产改动仅允许 Coordinator Command ViewState 始终输出固定 Basic/Skill/Ultimate/Heal 四 slot。null Definition 保留 Category，但必须 `SkillId=None`、不可用、`DefinitionMissing`、安全占位，并且不进入 Target/ASC/Ability/Cost query、零副作用；不得伪造可提交 SkillId 或修改 RequestAction/Gameplay。真实 ASC 动态撤销/恢复 Ability 从强制矩阵降为 `STATICALLY COVERED`：静态审查 missing Spec/instance -> `AbilityUnavailable`；安全隔离动态 case 可选且非阻断，严禁 mutating seam。其他 harness、Build、User PIE、Teacher/Reviewer Gate 与 P10-002 禁止保持不变。

**后续信息架构（非 001A）：** 战斗内 C 键我方/敌方详情作为 Phase 10 独立后续包，展示属性、状态、技能长说明、弱点等纯值信息；项目全部核心功能完成后再评估 G 键仓库/专属效果说明页。仅抽象功能分层，不复制参考商业游戏的素材、布局、文案或专有名。

### P10-001：可安全操作的行动条、技能说明与命令锁

**唯一可见结果：** 玩家回合显示 Basic/Skill/Ultimate 灰盒按钮、名称/说明/资源与禁用原因；点击一个可用技能只提交一次纯值 Command，提交至匹配结果返回或表现结束前所有命令入口保持锁定，敌方回合和不可用技能不能提交。

**前置 Gate：** Gate 0；Phase 6 Command/Target/Resolution 和 Phase 9 P9-005 只读 UI 基线只读审计完成；确认现有 `SubmitSelectedSkill` 是否已经防重入，禁止凭命名猜测；必须定位并处置现有 `GetCommandViewState` availability 查询中的 `SetPendingTarget/ClearPendingTarget` 可变副作用，在证明 query seam observationally pure 前不得实现按钮 availability。

**建议文件/允许范围：** `Source/HSR/UI/HSRBattleCommandTypes.h`、`HSRBattleCommandViewModel.h/.cpp`、`HSRBattleCommandWidget.h/.cpp`；最小扩展 `Source/HSR/Battle/HSRBattleCoordinator.h/.cpp`，使 `GetCommandViewState` 改用纯 availability seam；`Source/HSR/GAS/Ability/HSRGameplayAbilityBase.h/.cpp` 仅允许增加无写 `PendingTarget`、无激活、无事务、无 RNG 的纯 preflight API，不得改变真实提交路径；Development 验证入口仅限 `HSRBattleGameMode.h/.cpp`，并补齐 BattleResult delegate handle 的成对解绑；`tasks/execution-result.md`。实施前须把最终精确清单写入活动卡；未列文件禁止修改。

**执行者逐项步骤：**

1. Gate B 已确认现有 `GetCommandViewState` 会调用 `SetPendingTarget/ClearPendingTarget`。先在 Ability Base 建立 const 语义的纯 preflight API：输入仅为既有 Ability spec/actor info 与候选目标 ASC，输出结构化 `EHSRAbilityFailureReason`；不得写 `PendingTarget`、激活 Ability、Reserve/Commit/Rollback、准备伤害、修改 Cost/资源或消费 RNG。Coordinator availability builder 只能调用该 seam；真实 `RequestAction` 继续使用既有 pending-target/activation 事务路径，二者不得合并成第二命令管线。
2. 扩展纯值 Skill Button/ViewState：稳定 SkillId、`FText` 名称/说明、资源成本、TargetType、可用性、结构化禁用原因、选中态、`bCanSubmit/bCommandPending`；不得暴露可变 Runtime 引用。
3. 在 ViewModel 集中派生显示态与合法默认选择；Widget 不自行重新计算 Ability 条件。
4. 在唯一提交入口校验 Battle、行动者、Skill、Target、可用性和 pending 状态；先原子设置 pending，再提交一次 Command。同步拒绝必须产生纯值失败结果并按契约解锁。
5. 对匹配 Action 的权威成功/失败/取消清 pending；旧 Battle、旧 Action、重复结果零副作用。presentation lock 独立记录且只由匹配动画事件清除，提交必须同时满足两个锁均为 false；若没有可靠动画完成事件，本包保持 presentation lock 禁用。
6. 所有 bind/rebind 前先 unbind；Widget `NativeDestruct`、ViewModel `BeginDestroy`、Coordinator replacement、Finished/Reset 清理句柄和锁；GameMode 必须保存 BattleResult delegate handle，并在 EndPlay/Coordinator replacement 时精确移除。
7. 添加 Development-only 可审计日志/矩阵，不以 Delay/Timer 冒充异步，不改变 Gameplay 结果。

**用户 Editor 操作：** 在 `Content/UI/WBP_BattleCommandPanel.uasset` 或经审计确认的既有派生 WBP 中绑定三个按钮、名称/说明/资源/禁用原因/选中态和 pending 遮罩；按钮仅调用 C++ Widget 的 Select/Submit；保存、关闭 Editor、重开并确认 Class/事件绑定保持。用户资产 commit 独立于 Implementation。

**验证矩阵：** 主路径：玩家回合分别选择可用 Basic/Skill/Ultimate、正确说明/成本/Target、每次恰一 Command。失败：敌方回合、资源不足、无候选目标、空/未知 SkillId、重复点击、旧 Battle/Action、Coordinator/Widget 缺失均拒绝且不推进回合。生命周期：rebind、Widget 销毁重建、Coordinator replacement、Finished/Reset 后零迟到回调。两轮：连续两轮 PIE，每轮完整提交并结束，第二轮 BindGeneration/CommandCount 不倍增；Phase 6～9 关键矩阵不回归。

**证据等级：** C++/Build 为 `AGENT REPORTED` 后由 Reviewer 静态审计；Editor 资产字段与 PIE 为 `USER PROVIDED / REVIEWER INSPECTED`；不得把截图升级为内部字段独立验证。

**停止条件：** 纯 preflight 无法仅通过 Ability spec/actor info/候选目标 ASC 返回结构化失败原因；需要修改派生 Ability、改变真实 pending-target/Ability/Cost/Damage/Status/Turn/Resolution 语义、增加第二命令管线、广泛 Coordinator 重构、Tick/Timer 解锁、Config/Build.cs/新模块、或白名单外文件时立即停止。

**非目标：** 显式多目标 UI、双方完整面板、伤害浮字、胜负页、输入焦点、正式动画/音效/VFX、Cost/Cooldown 新规则。

**角色与 commit：** Implementation 只认领白名单 C++ 与执行报告；User 只认领 WBP/资产绑定；Teacher 记录真实问答；Reviewer 只认领审查；Coordinator 最后归档。各自独立 commit，Phase 中途不 push。

### P10-002：行动顺序、显式目标与双方战斗/状态面板

**唯一可见结果：** 界面事件驱动显示当前行动者、稳定行动顺序、玩家/敌方 HP/Energy/Toughness/Weakness/Break、状态列表；单目标技能必须由玩家显式选择合法目标后才可提交，目标死亡/失效时自动清除或选择唯一合法候选。

**前置 Gate：** P10-001 Reviewer `PASS` 或 `PASS WITH FOLLOW-UP` 且归档；命令锁、ActionId 和 Widget 生命周期不得回归。

**建议文件/允许范围：** 复用 P10-001 UI 文件；必要时新增 `HSRActionBarViewModel.h/.cpp` 或把最小行动顺序纯值字段加入既有 Types（两者只选一条，禁止并行架构）；最小扩展 `HSRTurnManager.h/.cpp`/`HSRBattleCoordinator.h/.cpp` 只为发布纯值顺序/参与者快照；Development harness 与执行报告。

**执行者逐项步骤：** 先冻结行动顺序事件的 BattleId/ParticipantId/sequence；构建玩家/敌方纯值 TargetViewState；把 Attribute/Status/Turn 委托集中到 ViewModel；合法目标变化时验证选择，不在 Widget 缓存 Actor；UI 状态列表复用 Phase 9 public snapshot；所有属性/状态/目标/顺序委托成对解绑；目标选择只提交稳定 ID。

**用户 Editor 操作：** 创建或扩展 `WBP_BattleActionBar`、双方 Participant Panel、Target Selector 与 Status Entry（最终精确路径由活动卡冻结）；只绑定只读字段和 SelectTarget；使用原创占位颜色/图形，无第三方资产；保存重开确认。

**验证矩阵：** 主路径覆盖顺序推进、双方数值/状态增删、显式切换目标并提交；至少覆盖 3 个参与者、同 Speed 稳定 tie-break、Delay 后顺序、死亡参与者移除，以及 Reset/Finished 后空队列。失败覆盖死亡/无效/非候选/旧目标、空面板、无图标；生命周期覆盖 Participant/ASC/Widget 销毁、Manager replacement、Reset/Finished；两轮验证顺序/属性/状态绑定不倍增，且 P10-001 防重入保持。

**证据等级：** 同 P10-001；行动顺序/属性/状态日志需能与 Runtime 真源逐项对账。

**停止条件：** 需要改变 Turn 排序、目标策略或 Status Runtime，缓存 Actor/ASC 到 Blueprint，或需要轮询时停止。

**非目标：** AoE、多选、队伍编成、行动插队新规则、正式 Portrait/Icon、伤害动画与结果页。

**角色/commit/交接：** Implementation → User Editor → Teacher → Reviewer → Coordinator；作者分别提交，Reviewer 放行后才建 P10-003。

### P10-003：伤害/治疗数字与规则无关的表现队列

**唯一可见结果：** 普攻/战技/终结技、DoT、Break 与 Heal 的既有 Resolution 产生一次可解释的灰盒浮动数字/标签；表现播放期间不会重复提交命令，隐藏或销毁表现层不影响伤害、状态、回合与胜负。

**前置 Gate：** P10-002 已归档；已有统一 Resolution/Damage Breakdown 是唯一数据源；先证明没有必要从 Attribute delta 反推伤害来源。

**建议文件/允许范围：** UI Types/ViewModel/Widget 的最小表现事件与队列；必要时 Coordinator 只转发既有纯值 Resolution；可新增单一 `HSRBattlePresentationTypes.h`，不得创建规则系统；Development harness 与执行报告。

**执行者逐项步骤：** 将 Resolution 映射为纯值 PresentationEvent（Battle/Action/EventId、Source/Target、数值、类型、Crit/Break/Heal）；按 EventId 幂等入队；规则结算先完成，表现只消费；Widget 动画完成只回报对应 EventId 以出队/释放表现锁；缺失 Widget、跳过动画、销毁、Finished/Reset 均排空且不能推进规则；禁止 Widget Timer/Tick，必要动画使用 UMG Animation finished delegate 并成对解绑。

**用户 Editor 操作：** 创建原创灰盒 `WBP_BattleFloatingNumber`/动画并绑定纯值字段与 Animation Finished 回调；验证无动画资产时即时完成降级；保存重开。

**验证矩阵：** 主路径覆盖 Damage/Crit/DoT/Break/Heal 各恰一次且数值匹配 Breakdown；失败覆盖重复 EventId、零/非法 payload、旧 Action/Battle、目标死亡同帧、Widget 缺失；生命周期覆盖动画中 Destruct、Reset/Finished、第二轮无旧浮字；两轮连续 PIE 且 CommandCount/PresentationCount 一致、不重复。

**证据等级：** Resolution 对账日志为 Agent/Reviewer 静态审计，视觉与动画为 User Provided；帧级观感不替代规则证据。

**停止条件：** 必须改变伤害公式/GE/Status 触发顺序、以动画通知结算 Gameplay、使用 Tick 扫描或引入外部素材时停止。

**非目标：** 正式 VFX/GameplayCue/音频、对象池性能优化、多段/AoE 新 Gameplay。

**角色/commit/交接：** 同上，User 资产独立；P10-004 等 Reviewer 放行。

### P10-004：胜负页、输入焦点与端到端 UI 生命周期

**唯一可见结果：** BattleResult exactly-once 后显示 Victory/Defeat 灰盒页并锁定战斗命令；键鼠和手柄可在技能、目标、确认间移动焦点，返回探索/teardown 后恢复既有输入模式且没有战斗 Widget、焦点或委托残留。

**前置 Gate：** P10-003 已归档；Phase 5 BattleResult/Return exactly-once 与 PlayerController 既有输入所有权先只读复核，不得让 Widget 接管跨图真源。

**建议文件/允许范围：** 既有 UI Types/ViewModel/Widget；最小 `HSRPlayerController.h/.cpp` 输入模式接缝（仅经审计确需）；Coordinator/Transition 只读或纯值结果接缝；Development harness 与执行报告。地图/Config 默认值禁止修改。

**执行者逐项步骤：** 以既有 BattleResult 事件派生只读 ResultViewState；首个匹配结果锁命令并显示一次，重复/旧结果忽略；Widget 只发确认/返回纯值意图，Coordinator/Transition 保持 exactly-once；定义键鼠/手柄初始焦点、无候选回退和输入模式恢复；NativeDestruct/level travel 前解绑动画、ViewModel、输入/结果委托；两轮跨图后无残留。

**用户 Editor 操作：** 创建/扩展 `WBP_BattleResult` 和焦点导航；为 Button 设置可聚焦和显式导航；分别验证键鼠、手柄、Victory、Defeat、返回探索；保存重开。

**验证矩阵：** 主路径 Victory/Defeat、结果页恰一次、确认返回、输入恢复；失败覆盖重复 Result/Confirm、旧 Battle、Widget 缺失、无可聚焦按钮、travel 中回调；生命周期覆盖动画/结果页中 teardown、两轮 Battle、探索输入恢复；完整回归 P10-001～003 与 Phase 5/9 关键路径。

**证据等级：** C++/Build、用户输入/PIE、Reviewer 分级同前；真实手柄未测试必须明确写未验证。

**停止条件：** 需要重写 Transition 协议、改变 PlayerController 全局输入架构、修改地图/Config、增加 UI Screen Stack（属于 Phase 17）时停止。

**非目标：** 主菜单/暂停/角色/背包 UI 栈、设置菜单、本地化管线、正式胜负演出。

**角色/commit/交接：** Implementation → User → Teacher → Reviewer → Coordinator；Reviewer 放行后进入收尾。

### P10-005：独立验收、教学与阶段归档

**唯一可见结果：** Phase 10 的 Build、资产、两轮 PIE、失败/修订、UI 生命周期、输入与回归证据形成可审计归档，并由 Independent Reviewer 给出最终结论；不新增 Gameplay。

**前置 Gate：** P10-001～004 三件套齐全且各自 Reviewer 已放行；所有角色作者与 dirty provenance 可精确区分。

**允许范围：** `tasks/*`、归档三件套、`docs/phase-10-execution-plan.md` 的状态附注、`PROJECT_STATE.md`、`README.md`、`todo_plan.md`、`worklog.md`、`learning-journal.md`、`docs/ui-guidelines.md`、必要 `gas-notes/battle-system-design`；Fresh Build 与只读 Git 检查。禁止修改 Source/Content/Config/Gameplay。

**执行步骤与验证：** fresh `HSREditor Win64 Development` 保留 UHT/C++/Link/metadata/exit 与首个真实错误；Editor 重开；P10-001～004 主/失败/生命周期/两轮矩阵；1280×720、1920×1080、键鼠和可用时手柄；无 Widget Tick 静态搜索；委托 bind/unbind 和重复命令计数；Phase 5～9 关键回归；用户资产字段证据仍按 User Provided；Teacher 保留真实回答与纠正；Reviewer 独立判定 `PASS`、`PASS WITH FOLLOW-UP` 或 `REVISE`。

**停止条件：** 任一工程包无最终 Reviewer、作者不明、dirty 无 provenance、运行证据缺失却被写成通过、或需要功能修复时停止并回到对应工程包，不在收尾中偷改 Gameplay。

**非目标：** Phase 11 成长、Save、正式美术、性能优化、网络和 Phase 17 UI 栈。

**角色/commit/交接：** User/Implementation/Teacher/Reviewer 分别提交真实产物；Coordinator 在 Gate 全闭后归档并提交 Phase closeout，之后才按项目授权 push。计划阶段不 commit/push。

## 4. Phase 10 完成定义

只有以下全部满足，Phase 10 才能标记 `Ready` 或 `Ready with inherited follow-ups`：

1. P10-001～005 均有 active/execution/final-review 归档和独立结论。
2. 行动条、技能描述/禁用原因、显式目标、双方属性/状态、行动顺序、伤害/治疗表现、胜负页和输入焦点有真实两轮 PIE 证据。
3. UI 只读消费纯值事件，只提交纯值 Command；零 Widget Tick；没有 Actor/ASC/GE Handle/Status Runtime/胜负真源泄漏到 Widget。
4. 动画与 pending 期间防重复命令；重复/旧 Battle/Action/Event/Result 均零副作用。
5. Coordinator、TurnManager、ASC、ViewModel、Widget、动画与输入委托在 replacement、Destruct、Reset、Finished、EndPlay、travel 中成对解绑；第二轮不倍增。
6. Fresh Build、Editor 重开、主路径、失败路径、生命周期、分辨率/输入和 Phase 5～9 回归均记录真实证据等级。
7. 作者归属、dirty provenance、角色 commits、Teacher、Reviewer、文档和远端交付闭合；历史 Reviewer 结论与 inherited follow-ups 不被改写。
8. 行动顺序证据包含至少 3 个参与者、同 Speed 稳定 tie-break、Delay 后顺序、死亡移除，以及 Reset/Finished 空队列；不得以当前 1v1 单一路径替代。

在此之前不得进入 Phase 11。
