# Phase 17 Patch 01 Execution Plan — Status、Break 与行动值模型

> 基线：2026-07-27，`HEAD=d9e486e`；P17-001～P17-004 已完成并提交，P17-005 尚未开始。
> 授权：用户明确要求在 P17-005 前提前关闭 `phase-roadmap-0-20.md` 跨阶段改进清单 1～3，并选择完整行动值模型。
> 范围：本补丁只处理 ActiveStatus 通用化、Break 可重复触发、Speed/Advance/Delay/Slow 行动值重排。Behavior Tree 迁移属于独立 Patch 02。

> 当前进度：PATCH-01A 已归档并由 Independent Reviewer 最终判定 `PASS`；PATCH-01B 活动卡已创建，等待 Implementation 只读复述与用户单独确认。

> 进度更新：PATCH-01B 已归档并由 Independent Reviewer 最终判定 `PASS`；PATCH-01C 完整行动距离模型活动卡已创建，等待 Task Gate 审查。
>
> 进度更新：PATCH-01C 已归档并由 Independent Reviewer 最终判定 `PASS`（`28d3213`）；PATCH-01D 最终回归与归档活动卡已创建，等待 Task Gate 审查。

## 1. 当前事实与 Gate

- `UHSRStatusDefinition::Validate` 只接受三个硬编码 StatusId；`UHSRStatusComponent` 又把 `Status.Buff.AttackUp` 放入 `ActiveStatus` 特殊槽位，其余状态放入 `AdditionalStatuses`，并维护两套幂等集合。
- `FHSRBattleParticipant::bBreakResultPublished` 是单个 Battle Actor 生命周期内的终身闩锁；第一次 Break 后，即使 Toughness 恢复，再次从正数归零仍被拒绝。
- `UHSRTurnManager` 仅在 `Initialize` 时读取一次 Speed、按 Speed 排序并循环数组；Break Delay 通过跳过一次候选实现。运行中 Speed 变化、Advance、Delay 与 Slow 没有统一模型。
- P17-001～004 的 UI/Map/PlayerController 代码不属于本补丁；工作区仅保留未跟踪 `.claude/**` 本地配置，禁止纳入补丁。

Gate 状态：`READY FOR PATCH IMPLEMENTATION`。三项属于相邻的 Battle Runtime 修复，但必须串行落地和独立验证，避免同时改写状态容器、Break 触发与排队算法后失去首错定位。

## 2. 补丁唯一结果

在不进入 P17-005、不新增 Editor 资产的前提下，Battle Runtime 达到以下单一可验收结果：任意合法 Status Definition 可由同一容器添加、刷新、叠层、驱散、过期和查询；同一目标每次 Toughness 从正数归零均产生一个独立且幂等的 Break；TurnManager 使用统一行动距离模型响应 Speed、Slow、Advance 和 Delay，同时不取消当前行动、不重复/丢失行动，并保持稳定 Tie-break。

## 3. 所有权与数据流

### 3.1 Status

```text
Status Definition
→ generic validation by fields/tags
→ UHSRStatusComponent::TMap<StatusId, Runtime Entry>
→ Active GE handle + turn lifecycle
→ sorted public snapshots / typed operation result
→ existing Battle ViewModel/UI consumer
```

- Definition 是静态合同；Component 是 Battle-local 运行态唯一所有者；ASC 只拥有 GE/Tag/Attribute。
- 不再存在 AttackUp 专用槽位、专用幂等集合或按 StatusId 选择核心分支。
- `StatusId` 与 `GrantedStatusTag` 必须一一对应并位于 `Status.*`；分类、效果类型、刷新策略、驱散和来源失效规则由字段验证。
- 未注册/未激活 Status 的查询返回结构化 `UnknownStatus`，不得崩溃或伪造默认 Buff。

### 3.2 Break

```text
Toughness Before > 0
→ authoritative damage transaction
→ Toughness After == 0
→ ActionId replay guard
→ one BreakResult
→ Break Status + Turn Delay
```

- Break 事实由本次权威伤害事务的正数到零边沿决定，不由角色终身 bool 决定。
- 相同 ActionId 的重复提交沿用 Coordinator 已有 action-resolution 幂等结果，不重复发布 Break/Status/Delay。
- Toughness 恢复到正数后，新 ActionId 再次归零必须再次触发。
- 目标死亡、Battle Finished、Reset、跨地图重建和单纯 Toughness 恢复都不得伪造 Break。

### 3.3 行动值

```text
ASC Speed delegate / Advance request / Delay request
→ UHSRTurnManager battle-local action-distance state
→ preserve current action lock
→ normalize remaining distances
→ stable next candidate
→ TurnStarted/TurnEnded
```

