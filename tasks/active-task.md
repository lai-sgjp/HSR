# TASK-P17-PATCH-01C — Complete Action-Distance Turn Model

Status: `PLANNED / TASK GATE REVIEW REQUIRED`

## Single outcome

TurnManager 使用统一行动距离决定下一行动者：运行中 Speed/Slow、Advance、Delay 与 Break Delay 均可重排未来行动，当前已开始的行动不被取消；高速角色可更频繁行动，所有请求 exactly-once，Tie-break、BattleEpoch、TurnSequence 与回合预算保持一致。

## Frozen model and ownership

- TurnManager 是 battle-local 行动距离和顺序唯一 authority；ASC Speed 是有效速度真源；Status/GAS/Coordinator 只能提交变化或纯值请求，UI 只读快照。
- 原创公式：`BaseActionDistance = 10000.0 / max(EffectiveSpeed, 1.0)`；距离越小越早行动。
- 每个参与者保存纯值 `EffectiveSpeed`、`BaseActionDistance`、`RemainingActionDistance` 与稳定 ParticipantId；不持久化、不跨地图、不保存到 SaveGame。
- 初始化时 Remaining=Base。选人前从所有合法候选 Remaining 减去最小值；零距离候选按 ParticipantId 字典序稳定 Tie-break。
- 当前行动一旦 TurnStarted 即锁定。Speed/Advance/Delay 只影响未来选择，不取消、不重播当前行动。
- ResolveAction 后，行动者 Remaining 增加当前 Base；不得简单重置并丢失超前/延后余额。
- Speed/Slow 通过 ASC Speed delegate 事件驱动，无 Tick。非当前参与者按 `NewRemaining = OldRemaining * NewBase / OldBase` 保留进度；当前参与者的新 Base 在本次行动结束补距时生效。
- 通用 Advance/Delay 请求为纯值：BattleEpoch、OperationId、TargetParticipantId、Ratio、Kind。Ratio 必须 finite 且 `[0,1]`；Advance 减 `Base*Ratio`（下限 0），Delay 加 `Base*Ratio`。
- Break Delay 适配为同一 Delay 请求、Ratio=`1.0`，保持延后一整个自身基础行动距离；PATCH-01B 的 admitted-alive pending-deferred-defeat 资格仍只限当前同步事务。
- OperationId battle-local exactly-once；无效、重复、旧 Epoch、未知/死亡目标、Finished 请求零变化并返回结构化结果。

## Frozen edge semantics

### Current actor adjustments

- 当前 actor 已锁定且 `RemainingActionDistance == 0` 时，Advance/Delay 不得丢失，也不得改变当前 actor。TurnManager 为其维护按接收顺序累计的纯值 `PendingPostActionOffset`。
- 接受请求时使用当时有效的 Base 将 Ratio 固化为绝对有符号距离：Advance 为 `-Base*Ratio`，Delay 为 `+Base*Ratio`；之后的 Speed 变化不得重新解释已接受 offset。
- Resolve 顺序固定为：完整广播 `TurnEnded`（同步 Speed 回调可更新 Base）→ 使用最新 Base recharge → 按请求顺序应用 pending offset，每次 Advance clamp 到 0 → 清空 pending → 选择下一 actor。
- 精确示例：当前 actor Base=100 时 Advance 0.25，接受后 pending=-25；即使行动结束前 Speed 令 Base=80，recharge 后 Remaining=`max(0,80-25)=55`。Base=100 时 Delay 0.3 后再变为 Base=80，则 Remaining=`80+30=110`。连续请求严格按接受顺序逐步应用。

### Generic request, result, and dedupe

