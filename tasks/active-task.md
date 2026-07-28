# TASK-P17-005 — Frontend Shell、Router 与统一快捷导航

Status: `PAUSED / P17 CHECKPOINT — CODE GATE PASS / PARTIAL USER PIE / FINAL TASK PASS NOT AVAILABLE`

## Role Lock / 角色锁定

Coordinator 已完成契约修订，Independent Reviewer=`PASS — CONTRACT GATE ONLY`，Teacher=`PASS`，Implementation Gate=`PASS`。下一角色仅为 `Implementation Agent / 低级执行模型`；首次只能读取本卡、复述并等待用户对 `TASK-P17-005` 的单独确认。不得自行扩展到 Party、Map、Challenge、Save 的真实业务数据、命令或事务。

## 当前 Phase

Phase 17。P17-001～004 已归档；P17-004 Build、Automation 与用户 PIE Gate 为 `PASS`。P17-005 是相邻工作包。

## 唯一任务目标

探索中通过 Esc/B/T/M/F4 打开 Pause Hub、Inventory、Party、Map、Challenge 的正确 Frontend Route；Character Detail 通过 Hub 进入现有页面。所有 Frontend 页面共同持有一次 UI pause session，Esc/Back 逐级返回，X close-to-root，重复/竞态输入零副作用；关闭后恢复 Exploration InputMode、鼠标、焦点与 IMC。

Party、Map、Challenge、Save 在本任务仅为真实可导航且明确标注“后续实现”的模块根页面，不读取、伪造或提交业务结果。

## 规划审查摘要

### 邻接性与拆分

- 复用 P17-001 ScreenStack、P17-002 pause/input/focus、P17-003 Character/Inventory 配对和 P17-004 travel teardown。
- 本任务只建立 Frontend Route/Shell 与导航入口；Character 深层养成、Inventory 分类操作、Party 编辑、Map 传送、Challenge 入场、Save 操作分别留给 P17-006～011。
- 不引入新模块、第三方依赖、Config 或 Gameplay authority。

### 所有权与数据流

- 固定状态形状：`ExplorationRoot -> FrontendShell(PauseHub) -> optional Module`。Router history 与 ScreenStack 的 Frontend 项必须一一对应；任何公开完成状态下不得只提交其中一个。
- `UHSRFrontendRouter` 只拥有纯值 route history；不保存 World、Actor、Widget、ASC 或 Slate 指针。UIManager 先执行 host/context/external-pause/travel 前置检查，前置拒绝不分配 Router token；构造合法 RouteRequest 后才分配 attempt-id。结构合法的 Success、NoOp 与后续业务拒绝都消费 token；相同正 token任何 payload均 `AlreadyProcessed`，零/负 token或非法结构为 `InvalidRequest` 且不消费，较旧正 token为 `StaleRequest`。
- `UHSRUIManagerSubsystem` 继续唯一拥有 Router、ScreenStack、Frontend pause token、Shell/Module/Widget/ViewModel 生命周期、InputMode 与 focus。Blueprint 不能绕过它提交 Router 或 ScreenStack。
- `UHSRFrontendShellWidget` 只展示当前 route、提交 navigation intent，并提供 Blueprint 表现事件；不直接 AddToViewport、SetInputMode、Pause、OpenLevel 或调用 Domain command。
- `AHSRPlayerController` 只转发 Enhanced Input navigation intent 到 UIManager；不拥有 route 或 pause。
- UIManager 成功打开 Shell 后只获取一次自身 pause token；Hub→Module 不再次暂停。Module Back 回 Hub；Hub Back 与 X 使用专用 close-session 事务，最后才释放自己的 token并恢复 Exploration。World 已暂停且无 UIManager token时返回 `ExternalPause`，不创建候选、不生成 token，任何 close/deinitialize 都不得解除外部 pause。
- 旧 `OpenPauseScreen()` 与 PlayerController `RequestOpenPauseScreen()` 保留为兼容 facade，但只能转发 `OpenFrontendModule(PauseHub)`；禁止保留第二条 pause 实现。HUD 注册参数以 `FrontendShellClass + FrontendModuleRootClass` 替换旧 `PauseWidgetClass`；旧 `PauseWidgetClass/Instance` 字段在本任务删除，Pause Hub 只由 Shell 表示。

### 原子事务与补偿矩阵

首次打开：验证 initialized/host/exploration/exact-root/no travel/no external pause → 创建 Shell 与可选 Module/VM 候选 → 构建 Router+ScreenStack 候选快照并验证一致 → attach → 应用 UIOnly policy → SetGamePaused(true) → 应用 focus → 发布实例、Router 和 ScreenStack。任一步失败按 `focus → pause → policy → attach → candidates → stack/router` 逆序恢复旧快照；恢复失败才标记 `Inconsistent`。

嵌套模块：验证已有合法 Shell session → candidate-first 创建/绑定 → 验证 Router+ScreenStack 候选 → attach → policy/focus → 发布；不再次调用 SetGamePaused。Character/Inventory Close 在 session 内恢复 Hub 的 UIOnly、paused 和 Hub focus，不能恢复 Exploration。

