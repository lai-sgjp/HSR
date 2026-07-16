# TASK-P1-004：第三人称探索可玩闭环

> 状态：Coordinator 已规划，当前正式交接给 Implementation Agent 进行首次只读复述。用户再次明确确认前，执行者不得调用任何工具。

## 唯一目标

把既有 Character、PlayerController 与 Enhanced Input 基线连接为一个可在灰盒地图中验收的垂直切片：正确 Spawn/Possess，Move、Look、Jump 可用，Interact 无副作用，占位探索 HUD 单实例显示，且 Exploration/UIOnly 模式切换及连续 PIE 不产生输入或 HUD 叠加。

本任务以一个工作包管理，但必须按 A～E 分段交接。不同人格与资产作者各自提交自己的产物，不得合并身份或冒充证据作者。

## 执行前强制门禁

Implementation Agent 第一次接收本卡时：

1. 只允许读取本文件，确认任务编号、目标、允许范围、分段步骤、Editor 操作、验证、Git、风险和非目标。
2. 不得读取源码、调用工具、修改文件、构建、启动 Editor/PIE 或执行 Git。
3. 完整复述后必须精确以以下句子结束：

`等待用户确认执行 TASK-P1-004。`

只有用户在该复述后再次明确确认，Segment A 才可开始。

## 分段交接链

`Coordinator 规划 commit → A Implementation Agent C++ commit → B 用户 UE Editor 资产 commit → C 用户 PIE 验证证据/验收或 D Reviewer commit → E Coordinator 归档 commit`

- 每段开始前必须已收到上一段的完整 commit hash、实际 diff 范围、验证证据和未验证项。
- 用户可以明确跳过独立 Reviewer；届时只能如实记录“用户人工验收通过”，不得创建、暗示或冒充 Reviewer 产物/commit。
- 验证发现缺陷时只记录第一处真实证据并交回 Coordinator。未经重新授权，不得在验证阶段顺手修改源码或资产。

## Segment A：Implementation Agent — C++ 基础设施

### 允许读取和修改

- `Source/HSR/Framework/HSRGameModeBase.h`
- `Source/HSR/Framework/HSRGameModeBase.cpp`
- `Source/HSR/UI/HSRHUD.h`
- `Source/HSR/UI/HSRHUD.cpp`
- `Source/HSR/UI/HSRUserWidget.h`
- `Source/HSR/UI/HSRUserWidget.cpp`
- `Source/HSR/HSR.Build.cs`：只允许增加 `UMG` 模块依赖，不得重排或改动其他依赖
- `tasks/execution-result.md`：只填写 Segment A

`Source/HSR/Player/HSRPlayerController.h/.cpp` 默认只读。若实现 HUD 生命周期或 ControlMode 协调确实必须修改它们，立即停止并申请扩权；本任务不预先授权。

### 实现要求

- `AHSRGameModeBase` 职责保持最小，只提供可由派生 Blueprint 配置的 GameMode 基线；不得保存跨地图玩家状态或承载 UI/Input 业务。
- 建立最小 `AHSRHUD` 与 `UHSRUserWidget` 边界。
- HUD 只有一个明确的探索 Widget 创建入口；重复调用必须幂等，不得重复 `AddToViewport`。
- WidgetClass 为空、OwningPlayer 无效或 `CreateWidget` 失败时安全返回并提供可定位日志，不得崩溃或留下错误引用。
- Widget 使用正确 Owning Player；UObject/Widget 引用遵循 UPROPERTY/TObjectPtr 与 GC 生命周期规则。
- Character、Controller、HUD、Widget 均不得新增自定义 Tick 或轮询式 UI 更新。
- 不在 C++ 中硬编码具体 Blueprint、地图、输入资产或正式美术路径。

### 构建证据

- 运行真实 `HSREditor Development Win64` 构建。
- 必须证明本轮 UHT（如触发）、目标 C++ Compile、Link 和退出码 0；旧日志或单纯 up-to-date 不能冒充目标文件实际编译证据。
- 失败时记录第一处真实错误并停止；不得 clean/reset/删除缓存或修改未授权文件。
- 构建派生产物不得暂存或提交。

### Segment A Git

提交前检查实际 diff，仅提交本段允许文件。Implementation Agent 创建独立 commit，并交回完整 hash、文件清单、构建证据和未验证项。不得包含 Content、Config、未跟踪 Blueprint 或派生产物。

## Segment B：用户 / UE Editor 执行者 — 资产与地图

### 唯一允许资产

已有且当前未跟踪、归属用户的资产：

- `Content/Blueprints/Character/BP_HSRExplorationCharacter.uasset`
- `Content/Blueprints/Input/BP_HSRPlayerController.uasset`

本段允许新建：

