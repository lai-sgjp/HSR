# TASK-P9-005：只读状态 UI/Debug 与阶段回归

状态：`COMPLETE — PASS WITH FOLLOW-UP`

## 唯一结果

事件驱动的最小状态 Debug/UI 视图显示 StatusId、分类、层数、RemainingTurns 和 Add/Refresh/Trigger/Dispel/Expire 结果；UI 只读消费 Runtime 快照，不保存 Handle、不递减、不轮询、不使用 Widget Tick。隐藏/销毁 UI 不改变规则，委托成对解绑。

## 范围

Implementation 只可最小扩展既有 Battle ViewState/ViewModel/Widget、Status Runtime/GameMode Development debug 接缝与 `tasks/execution-result.md`；实际精确路径须先只读确认并在复述中列明。禁止 Phase 10 完整 UI、输入焦点、行动条、Config/Content 未授权改动、Tick/Timer、Save/网络。

用户负责状态显示资产和 BP/WBP 绑定、保存重开；Implementation 不代创建。缺失 UI 不得改变规则。

## 验证

Add/Stack/Refresh/DoT Trigger/Break/Dispel/Expire/Reset/Finished/Actor与Widget销毁、两轮 PIE 均事件更新且无重复绑定；回归 P9-004 `38/2`、P9-003 `36/2`、P9-002 `16/2`、P9-001 `28/2`，零失败。Fresh Build 覆盖 UHT/C++/Link/metadata；静态审查 UI 只读、无 Tick、解绑和 Handle 不泄漏。

## 首次响应门禁

执行者首次只读复述唯一结果、精确白名单、User 资产、事件/解绑、矩阵与 Build/PIE Gate，最后写：

`等待用户确认执行 TASK-P9-005。`

用户确认前不得实施；不授权 Phase 10 或 Git 写操作。
