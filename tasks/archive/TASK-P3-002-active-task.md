# TASK-P3-002：事件驱动交互提示观察层与生命周期专项

> 最终处置（2026-07-18）：Independent Reviewer commit `d93dbe8725ce735eae3aa0c5ae38a057a99a716e` 结论为 `PASS WITH FOLLOW-UP`；最终 Implementation/A4 commit `20ab55590f0e192c003f15cdad263cf303636d50` 已按用户授权由主 Agent 代理精确提交六个 Source 与 `tasks/execution-result.md`，并清除 PlayerController 文件尾空行。Build/UHT、用户 PIE、OnUnPossess 归属笔误、历史 CE、资产追认、同 Git 身份与代理提交边界继续保留。

> 状态：`COORDINATOR HANDOFF READY / WAITING FOR INDEPENDENT REVIEWER`
> 创建日期：2026-07-18  
> 唯一活动任务；P3-001 已以 Reviewer `PASS WITH FOLLOW-UP` 归档，Phase 3 仍为 `Not verified`。

## Role Lock / 首次确认门禁

低级执行模型当前且仅能担任 `Implementation Agent / 低级执行模型`。不得切换为 Coordinator、Reviewer、Teacher、资产作者或架构负责人，不得修改本任务卡、项目状态或长期文档，不得自行扩大文件/资产范围。

首次收到本卡后只能只读复述，必须覆盖：任务编号、唯一可见结果、精确允许文件、数据流、UI 观察边界、Delegate 初始快照/解绑、HUD 重建、Re-Possess、连续 PIE、反射/GC/Tick、用户 Editor 操作、失败路径、验证证据和明确非目标；最后逐字写：

`等待用户确认执行 TASK-P3-002。`

随后停止。只有用户在复述后另发明确确认，才可调用工具或实施；用户授权“进入 P3-002”不替代本任务编号的二次确认。

## 当前 Gate 与历史边界

- P3-001 归档材料：
  - `tasks/archive/TASK-P3-001-active-task.md`
  - `tasks/archive/TASK-P3-001-execution-result.md`
  - `tasks/archive/TASK-P3-001-final-review.md`
- P3-001 Reviewer commit `e99078deff05291f7ef71b873b2244ec0a017718`，结论 `PASS WITH FOLLOW-UP`。
- P3-001 最终修订 commit `64ac9770248899f2980262bb28019835896980c0`，主 Agent 根据用户明确代理授权代为提交，精确六文件。
- P3-001 follow-up 继续有效：修复后完整 Build/UHT 控制台未保存；最终 PIE 主要为用户证据；`OutOfRange` 未动态命中；`9356ad0` 的历史 Git Gate 偏差和同一 Git 身份证据等级不得改写。
- P3-002 不返工 P3-001 Gameplay 协议，只建立事件驱动 Prompt 观察层与生命周期闭环。

## 唯一可验收结果

在 `Map_Phase1_Exploration` 中，玩家进入灰盒对象范围时，现有单实例探索 HUD 通过 `InteractionComponent candidate event → InteractionViewModel snapshot → UserWidget presentation event` 显示接口提供的 `FText` 提示；离开、正常删除、弱目标失效或观察 Pawn 被解除 Possess 时立即隐藏。HUD 重建、同会话 Re-Possess 和至少两轮连续 PIE 后，旧 Widget/ViewModel 不再接收回调，新观察链只有一个初始快照和一套有效 Delegate，不重复显示、不访问失效对象。

## 固定数据流与职责

```text
P3-001 Overlap
→ UHSRInteractionComponent::OnCandidateChanged
→ UHSRInteractionViewModel（绑定/解绑 + 当前快照）
→ UHSRUserWidget C++ presentation event
→ WBP_ExplorationHUD 只更新 Text/Visibility

HUD Create/Rebuild 或 Controller Possess
→ RefreshInteractionObserver(Current Pawn)
→ 先 Teardown 旧 Component
→ Bind 新 Component
→ 恰好广播一次当前 Prompt snapshot

UnPossess / Widget Destruct / HUD Remove
→ Teardown
→ RemoveDynamic/句柄解绑
→ 清空弱观察目标与 Prompt
```

- `UHSRInteractionComponent`：候选与 Prompt 真源入口；继续不依赖 Widget。
- `UHSRInteractionViewModel`：观察一个 InteractionComponent，读取共享 Interface Prompt，发布只读显示状态；不提交交互命令、不修改 Gameplay。
- `UHSRUserWidget`：持有 ViewModel，连接 C++ 观察层与 Blueprint 表现事件；不 Cast 灰盒 Actor。
- `AHSRHUD`：单实例 Widget 创建/销毁与刷新当前 Pawn 观察目标；不决定 Prompt 文本或候选。
- `AHSRPlayerController`：在 OnPossess/OnUnPossess 通知 HUD 刷新/清空观察目标；不直接绑定候选 Delegate。
- `WBP_ExplorationHUD`：只消费 `Visible + FText`，更新布局；不执行 Interaction、不读写 Component。

## 精确允许修改文件

### Implementation Agent C++

- `Source/HSR/UI/HSRInteractionViewModel.h`
- `Source/HSR/UI/HSRInteractionViewModel.cpp`
- `Source/HSR/UI/HSRUserWidget.h`
- `Source/HSR/UI/HSRUserWidget.cpp`
- `Source/HSR/UI/HSRHUD.h`
- `Source/HSR/UI/HSRHUD.cpp`
- `Source/HSR/Player/HSRPlayerController.h`
- `Source/HSR/Player/HSRPlayerController.cpp`
- `Source/HSR/Interaction/HSRInteractionComponent.h`
- `Source/HSR/Interaction/HSRInteractionComponent.cpp`
- `tasks/execution-result.md`

