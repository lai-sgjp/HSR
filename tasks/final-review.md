# TASK-P17-PATCH-01C Final Re-review 3

## 审查对象

- 任务编号：`TASK-P17-PATCH-01C`
- 任务名称：Complete Action-Distance Turn Model
- 审查角色：Independent Prompt Reviewer + Safety Reviewer
- 审查日期：2026-07-27
- 本轮修订提交：`e920be8`（并复核全部 PATCH-01C 实现提交 `ee6e70b`、`5af130e`、`57d2c0e`、`e920be8`）

## 独立证据

- 已对照 `tasks/active-task.md` 的 Frozen contract、Required matrix、Evidence gate 和上一轮结论 `766c336`，审查完整累积实现、测试代码、`e920be8` diff 与日志。
- `e920be8` 仅修改 allowlist 内的 `HSRTurnManager.h/.cpp`、`HSRCombatPatchTests.cpp` 与 `tasks/execution-result.md`；`git diff --check e920be8^ e920be8` 通过。
- `Saved/Logs/HSR.log` 于 2026-07-27 12:50 实际运行 `Automation RunTests HSR.Battle.Patch`：ActionDistance、RepeatableBreak、StatusGeneric 均为 Success，最终 exit code 0。该日志证明新增 ASC callback 测试被执行，但不等于完整 matrix 已通过。
- Implementation 报告 Development Editor Build 通过；本审查未重复运行。PIE 未运行。`learn/SaveSystem.md` 和 `.claude/**` 未被审查暂存或提交。

## 已确认的真实新增覆盖

- `WITH_DEV_AUTOMATION_TESTS` seam 只暴露纯值 distance 和 binding count，未加入反射、UI 或 Shipping API，范围符合任务卡。
- ActionDistance 实际对非当前参与者调用 ASC Speed Up，断言 Base 反比变化；对当前 actor 调用 ASC Speed 变更，断言当前 actor 未被取消且 Remaining 未在 Resolve 前变化；fixture 的两个 delegate binding 已断言存在。
- admitted-alive 继续共享 internal request admission；非法 Kind/Ratio 的不消费、未知 Target 的有效 ID 消费与重放 DuplicateOperation 仍有可执行断言。

## 仍未被真实可执行 case 覆盖的阻断

| 严重度 | Required matrix 项 | 当前证据缺口 | 最小可验收补充 |
|---|---|---|---|
| Blocking | 当前 actor Advance/Delay、Speed 后 pending 固化、低 recharge 下反向顺序 | 仅有 current Delay 0.3 和 pending 数量；没有 Advance，未验证 Base=100→80 后 55/110，也没有 `Delay→Advance=0`、`Advance→Delay=30`。 | 对 current actor 提交 Advance/Delay，触发真实 Speed callback 后 Resolve，逐项断言 Base、Remaining、pending 顺序和 exact 结果。 |
| Blocking | TurnEnded 同步 Speed callback 与 lifecycle budget | 当前 Speed 修改发生在 Resolve 前；没有 TurnEnded listener 触发 Speed，未验证 callback 在完整 TurnEnded 后、recharge 前生效，也未计数 TurnEnded/TurnStarted。 | 绑定 lifecycle recorder 和 TurnEnded listener，断言每次 Resolve 恰好一个 End、仍有候选恰好一个 Start、sequence 仅随 Start 增长。 |
| Blocking | A/B/C 距离、tie 与速度频率 | fixture 仅二人；未验证初始 Base/Remaining、lexical ParticipantId epsilon tie，或多次 Resolve 后高速角色频率更高。 | 使用三参与者受控 fixture，记录顺序和距离，覆盖 exact tie 与 epsilon 边界。 |
| Blocking | 非当前 B Speed Up / C Slow 比例换算与下一行动者 | 只对一个非当前角色验证 `NewBase/OldBase=0.5`；未验证 `NewRemaining=OldRemaining*NewBase/OldBase`、C Slow 或 next actor。 | 分别触发 B/C 的 ASC 回调，断言 EffectiveSpeed/Base/Remaining、current 不变及下一个 actor。 |
| Blocking | Advance/Delay 全部边界与 exactly-once | 未覆盖 Advance 0/0.25/1、Delay 0/1、非当前目标、重复 Accepted 操作的完整 state/lifecycle 零变化。 | 参数化边界矩阵，比较 old/new Result 和 manager snapshot。 |
| Blocking | 数值、拒绝与零变更 | 未实际触发 NaN/Inf Speed callback（日志无 `SpeedRejected`）；InvalidRatio/UnknownTarget 只断言 Result/ID，未比较 Base、Remaining、pending、current/next、epoch、sequence、delegate 和 lifecycle 前后快照。旧 Epoch 未重放为 Duplicate；没有死亡目标、Finished、唯一存活或无候选。 | 将所有拒绝类构造为 runtime case，断言 structured result/log、ID 消费语义、完整零变更快照和无事件。 |
| Blocking | binding 生命周期与 Init 原子回滚 | 已有 nth-bind failure injection、binding count seam 但没有调用；没有断言第 N 绑定失败后此前 handle 解绑、Reset、未发出首个 TurnStarted；没有 Reset/reinitialize 后 stale ASC callback 零副作用。 | 在测试中设置 N=1/合适值，统计 bindings/lifecycle；Reset 后改变旧 ASC Speed 并比较快照。 |
| Blocking | Break `+1.0 Base` 与 deferred-defeat | RepeatableBreak 仍仅证明 Delay accepted 和旧 PATCH-01B 计数，未读取距离/pending 证明精确 `+Base`；未单独覆盖 admitted same-frame lethal、already-dead、replay、Reset ActionId reuse 的 action-distance 结果。 | 在 Break 前后读取 target distance/pending，断言一次且仅一次 +Base；覆盖四类 deferred-defeat 边界。 |
| Blocking | 运行时审计 | 仅 InvalidRequest/非有限 Speed 的代码有日志，且本轮只出现 InvalidRequest；Accepted、Duplicate、InvalidEpoch/Target、Defeated、Finished、ArithmeticFailure 未记录 OperationId、old/new Speed/Base/Remaining、current/next、sequence。 | 统一记录每次请求结果；Automation 断言代表性 accepted/reject record 的字段齐全。 |

## 审查结论

`REVISE`

实现的核心路径与新增 runtime seam 均可编译并有真实 Patch Automation PASS，但 Required matrix 仍有上述未执行的关键场景。尤其当前 pending 顺序、TurnEnded 同步顺序、数值选择、绑定回滚与 Break 的精确距离尚无可验证证据，因此不得因 Build 或三项 Patch PASS 归档。修订继续限定在 allowlist；补齐后重跑 Development Editor Build、`Automation RunTests HSR.Battle.Patch` 和 `git diff --check`，再交独立复审。

## Git 交付

- 本审查提交仅包含 `tasks/final-review.md`。
- 未暂存或提交 `learn/SaveSystem.md`、`.claude/**` 或任何生产代码。
