# TASK-P17-PATCH-01C Final Review

## 审查对象

- 任务编号：`TASK-P17-PATCH-01C`
- 任务名称：Complete Action-Distance Turn Model
- 审查角色：Independent Prompt Reviewer + Safety Reviewer
- 审查日期：2026-07-27

## 审查输入与独立证据

- 已阅读 `.agents/agents.md` 的 Automatic Role Handoff 规则、`tasks/active-task.md`、`tasks/review-template.md`、`tasks/execution-result.md` 及 commit `ee6e70b` 的完整 diff 和相关 Coordinator/Status/Turn 调用链。
- `git show --stat ee6e70b` 显示仅修改 `HSRBattleParticipant.h`、`HSRTurnManager.h/.cpp`、`HSRBreakTypes.h`、`HSRCombatPatchTests.cpp`、`tasks/execution-result.md`；均处于 allowlist，未触及既存用户改动 `learn/SaveSystem.md` 或 `.claude/**`。
- 独立执行 `git diff --check ee6e70b^ ee6e70b`，通过。
- `Saved/Logs/HSR.log` 记录 2026-07-27 的受控运行：`Automation RunTests HSR.Battle.Patch` 发现并运行 3 项测试，`ActionDistance`、`RepeatableBreak`、`StatusGeneric` 均为 Success，最终 exit code 0。该日志证明自动化实际运行过，但不能弥补下列缺失断言。
- Implementation 报告声称 Development Editor Build 成功；本审查未重新运行 Build。PIE 未运行，且本任务不需要新的用户可见入口。

## 范围与实现审查

- 单一目标、allowlist 和无 Tick 的边界符合任务卡；旧 Break skip-once 容器已从 TurnManager 删除，Break 入口目前转发至统一 Delay 请求。
- 公式、ParticipantId tie-break、当前行动锁、delegate 的 weak ASC/epoch 基本骨架及 Initialize 绑定失败回滚 seam 均已出现于实现中。
- 但冻结契约要求的请求验证、结构化结果、deferred-defeat 权限收口、运行时可观测性以及完整 Automation 矩阵尚未满足，不能归档。

## 问题清单

| 严重度 | 文件/符号 | 问题 | 必须采取的动作 |
|---|---|---|---|
| Blocking | `Source/HSR/Battle/HSRTurnManager.cpp` — `RequestActionDistanceAdjustment` | 未验证 `Request.Kind`。经 `static_cast<EHSRActionDistanceAdjustmentKind>` 传入未知枚举值会走 Delay 分支、消费 OperationId 并修改状态，而任务卡要求非法 Kind 返回 `InvalidRequest` 且不消费 ID。 | 在任何 `ConsumedOperationIds.Add` 前显式只接受 Advance/Delay；添加非法 Kind 首次请求及同 ID 重放的零变更断言。 |
| Blocking | `Source/HSR/Battle/HSRTurnManager.cpp` — `MakeAdjustmentResult` | 结果对象在 mutation 后才读取 Participant，并把 `OldBase == NewBase`、`OldRemaining == NewRemaining`；accepted Advance/Delay 无法报告冻结契约要求的真实 old/new 快照。`NextParticipantId` 也始终复制 current，无法成为有意义的结构化快照。 | 在验证后、写入前捕获 old snapshot，在成功写入后捕获 new snapshot；明确 pending 的前后计数与 current/next 语义，并用测试验证实际差异与 duplicate/reject 零变化。 |
| Blocking | `Source/HSR/Battle/HSRTurnManager.h/.cpp` — public `bAllowPendingDeferredDefeat` 参数 | 通用公开入口和 `ConsumeBreakDelay` 均可由任意调用方传 `true` 绕过死亡目标拒绝，未实现“仅 Coordinator 当前同步事务可保留 admitted-alive 资格”的边界。 | 将该资格收口为 Coordinator 的窄转发路径（不得进入 DTO 或普通请求）；普通调用者即使传入等价输入也必须拒绝死亡目标，并补 same-frame lethal/已死亡/重放回归。 |
| Blocking | `Source/HSR/Tests/HSRCombatPatchTests.cpp` — `FHSRActionDistancePatchTest` | 新测试仅覆盖 current Delay、一次 duplicate、Resolve 和旧 epoch，未覆盖 Required matrix：A/B/C 行动频率、Speed Up/Slow 比例换算、当前 Advance/Delay 的有序 pending 与 Speed 变化、ratio 0/边界/NaN/Inf、epsilon tie、唯一/无合法参与者、Reset/旧 callback、部分绑定失败回滚、delegate count，以及 Break `+1.0 Base` 的数值证据。现有 RepeatableBreak 只验证“Delay accepted”，未验证距离增量。 | 以受控 runtime Automation 扩充矩阵；必要时只添加任务卡允许的 `WITH_DEV_AUTOMATION_TESTS` 纯值 seam（距离/pending/delegate count/nth bind failure）。每类断言 Base、Remaining、pending、current/next、epoch、sequence、lifecycle 次数与旧 callback 零副作用。 |
| Risk | `Source/HSR/Battle/HSRTurnManager.cpp` — `HandleSpeedChanged`、调整入口 | 非有限 Speed callback 直接静默返回，调整请求也没有记录 OperationId、old/new Base/Remaining、current/next、sequence 和接受/拒绝结果；不满足任务卡的结构化记录与运行时证据要求。 | 对非法 Speed 及每次调整生成可审计的结构化日志/测试记录，不得改变 state 或 lifecycle；将其纳入 Automation 断言。 |

## 审查结论

`REVISE`

实现的基础方向正确，且现有三项 Automation 有真实 PASS 日志；但存在一个可触发的非法枚举状态写入、错误的 old/new 结果快照、deferred-defeat 权限未收口，以及绝大多数强制测试矩阵缺失。修复必须限定在现有 allowlist 内，不得改动 `learn/SaveSystem.md` 或 `.claude/**`。修订后需要重新执行 Development Editor Build、`Automation RunTests HSR.Battle.Patch`、`git diff --check`，再进行独立复审。

## Git 交付审查

- 本次审查提交仅包含本文件。
- 未暂存或提交任何既存用户/本地改动。

## 归档与下一步

- 活动卡不可归档。
- 唯一下一步：由原 Implementation Agent 在同一 allowlist 内按上述最小修订修复并重新提交证据；随后重新触发独立 Review。