InteractionComponent 只允许增加 Prompt 只读快照/候选事件所需的最小 API 或诊断，不得改变 P3-001 注册、选择、执行和失败语义。`Source/HSR/HSR.Build.cs` 已具备 UMG/Slate 依赖，默认只读；真实编译证明缺依赖时停止请求扩权。

### 用户 Editor 资产作者

- `Content/UI/WBP_ExplorationHUD.uasset`

只读核对，默认禁止修改：

- `Content/Blueprints/UI/BP_HSRHUD.uasset`
- `Content/Blueprints/Character/Player/BP_HSRExplorationCharacter.uasset`
- `Content/Input/IA_Interact.uasset`
- `Content/Input/IMC_Exploration.uasset`
- `Content/Blueprints/Exploration/BP_HSRGrayboxInteractable.uasset`
- `Content/Maps/Map_Phase1_Exploration.umap`
- `Content/UI/WBP_AttributeDebug.uasset`

## 禁止修改

- `Config/`、`HSR.uproject`、`Source/HSR/HSR.Build.cs`（除非先扩权）、`Source/HSR/GAS/`、Character/Exploration/Framework、P3-001 Interface/Types/Graybox Actor。
- 除 `WBP_ExplorationHUD` 外全部 Content；不得创建重复 `WBP_InteractionPrompt`，除非 Coordinator 先修订卡并由用户确认。
- 不删除或改写 P3-001 归档、reviewer commit、follow-up 或历史日志。
- 禁止 Widget Tick、ViewModel Tick、Timer 轮询、每帧 Cast、世界扫描、`GetAllActorsOfClass`。
- 禁止 Git stage/commit/push/reset/clean；角色提交在 Reviewer 前由 Coordinator 另行 Gate。

## 固定实现要求

### Delegate、快照与去重

1. ViewModel `Initialize/Observe` 必须先对旧 Component 执行 Teardown，再绑定新 Component。
2. 重复绑定同一 Component 必须幂等；不得叠加 `AddDynamic` 或重复句柄。
3. 每次成功建立新观察关系后恰好推送一次当前快照；不得依赖下一次 Overlap 才显示已有候选。
4. 候选真正变化才更新 Prompt；同值重复事件不得制造重复 UI 更新。
5. Candidate 有效且实现 Interface 时，用 `IHSRInteractableInterface::Execute_GetInteractionPrompt` 读取 `FText`；不得 Cast `AHSRGrayboxInteractable`。
6. Candidate 为 null、失效、不实现接口或观察目标清空时，Prompt 状态必须为隐藏且文本清空。
7. Widget/ViewModel teardown 必须解绑原 Component，再清弱引用；旧实例销毁后回调计数保持零增长。

### HUD、Re-Possess 与连续 PIE

- `ShowExplorationHUD()` 创建 Widget 后立即刷新当前 Pawn 观察关系；保留现有 HUD 单实例不变量和 Phase 2 Attribute ViewModel 行为。
- `RemoveExplorationHUD()` 必须触发 Widget/ViewModel teardown 后再释放实例。
- HUD 重建 next-tick 路径不保留旧 Widget 的弱/裸回调；新 Widget 只获得一次 Prompt 初始快照。
- PlayerController OnPossess 在 HUD/Widget 已存在时刷新到新 Pawn；HUD 尚未创建时安全无操作，后续 Show 读取当前 Pawn。
- OnUnPossess 在 Super 调用顺序允许时清空旧 Pawn 观察关系；不得破坏 Enhanced Input Context 生命周期。
- 至少提供 Development 日志/计数区分 Component 事件、ViewModel 状态广播、当前 Widget 接收和旧实例 teardown；不建立 Phase 19 Debug 系统。

### 反射、GC 与 Tick

- 新 UObject/ViewModel、dynamic multicast、UFUNCTION/UPROPERTY 使用最小正确反射；`*.generated.h` 为最后 include，所有反射字段有安全默认值。
- Widget 对 ViewModel 使用 `UPROPERTY/TObjectPtr`；ViewModel 对 Component 使用 `TWeakObjectPtr` 或不会延长 Pawn 生命周期的等价弱引用。
- 动态 Delegate 使用 `AddUniqueDynamic/RemoveDynamic` 或明确句柄的成对生命周期；不能只依赖 GC 自动清理。
- ViewModel、Widget、HUD、InteractionComponent 不新增自定义 Tick；PlayerController 现有 Enhanced Input 引擎 Tick 不得误删。

### UI 与失败路径

- Blueprint 只接收 C++ presentation event 或只读 ViewModel 状态，设置 Prompt Text/Visibility；不得调用 `TryInteract`、Interface Execute、Register/Unregister 或修改 Candidate。
- 缺 OwningPlayer、Pawn、InteractionComponent、ViewModel、WidgetClass 或 Interface Prompt 时记录包含实例的可定位日志并安全隐藏，不崩溃。
- 对象在范围内变为 `Unavailable` 本包不自动隐藏，除非候选/Prompt 状态事件明确变化；不可为此增加 Tick。可见性语义继续以“是否存在候选 Prompt”为准，交互失败仍由 P3-001 Result 表达。
- 目标正常删除通常先 EndOverlap→候选 null→隐藏；弱失效兜底同样必须隐藏。

## 实现步骤

