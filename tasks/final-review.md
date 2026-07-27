# TASK-P17-PATCH-01C Final Re-review 4

## 审查对象

- 任务编号：`TASK-P17-PATCH-01C`
- 任务名称：Complete Action-Distance Turn Model
- 审查角色：Independent Prompt Reviewer + Safety Reviewer
- 审查日期：2026-07-27
- 实施提交：`ee22849`（并复核所有 PATCH-01C 实现提交）

## 独立验证

- 完整 diff 仅涉及 allowlist 内的 `HSRTurnManager.h/.cpp`、`HSRCombatPatchTests.cpp`、`tasks/execution-result.md`；用户本地 `learn/SaveSystem.md` 与 `.claude/**` 未被暂存或提交。`git diff --check` 通过。
- 本审查独立运行 Development Editor Build：`HSREditor Win64 Development ... -WaitMutex -NoHotReload`，exit code `0`。仅出现 UE 5.6 关于非首选 VS toolchain 与 AIModule 未来 API 的外部警告，无本项目编译错误。
- 本审查独立运行 `UnrealEditor-Cmd ... -ExecCmds="Automation RunTests HSR.Battle; Quit"`，exit code `0`。`Saved/Logs/HSR.log`（2026-07-27 13:22 UTC）显示 ActionDistance Baseline、CurrentPending、LifecycleOrdering、NumericAndBinding、RequestMatrix、ThreeParticipant、RepeatableBreak、StatusGeneric、MapContract 均为 Success，最终 `TEST COMPLETE. EXIT CODE: 0`。
- 已逐项审查日志中的 ActionDistanceCase 数值：CurrentPending=55/0/30、Lifecycle Starts=2 Ends=1 Recharge=50、ThreeParticipant Base=100/50/200 Counts=5/11/2、Runtime NaN/Inf 拒绝、全部 request result、正常/致死 Break 的 `+1.0 Base`。

## 已满足的冻结矩阵

- CurrentPending 的冻结距离、逐项 clamp、TurnEnded 同步 callback/recharge/lifecycle、A/B/C Base/tie/18 次频率、B SpeedUp/C Slow 的进度比例和 next actor 均有真实可执行断言。
- Advance/Delay 的 0/0.25/0.3/1 边界、NaN/Inf ratio、未知/死亡/Finished/overflow 的结构化结果与原子性，以及初始化/运行时 Speed 的 finite/nonfinite、nth-bind rollback、Reset、stale epoch、Finish unbind 都有可执行覆盖。
- Break 复用统一 Delay 路径；正常、缓存 replay、Reset 后 ActionId 重用、already-dead 和 same-frame admitted-alive lethal 均在 RepeatableBreak 中验证了计数或距离。
- 每个通用请求结果现在输出包含 Reason、OperationId、old/new speed/base/remaining/pending、current/next、epoch/sequence 的结构化记录，最新 Automation 日志实际涵盖所有 result enum。

## 剩余问题

| 严重度 | 文件/证据 | 问题 | 最小修订 |
|---|---|---|---|
| Blocking | `Source/HSR/Tests/HSRCombatPatchTests.cpp` — RequestMatrix | 冻结契约要求 OperationId 在 battle-local 域中跨 Kind/Target 全局 exactly-once。现有 duplicate 仅以同一 Kind、同一 Target 重放；没有真实 case 证明“先以 Target A/Advance 接受，再以同一 ID 提交 Target B/Delay”仍返回 DuplicateOperation 且两名参与者、pending、sequence/lifecycle 均零变更。 | 在 RequestMatrix 添加一条跨 Kind/Target replay，并对双方 snapshot、pending、epoch、sequence 和 lifecycle 做前后比较。 |
| Blocking | `Source/HSR/Tests/HSRCombatPatchTests.cpp` — NumericAndBinding | Reset 后确实广播旧 ASC，但 manager 当时为空；reinitialize 后测试的是手动调用旧 epoch + 新 ASC，而非旧 ASC 的真实 attribute delegate 广播。FinishBattle 也仅检查 binding count，未在完成后广播旧 ASC 并比较状态。故“Reset/reinitialize/Finish 后旧 ASC callback 零副作用”尚无完整可执行证据。 | reinitialize 成 Fresh participants 后广播旧 `BindB` ASC，比较 Fresh snapshot、epoch、sequence 和 lifecycle；FinishBattle 后同样广播当前旧 ASC，断言 Finished state、binding count 和所有快照不变。 |

## 审查结论

`REVISE`

当前实现已通过独立 Build 和完整 `HSR.Battle` Automation，绝大多数严格矩阵已有真实数值证据；但上表两项都是任务卡明示的 exactly-once 与 delegate 生命周期边界，尚不能由代码存在或 binding count 替代。完成这两条最小 Automation 修订、重跑相同 Build/Automation 与 `git diff --check` 后，再次独立复审。

## Git 交付

- 本审查提交仅包含 `tasks/final-review.md`。
- 未提交任何生产修订或用户本地文件。
