# TASK-P15-003 Execution Result

Status: `ARCHIVED / PASS`

- Battle/Exploration Return DTO 携带稳定 MapId，MapSubsystem 负责 package 与 authority World 校验及最终位置提交。
- Return placement-before-consume；重复消费为 `AlreadyConsumed`。
- 仅胜利永久解决 Encounter；战败/中断与 return failure 均可回滚重试。
- Return commit failure 有限重试后清理；普通 Teleport 与 Battle Return 双向互斥；source/target/null TravelFailure 与 CoreTicker timeout 均能清理事务。
- 最终 Build 11/11；`HSR.Map`、`HSR.BattleReturn` exit 0；diff-check 通过。
- 用户同会话 PIE：战败→同 Encounter 重试→胜利→第三次 resolved reject；两次 Map.B Return 均 exactly-once。