1. 记录工作树基线，确认 P3-001 归档和 Coordinator Markdown，不修改范围外文件。
2. 新建最小 InteractionViewModel，完成 observe/teardown、共享接口 Prompt 快照与状态变化去重。
3. 最小扩展 UserWidget，使其持有 ViewModel、在 Construct/Destruct 成对连接，并向 Blueprint 发出只读 Prompt presentation event。
4. 最小扩展 HUD，在创建、移除、重建和 Pawn 刷新时管理观察关系；保持 Attribute HUD 不变量。
5. 最小扩展 PlayerController OnPossess/OnUnPossess 通知 HUD；保持 IA/IMC、ControlMode、Move/Look/Jump 行为不变。
6. 如确有需要，最小扩展 InteractionComponent 的只读 Prompt 快照/诊断；禁止改 P3-001 Gameplay 语义。
7. 执行 fresh `HSREditor Win64 Development`，保存完整 UHT/C++/Link 输出、退出码和第一处 warning/error。
8. 只更新 `tasks/execution-result.md`，记录真实 diff、构建与未执行 Editor/PIE 项，停止交接用户。

## 用户 Editor 操作

1. 完全关闭并重开 Editor，确认新 ViewModel/UserWidget/HUD/Controller C++ 类型加载且 Output Log 无反射默认值 Error。
2. 打开 `/Game/UI/WBP_ExplorationHUD`，只添加一个最小 Prompt 容器与 Text。
3. 实现 C++ 暴露的 Prompt presentation event：Visible=true 时设置传入 FText 并显示；false 时清文本并隐藏。不得 Event Tick、Cast Graybox、GetAllActors 或调用交互业务函数。
4. 保存资产，关闭重开 Editor，核对 Parent、事件实现、HUD WidgetClass、Attribute Debug 区域和 IA/IMC 引用保持。
5. 用户本人独立提交且只提交 `Content/UI/WBP_ExplorationHUD.uasset`，回传完整 hash；不得与 C++ 作者提交混合。

## 验收矩阵

- [ ] fresh `HSREditor Win64 Development`：完整 UHT/C++/Link 输出与退出码 0；无新反射默认值 Error。
- [ ] 进入范围显示接口 `FText`，离开/删除立即隐藏；无 Widget/ViewModel Tick。
- [ ] 初次绑定当前已有候选时恰好一次快照，候选未变化不重复更新。
- [ ] 无 Pawn、无 Component、无 Candidate、Candidate 失效均安全隐藏并有可定位日志。
- [ ] HUD 重建：旧 Widget/ViewModel teardown 后回调零增长；新实例只有一次初始快照。
- [ ] 同会话 Re-Possess：旧观察关系解绑，新 Pawn 观察关系唯一；一次输入仍只执行一次。
- [ ] 至少两轮连续 PIE：无跨会话残留 Delegate、重复 Prompt、失效对象、Error、Ensure 或 GC warning。
- [ ] Move/Look/Jump、Interact、Exploration/UIOnly、Phase 2 Attribute HUD/测试入口无回归。
- [ ] Editor 重开后 WBP、HUD、角色、输入与地图引用保持。
- [ ] UI 不 Cast 具体业务 Actor、不执行 Gameplay 命令、不拥有 Candidate 真源。

## 明确非目标

- 多候选排序、距离实时重排、正式交互菜单、NPC/宝箱/传送业务。
- `Unavailable` 自动隐藏、Result toast、输入图标切换、动画、音效、VFX、正式美术。
- GAS Tags、Ability、Effect、SaveGame、Encounter、Battle、Phase 4。
- CommonUI、MVVM 插件、新模块、新 Collision Channel、Config 修改。
- 网络复制、RPC、Prediction、PlayerState ASC。
- 自动化测试框架或 Phase 19 Debug 工具。

## 扩权与停止规则

发现必须修改未列文件、创建新 Widget、改 Config/Build.cs、触碰 P3-001 Gameplay 语义或引入新依赖时，立即停止，在执行报告写明第一处证据、精确所需路径、最小变更和所需授权。Coordinator 修订任务卡且用户重新确认前不得继续。

## Reviewer 检查重点

- 精确文件/资产范围、作者和提交是否分离。
- Delegate 是否唯一绑定、初始快照一次、teardown 成对、旧实例零回调。
- HUD rebuild、Re-Possess、连续 PIE 是否有真实计数/日志而非笼统确认。
- UI 是否纯观察、无具体 Actor Cast、无业务写入、无 Tick。
- P3-001 Gameplay 与 Phase 2 Attribute HUD 是否无回归。
- Build/UHT、Editor 重开、PIE 和失败路径证据来源是否如实。

## 当前执行结果

- 已完成：Implementation commit `9fb5deb`、用户 Prompt 资产 commit `6d86e1a`、构建报告和部分 PIE 日志已存在。
- 未闭合：同 Component Observe 幂等/快照去重、Widget 独立 teardown、OnUnPossess 清理、旧实例零回调计数、HUD rebuild/Re-Possess 真实运行证据、范围外资产处置。
- 当前下一步：按下方 Coordinator 最小修订交接处理，不进入 Reviewer。
- 审查结论：未审查。

## 2026-07-18 Coordinator 只读交付核对

本节是 Coordinator Gate，不是 Reviewer 结论。当前判定为 `REVISE`。

### 已确认事实

- `9fb5debecc024f92acb055514d0d9b2c06249878` 包含任务卡允许的 P3-002 C++ 文件与 `tasks/execution-result.md`，没有 Config/GAS/Character/Exploration/Phase 4 夹带。
- `6d86e1afa7e35f72e85a18a58db1ce23b9aeddec` 只修改允许资产 `Content/UI/WBP_ExplorationHUD.uasset`。
- ViewModel 使用弱 Component、`AddUniqueDynamic/RemoveDynamic`，通过共享 Interface 读取 Prompt；Widget 通过 BlueprintImplementableEvent 表现，没有 Cast 灰盒 Actor；未新增 Widget/ViewModel/InteractionComponent Tick 或世界扫描。
- 最新日志有三轮 PIE 启动记录，且没有 `LogClass`/`LogAutomationTest` 反射错误、Ensure 或 GC warning。

