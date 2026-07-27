# TASK-P17-PATCH-01C Final Re-review 2

## 审查对象

- 任务编号：`TASK-P17-PATCH-01C`
- 任务名称：Complete Action-Distance Turn Model
- 审查角色：Independent Prompt Reviewer + Safety Reviewer
- 审查日期：2026-07-27
- 本轮修订提交：`57d2c0e`

## 独立证据

- 按 Automatic Role Handoff 阅读 `tasks/active-task.md`、`tasks/execution-result.md`、上一轮结论 `ea40df7`，审查 `57d2c0e` 完整 diff、当前 TurnManager/Coordinator/Automation 代码与运行日志。
- `57d2c0e` 仅修改 `HSRTurnManager.h/.cpp`、`HSRCombatPatchTests.cpp`、`tasks/execution-result.md`，全部在 allowlist；`git diff --check 57d2c0e^ 57d2c0e` 通过。
- `Saved/Logs/HSR.log` 在 2026-07-27 12:45 实际运行 `Automation RunTests HSR.Battle.Patch`：ActionDistance、RepeatableBreak、StatusGeneric 均为 Success，最终 exit code 0。日志还实际出现了 InvalidRequest 的结构化警告。Implementation 报告称同一修订的 Development Editor Build 通过；本审查未重新运行 Build，PIE 未运行。
- `learn/SaveSystem.md` 和 `.claude/**` 仍为未提交本地文件，未被本次审查修改或暂存。

## 上轮阻断复核

- `ConsumeAdmittedBreakDelay` 现在构造固定 `Delay(1.0)` 请求并调用 `RequestActionDistanceAdjustmentInternal(..., true)`；普通 Break 与 admitted-alive Break 共享 ID 去重、有限值检查、current pending 最大 recharge 预演、old/new snapshot 和写入路径。不存在第二套 Delay 写入。该阻断关闭。
- 非有限 Ratio、未知 Target 与同 ID 重放新增了真实 Automation 断言；日志证明该测试实际运行。非法 Kind、非法 Ratio 不消费 ID，未知 Target 的有效 ID 重放返回 DuplicateOperation，代码路径符合冻结的消费规则。
- 非有限 Speed callback 已添加拒绝日志，但本轮 Automation 没有实际触发该 callback；运行日志没有 `SpeedRejected` 证据。

## 真实性阻断清单

| 严重度 | 文件/符号 | 缺失或问题 | 必须采取的动作 |
|---|---|---|---|
| Blocking | `Source/HSR/Tests/HSRCombatPatchTests.cpp` — `FHSRActionDistancePatchTest` | A/B/C 初始行动距离、稳定 tie-break 和“高速角色在足够多次 Resolve 后多行动”没有测试。 | 构造三参与者受控 fixture，逐次记录 Base/Remaining、TurnStarted 顺序和 sequence，断言 lexical tie 与高速频率。 |
| Blocking | 同上 | 没有测试 A 当前行动期间 B Speed Up、C Slow 的比例换算，也没有 `TurnEnded` 同步 callback 改变当前 actor Base 后再 recharge 的固定顺序。 | 使用 ASC Speed 变更和 TurnEnded listener，断言 A 不被取消、B/C 的 `OldRemaining * NewBase / OldBase`、A 使用新 Base recharge、恰好一次后继 TurnStarted。 |
| Blocking | 同上 | Advance/Delay 仅测试 current Delay 0.3；没有 0/0.25/1 边界、非当前目标、当前 Advance、Speed 后 pending 固化，以及 `Delay→Advance` / `Advance→Delay` 的逐项 clamp 数值。 | 逐个断言 old/new Remaining 和 pending 顺序；覆盖任务卡给出的 Base=100→80 精确示例及两个反向组合的 0/30 结果。 |
| Blocking | 同上 | 没有 epsilon 候选冻结、唯一存活者连续行动、无合法候选 Finished、Finished 请求、死亡目标、Reset/new battle、旧 ASC callback、delegate count 或 nth-bind failure 原子回滚的 Automation 证据。 | 使用任务卡许可的 `WITH_DEV_AUTOMATION_TESTS` seam，逐项断言 epoch、sequence、lifecycle count、binding count 和旧 callback 零副作用。 |
| Blocking | 同上及 RepeatableBreak 测试 | Break 回归只证明 Delay 被接受；没有证明它实际造成恰好 `+1.0 Base`，也没有 admitted-alive same-frame lethal、already-dead、replay、current-target 分支的距离和 exactly-once 证据。 | 对 Break 前后 Remaining/pending、Status/Delay 计数和 turn budget 做数值断言；补齐四类 deferred-defeat 边界。 |
| Blocking | `RequestActionDistanceAdjustmentInternal` 与 Automation | 非有限 Speed callback 没有真实触发或日志证据；非法 Ratio/未知 Target 的测试仅检查 Result/ID 复用，没有比较 Base、Remaining、pending、current/next、epoch、sequence、delegate/lifecycle 前后快照，不能证明“零变更”。 | 在 Automation 中触发 NaN/Inf Speed callback，检验 `SpeedRejected` 日志及完整快照不变；对所有拒绝类在请求前后比较冻结字段和事件计数。 |
| Risk | `HSRTurnManager.cpp` — request logging | 仅 InvalidRequest 和非有限 Speed 写日志；Accepted、Duplicate、InvalidEpoch、InvalidTarget、DefeatedTarget、Finished、ArithmeticFailure 未记录 OperationId、old/new Base/Remaining、current/next、sequence，未满足任务卡的运行时审计要求。 | 为每次请求结果生成统一结构化记录，至少包含任务卡指定字段，并让 Automation 断言关键拒绝/接受记录存在。 |

## 审查结论

`REVISE`

本轮已关闭 admitted-alive 双路径和请求拒绝重放的代码问题，且三项 Automation 有真实 PASS 日志。然而 Required matrix 是任务卡的验收条件，不是可选扩展；当前 `ActionDistance` 测试仍只覆盖少量请求路径，无法证明行动距离模型的数值、delegate 生命周期、候选算法和 Break/deferred-defeat 回归。活动卡不得归档。实施者必须在现有 allowlist 内补齐上表受控证据，重新运行 Development Editor Build、`Automation RunTests HSR.Battle.Patch` 与 `git diff --check`，然后再次独立复审。

## Git 交付

- 本审查提交仅包含 `tasks/final-review.md`。
- 未暂存或提交用户的 `learn/SaveSystem.md` 或 `.claude/**` 改动。
