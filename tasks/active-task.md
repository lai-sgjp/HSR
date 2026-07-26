# TASK-P15-004：Save v5 地图状态投影

Status: `PLANNED / AUTHORIZED`

## 唯一验收结果

当前 MapId、稳定位置、Region/Teleport 解锁与最小探索 Flag 可通过 candidate-first Save v5 捕获和恢复；旧 v1-v4 保守迁移为空 Map 状态；非法 Map、Teleport、Transform 或重复 ID 拒绝且不污染现有 Runtime；重复 Load 不重复通知或旅行。

## 边界

- Save 只捕获、验证并提交纯值 Map DTO，不直接调用 OpenLevel。
- 不保存 Actor、World、Subsystem、ASC、GE Handle、Widget 或 UObject 指针。
- 不进入 Phase 16 的备份、损坏文件恢复或通用迁移框架。
- 修改前先核对现有 Save schema、candidate-first 事务和 MapSubsystem snapshot API。

## 验证

- Development Editor Build。
- `HSR.Map` 与 `HSR.Save` 定向 Automation。
- 用户 Editor Gate：在 Map B 非默认位置保存，关闭并重开 Editor，Load 后恢复 Map/位置/解锁；重复 Load 无重复状态变化。
