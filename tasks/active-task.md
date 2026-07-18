# TASK-P3-002：事件驱动交互提示观察层与生命周期专项

> 状态：`PLANNED / WAITING FOR IMPLEMENTATION FIRST READ AND USER CONFIRMATION`  
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

- 已完成：Coordinator 规划、文件/资产范围锁定和 P3-001 归档。
- 未实施：全部 P3-002 C++、WBP、Build、Editor/PIE。
- 当前下一步：Implementation Agent 首次只读复述并等待用户确认 `TASK-P3-002`。
- 审查结论：未审查。