- 原创基础公式：`BaseActionDistance = 10000.0 / max(EffectiveSpeed, 1.0)`。
- 每名参与者保存 `EffectiveSpeed`、`BaseActionDistance`、`RemainingActionDistance` 和稳定初始顺序/ParticipantId。
- 初始化时 Remaining 等于 Base；选取下一行动者前，从所有合法候选的 Remaining 中减去最小值，最小者行动。
- 行动开始后当前参与者锁定；变化只更新未来 Remaining，不更换或取消当前行动。
- 当前行动结束后，该参与者为下一轮增加一次当前 BaseActionDistance；高速角色因此可能比低速角色更频繁行动。
- Speed/Slow：ASC Speed delegate 触发。对非当前参与者按 `NewRemaining = OldRemaining * NewBase / OldBase` 保留已完成比例；当前参与者的新 Base 在行动结束重置时生效。
- Advance：`Remaining -= BaseActionDistance * Ratio`；Delay：`Remaining += BaseActionDistance * Ratio`。Ratio 必须有限且位于 `[0, 1]`。
- Break Delay 使用同一 Delay 请求，Ratio=`1.0`，保持“延后一整次自身基础行动距离”的语义。
- OperationId exactly-once；无效/死亡/Finished/旧 Epoch 请求零变化并返回结构化结果。
- 同 Remaining 使用 ParticipantId 字典序稳定 Tie-break；不得依赖 UObject 地址、数组偶然顺序或浮点不稳定比较。

## 4. 串行工作包

### PATCH-01A — ActiveStatus 通用化

唯一结果：Attack、Speed、Shield 三种测试 Definition 走相同容器和代码路径；新增合法 Buff 不修改核心分支；未知查询结构化失败。

实施：

1. 将 Definition validation 从固定 StatusId 白名单改为字段/Tag 驱动。
2. 将 `ActiveStatus + AdditionalStatuses` 合并为单一 Runtime map。
3. 合并 OperationId 去重、Handle 查询、Clear、Dispel、SourceInvalid、turn expiry 和 public snapshot 路径。
4. 保留现有 AttackUp、DoT、Break 的可观察叠层/刷新/驱散合同。

失败矩阵：空/非 `Status.*` ID、Tag 不匹配、无效分类、无效 EffectKind、错误 GE duration、重复 OperationId、无 ASC、死亡目标、未知查询、Remove failure。

### PATCH-01B — 可重复 Break

唯一结果：同一目标 `>0→0` 发布一次，恢复后第二次 `>0→0` 再发布一次；同一 ActionId replay 不重复。

实施：

1. 移除 `bBreakResultPublished` 及 `AlreadyPublished` 生命周期闩锁语义。
2. 由本次 Toughness before/after 边沿与权威 ActionId 决定 Break。
3. 验证每个独立 Break 均恰好提交一次 Break Status 与一次 Delay。
4. Reset/重建清空 battle-local 事务状态，不跨地图保存 Runtime 标记。

失败矩阵：初始已为零、未命中弱点、伤害未归零、重复 ActionId、死亡目标、Battle Finished、恢复但未再次归零、重建时零值。

### PATCH-01C — 完整行动值模型

唯一结果：A/B/C 在运行中发生 Speed Up、Slow、Advance、Delay 和 Break Delay 后，下一行动者与后续频率按行动距离重排；当前行动锁、Tie-break 和 exactly-once 保持。

实施：

1. 新增纯值行动条目、调整请求/结果类型和公开只读快照。
2. TurnManager 在 Initialize 绑定每个 ASC 的 Speed delegate，Reset/Finish/失效时安全解绑。
3. 以最小 Remaining 推进行动时间；替换数组环形游标和 skip-once Break map。
4. 将 Break Delay 适配到通用 Delay 请求。
5. 保持现有 TurnStarted/TurnEnded/ActionResolved 消费者合同和 BattleEpoch/TurnSequence 单调性。

失败矩阵：零/NaN Speed、无效 Ratio、重复 OperationId、旧 Epoch、未知/死亡目标、Finished、当前行动期间 Speed 变化、相同 Remaining、连续 Advance/Delay、唯一剩余角色、全部失效。

### PATCH-01D — 回归与归档

唯一结果：fresh Development Editor Build、Status/Break/Turn 定向 Automation 和既有 Battle/Progression 回归通过，文档明确区分自动化、PIE 与未验证边界。

## 5. 精确候选 Allowlist

最终实施只可从以下路径选择实际需要的文件；不得因为列入候选就机械修改全部文件：