- 通用 DTO 固定为 `BattleEpoch, OperationId, TargetParticipantId, Ratio, Kind`；Kind 仅为 Advance/Delay。结构化结果至少区分 `Accepted, DuplicateOperation, InvalidRequest, InvalidEpoch, InvalidTarget, DefeatedTarget, Finished, ArithmeticFailure`，并记录 old/new Base、Remaining、pending、current/next、epoch 与 sequence 快照。
- 空 OperationId、非 finite/越界 Ratio、非法 Kind 属于 `InvalidRequest` 且不消费 ID。其余具备有效 ID 的请求在 battle-local、跨 Kind/Target 的全局 OperationId 域首次观察时即消费；即使随后为旧 Epoch、未知/死亡目标或 Finished，也不能换条件重放。重复返回 `DuplicateOperation` 且零变更。
- Break 固定映射为 `OperationId=ActionId, Kind=Delay, Ratio=1.0, BattleEpoch=current`。旧 `PendingBreakDelayActionIds`、`ConsumedBreakDelayActionIds` 与 skip-once 分支必须删除；`ConsumeBreakDelay` 若保留只能是不存储状态的窄兼容转发，不得产生第二次 Delay。

### Numeric and delegate lifecycle

- 初始化 Speed 非 finite 时整个 Initialize 原子失败并解除本轮已建立的全部绑定；有限 `Speed <= 1` 统一 clamp 为 1。所有 Base/Remaining/pending 中间值先验证 finite，溢出返回 `ArithmeticFailure` 且事务零变更。
- 运行中 NaN/Inf Speed 回调必须结构化记录，并保持旧 Speed/Base/Remaining/pending 完全不变；Speed=0 或负有限值按 clamp=1 处理。
- 每个绑定身份固定为 `(ParticipantId, weak ASC identity, delegate handle, bound BattleEpoch)`。Initialize 必须先完成全量验证，再在首次 TurnStarted 前建立全部绑定；任一失败均原子 Reset。回调同时核对 manager state、epoch、ParticipantId 与 ASC identity。
- `FinishBattle`、`Reset` 与重新 Initialize 必须幂等解除全部绑定并清空 handle；旧 ASC/旧 epoch callback 零副作用。Speed callback 只更新纯值，不广播 lifecycle、不调用 Resolve/Advance/Delay；选择/resolve 期间使用重入保护或延迟纯值处理。
- 同步顺序固定为：当前 actor 锁定 → 广播完整 `TurnEnded` → 处理由 Status 移除等触发的同步 Speed 回调 → 用最新 Base recharge 并应用 pending → 选择 next → 恰好一次 `TurnStarted`。

### Candidate selection and lifecycle budget

- 固定 `DistanceEpsilon=1e-4`。从 eligible participant 求严格 finite 最小 Remaining，统一减去该最小值并在 `abs(value)<=epsilon` 时 snap 为 0；候选集合使用 `abs(Remaining-Min)<=epsilon`，仅在集合内按 ParticipantId 字典序选择。禁止使用 `IsNearlyEqual` 作为排序比较器。
- 每次成功 `TurnStarted` 恰好令 `TurnSequence +1`。每次合法 Resolve 恰好一个 `TurnEnded`；仍有候选时恰好再有一个 TurnStarted，无候选才 Finished；唯一存活 participant 可连续再次行动。调整请求和 Speed callback 均不得改变 TurnSequence 或 lifecycle 次数。
- `GetOrderedParticipants()` 仅表示稳定 participant registry/诊断视图，不承诺未来行动顺序，也不得改变现有 UI DTO。若测试需要距离快照，只能添加非 UI 的纯值 `WITH_DEV_AUTOMATION_TESTS` seam；正式行动条快照不在本任务范围，需停下扩权。

### Deferred-defeat admission boundary

- 通用调整入口默认拒绝死亡目标。仅 Coordinator 当前同步事务满足 `bTargetAliveAtAdmission && PendingDefeatedParticipantId==Target` 时，可在转发 Break Delay 时保留 admitted-alive 资格；该资格不得进入 DTO、TurnManager 状态或普通请求。
- Break Status 与 Delay 各自 exactly-once；Status 成功不得导致 Delay 重复，Delay 也不得同时执行旧 skip。same-frame lethal、already-dead、replay、Reset 后复用 ActionId 必须回归。