Back：Module 成功关闭并双提交到 Hub；Hub Back 调用专用 close-session。X：先构建完全关闭候选，清理 Module Widget/VM 与 Shell，双提交到 exact root，恢复 GameOnly/Exploration IMC/focus，最后只释放自身 pause；任一步失败恢复旧 session，禁止用循环 `RequestBack()` 产生半关闭状态。

失败矩阵至少覆盖 missing class、create、VM snapshot/bind、attach、Router/ScreenStack validation、policy、pause、focus、close policy/unpause 与补偿失败。每条断言 Router history、ScreenStack、实例/VM、pause token、World paused 和 resolved input policy 同时一致。

### Travel 与上下文

- Frontend UIOnly 没有合法旅行入口；P17-005 不恢复 Frontend route。授权 travel teardown 遇到 Frontend session 时原子清理为 exact root并丢弃 route，不冻结 Shell/Module descriptor；旧 host/route token/callback不得在新 host复活。
- Open 在 MapSubsystem/BattleTransitionSubsystem travel pending、无 Host、Battle control mode、非 Exploration且不存在合法 Frontend session时拒绝且零污染。
- 当前 Dialogue runtime 没有冻结的全局 active-query，本任务不臆造 Dialogue API；Dialogue Overlay 与 Frontend 竞争保留为 `NOT VERIFIED / P17-012`，不把缺少的判据伪装成已实现。

### 输入边界

- 使用六个 Digital Input Action：`IA_UI_PauseBack`、`IA_UI_Inventory`、`IA_UI_Party`、`IA_UI_Map`、`IA_UI_Challenge`、`IA_UI_CloseToRoot`，避免 Axis1D 同时按键求和值碰撞；统一放入 `IMC_FrontendNavigation`。
- Action 的 `bTriggerWhenPaused=true`（已从 UE5.6 `InputAction.h` 验证）；IA/IMC 不添加 Trigger或 Modifier，C++ 统一使用 `BindAction(..., ETriggerEvent::Started, ...)` 单次绑定。缺少单项 IA 时跳过该绑定并记录 warning，缺 Frontend IMC 时保持旧输入状态并返回受控失败，禁止 crash或半注册。
- Esc 在 root 打开 Hub，在 Module 回退、Hub 关闭；B/T/M/F4 在 root直达对应模块（history自动含 Hub）。session 内切换使用唯一的 atomic replace：history 始终 `[Hub, ActiveModule]`，ScreenStack 同步 Replace 顶层；候选失败保留旧 module；当前 module 同键为消费 attempt-token 的 NoOp。X 只执行专用 close-session。
- 手柄本卡不新增全局快捷映射；只验证 UMG 默认焦点移动、按钮激活，以及焦点落在 WBP Back button 后通过默认 Accept 激活（不是 Gamepad Back 快捷键）。若无物理设备为 NOT VERIFIED，不得用键盘 Automation 替代。

## 允许修改文件

- `tasks/active-task.md`
- `tasks/execution-result.md`
- `Source/HSR/UI/Frontend/HSRFrontendRouteTypes.h`
- `Source/HSR/UI/Frontend/HSRFrontendRouter.h`
- `Source/HSR/UI/Frontend/HSRFrontendRouter.cpp`
- `Source/HSR/UI/Frontend/HSRFrontendShellWidget.h`
- `Source/HSR/UI/Frontend/HSRFrontendShellWidget.cpp`
- `Source/HSR/UI/Frontend/HSRFrontendModuleRootWidget.h`
- `Source/HSR/UI/Frontend/HSRFrontendModuleRootWidget.cpp`
- `Source/HSR/UI/HSRUIManagerSubsystem.h`
- `Source/HSR/UI/HSRUIManagerSubsystem.cpp`
- `Source/HSR/UI/HSRScreenWidget.h`
- `Source/HSR/UI/HSRScreenWidget.cpp`
- `Source/HSR/UI/HSRScreenStackTypes.h`
- `Source/HSR/UI/HSRScreenStack.h`
- `Source/HSR/UI/HSRScreenStack.cpp`
- `Source/HSR/UI/HSRInputModeCoordinator.h`
- `Source/HSR/UI/HSRInputModeCoordinator.cpp`
- `Source/HSR/UI/HSRHUD.h`
- `Source/HSR/UI/HSRHUD.cpp`
- `Source/HSR/Player/HSRPlayerController.h`
- `Source/HSR/Player/HSRPlayerController.cpp`
- `Source/HSR/Tests/HSRFrontendNavigationTests.cpp`
- `tasks/final-review.md`（Reviewer only）

用户 Editor 资产 allowlist（Codex 不修改）：

- `Content/Input/IA_UI_PauseBack.uasset`
- `Content/Input/IA_UI_Inventory.uasset`
- `Content/Input/IA_UI_Party.uasset`
- `Content/Input/IA_UI_Map.uasset`
- `Content/Input/IA_UI_Challenge.uasset`
- `Content/Input/IA_UI_CloseToRoot.uasset`
- `Content/Input/IMC_FrontendNavigation.uasset`
- `Content/UI/P17/Frontend/WBP_FrontendShell_P17.uasset`
- `Content/UI/P17/Frontend/WBP_FrontendModuleRoot_P17.uasset`
- `Content/UI/WBP_ExplorationHUD.uasset`（仅保存 Exploration HUD 资产引用或编译持久化时）
- `Content/Blueprints/UI/BP_HSRHUD.uasset`（仅保存 HUD Class Defaults 引用时）
- `Content/Blueprints/Input/BP_HSRPlayerController.uasset`（仅保存 Input Action/IMC 引用时）