### 阻断 1：OnUnPossess 未清理观察关系

`AHSRPlayerController::OnUnPossess()` 当前只移除输入 Context 并调用 `Super`，没有按任务卡调用 HUD `ClearInteractionObserverInstance()`。必须在 Pawn/组件关系仍可定位的安全顺序中清理观察者，再完成 UnPossess；不得破坏 Enhanced Input 生命周期。

### 阻断 2：重复 Observe 与初始快照不满足幂等

日志在同一帧记录重复 Observe：

- `HSR.log:1243/1252`
- `HSR.log:1371/1379`
- `HSR.log:1493/1501`

当前 `Observe()` 对同一 Component 也会 Remove→Add→再次广播 snapshot；ViewModel 没有缓存 `Visible/FText` 去重。最小修订必须：

1. 重复 Observe 同一 Component 不重复绑定、不重复发布相同状态。
2. 新 Widget 连接已存在 ViewModel 时仍恰好收到一次当前 snapshot，不能因简单 early-return 丢失状态。
3. Candidate 事件只有在 `Visible` 或 `FText` 真正变化时广播；为新 Widget 提供明确一次的强制当前快照入口。
4. 调整 HUD/ViewModel/Widget 调用顺序，确保“新观察关系一次 + 新 Widget 一次”，而不是先隐藏再显示或同帧两次相同 snapshot。

### 阻断 3：Widget 独立销毁未 Teardown Component

`UHSRUserWidget::NativeDestruct()` 当前只从 ViewModel 的 Prompt Delegate 解绑 Widget，没有调用 ViewModel Teardown。HUD 正常 Remove 会先 Clear，但 Widget 独立销毁/替换时 HUD 仍可能持有观察 Component 的 ViewModel。必须建立单实例所有权下的安全 teardown：旧 Widget 销毁后 ViewModel 与 Component Delegate 解绑，且不会影响随后 HUD 创建的新观察链。

### 阻断 4：缺少可核验实例/计数诊断

当前日志只有 Component 名称，没有 ViewModel/Widget 实例标识、状态广播计数、Widget 接收计数和 teardown 后旧实例计数。无法证明“旧实例零回调、新实例一次 snapshot”。只增加 Development 范围的最小实例标识与计数日志，不建立 Phase 19 Debug 框架，不在 Shipping 暴露测试业务。

### 阻断 5：HUD rebuild 与 Re-Possess 测试实际失败

最新日志明确包含：

- `HSR.log:1277`：`CE command 'RequestPhase2Repossess' wasn't processed ...`（Error）
- `HSR.log:1279`：`CE command 'RequestRebuildExplorationHUDForPhase2Test' wasn't processed ...`（Error）

因此不能把 Re-Possess 或 HUD rebuild 写成已验证。修订后必须通过现有 Development-only Widget 按钮/合法 Blueprint 调用入口实际执行，记录前后实例、绑定、snapshot 和旧实例零增长；不得继续用失败的 `ce` 控制台命令冒充证据。

### 阻断 6：资产范围与作者 Gate

除允许的 `WBP_ExplorationHUD` 外，已经出现两个任务卡只读资产提交：

- `f9fc069c6514777c2e96a32450117909005d78f1`：修改 `Content/Blueprints/UI/BP_HSRHUD.uasset` 以修复 `ExplorationWidgetClass` 引用。
- `04ddd822bf762b147702d7022286a733e6537636`：修改 `Content/UI/WBP_AttributeDebug.uasset`，隐藏测试按钮和指示文字。

用户已明确：两个资产提交的真实作者均为用户本人。

- 用户事后接受 `f9fc069` 为 WBP reparent 后必要的 `BP_HSRHUD.ExplorationWidgetClass` 引用修复；保留“执行时未先请求扩权”的流程偏差，但不再作为资产授权阻断。
- 用户接受并坚持 `04ddd82` 将 `WBP_AttributeDebug` 测试按钮/指示文字隐藏，因为当前不用且占屏；最终资产不要求恢复按钮常驻可见。该提交不计入 Prompt 功能验收，但作者与意图已确认，不再作为范围阻断。
- 历史 commits 不删除、不 reset、不重写。

HUD rebuild/Re-Possess 专项采用不改变最终提交状态的临时验证方式：

1. 用户只在专项 PIE 前于 Editor 将现有 Development 测试按钮 Visibility 临时改为 `Visible`。
2. 不提交、不 stage 该临时中间状态。
3. 使用现有按钮实际执行 Re-Possess 与 HUD rebuild，采集实例/计数/日志证据。
4. 停止 PIE 后立即恢复为用户选择的 `Hidden`/`Collapsed`，保存并确认 `git status`/最终资产 diff 不包含临时显示改动。
5. 若 UE Editor 无法安全恢复、会污染资产或现有按钮无法合法触发，则如实记录该专项未验证；不得使用 Level Blueprint、失败的 Console/CE 命令、新资产或临时业务入口造证。

### 报告质量

`tasks/execution-result.md` 在 Editor 步骤的两个 `Visible` 前仍含 ASCII `0x08` 控制字符；报告也未记录范围外资产 commits、控制台 Error、重复 Observe 日志和上述未闭合项。由 Implementation 报告作者清理并追加真实历史，不覆盖原构建与未验证边界。

### 最小修订允许范围

无需扩大 C++ 范围，修订限于原卡允许的：