- `Content/Blueprints/Framework/BP_HSRGameMode.uasset`
- `Content/Blueprints/UI/BP_HSRHUD.uasset`（仅在 C++/配置方案确有需要时）
- `Content/UI/WBP_ExplorationHUD.uasset`
- `Content/Maps/Map_Phase1_Exploration.umap`

以上两个现有 Blueprint 是用户此前创建的产物。Coordinator 规划提交和 Segment A Implementation commit 均不得修改、暂存或提交它们。

### Editor 配置

1. 在角色/Controller 派生 Blueprint 的正确职责位置绑定现有 IA/IMC 引用。
2. 配置 GameMode 的 Default Pawn、PlayerController 与 HUD 类。
3. 配置探索 WidgetClass，并确认 Owning Player 正确。
4. 创建灰盒地面和一个可用 PlayerStart。
5. 在该地图 World Settings 中选择 `BP_HSRGameMode`。
6. 保存并关闭 Editor；重开后手动打开 `Map_Phase1_Exploration` 验证引用保持。

禁止修改 `Config/DefaultEngine.ini`；本任务不把探索地图设置为项目默认地图。

### Segment B Git

用户只提交本段允许的 `.uasset/.umap`。提交前使用 `git status --short` 和 `git diff --cached --name-only` 精确排除 `Binaries/`、`Intermediate/`、`Saved/`、DerivedDataCache、Config、源码及其他派生产物。用户创建独立资产 commit 并回传完整 hash与实际资产清单。

## Segment C：用户 — Editor 重开与 PIE 验证

必须记录实际观察，不以代码存在代替运行证据：

1. Editor 重开后地图、Blueprint、IA/IMC、GameMode、HUD 与 WidgetClass 引用保持。
2. PIE 仅生成一个目标 Pawn，且由目标 PlayerController 正确 Possess。
3. Move、Look、Jump 实际可用；Interact 触发后无 Gameplay 副作用。
4. HUD 只创建并显示一次，没有重复 Widget 或 Widget Tick。
5. 执行 `Exploration → UIOnly → Exploration`，并重复设置相同模式；UIOnly 阻止探索输入，恢复后输入正常。
6. 连续停止/启动 PIE，确认 Pawn、Input Binding、Mapping Context 与 HUD 不叠加。
7. 检查 Output Log，记录第一处真实 Error、Ensure 或相关 Warning；没有时也要明确记录。
8. 至少验证一条真实失败路径：空 WidgetClass、缺 PlayerStart、错误默认类、缺 IA/IMC 或错误 Pawn 类型之一。必须安全失败并记录现象与日志，测试后恢复正确配置。

若任何测试发现 bug，只写入 `tasks/execution-result.md` 的 Segment C 并交给 Coordinator 决定受控修复；验证者不得借机修改 C++ 或扩大资产范围。

## Segment D：验收 / Reviewer

- 默认由 Reviewer 对任务卡、A/B commits、执行结果与真实 Editor/PIE 证据作独立审查并创建 Reviewer commit。
- 若用户亲自审查并明确跳过 Reviewer，只记录用户结论、时间、已验收证据和保留未验证项，不伪造 Reviewer commit。
- 未满足失败路径、Editor 重开或完整 PIE 证据时，不得判定本工作包通过。

## Segment E：Coordinator 归档

Coordinator 核对所有角色 commit、实际 diff、资产作者、构建/Editor/PIE 证据和未验证项后，归档活动卡与执行报告，更新项目状态/进度/日志并创建独立 Coordinator commit。归档前不得创建 P1-005；本任务不 push。

## 最终验收标准

- Segment A 有真实 UHT/Compile/Link 成功证据与独立 Implementation commit。
- Editor 重开后全部授权资产和引用保持。
- 单一 Pawn 正确 Spawn/Possess；Move、Look、Jump 有实际 PIE 证据。
- Interact 仍是无副作用占位入口。
- HUD 单实例，空 WidgetClass/CreateWidget 失败安全，无自定义 Tick。
- UIOnly 停止探索输入并可恢复；重复模式设置幂等。
- 连续 PIE 后 Context、Binding、Pawn、HUD 均不叠加。
- 至少一条真实失败路径得到安全验证并恢复配置。
- Output Log 无未解释的相关 Error/Ensure/Warning。
- 各角色只提交自己的授权产物，派生产物未入库。

## 明确非目标

- AnimBP、动画状态机、正式 Mesh/动画/美术资源
- GAS、ASC、AttributeSet、GameplayEffect
- 交互扫描、交互对象或实际 Interact Gameplay
- Enemy、Encounter、Battle、SaveGame、联网复制
- 第三方资源、Phase 2 或后续 Phase 内容
- 修改项目默认地图或其他 `DefaultEngine.ini` 配置

## 当前正式交接

`Coordinator → Implementation Agent（首次只读复述）`

正式接收条件：Implementation Agent 只读本卡，完整复述所有边界，并精确以 `等待用户确认执行 TASK-P1-004。` 结束。用户随后再次明确确认前，零工具调用。
