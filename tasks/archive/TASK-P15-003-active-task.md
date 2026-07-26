# TASK-P15-003: Battle Return 稳定地图上下文适配

Status: `ARCHIVED / PASS`

目标：Battle Result 通过稳定 `MapId + ReturnTransform` 返回探索地图；目的 World、Pawn 放置与 Map location commit 成功后才消费；失败不得污染 Map State 或永久锁死 Encounter。

范围：Map/Battle Transition adapter、Return Consumer 与定向测试；不改 Battle 规则、Save、Config 或 Content。