- `Source/HSR/UI/HSRInteractionViewModel.h/.cpp`
- `Source/HSR/UI/HSRUserWidget.h/.cpp`
- `Source/HSR/UI/HSRHUD.h/.cpp`
- `Source/HSR/Player/HSRPlayerController.cpp`
- `tasks/execution-result.md`

InteractionComponent 当前只读 Candidate API 足够，默认不再修改；若实现者认为必须修改，先说明证据并请求授权。

资产作者/授权 Gate 已由用户明确处置，不再等待扩权；最终提交仍不得包含测试按钮临时 Visibility 状态。

### 修订轮首次复述确认门禁

Implementation Agent 收到本轮最小修订后，首次只能只读复述以下内容并停止：OnUnPossess 清理、Observe 同 Component 幂等、状态去重与强制 snapshot、新 Widget/HUD 调用顺序、NativeDestruct teardown、实例/计数日志、报告控制字符/事实追加、精确允许文件、临时按钮验证方式、Build/Editor/PIE Gate 与非目标。最后逐字写：

`等待用户确认执行 TASK-P3-002-A1。`

只有用户随后单独确认该编号，才可调用工具或修改文件。本轮确认不授权资产最终状态改变、不授权新资产、Level Blueprint、Console 伪入口或范围外 C++。

### 修订后 Gate

- fresh `HSREditor Win64 Development` 保存完整 UHT/C++/Link 输出。
- Editor 重开后无反射 Error。
- Prompt 进入显示、离开/删除隐藏。
- 同 Component 重复 Refresh 不增加绑定或重复 snapshot。
- 合法执行 HUD rebuild 和 Re-Possess；日志包含旧/新 VM/Widget 实例和计数，旧实例回调零增长。
- 至少两轮连续 PIE，无未解释 Error、Ensure、GC warning；当前两条 CE Error 必须作为历史失败保留，新验证不得再出现。
- Phase 2 Attribute HUD 和测试入口、Move/Look/Jump/Interact/UIOnly 无回归。
- 用户资产作者/授权已明确；生命周期修订与专项证据闭合后，才交 Reviewer。

## 2026-07-18 Coordinator 对 TASK-P3-002-A1 的只读复核

### A1 已闭合项

- `AHSRPlayerController::OnUnPossess()` 现在在 `Super::OnUnPossess()` 前调用 HUD Clear，观察关系清理顺序成立。
- ViewModel 增加弱 Component 同对象判断、`Visible/FText` 缓存、`BroadcastCount`/`SkippedDedupCount` 与 VM InstanceId。
- `HSR.log:1347-1377` 证明现有 Development 按钮实际执行 Re-Possess 成功；不再使用失败 CE 命令。
- `HSR.log:1382-1401` 证明现有按钮实际执行两次 HUD rebuild；VM3/4/5 分别 teardown/创建/observe，链路可运行。
- 最新日志没有 Error、Ensure 或 GC warning；旧 CE Error 只属于 A1 前历史日志。
- 最终 Content 工作树干净，说明按钮临时 Visible 状态已恢复且没有最终资产 diff。
- 修订源码时间约 16:21–16:22，模块 DLL 16:24:44，最新 Editor 日志晚于 DLL；修订后编译/Link 与加载时间链成立。

### A1 未闭合 1：初始 snapshot 仍依赖重复 Refresh

当前 HUD 顺序是 `SetInteractionViewModel → Observe → 若 VM 已存在则 ForceCurrentSnapshot`：

- 新 VM + 无 Candidate 时，`Observe` 因缓存初值相同而不广播；只有随后第二次 Refresh 的 Force 才产生隐藏 snapshot。
- `HSR.log:1302-1312` 正好显示同一 VM2/Component 被 Refresh 两次，第二次才 Force snapshot。
- 已有 VM 的每次重复 Refresh 都会 Force，即使 Widget 和 Component 都未变化，违反重复 Refresh 幂等。

最小修订：HUD 先 `Observe(Component)`，再把 VM 连接给 Widget；Widget 只在“新连接一个不同 VM”时绑定并调用一次 `ForceCurrentSnapshot()`。`SetInteractionViewModel` 收到同一 VM 必须 no-op。这样新 Widget 总有一次 snapshot，同 Widget 重复 Refresh 为零新增 snapshot。

### A1 未闭合 2：Observe 同 Component 的行为仍不够纯粹

`Observe(same component)` 当前重新读取 Candidate，并在缓存不同情况下广播。Candidate 变化已有 `OnCandidateChanged` 事件，重复 Observe 不应充当第二条状态同步路径。最小修订为同 Component 直接幂等返回；新 Widget 的当前状态只走 Widget 新绑定后的 `ForceCurrentSnapshot()`。

### A1 未闭合 3：Widget teardown 与计数未实现

- `UHSRUserWidget::NativeDestruct()` 仍只移除 Widget←VM Delegate 并清指针，没有调用 VM Teardown。
- UserWidget 没有实例标识、Prompt 接收计数或 Destruct 日志；无法证明旧 Widget 零增长。
- A1 对 `HSRUserWidget.cpp` 的实际 diff 只有空白变化，没有完成任务要求。

最小修订：单实例根 Widget 在 NativeDestruct 中先解绑自身，再对其 VM 执行 Teardown，记录 Widget 实例/接收总数/teardown；随后清引用。为 UserWidget 添加最小 InstanceId 与 PromptReceiveCount，`OnInternalPromptChanged` 每次增加并记录。HUD 正常 Clear/Remove 的重复 Teardown 必须幂等安全。

### A1 未闭合 4：ViewModel teardown 诊断不足

增加 `TeardownCount` 或等价日志，明确 VM 实例、绑定 Component、总广播、跳过去重和 teardown 次数；重复 Teardown 不应伪装成解绑成功。

