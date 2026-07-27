# TASK-P17-PATCH-01C Task Gate Review

## 审查对象

- 任务编号：`TASK-P17-PATCH-01C`
- 任务名称：Complete Action-Distance Turn Model
- 审查角色：Prompt Reviewer + Safety Reviewer（仅实施前 Task Gate）
- 审查日期：2026-07-27
- 结论：`REVISE`

## 审查输入与证据等级

- 一级（仓库事实）：`.agents/agents.md` 第 38～44 节、`tasks/active-task.md`、`tasks/review-template.md`、`docs/phase-17-patch-01-execution-plan.md`、`docs/phase-roadmap-0-20.md` 改进方向 3。
- 一级（当前代码）：`HSRBreakTypes.h`、`HSRBattleParticipant.h`、`HSRTurnManager.h/.cpp`、`HSRBattleCoordinator.h/.cpp`、`HSRCombatPatchTests.cpp`，以及 TurnManager 的 Coordinator、Status 和既有测试消费者。
- 一级（前置边界）：PATCH-01B 已归档提交及其 same-frame deferred defeat、Break Status、Break Delay exactly-once 合同。
- 本轮是实施前契约审查；没有 Implementation 产物、Build、Automation 或 PIE 证据，也不将其标记为已完成。

## 已通过项

- 单一结果成立：只迁移 battle-local TurnManager 到行动距离模型，不提前实现 UI、资产、Save、网络、AI、Behavior Tree 或 P17-005。
- 所有权方向正确：ASC Speed 是数值真源，TurnManager 是行动距离与下一行动者唯一 authority，Coordinator 只路由请求。
- 公式、非当前参与者 Speed 比例换算、当前行动锁、Resolve 后补距、稳定 ParticipantId Tie-break、无 Tick、Reset/Finish 清理等总体方向正确。
- Break Delay 迁移到通用 Delay、移除旧 `PendingBreakDelayActionIds`/skip-once，可避免两套延后机制并存。
- 候选 allowlist 足够完成本任务；目前无需新增生产文件、Build.cs、Config、Content、UI 或反射接口。
- Automation 与 PIE 分工合理：数值矩阵由受控 runtime Automation 证明，PIE 只回归现有 Battle 行动不中断和 Break，不为测试临时创建资产或 UI。

## 必须修订的问题