- `Source/HSR/Data/Definitions/HSRStatusDefinition.h`
- `Source/HSR/Data/Definitions/HSRStatusDefinition.cpp`
- `Source/HSR/Status/HSRStatusTypes.h`
- `Source/HSR/Status/HSRStatusComponent.h`
- `Source/HSR/Status/HSRStatusComponent.cpp`
- `Source/HSR/Data/HSRBreakTypes.h`
- `Source/HSR/Battle/HSRBattleParticipant.h`
- `Source/HSR/Battle/HSRTurnManager.h`
- `Source/HSR/Battle/HSRTurnManager.cpp`
- `Source/HSR/Battle/HSRBattleCoordinator.h`
- `Source/HSR/Battle/HSRBattleCoordinator.cpp`
- `Source/HSR/Battle/HSRBattleGameMode.h`
- `Source/HSR/Battle/HSRBattleGameMode.cpp`
- `Source/HSR/Tests/HSRCombatPatchTests.cpp`（新增）
- `Config/DefaultGameplayTags.ini`（PATCH-01A 用户扩权：仅新增 `Status.Buff.SpeedUp` 与 `Status.Buff.Shield`）
- `Source/HSR/Battle/HSRBattleGameMode.cpp`（PATCH-01A 用户扩权：仅更新 P9 `OldRemoveFailure` 原子回滚断言，以及 `Status.Unsupported` 通用根规则对应的 `InvalidDefinition` 预期）
- `docs/phase-17-patch-01-execution-plan.md`
- `tasks/active-task.md`
- `tasks/execution-result.md`
- `tasks/final-review.md`
- `worklog.md`
- `todo_plan.md`
- `learning-journal.md` 或对应 `learn/` 专题文档

除上述两条已授权 Gameplay Tag 外，若编译或测试要求修改其他范围外生产文件、Config、Content、Build.cs、新模块或插件，立即停止并申请扩权。

## 6. 用户 Editor 工作

本补丁不要求创建或修改正式 Content 资产。自动化使用 transient Definition/GE/ASC fixture。

代码与自动化通过后，用户只需在现有 Battle PIE 中验证：

1. 原有 AttackUp、DoT 与 Break Status 显示/过期没有回归。
2. 可用现有测试入口时，观察 Speed 改变后的行动条顺序；若尚无合法 UI/资产入口则标记 `NOT EXECUTABLE`，不得临时在 Widget 或 Blueprint 伪造规则。
3. 完成两次独立 Break（中间恢复 Toughness）的真实 PIE 入口若不存在，则动态路径保留 `NOT USER VERIFIED`，Automation 不冒充用户视觉证据。

## 7. Build、Automation 与证据

- `HSREditor Win64 Development` fresh Build；记录 UHT、Compile、Link、metadata 与 exit code。
- 新增 `HSR.Battle.Patch.StatusGeneric`、`HSR.Battle.Patch.RepeatableBreak`、`HSR.Battle.Patch.ActionValue` 定向 Automation。
- 回归既有 Status P9、Break P8、Turn P5/P8、Battle command P6～10、`HSR.Progression` 与 P17 UI 窄测试；如果旧测试只存在 PIE harness，记录真实边界，不伪称 Automation。
- 至少验证一个 happy path 和一个受控失败路径；记录 ActionId/OperationId、BattleEpoch、TurnSequence、RemainingActionDistance、Break event count 和 public status snapshot。
- `git diff --check`；提交前审计精确 allowlist 与 `.claude/**` 排除状态。

## 8. 风险与停止条件

- GAS Attribute delegate 回调可能发生在 TurnStarted/TurnEnded 或 GE application 的同步栈中；禁止回调中取消当前行动或重入 ResolveAction。
- 浮点行动距离只用于排序/展示，不作为持久化或跨地图真源；比较必须使用明确容差和稳定 Tie-break。
- Status 容器迁移必须先保留旧三种 Definition 的结果，再删除特殊分支；不得顺带重写完整 Status 系统。
- Break 恢复来源仍由既有权威或测试 fixture 提供；本补丁不新建 Toughness 自动恢复规则。
- 不修改 Save DTO；Status、Break 和行动距离均为 Battle-local Runtime。
- 不新增网络复制、预测、Tick、Delay 节点、CommonUI、Behavior Tree、Config 或 Content 资产。
- 任一工作包无法独立保持旧 Battle 回归时停止在首个真实错误，不继续叠加下一包。

## 9. 明确非目标

- 改进方向 4 Behavior Tree 迁移；它使用独立 `phase-17-patch-02-execution-plan.md`。
- P17-005 Frontend Shell/Router。
- 正式 Action Bar UI、Buff 图标、美术、音频、VFX 或 GameplayCue。
- 复制商业游戏的行动值常量、精确公式、文本或 UI 表达。
- 多人、SaveGame 持久化行动队列、跨地图保留 Status/ASC/Actor。

## 10. 完成定义与下一步

只有 PATCH-01A～D 均完成、Build/定向测试/回归/失败矩阵有证据、用户可执行项有真实 PIE 或明确 `NOT VERIFIED`、文档与 Git 范围审计闭合后，本补丁才可判定 `PASS` 或 `PASS WITH FOLLOW-UP`。

补丁完成后的唯一下一步是单独规划并实施 Behavior Tree Patch 02；不得把本补丁完成自动外推为 P17-005 已开始。
