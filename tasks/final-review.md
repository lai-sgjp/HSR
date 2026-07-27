# TASK-P17-PATCH-01C Final Re-review

## 审查对象

- 任务编号：`TASK-P17-PATCH-01C`
- 任务名称：Complete Action-Distance Turn Model
- 审查角色：Independent Prompt Reviewer + Safety Reviewer
- 审查日期：2026-07-27
- 本轮修订提交：`5af130e`

## 独立证据

- 已按 Automatic Role Handoff 重读 `tasks/active-task.md`、`tasks/execution-result.md`、上轮审查 `50cceae`，并审查 `5af130e` 完整 diff、TurnManager 与 Coordinator 调用链。
- `git show --stat 5af130e` 仅涉及 `HSRBattleCoordinator.cpp`、`HSRTurnManager.h/.cpp`、`HSRBreakTypes.h`、`HSRCombatPatchTests.cpp`、`tasks/execution-result.md`，均在 allowlist 内；`git diff --check 5af130e^ 5af130e` 通过。
- `Saved/Logs/HSR.log` 的 2026-07-27 12:39 受控运行实际发现并执行 `HSR.Battle.Patch.ActionDistance`、`RepeatableBreak`、`StatusGeneric`，三项均 Success，最终 exit code 0。Implementation 报告称 Development Editor Build 成功；本审查未重新执行 Build。PIE 未运行。
- `learn/SaveSystem.md` 与 `.claude/settings.json`、`.claude/settings.local.json`、`.claude/statusline-command.sh` 仍是未提交本地文件；未被本轮审查修改或暂存。

## 上轮阻断复核

- 未知 Kind：已在消费 OperationId 前显式限制为 Advance/Delay；新增测试实际验证 InvalidRequest 后使用同一 ID 的合法请求可被接受。该项关闭。
- old/new 快照：accepted 请求现在先捕获 before snapshot，并新增 old/new pending count。Base 在此类请求中不变、Remaining 的 before/after 实现已能产生不同值；但当前测试只检查 pending count，尚未给出 Remaining 的数值断言。
- deferred-defeat：普通 `RequestActionDistanceAdjustment` 与 `ConsumeBreakDelay` 已去除公开 bypass。Coordinator 通过友元私有 `ConsumeAdmittedBreakDelay` 走 admitted-alive 窄路径，公开权限绕过项关闭。

## 问题清单

| 严重度 | 文件/符号 | 问题 | 必须采取的动作 |
|---|---|---|---|
| Blocking | `Source/HSR/Tests/HSRCombatPatchTests.cpp` — `FHSRActionDistancePatchTest` | Required matrix 仍未实现。当前测试只有 current Delay、duplicate、非法 Kind、Resolve 与旧 epoch；没有 A/B/C 初始距离与高速多行动、非当前 Speed Up/Slow 的比例换算、当前 Speed 在 TurnEnded 同步回调后 recharge、Advance/Delay 0/0.25/1 和有序组合、epsilon tie、NaN/Inf/未知/死亡/Finished、唯一/无候选、Reset/旧 callback、nth-bind rollback/delegate count，或 Break `+1.0 Base` 的距离数值证据。 | 在现有 allowlist 内补齐受控 runtime Automation。每类必须断言 Base、Remaining、pending、current/next、epoch、sequence、lifecycle 次数与 delegate/旧 callback 零副作用；不得仅以“测试通过”替代数值断言。 |
| Blocking | `Source/HSR/Battle/HSRTurnManager.cpp` — `ConsumeAdmittedBreakDelay` | admitted-alive 分支复制了 Delay 写入逻辑，未复用通用请求的 current pending 最大 recharge 预演及结构化结果路径。若当前目标存在接近有限上限的已有 pending，该路径可以接受一项随后在 Resolve 时才发生的算术失败，违背已接受 pending 不得延迟失败的原子保证；也削弱 Break 映射为同一通用 Delay 请求的契约。 | 将 private admitted-alive 资格作为同一私有通用事务的狭窄授权，仅放宽目标资格，保留相同的 Ratio=1、预演、old/new 结果、dedupe 与零变更规则；补 same-frame lethal、already-dead、replay 及 current-target admission 覆盖。 |
| Risk | `Source/HSR/Battle/HSRTurnManager.cpp` — `HandleSpeedChanged`、调整入口 | 非有限 Speed callback 仍静默返回，调整入口也未生成任务卡所要求的 OperationId、old/new Base/Remaining、current/next、sequence、接受/拒绝结构化记录。现有 Automation 日志只证明测试成功，不能提供这些运行时审计数据。 | 为每次调整和非法 Speed 拒绝写入可审计的结构化记录或等价受控测试记录；确保没有 state/lifecycle 副作用，并将其列入自动化断言。 |
| Risk | `Source/HSR/Tests/HSRCombatPatchTests.cpp` | 旧 Epoch 只断言一次 `InvalidEpoch`，没有按照任务卡验证“被拒绝后用同一 OperationId 重放”必须得到 `DuplicateOperation` 和零变更；其它拒绝类型同样没有重复语义证据。 | 为旧 Epoch、未知/死亡目标和 Finished 至少补充同 ID replay、状态快照与 lifecycle 零变化断言。 |

## 审查结论

`REVISE`

本轮已关闭上轮三项直接代码缺陷，且 Automation 有真实复跑 PASS 证据；但任务卡把 Required matrix 明确列为验收要求，当前覆盖仍不足，且 admitted-alive 的复制分支未保持通用事务的算术原子性。因此活动卡仍不可归档。修订必须继续限制在当前 allowlist，且不得改动或提交任何用户本地文件。完成后重新运行 Development Editor Build、`Automation RunTests HSR.Battle.Patch` 与 `git diff --check`，再触发独立复审。

## Git 交付

- 本审查提交仅包含 `tasks/final-review.md`。
- 未暂存或提交 `learn/SaveSystem.md` 或 `.claude/**`。