| 严重度 | 契约位置/代码事实 | 问题 | Task Card 必须冻结的修订 |
|---|---|---|---|
| Blocking | Frozen model：Advance 下限 0；当前行动锁；Resolve 后增加 Base | 当前行动者被选中后 Remaining 已为 0。若行动中收到 Advance，按现文字先 clamp 到 0，效果会永久丢失；Delay 则会保留并在 Resolve 补 Base 后生效，二者不对称。 | 明确当前行动者的调整作用于“下一次补距后的余额”。建议为当前行动者维护纯值 pending adjustment：Resolve 时先增加最新 Base，再按请求顺序应用 pending Advance/Delay（每一步 Advance clamp 到 0）；或冻结数学等价方案。必须给出“当前行动中 Advance 0.25”和“Delay 0.3 后 Speed 改变”的精确预期，禁止丢失请求。 |
| Blocking | 通用请求描述；现有 `FHSRTurnDelayRequest` 只有 ActionId/Target | DTO/结果尚未自包含：缺少精确 enum、结果原因、OperationId 消费时点和去重域。实现者无法唯一决定旧 Epoch/invalid target 是否占用 ID、Advance 与 Delay 是否共享 ID 域、Break ActionId 如何映射。 | 冻结一个 battle-local 通用 DTO（至少 `BattleEpoch, OperationId, TargetParticipantId, Ratio, Kind`）和结构化结果 enum/record。规定：空 ID/非法 Ratio/非法 Kind 是 `InvalidRequest` 且不占用；其余带有效 ID 的请求在首次观察时按 battle-local 全局 OperationId 域消费，即使随后为旧 Epoch、未知/死亡、Finished，也不能换条件重放；重复返回 `DuplicateOperation` 且零变化。Break 使用 `OperationId=ActionId, Kind=Delay, Ratio=1.0, BattleEpoch=current`。若希望不同策略，必须在卡中同等精确地冻结。 |
| Blocking | Speed 规则与 required matrix 的 zero/NaN Speed | `10000/max(Speed,1)` 没有定义 NaN/Inf 的行为，也没有定义运行中非法 Speed 回调是否修改旧状态。连续 Delay 也可能让 Remaining 溢出。 | 冻结数值合同：初始化的非 finite Speed 必须使 Initialize 原子失败并清理绑定；有限 `Speed <= 1` 统一 clamp 为 1（或明确另一规则）。运行中非 finite Speed 回调必须结构化记录并保持旧 Speed/Base/Remaining 完全不变；所有 Base/Remaining 运算先检查 finite，溢出时请求失败且事务性零变化。测试 `0, negative, NaN, +Inf`。 |
| Blocking | ASC delegate lifecycle/reentrancy；当前 Initialize/Reset 没有 Speed 绑定 | 缺少回调身份与陈旧回调判定、绑定失败回滚、Finish 是否立即解绑，以及 TurnEnded 同步栈中 Speed GE 过期时的确定顺序。 | 卡中要求每个 ParticipantId 保存 ASC weak identity + delegate handle + bound epoch；Initialize 在首次 TurnStarted 前完成全部绑定，任一失败原子 Reset；回调同时核对 manager state、BattleEpoch、ParticipantId 和 ASC identity。`FinishBattle` 与 `Reset` 幂等解绑并清空 handle。冻结顺序：TurnEnded 同步回调（包括 Status 移除导致的 Speed 变化）完成后，再使用最新 Base 给行动者补距，再选择/广播下一 TurnStarted。Speed 回调只改纯值、不广播 lifecycle、不调用 Resolve/Advance，选择过程加重入保护或延迟纯值处理。旧 ASC/旧 Epoch callback 零副作用。 |
| Blocking | 浮点 Tie-break、TurnSequence、所谓“回合预算” | 仅写“明确容差”不够；用 `IsNearlyEqual` 作排序关系可能非传递。且项目没有独立 RoundBudget 类型，“回合预算一致”无法验收。 | 冻结常量和算法：例如 `DistanceEpsilon=1e-4`（或项目选择值），先求严格 finite 最小值并统一减法/近零 snap；候选集合使用 `abs(Remaining-Min)<=epsilon`，只在集合内按 ParticipantId 选，不用近似比较器排序。定义预算为：每次成功 TurnStarted 仅 `TurnSequence +1`；一次合法 Resolve 恰好一个 TurnEnded，若仍有候选则恰好一个后续 TurnStarted；调整请求不改变 TurnSequence/lifecycle 计数。无合法候选才 Finished；唯一合法候选可连续再次行动。 |
| Blocking | Break Delay migration；PATCH-01B deferred defeat | 只写“适配为 Delay”尚不能保证旧 skip-once 数据完全删除、也未冻结同帧死亡准入如何穿过新通用 API。 | 明确 `ConsumeBreakDelay`/两个旧集合和 skip-once 分支必须删除或成为不保存状态的窄兼容转发，生产中只能有一次 `+Base*1.0`。新调整入口默认拒绝死亡目标；仅 Coordinator 当前同步事务中 `bTargetAliveAtAdmission && PendingDefeatedParticipantId==Target` 时允许该目标，且此资格不得储存在 TurnManager/DTO、不得用于普通请求。Break Status 与 Delay 各 exactly-once，Delay 不因 Status 成功而重复，也不得同时执行旧 skip。补 same-frame lethal、already-dead、replay、Reset/reused ActionId 回归。 |
| Risk | `GetOrderedParticipants()` 现有 Coordinator/UI 消费 | 行动距离模型不再拥有固定“按 Speed 排序的循环数组”；现有 getter 名称可能误导，但 Coordinator 仍用它查 actor/构造快照。 | 保持 participants 存储可查且不改变现有 UI DTO；任务卡说明该 getter 只返回稳定 participant registry，不承诺未来行动顺序。若需新只读测试快照，只能是非 UI 的纯值/`WITH_DEV_AUTOMATION_TESTS` seam；正式行动条快照需停下扩权。 |
| Risk | Required matrix 与测试入口 | 当前矩阵没有明确检查 TurnEnded 回调造成 Speed 变化、调整当前行动者、操作拒绝后用同 ID 重放、Initialize 半绑定失败。 | Automation 补齐这些四类，且分别断言 Base/Remaining、current/next、epoch、sequence、delegate count/旧 callback 零副作用。回归至少覆盖 `HSR.Battle.Patch.RepeatableBreak`、现有 `HSR.Battle.Patch`、Turn/P8/P9；不存在的 Automation 不得写成通过，PIE harness 只验证现有入口。 |

## Safety / 范围结论

- 不需要扩展 allowlist；上述修订均可由 Coordinator 只修改 `tasks/active-task.md` 完成。
- `HSRBreakTypes.h` 虽然命名偏 Break，但已在 allowlist 且现有请求位于此处，可做最小通用 DTO 迁移；若实现时决定新建专用类型文件，必须停止申请授权。
- `HSRBattleParticipant.h` 可保存纯值行动字段；delegate handle/绑定身份更适合由 TurnManager 私有条目拥有，避免把生命周期状态暴露成 participant 数据。
- 禁止为了 PIE 构造 Speed/Advance/Delay 而新增 GameplayTag、GE、DataAsset、Blueprint 开关、Widget 或 Content。
- 工作树中现有 `learn/SaveSystem.md` 与 `.claude/**` 不属于本 Reviewer 和 PATCH-01C，必须保持未暂存、未提交。

## 审查结论

`REVISE`

活动卡方向与文件范围可行，但上述六项 Blocking 合同必须先写入 `tasks/active-task.md`，否则当前行动中的 Advance、幂等拒绝语义、非法 Speed、同步 delegate 顺序、浮点候选和 Break 延后都存在多种不兼容实现。此结论不需要用户扩权；应自动交给 Coordinator 在现有任务范围内修订，再由本 Task Gate 复审。

**Implementation Agent 现在不得开始只读契约复述。** 只有 Task Gate 复审为 `PASS` 后，Coordinator 才能触发 Implementation 的只读复述；实际实现仍须用户对 `TASK-P17-PATCH-01C` 单独确认。

## Git 交付

- Reviewer 只允许提交本文件 `tasks/final-review.md`。
- 不提交生产代码、活动卡、计划、状态文档、用户文件或 `.claude/**`。