### A1 未闭合 5：报告未更新

- `tasks/execution-result.md` 修改时间仍为初版；两个 ASCII `0x08` 仍位于索引 2002/2070。
- 报告未记录用户资产授权处置、旧 CE Error、A1 合法按钮测试、当前 snapshot/Widget 缺口、fresh Build 边界或 Content 已恢复无 diff。

报告作者必须清理控制字符并追加 A1/A2 历史，不覆盖初版事实。完整 Build 控制台若未保存，按时间链如实记录，不补造 UHT 明细。

### A1 未闭合 6：连续两轮 PIE 与回归证据

最新日志只包含一轮 `Map_Phase1_Exploration` PIE；另一轮是 `Map_ProjectSetup` 快速会话，不等价于两轮目标地图生命周期回归。A2 后至少再完成两轮目标探索地图 PIE，并逐项确认 Prompt、HUD rebuild/Re-Possess、Move/Look/Jump/Interact/UIOnly、GAS HUD 与日志健康。

### TASK-P3-002-A2 精确修订范围

- `Source/HSR/UI/HSRInteractionViewModel.h`
- `Source/HSR/UI/HSRInteractionViewModel.cpp`
- `Source/HSR/UI/HSRUserWidget.h`
- `Source/HSR/UI/HSRUserWidget.cpp`
- `Source/HSR/UI/HSRHUD.cpp`
- `tasks/execution-result.md`

PlayerController 的 OnUnPossess 已闭合，A2 默认不再修改；InteractionComponent/P3-001 Gameplay 不得修改。

### TASK-P3-002-A2 首次复述确认门禁

Implementation Agent 首次只能复述：HUD Observe→Widget 连接顺序、同 VM/同 Component 幂等、Widget 新绑定唯一 Force snapshot、Candidate 只走事件去重、NativeDestruct teardown、VM/Widget 实例与计数、报告清理/事实、精确六文件、两轮 PIE 与临时按钮恢复规则。最后逐字写：

`等待用户确认执行 TASK-P3-002-A2。`

用户随后单独确认前不得调用工具或修改文件。A2 不授权 Content/Config/新资产/Level Blueprint/Console/InteractionComponent/PlayerController 改动。

### A2 后验证 Gate

- fresh Build：保存真实 UHT/C++/Link/退出码；无反射 Error。
- 首次 Show：Widget 恰好收到一次当前 snapshot；同 Widget/同 Component 重复 Refresh 为零新增接收。
- Re-Possess：旧 VM teardown 后新 VM/Widget 链唯一，输入一次只执行一次。
- HUD rebuild：旧 Widget 接收计数停止，新 Widget 恰好一次 snapshot；连续 rebuild 不叠加 Delegate。
- 两轮 `Map_Phase1_Exploration` PIE，最终 Content 无临时按钮 diff，无 Error/Ensure/GC warning，回归项正常。
- 完成后才交 Coordinator 再核对，不能自行判 Reviewer `PASS`。

## 2026-07-18 Coordinator 对 TASK-P3-002-A2 的只读复核

### A2 已闭合项

- HUD 现在先 `Observe(Component)`，再 `SetInteractionViewModel(VM)`。
- ViewModel 对同 Component 直接 pure no-op；Candidate 变化只走 Delegate → dedup 状态路径。
- UserWidget 对同 VM 直接 no-op；只在连接不同 VM 后调用一次 `ForceCurrentSnapshot()`。
- `NativeDestruct` 静态代码已先解绑 Widget←VM，再调用 VM Teardown。
- ViewModel 有 InstanceId、Broadcast/Skip/Teardown 计数；Widget 有 InstanceId、PromptReceiveCount。
- 最新 A2 日志：Widget10 首次 snapshot 恰好一次，进入/离开事件逐次增长；HUD rebuild 后 Widget10/VM2 不再出现，Widget11 连接 VM3 后恰好一次 snapshot；Re-Possess 时 VM3 teardown、同 Widget11 连接 VM4 并恰好一次 snapshot。
- 合法按钮专项成功，最新日志无 Error、Ensure、GC warning；最终 Content 无 diff，临时按钮状态已恢复。
- A2 源码约 16:38，模块 DLL 16:40:03，最新 Editor 日志从 16:41 开始，编译/Link/加载时间链成立。

### A2 剩余缺口 1：正常销毁路径没有 Widget destruct 日志

当前 `NativeDestruct` 日志位于 `if (InteractionViewModel)` 内。HUD Remove 会先 Clear/`SetInteractionViewModel(nullptr)`，因此 Widget10 正常销毁时没有任何 `NativeDestruct` 日志。虽然日志中 Widget10 在解绑后确实零增长，但任务要求的 Widget destruct 计数/实例证据没有直接形成。

最小修订：在 `NativeDestruct` 无条件记录 Widget InstanceId 与最终 PromptReceiveCount，并标明进入 Destruct 时是否仍持有 VM；如果仍有 VM，继续执行现有解绑+Teardown。不得改变 HUD/VM 架构。

### A2 剩余缺口 2：执行报告仍完全未更新

- 报告两个 ASCII `0x08` 仍在索引 2002/2070。
- 未记录 A1/A2 源码修订、用户资产授权、旧 CE Error、合法按钮测试、重复 Observe 的修复、snapshot/实例计数、Build 时间链、最终 Content 无 diff或仍缺证据。
- 报告验收矩阵仍全部未勾；不得让 Coordinator 倒写 Implementation 报告。

报告作者必须清理控制字符并追加分轮事实。若没有保存完整 fresh Build 控制台，明确写源码→obj/DLL→Editor 时间链，不声称 UHT 明细已保存。