## Exact candidate allowlist

- `Source/HSR/Data/HSRBreakTypes.h`
- `Source/HSR/Battle/HSRBattleParticipant.h`
- `Source/HSR/Battle/HSRTurnManager.h`
- `Source/HSR/Battle/HSRTurnManager.cpp`
- `Source/HSR/Battle/HSRBattleCoordinator.h`
- `Source/HSR/Battle/HSRBattleCoordinator.cpp`
- `Source/HSR/Battle/HSRBattleGameMode.h`（仅复用/最小扩展 `WITH_DEV_AUTOMATION_TESTS` fixture；禁止新属性/开关）
- `Source/HSR/Battle/HSRBattleGameMode.cpp`（仅 fixture 与现有 P8/P9/Turn harness）
- `Source/HSR/Tests/HSRCombatPatchTests.cpp`
- `tasks/execution-result.md`

Implementation 不得修改 active-task、计划、PROJECT_STATE、worklog/todo/learn、Config、Content、Save、UI 或网络代码。新增生产文件、Build.cs、Gameplay Tag/GE/DataAsset 或 Turn UI 快照文件需停止申请授权。

## Required matrix

- 专项补齐四类 Task Gate 场景：当前 actor 的 Advance/Delay（含随后 Speed 改变）；`TurnEnded` 同步回调导致 Speed 改变；请求被拒后以同一 OperationId 重放并验证已消费语义；Initialize 部分绑定失败的原子回滚。
- 上述四类分别断言 Base/Remaining/pending、current/next、epoch、sequence、delegate count 与旧 callback 零副作用；不得把不存在或未运行的 Automation 记为通过。

- A/B/C 初始速度产生可解释的行动距离与稳定顺序；高速角色在足够多次 resolve 中可比低速角色多行动。
- 当前 A 行动期间 B Speed Up、C Slow：A 不变，后续 Remaining 按进度比例换算，下一行动者正确。
- Advance 0/0.25/1、Delay 0/0.3/1、连续组合及边界值；重复 OperationId 全部零变化。
- Break Delay 只接受一次并表现为 `+1.0 Base`；不得再保留独立 skip-once 容器或双重延后。
- 相同 Remaining 使用 ParticipantId Tie-break；浮点比较使用明确容差，不依赖 UObject 地址或数组偶然顺序。
- 当前行动者 Speed 变化不取消当前行动；Resolve 后使用新 Base 补距。
- 无效 Ratio、NaN/Inf、旧 Epoch、未知/死亡目标、Finished、全部失效、唯一存活角色均结构化且无重复/丢失行动。
- Reset/新 Battle 清理 delegate、OperationId 与行动距离；旧 ASC Speed callback 零副作用；不得泄漏绑定。
- same-frame deferred defeat 仍通过 PATCH-01B 回归；Status/Break 不因行动值迁移回归。

## Evidence and user gate

- Development Editor Build。
- `HSR.Battle.Patch.ActionDistance` 真实/受控 runtime Automation；回归 `HSR.Battle.Patch`、适用 Turn/P8/P9/Battle tests。
- 日志记录每次 OperationId、old/new Speed、Base/Remaining、current/next、TurnSequence、接受/拒绝结果。
- PIE 复用既有 Battle development harness；若无合法 Speed/Advance/Delay 资产入口，结构化 harness 证明 runtime，用户只验证现有 Battle 行动不中断与 Break 回归，不得临时创建 Content/Blueprint 规则。
- `git diff --check` 与 allowlist/provenance 审计。

## Explicit non-goals and stop conditions

不实现正式行动条 UI、动画、VFX、网络预测/复制、SaveGame、AI、Behavior Tree、P17-005 或新 Content/Config。不得复制商业游戏常量/公式。需要白名单外生产消费者、反射接口、资产或扩大 Ratio/公式契约时停止请求最小授权。
