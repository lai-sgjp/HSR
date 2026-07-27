# TASK-P17-PATCH-01B — Archived Active Task

Final status: `PASS / ARCHIVED / 2026-07-27`

唯一结果：同一目标每次 `Toughness > 0 -> 0` 均产生独立 Break；恢复后再次归零可再次触发；Replay、0→0、死亡、Finished、Reset/stale BattleId 均保持精确零副作用。

最终授权包含 RepeatableBreak Automation GameMode factory，以及同帧致死事务的 Status/Delay admitted-alive/pending-deferred-defeat 默认拒绝参数；禁止临时 Health 写入、Turn 排序/Delay 算法变化和普通死亡绕过。

Implementation commits：`60ee700`、`418b8a1`、`2412d9b`、`e5756c9`、`d1d3fc2`；Reviewer final `4a074c5`。完整 REVISE/BLOCKED 历史保留于审查归档与 Git。