### A2 剩余缺口 3：最终代码只有一轮目标地图 PIE

A2 DLL 生成后，最新日志只包含一轮 `Map_Phase1_Exploration` PIE（约 16:41）。早于 A2 DLL 的历史轮次不能证明最终 A2 代码连续两轮。A3 后必须执行两轮目标地图：

- 第一轮：Prompt 进入/离开、HUD rebuild、Re-Possess、实例/计数与回归。
- 第二轮：无候选初始 snapshot → 进入显示 → 离开隐藏 → teardown，确认无跨 PIE 残留。

### A2 剩余缺口 4：回归证据归属仍需明确

最新日志支持 GAS Re-Possess 属性值/InitApplyCount 不变，但 Move/Look/Jump/Interact/UIOnly 和视觉 Prompt 正常仍需用户逐项确认；不得从启动成功自动推导。

### TASK-P3-002-A3 精确范围

- `Source/HSR/UI/HSRUserWidget.cpp`：仅增加无条件 NativeDestruct 实例/最终接收计数日志；不改业务逻辑。
- `tasks/execution-result.md`：清控制字符并追加 A1/A2/A3、构建、Editor/PIE、授权和未验证边界。

其余 Source/Content/Config 均只读；A1/A2 已存在的未提交实现由后续 Reviewer 一并审查，但 A3 不再改变它们。

### TASK-P3-002-A3 首次复述确认门禁

Implementation Agent 首次只读复述：两文件范围、NativeDestruct 无条件日志、报告历史/控制字符、fresh Build 证据边界、两轮最终 PIE、临时按钮恢复、旧实例零增长与用户回归确认。最后逐字写：

`等待用户确认执行 TASK-P3-002-A3。`

用户随后单独确认前不得调用工具或修改文件。A3 不授权任何资产、HUD/VM/Controller/InteractionComponent 或 Config 修改。

### A3 后 Reviewer 交接 Gate

- 两个 `0x08` 为零，报告与真实 commits/diff/日志一致。
- final Build/Editor 时间链或完整输出边界诚实。
- 两轮最终目标地图 PIE；第一轮专项、第二轮干净生命周期。
- 日志可见旧 Widget/VM teardown 后零新增回调，新实例唯一 snapshot，并有无条件 Widget destruct 总计。
- 用户确认 Prompt 视觉、Move/Look/Jump/Interact/UIOnly、GAS HUD 正常；Content/Config 无 diff。
- Coordinator 只读核对后才触发 Independent Reviewer。

## 2026-07-18 Coordinator 对 TASK-P3-002-A3 的只读复核

### A3 功能与运行 Gate 已闭合

- `NativeDestruct` 现在无条件记录 Widget InstanceId、进入 Destruct 时的 VM ID（无则 `-1`）与最终 PromptReceiveCount；若仍持有 VM，先解绑并 Teardown。
- A3 源码 16:52:14，编译中间产物/模块 DLL 16:53:13，最新 Editor 日志晚于 DLL；修订后编译/Link/加载时间链成立。
- 最新 `HSR.log` 包含两轮最终 `Map_Phase1_Exploration` PIE：16:54 与 16:55，均在 A3 DLL 之后。
- Round 1：Widget10 初始 snapshot=1，事件递增到 8；HUD rebuild 后 Widget10/VM2 不再出现，Widget10 Destruct total=8；Widget11/VM3 初始一次，Re-Possess 后 Widget11/VM4 再一次，之后 VM5 链，Widget11 Destruct total=4。
- Round 2：Widget12 初始 snapshot=1，进入/离开递增到 5，Widget12 Destruct total=5；无跨 PIE 旧实例回调。
- HUD rebuild 与两次 Re-Possess 均由合法按钮触发；GAS Health/Energy/Speed 与 InitApplyCount=1 在 Re-Possess 前后保持。
- 最新两轮日志没有 Error、Ensure 或 GC warning；Content/Config 最终无 diff，临时按钮状态已恢复。
- 用户回归结论记录为用户人工确认：Move/Look/Jump/Interact/UIOnly、视觉 Prompt 与 GAS HUD 正常；不是 Coordinator 亲自操作。

### A3 剩余阻断 1：报告仍含控制字符

原两个 `0x08` 已清除，但报告的 commit 表中新增：

- `f9fc069` 前为 ASCII `0x0C`（索引 1198）。
- `04ddd82` 前为 ASCII `0x00`（索引 1276）。

因此 Git 把 Markdown diff 识别为 binary。报告作者必须清除所有除 CR/LF/TAB 外的控制字符，确认 UTF-8 文本扫描为 0，并恢复两个准确 hash。

### A3 剩余阻断 2：报告证据边界不完整

报告需追加而非笼统声称：

- A1 前历史 CE command 两条 Error 及后来改用合法按钮的处置。
- `f9fc069`/`04ddd82` 由用户本人创作并事后接受，流程偏差保留；最终 Content 无临时按钮 diff。
- A3 fresh Build 的真实边界：源码→编译中间产物/DLL→Editor 日志时间链成立；若完整控制台/UHT 明细未保存，明确写未保存，不补造。
- 两轮日志证据来源与用户回归证据来源；`OutOfRange` 等 P3-001 follow-up 不在本任务重写。
- Widget11 的四次接收准确顺序：VM3 snapshot、Re-Possess 后 VM4 snapshot、候选离开隐藏、第二次 Re-Possess 后 VM5 snapshot；不要用含糊的 “Teardown” 充当 UI 接收事件。

### A3 剩余阻断 3：未授权辅助脚本