## 禁止修改内容

- 现有用户未提交的 `Content/AI/**`、Enemy/Player Blueprint、Enemy DataAsset、地图和 `learn/SaveSystem.md`。
- Config、Build.cs、uproject、Save schema、Map/Party/Challenge/Inventory/Character Domain authority。
- 删除、批量移动、Git stage/commit/push、第三方资产和新模块。
- 不把占位模块页面称为真实业务完成；不在 Widget/Blueprint 中伪造 Party、Teleport、Challenge 或 Save 成功。

## 实现步骤

1. 修正 attempt-token Router 语义和纯值测试；定义 Router/ScreenStack 一致性 helper，不暴露 Blueprint bypass。
2. 先用 Automation backend 完成统一 session 的 open/module/back/X、共享 pause 与逐级失败补偿。
3. 接入现有 Character/Inventory 的 session-aware open/close，再建立 Shell/ModuleRoot Widget 基类与 HUD class registration/GC ownership。
4. 接入 travel teardown/root fallback 与 stale host/route token测试。
5. PlayerController 接入六个 Digital Action 与常驻 Frontend IMC；核对 UE5.6 paused trigger，UIOnly 下 Back/X 仍有效，Battle/Travel/无 Host 拒绝。
6. 替换当前临时 switch 版 `OpenFrontendModule()` 与循环版 `CloseFrontendToRoot()`；不得基于临时桥接继续堆叠或把它作为最终实现。
7. 补充窄 Automation：route history、重复/竞态、pause ownership、missing class/create/attach/policy/focus failure、Back/X、teardown/stale callback。
8. Development Editor Build、`HSR.UI.FrontendNavigation` Automation、`git diff --check`。

## UE Editor 手动操作

1. 创建六个 Digital IA，全部勾选 `Trigger When Paused`；IA/IMC 不添加 Trigger或 Modifier，创建 IMC 并分别映射 Esc/B/T/M/F4/X。单次触发由 C++ `ETriggerEvent::Started` 负责。
2. 创建 `WBP_FrontendShell_P17`（父类 `UHSRFrontendShellWidget`，Is Focusable=true）：Hub tiles 包含 Character/Inventory/Party/Map/Challenge/Save，绑定纯 Route Intent；实现 Back、X 和 preferred focus；`OnRouteChanged` 只切换展示。
3. 创建 `WBP_FrontendModuleRoot_P17`（任务卡提供的 ModuleRoot C++ 父类）：至少显示模块名、`功能将在后续任务实现`、Back；不得读取 Domain snapshot或显示成功反馈。Save 只从 Hub tile进入。
4. 在 PlayerController Class Defaults 分别绑定六个 IA 与 Frontend IMC；在 HUD Class Defaults 绑定 Shell/ModuleRoot/现有 Character/Inventory class。Save All、关闭并重开后逐项确认父类与引用。
5. 1920×1080 与 1280×720 分别记录：无裁切/重叠、文本可读、焦点可见、鼠标点击、键盘路径、Back/X、关闭后 W/A/S/D、鼠标、GameOnly 与 Exploration IMC。缺类受控失败必须证明旧 UI/pause/input零泄漏。
6. 可用物理手柄时只验证默认焦点移动、按钮激活和 Back button；不可用写 `NOT VERIFIED`。Standalone/Packaged/Shipping 未执行也必须明确记录。

## 验收标准

- [ ] Esc/B/T/M/F4 在合法探索上下文进入正确 route；Character/Inventory 复用现有真实页面。
- [ ] Party/Map/Challenge/Save 根页面只显示明确的后续实现状态。
- [ ] Frontend 打开期间保持暂停；Back 分级、X 直返 root；重复/竞态零副作用。
- [ ] 外部 pause、Battle、Travel pending、无 Host、缺类与各候选失败保持旧状态且无泄漏。
- [ ] Dialogue Overlay 竞争明确记录为 `NOT VERIFIED / P17-012`，本任务不臆造 active-query API。
- [ ] 关闭后 GameOnly、隐藏鼠标、焦点和 Exploration IMC 恢复。
- [ ] Development Editor Build 与窄 Automation 明确 Success。
- [ ] 两种目标分辨率和 Editor 重开由用户提供 PIE/持久化证据。
- [ ] 未验证的物理手柄、Standalone、Packaged/Shipping 如实标注。

## 执行后文档

Implementation 只写 `tasks/execution-result.md`。工程、Editor、PIE 和 Independent Review 完成后再由 Coordinator 更新归档、`worklog.md`、`todo_plan.md` 与学习文档。

## Git 交付

本任务不执行 stage、commit 或 push；Phase 17 收尾策略保持不变。
