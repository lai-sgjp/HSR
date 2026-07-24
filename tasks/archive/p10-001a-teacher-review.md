# TASK-P10-001A Teacher Review

## Evidence

- Evidence level: `USER ANSWERED / TEACHER ASSESSED`
- Assessment result: `3/3 mastered`
- Teaching gate: `PASS`
- Follow-up answer required: No

## Q1 — TurnStarted handler 与非递归 drain

### 用户原始回答要点

用户指出，`TurnStarted` 会在 `RequestAction -> RequestActionCore -> ResolveAction -> Advance -> Broadcast` 的同步调用过程中触发。若 handler 直接调用 `RequestAction`，会在外层 depth 尚未归零、技能点提交、伤害及 defeat 等工作可能尚未完整收尾时递归重入。正确做法是只记录 key，等待 `RequestActionCore` 完全返回，且 GameplayEffect、韧性、Break、formal transaction、`PendingDefeated`、`ResolveAction` 回合推进与 depth guard 均完成后，再由最外层 drain 处理。

### 教师判定

`mastered`

用户准确理解了同步回调、事务边界、递归重入风险，以及“handler 只排队、最外层安全点非递归 drain”的设计目的。

### 非阻断校正

列举的技能点提交、伤害、defeat 等步骤不应被理解为所有分支永远采用同一套固定逐行顺序。关键契约是：`TurnStarted` 发生时，外层 `RequestAction` 仍可能没有完成全部事务、缓存、发布和生命周期收尾，因此 handler 不得重入。用户使用了“可能未完”，不构成技术错误。

## Q2 — canonical key 与 Manager Identity

### 用户原始回答要点

用户说明 canonical key 包含 Manager 指针/identity、Epoch、Sequence 与 ParticipantId：Manager identity 隔离不同实例，Epoch 隔离同一 Manager 的不同初始化周期，Sequence 区分回合，ParticipantId 区分参与者。若缺少 identity，旧或隔离 Manager 可能与新或生产 Manager 出现相同的 Epoch、Sequence 和 ParticipantId，造成错误消费，或因旧 consumed 状态误拒当前合法行动；用户同时提到了绑定来源检查。

### 教师判定

`mastered`

用户准确理解了 manager-local 数值可能跨实例碰撞，以及事件源 identity 必须与当前 bound manager 一致的生命周期隔离要求。

## Q3 — post-turn ViewState 与 terminal 边界

### 用户原始回答要点

用户指出，`Finalize` 发布 ViewState 时 `CurrentActor` 仍为 Enemy，`ResolveAction` 成功后才推进到 Player，因此必须再次发布 ViewState，使玩家技能、资源、目标及按钮恢复为正确状态。terminal 时由 `ResolveDefeat`、`FinishBattle` 与 `BattleResultReady` 接管，不应再发布命令态，否则空或错误的命令快照可能干扰结算，或制造非法输入窗口。

### 教师判定

`mastered`

用户准确区分了伤害 Resolution 发布、权威回合推进、命令 UI 刷新和终局结果真源的先后边界。

### 非阻断校正

若 `ResolveAction` 失败，也不能发布 post-turn ViewState，因为此时没有权威的新回合状态。post-turn 发布只适用于回合推进成功且战斗未终结的路径。

## Overall Assessment

`USER ANSWERED / TEACHER ASSESSED`

用户已掌握 TASK-P10-001A 的三个核心边界：

1. `TurnStarted` 同步回调只记录 pending key，避免在尚未完整收尾的 `RequestAction` 事务中递归重入，并由最外层非递归 drain 统一提交。
2. canonical key 通过 `ManagerIdentity + Epoch + Sequence + ParticipantId` 同时隔离实例、战斗周期、回合和参与者，拒绝旧 Manager 的数值碰撞事件。
3. `Finalize` 时行动者仍可能是 Enemy；成功 `ResolveAction` 至 Player 且非终局后必须再次发布 ViewState，而 terminal 路径由 BattleResult/Finished 真源接管，不再发布可操作命令态。

最终判定：`3/3 mastered`，Teacher Gate 通过，无需补答。