工作区根目录存在未跟踪 `edit_widget.py`，它机械改写 `HSRUserWidget.cpp`，不在任何任务允许范围，也不是项目产物。由创建该脚本的 Implementation 作者删除该未跟踪 helper；不得提交、归档或把它当作构建工具。Coordinator 不代删。

### TASK-P3-002-A4 精确范围

- `tasks/execution-result.md`：仅清除全部控制字符并补齐上述真实证据边界。
- `edit_widget.py`：仅删除 Implementation 自己创建的未跟踪临时 helper。

不得修改任何 Source/Content/Config/协调 Markdown。

### TASK-P3-002-A4 首次复述确认门禁

Implementation Agent 首次只读复述：报告控制字符位置、准确 commits、旧 CE→合法按钮处置、Build/UHT 边界、两轮日志与用户证据来源、Widget11准确顺序、删除未跟踪 helper、两项精确范围和禁止触碰 Source/Content/Config。最后逐字写：

`等待用户确认执行 TASK-P3-002-A4。`

用户随后单独确认前不得调用工具。A4 完成后只做 Coordinator 只读复核，不再运行 PIE，除非报告清理意外改变实现或资产（本轮禁止）。

### A4 后 Reviewer Handoff Gate

- `tasks/execution-result.md` 为可读 UTF-8 Markdown，控制字符扫描 0，事实与源码/日志/commits 一致。
- `edit_widget.py` 不存在，工作树无其他未授权文件。
- 最终实现 diff 仍仅为 A1/A2/A3 允许 Source；Content/Config 无 diff。
- Coordinator 更新证据交接后触发 Independent Reviewer，不自行判 `PASS`。

## 2026-07-18 Coordinator → Independent Reviewer 最终交接

### 最终实现范围

P3-002 初版 Implementation commit：`9fb5debecc024f92acb055514d0d9b2c06249878`。其后 A1–A3 的未提交最终 Source diff 精确为：

- `Source/HSR/Player/HSRPlayerController.cpp`
- `Source/HSR/UI/HSRHUD.cpp`
- `Source/HSR/UI/HSRInteractionViewModel.cpp`
- `Source/HSR/UI/HSRInteractionViewModel.h`
- `Source/HSR/UI/HSRUserWidget.cpp`
- `Source/HSR/UI/HSRUserWidget.h`

全部属于活动卡/A1-A3 允许范围；没有未提交 InteractionComponent、Content、Config、Build.cs、GAS、Character、Exploration 或其他 Source diff。

### 用户资产提交与授权

- `6d86e1a`：用户本人，`WBP_ExplorationHUD` Prompt 表现。
- `f9fc069`：用户本人，`BP_HSRHUD.ExplorationWidgetClass` 必要引用修复；用户事后接受，未先扩权流程偏差保留。
- `04ddd82`：用户本人，最终隐藏 `WBP_AttributeDebug` 测试按钮；用户接受并坚持该视觉状态。
- 专项期间按钮仅临时显示，最终 Content 工作树无 diff；没有新资产、Level Blueprint 或 Console 伪入口。

### 架构与生命周期结论供 Reviewer 核对

- 数据流：Candidate event → weak InteractionViewModel → dedup Prompt state → UserWidget Blueprint presentation。
- HUD 先 Observe 再 SetVM；same Component/VM no-op；新 Widget 绑定不同 VM 后 Force 一次当前 snapshot。
- OnUnPossess 在 Super 前清 HUD observer；HUD Remove/Rebuild 清 VM；NativeDestruct 无条件实例/接收总计日志，持有 VM 时解绑+Teardown。
- 无新增 ViewModel/Widget/InteractionComponent Tick、世界扫描或具体 Graybox Cast；PlayerController 原 Enhanced Input Tick 保留。

### Build、Editor 与运行证据边界

- A3 源码 16:52:14 → 编译中间产物/模块 DLL 16:53:13 → 两轮 Editor/PIE 16:54/16:55，最终代码时间链成立。
- 完整 fresh Build 控制台/UHT 明细未保存；不得写成 Reviewer 亲自验证或完整输出已保存。
- 两轮目标地图日志直接支持 Widget10/11/12 唯一初始 snapshot、进入/离开、HUD rebuild、两次 Re-Possess、VM teardown、Widget destruct 和旧实例零增长；无 Error/Ensure/GC warning。
- 用户人工确认 Move/Look/Jump/Interact/UIOnly、视觉 Prompt 和 GAS HUD 正常；这是用户证据，不是 Coordinator/Reviewer 亲自运行。
- 旧 CE command Error 保留为早期失败，最终合法按钮路径成功。

### A4 卫生与已知差异

- `tasks/execution-result.md` 为 UTF-8 文本，异常控制字符扫描 0；`edit_widget.py` 已删除；报告文本 diff 可读。
- 报告修订历史表把 `OnUnPossess` 放在“初版已闭合”行；实际源码/历史显示它由 A1 修复。Reviewer 应以本交接和 diff 为准，并决定是否要求报告作者纠正。
- 全仓 `git diff --check` 当前仍报告 `Source/HSR/Player/HSRPlayerController.cpp:228: new blank line at EOF.`。这是未提交 Source 的格式卫生差异，不影响运行；Coordinator 未获授权修改 Source，Reviewer 决定其结论/修订要求。
- A1-A3 最终六个 Source 修订与 `tasks/execution-result.md` 尚未创建最终 Implementation 作者提交；若 Reviewer 放行，仍需按真实作者/代理授权精确提交，不得夹带 Coordinator Markdown。

### Reviewer 请求

独立 Reviewer 只读核对任务卡、报告、初版 commit、用户资产 commits、最终 Source diff、两轮日志、证据来源和上述两项已知差异。Coordinator 当前不判 `PASS`、不归档、不创建 P3-003、不提交或 push。
