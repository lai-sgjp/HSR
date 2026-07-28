# TASK-P17-PATCH-03A Execution Result

Status: `IMPLEMENTED / CODE GATE EVIDENCE READY / USER EDITOR GATE NOT VERIFIED`

## 实际修改

- `UHSRUIManagerSubsystem` 不再为 Character、Inventory 或 placeholder module 向全局 ScreenStack 提交 Push/Replace/Pop。全局稳定状态只保留 root depth 1 与 FrontendShell depth 2。
- Module open/replace 继续使用 candidate-first Widget/ViewModel、policy、focus 和 Router publication；Router failure 显式恢复旧 route、policy 与旧模块 focus。
- Module Back 根据 Router active module 关闭对应实例并 Router Back，不再依赖全局 top ScreenId；Hub Back/X 仍关闭整个 session 到 exact root。
- 强制 host teardown 的 module cleanup 不再误 Pop FrontendShell；只有 Shell/session cleanup 可以修改全局栈。
- 删除已失去用途的 Character/Inventory ScreenRequest builders 和 Push compensation helpers。
- Automation 的 depth-3 断言全部改为 depth 2，并新增 Character -> Inventory route-submit failure 的 route/ownership/focus/depth 保留断言。

## 验证证据

- 首次 sandbox Build 在读取 `C:\Users\Lai\AppData\Local\UnrealEngine\Intermediate\Build\UnrealBuildTool.Env.BuildConfiguration.xml` 时因权限失败；未进入源码编译。获批重跑后成功。
- 最终 `HSREditor Win64 Development -NoHotReload -WaitMutex`：UHT 成功，`HSRUIManagerSubsystem.cpp` 编译，`UnrealEditor-HSR.lib/.dll` 链接与 metadata 成功，exit code 0。MSVC 14.51 非 preferred 与既有 AIModule deprecation warning 保留为非阻断 warning。
- 第一次新增焦点补偿测试运行暴露 route-submit failure injection 未经过 UIManager seam，`CrossTypeReplace` 失败；其余 10 项成功。修订三条 module open 路径统一调用 `SubmitFrontendRoute` 后重跑。
- 最终 `Automation RunTests HSR.UI.FrontendNavigation`：发现 11 项，`Success=11`、`Fail=0`，Editor-Cmd exit code 0。
- `git diff --check` 在最终审计执行；结果记录在下方收口。

## 未验证 / Editor 待办

- `/Game/UI/P17/Frontend/WBP_FrontendShell_P17` 的 module host/switcher、Save All、Editor reopen、Character -> Inventory -> Back -> X happy PIE、受控缺失 module class failure PIE、1920x1080 与 1280x720 均为 `NOT VERIFIED`。
- Physical controller、Standalone、Packaged、Shipping 继续 `NOT VERIFIED`。
- 本任务不处理 Character read model 的 `PartySlotEmpty`；该问题属于 PATCH-03B。

## Git 与范围

- 未修改 Content、Config、Domain、ScreenStack、Router 或 Widget 类。
- 未 commit、未 push。用户既有 Character/Enemy/Map/AI/`.claude`/学习文件保持隔离。

## 2026-07-28 USER PROVIDED PIE evidence

- Selected Viewport PIE successfully logged `OpenPause Success Stack=2`、`CharacterDetail Open Success Stack=2`、`Inventory Open Success Stack=2` and `Inventory Close Success Stack=2`. This dynamically proves the happy navigation path uses the corrected two-entry global ScreenStack.
- Exploration input context was re-added after close and the user reported the navigation itself was normal.
- The returned viewport screenshot was in Wireframe. The same log proves `Set new viewmode: Wireframe` occurred before `OpenPause Success`, so this is an Editor ViewMode change rather than Frontend close/input restoration corruption. User was instructed to restore Lit with `Alt+4` or the View Mode menu.
- Character initialization still logs `SelectionResult=6 / PartySlotEmpty`; this remains PATCH-03B and does not invalidate 03A navigation evidence.
- Exact 1920x1080/1280x720 labels、Editor reopen persistence and the controlled missing-module-class failure remain `NOT VERIFIED`.
- User explicitly deferred the 1920x1080 and 1280x720 checks. They remain `USER ACCEPTED / NOT VERIFIED` follow-ups rather than task PASS evidence.
- A later supplied log contains only `OpenPause Success Stack=2` followed by return to Exploration. It has no Character open、Map request or structured missing-class result, so it does not prove the controlled missing-module-class matrix. It also again records `Set new viewmode: Wireframe` before opening the Hub.

## Asset Gate X-key revision

- User screenshot proves `IMC_FrontendNavigation` correctly maps Escape only to `IA_UI_PauseBack` and X only to `IA_UI_CloseToRoot`; the earlier duplicate-mapping diagnosis was withdrawn.
- Repeated real PIE showed Character remained after the missing Map module attempt, but X did not close to Exploration. The log only showed a later `CharacterDetail Close Success`, consistent with Escape/Back.
- Root cause: Frontend uses `FInputModeUIOnly`, so the focused UMG receives keyboard events while the PlayerController Enhanced Input close handler is not a reliable X path. Shared `UHSRScreenWidget::NativeOnKeyDown` handled only Escape/Gamepad Back and ignored X.
- The exact task allowlist was expanded to `HSRScreenWidget.h/.cpp`. Owned ScreenWidgets now consume X and submit the existing UIManager `CloseFrontendToRoot` transaction; Esc behavior and child Blueprint close functions remain unchanged.
- First revision Build failed in UHT because adding a base `RequestCloseToRoot` UFUNCTION conflicted with existing Shell/ModuleRoot UFUNCTIONs. The base helper was renamed to a private non-UFUNCTION `SubmitCloseToRoot`; final UHT and build then succeeded.
- Final focused Automation exercises the full UIOnly X transaction: open Character at global depth 2, route X through an owned ScreenWidget, assert exact root depth 1、closed Router history and released pause. Final `HSR.UI.FrontendNavigation` remains 11/11 Success, 0 Fail; final Development Editor Build compiled ScreenWidget/tests, linked lib/dll and wrote metadata successfully.
- Corrected real PIE after this revision is still required. The modified `Content/Input/IMC_FrontendNavigation.uasset` is user-owned Editor provenance and is not staged by Implementation.
