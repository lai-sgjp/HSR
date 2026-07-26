# TASK-P15-002：A↔B 普通地图旅行事务

Status: `ARCHIVED / PASS`

唯一结果：普通地图旅行执行 `Preflight -> Pending -> OpenLevel -> authority World/Arrival validation -> Pawn placement -> Commit/Consume`；失败不污染已提交位置，并可有限重试、取消及重新请求。

范围：Map types/subsystem、ArrivalPoint/ArrivalConsumer、Map Automation 和用户 A/B 地图接线；不修改 Battle/Save/Config。

最终资产：用户已将测试用 Broken Arrival 恢复为 `Arrival.FromA`，删除临时失败入口并保存、重开确认正常。
