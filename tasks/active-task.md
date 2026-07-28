# TASK-P17-PATCH-03A — Frontend Boundary Contract Reconciliation

Status: `ASSET GATE READY / CODE GATE PASS / USER EDITOR EVIDENCE REQUIRED`

## Role Lock / 角色锁定

本卡尚未授权 Implementation。下一步只能进行 Task Gate 审查；Task Gate PASS 后，Implementation Agent 首次只读复述本卡并以“等待用户确认执行 TASK-P17-PATCH-03A。”结束。用户另行明确确认前不得修改 Source、Content、Config、任务报告或执行 Build/Automation。

## 当前阶段

P17-PATCH-03 Gate 0 已完成规划收束。P17-005 仅以 checkpoint `4ef49f7` 暂停归档，最终 PASS 不可用；本任务只纠正其 ScreenStack/Router 双重模块历史，不处理 `PartySlotEmpty` 或任何 Domain 模块业务。

## 唯一任务目标

把 Frontend 顶层状态固定为 `ExplorationRoot -> FrontendShell`，Character、Inventory 与 placeholder module 只存在于内部 Router history。保持现有打开、跨类型切换、Back、X、pause/input/focus 与 travel teardown 可见行为，同时删除第三个全局 module ScreenStack entry 及 depth-3 契约。

## 允许修改文件（Task Gate 已冻结）

- `Source/HSR/UI/HSRUIManagerSubsystem.h`
- `Source/HSR/UI/HSRUIManagerSubsystem.cpp`
- `Source/HSR/Tests/HSRFrontendNavigationTests.cpp`
- `tasks/execution-result.md`

只读审查确认 ScreenStack、Router 和 Widget 公共类型无需修改；它们可作为只读依赖。未列文件禁止修改；不授权 Content、Config、Domain、Map、Character、Inventory、Save、Git commit 或 push。

## ScreenStack 迁移矩阵

| 入口/事务 | 旧全局栈动作 | 新合同 | 失败恢复 |
|---|---|---|---|
| 首次打开 Hub | Push FrontendShell/Pause | 保留 Push，稳定 depth=2 | 恢复 exact root、policy、pause、focus、route |
| Character open/replace | Push/Replace Character entry | 不改全局栈；candidate Widget/VM 后只提交 Router | create/attach/focus/route 失败保留旧 module/Hub，depth=2 |
| Inventory open/replace | Push/Replace Inventory entry | 不改全局栈；candidate Widget/VM 后只提交 Router | 同上 |
| Placeholder open/replace | Push/Replace placeholder entry | 不改全局栈；candidate Widget 后只提交 Router | 同上 |
| Module Back | Pop module entry，再 Router Back | 不 Pop；Router Back 成功后释放旧 module 并聚焦 Hub | route/focus 失败保留旧 module/route |
| Hub Back / X | CloseToRoot | 保留 session close；Router 与全局栈一起回 root | pause/policy/focus/route 失败恢复旧 depth=2 session |
| Travel teardown | 循环 Pop 到 root | depth=2 时只关闭 FrontendShell；route 丢弃 | old host/token/callback 拒绝，不恢复 module route |
| Compensation helpers | 重建 Character/Inventory entry | 删除 module Push 补偿依赖；恢复 Router/Widget ownership | 只有恢复失败才置 `Inconsistent` |

测试必须把原 depth=3 断言改成 depth=2，并继续覆盖 Character/Inventory/placeholder 全排列、Back/X、直接 shortcut、create/attach/policy/pause/focus/route failure、compensation failure、external pause、duplicate binding、travel/stale host/token/callback。

## 实施合同

1. 先把所有 depth-3、module ScreenStack Push/Replace/Pop 及 compensation 依赖列为迁移矩阵。
2. Router 是 module history 唯一权威；ScreenStack 只保存 ExplorationRoot 和 FrontendShell。
3. UIManager 仍唯一拥有 Router、Widget/ViewModel、pause、input/focus 和 session transaction。
4. Module replace 必须 candidate-first；create/attach/policy/focus/Router 失败保留旧 module 与 route。
5. Back 从 module 返回 Hub；Hub Back 与 X 关闭 session 到 exact root。Frontend travel teardown 丢弃 route，旧 host/token/callback 不得恢复。
6. Public Character/Inventory facade、一次性 input binding 与外部 pause 边界保持兼容。
7. Blueprint 不拥有 route history、ScreenStack、pause 或业务规则。

## UE Editor 手动操作（仅 Code Gate PASS 后）

用户在 `/Game/UI/P17/Frontend/WBP_FrontendShell_P17` 检查/绑定 C++ module host/switcher 与表现回调；不得在 Blueprint 维护 route history 或 pause/input policy。Save All、关闭并重开 Editor；happy PIE 为 Character -> Inventory -> Back -> X，failure PIE 为在已授权测试副本/字段中临时清空一个 module class reference 并确认旧 route/session 可用，随后恢复引用并 Save All。

## 验收标准

- [ ] 所有公开稳定状态的 ScreenStack depth 只为 1 或 2，Frontend session 为 2；不存在 module 全局 entry。
- [ ] Router history 与实际 module Widget/ViewModel 一致，跨类型 replace、Back、X、重复/同帧输入正确。
- [ ] 每个候选失败阶段保留旧 route/module/session；补偿失败才进入 `Inconsistent`。
- [ ] travel teardown、old host/token/callback、external pause 和 input binding 回归通过。
- [ ] fresh `HSREditor Win64 Development` Build 与完整 `HSR.UI.FrontendNavigation` Automation 通过。
- [ ] 用户 Save All/reopen、happy/failure PIE 和两目标分辨率证据如实记录；缺失项标 `NOT VERIFIED`。

## 当前门禁

Task Gate=`PASS`，Implementation commit=`494f700`，Code Gate=`PASS / ASSET GATE READY`。当前只等待用户完成 allowlisted WBP 检查、Save All/reopen、happy/failure PIE 与两分辨率证据；没有授权继续 PATCH-03B。

## Git 交付

当前未授权 commit 或 push。后续每个角色只提交自己的实际产物，使用项目固定 commit message，并精确 stage allowlist 文件。
