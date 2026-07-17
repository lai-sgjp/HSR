# Active Task

## Role Lock / 角色锁定

低级执行模型当前且仅能担任 `Implementation Agent / 低级执行模型`。不得切换为 Coordinator、Architect、Reviewer、Teacher 或其他角色，不得改写任务目标、扩大文件范围、改变 Phase 或验收标准。发现歧义、UE5.6 API 不确定、需要额外文件或需要更高权限时，立即停止并报告所需角色与授权；获得授权前不得修改文件。

## 任务编号

`TASK-P2-001`

## 任务名称

最小 GAS 属性初始化可见闭环

## 当前 Phase

Phase 2 已由用户明确授权进入协调规划。Phase 1 为 `Ready`；Phase 2 Gameplay 尚未实施、构建或 PIE 验证，当前状态为 `Not verified`。

## 任务目标

唯一目标：进入 `Content/Maps/Map_Phase1_Exploration.umap` 的 PIE 后，当前探索角色以 `Owner Actor = Avatar Actor = 当前 Character` 的有效 ASC，通过一次初始化 GameplayEffect 获得 Health、MaxHealth、Energy、MaxEnergy、Speed，并由无 Tick 的 Attribute Delegate 链显示在 Debug HUD；三段协调函数必须在同一实例上分别具备可审查的幂等保护，不得重复应用 GE 或重复绑定 Delegate。

本任务不实现 Ability、伤害、治疗、Meta Attribute、Cost、Cooldown、TurnSystem、交互、战斗、Save、复制或预测。

## 规划审查摘要

### 1. 子 Agent 分析

- Coordinator：Phase 1 证据足以进入相邻 Phase 2；现有模块依赖已包含 `GameplayAbilities`、`GameplayTags`、`GameplayTasks`。
- Architect：ASC 与 AttributeSet 由 `AHSRCharacterBase` 构造并持有；当前单机模型固定 `Owner=Avatar=self`；UI 只观察，不拥有或写回 Gameplay 状态。
- Prompt/Safety Review：C++、用户 Editor 资产、构建、Editor 重开和 PIE 属于同一可见垂直闭环，但作者与提交必须分段真实记录；Content、Config 和 Git 权限不能由 Implementation Agent 推定。

### 2. 是否适合当前 Phase

适合。任务只覆盖 Phase 2 的 ASC、Core AttributeSet、初始化 GE 和 Attribute Delegate 最小闭环，不跨入 Phase 3。

### 3. 是否需要拆分

不再按类机械拆分。任务内部严格串行：

1. Segment A：Implementation Agent 实现获准 C++，产出执行报告与真实构建证据。
2. Segment B：用户作为 UE Editor 资产作者创建/修改精确资产并独立提供作者与路径证据。
3. Segment C：用户执行 Editor 重开、PIE 主路径和缺失 GE 失败路径。
4. Segment D：Reviewer 独立审查；Coordinator 根据结论归档。

属性动态改值、Max 降低后的 Current 收敛专项、完整变化路径 Clamp 证明、Widget 重建和 Re-Possess 深度回归及其运行证据全部推迟到 `TASK-P2-002`。本任务只要求初始化后的可见闭环、基本非负/Current 不越过当前 Max 的安全底座，以及三段协调函数的代码级幂等；已创建 ViewModel 的对称解绑代码可以保留并接受代码审查，但 P2-001 不因缺少上述专项运行证据失败。

### 4. 本轮最小任务

完成 `Character → ASC → Actor Info → Init GE → AttributeSet → Attribute Delegate → Debug HUD` 一条可见数据流。

### 5. 风险审查

- Blocking：用户未确认 Editor 资产的精确包路径或作者身份时，不得创建、修改、暂存或提交资产。
- Blocking：若必须修改只读文件、Config、地图、Controller、GameMode、Build.cs 或新增未列出的持久文件，停止并申请扩权。
- Risk：Actor Info、初始化 GE、Delegate 若共用一个布尔值，可能掩盖部分初始化或造成重复副作用；必须三段独立状态。
- Risk：Instant GE 没有可用于长期去重的 Active Handle；初始化一次性必须由 Pawn 实例自己的显式状态保证。
- Risk：Widget/ViewModel 生命周期晚于或早于 Pawn 时，必须先推送当前快照并成对解绑，不能依赖 Tick。
- Risk：UE5.6 API、头文件或委托签名不确定时先检查本机引擎源码/实际编译错误，不得猜 API。

### 6. 最终低级模型 Prompt

你的首次工具调用只能读取 `tasks/active-task.md`，不得在同一次调用中读取任何其他文件。读取本卡后不得再调用工具；必须先向用户复述：任务编号、唯一结果、Segment A～D、允许修改文件、只读文件、禁止范围、用户 Editor 资产路径、三段式幂等、UE5.6 API 路径、构建/Editor/PIE/失败路径证据与停止条件。复述最后一行必须精确写：

`等待用户确认执行 TASK-P2-001。`

然后停止。只有用户在该复述之后另发明确确认执行 `TASK-P2-001`，才能调用工具。该确认不授权修改 Content、Config、Git 或任何未列文件。

## 执行模型与交接

- Coordinator：本卡作者；只负责范围、门禁、状态同步和后续归档，不代写实现、不冒充用户资产作者。
- Implementation Agent：仅执行 Segment A；用户再次确认前只读复述。
- 用户 / UE Editor 资产作者：仅执行 Segment B、C；亲自在 Editor 创建/修改资产、保存、重开并运行 PIE，回传截图、日志和现象。
- Reviewer：读取活动卡、执行报告、实际 diff 和用户证据，依据 `tasks/review-template.md` 给出独立结论。
- Teacher：本任务不进入 Teacher；仅 Phase 2 收尾时由 Coordinator 另行交接。

## 执行前必须读取的文件

低级执行模型默认只以本文件为上下文入口。用户再次确认后，Segment A 可读取：

- `Source/HSR/Character/HSRCharacterBase.h`
- `Source/HSR/Character/HSRCharacterBase.cpp`
- `Source/HSR/UI/HSRUserWidget.h`
- `Source/HSR/UI/HSRUserWidget.cpp`
- `Source/HSR/UI/HSRHUD.h`
- `Source/HSR/UI/HSRHUD.cpp`
- `Source/HSR/HSR.Build.cs`（只读，用于确认依赖）

不得自行补读全局文档以扩展需求。

## Segment A 允许修改文件

- `Source/HSR/Character/HSRCharacterBase.h`
- `Source/HSR/Character/HSRCharacterBase.cpp`
- `Source/HSR/GAS/HSRAbilitySystemComponent.h`
- `Source/HSR/GAS/HSRAbilitySystemComponent.cpp`
- `Source/HSR/GAS/Attribute/HSRCoreAttributeSet.h`
- `Source/HSR/GAS/Attribute/HSRCoreAttributeSet.cpp`
- `Source/HSR/UI/HSRAttributeViewModel.h`
- `Source/HSR/UI/HSRAttributeViewModel.cpp`
- `Source/HSR/UI/HSRUserWidget.h`
- `Source/HSR/UI/HSRUserWidget.cpp`
- `Source/HSR/UI/HSRHUD.h`
- `Source/HSR/UI/HSRHUD.cpp`
- `tasks/execution-result.md`

未列出的文件默认禁止修改；若其中某个 UI 文件最终无需修改，保持原样，不为填满清单制造改动。

## 只读文件与资产

- `Source/HSR/HSR.Build.cs`
- `HSR.uproject`
- `Source/HSR/Character/HSRExplorationCharacter.h`
- `Source/HSR/Character/HSRExplorationCharacter.cpp`
- `Source/HSR/Player/HSRPlayerController.h`
- `Source/HSR/Player/HSRPlayerController.cpp`
- `Source/HSR/Framework/HSRGameModeBase.h`
- `Source/HSR/Framework/HSRGameModeBase.cpp`
- `Content/Blueprints/Character/Player/BP_HSRExplorationCharacter.uasset`
- `Content/UI/WBP_ExplorationHUD.uasset`
- `Content/Blueprints/UI/BP_HSRHUD.uasset`
- `Content/Maps/Map_Phase1_Exploration.umap`
- `PROJECT_STATE.md`
- `todo_plan.md`
- `worklog.md`
- `docs/phase-2-execution-plan.md`
- 本活动卡

## Segment B 用户 Editor 资产范围与确认门禁

以下资产只能由用户在 UE Editor 中创建或修改，Implementation Agent 不得代办：

- 新建 `Content/GameplayEffects/BP_GE_InitializeCoreAttributes.uasset`（用户明确选择以 `BP_` 标识 Blueprint 特殊类）
- 新建 `Content/UI/WBP_AttributeDebug.uasset`
- 修改 `Content/Blueprints/Character/Player/BP_HSRExplorationCharacter.uasset`，仅绑定初始化 GE
- 修改 `Content/UI/WBP_ExplorationHUD.uasset`，仅接入属性调试显示

进入 Segment B 前，用户必须明确确认上述四个包路径与实际 Content Browser 路径一致，并确认自己是本轮资产作者。若 Editor 自动产生其他 `.uasset`、重定向器、地图或 Config 改动，停止，不保存为任务产物，并交回 Coordinator 判定。资产提交必须由用户本人完成或由用户明确授权代办，且记录真实作者；该授权不能由本卡预设。

## 禁止修改内容

- `Config/**`、`Content/Maps/**`、`Content/Input/**`、动画/骨骼/材质/第三方资产。
- `HSR.uproject`、`Source/HSR/HSR.Build.cs`、Target、模块或插件声明。
- ExplorationCharacter、PlayerController、GameMode、输入、相机、动画与 Phase 1 行为。
- GameplayAbility、Ability 授予/激活、Damage/Healing、Meta Attribute、ExecutionCalculation、Cost、Cooldown、GameplayCue、新 Gameplay Tags。
- Interaction、Encounter、Battle、TurnSystem、SaveGame、复制、RPC、Prediction、PlayerState ASC。
- 删除、重命名、覆盖历史、批量操作、`git reset`、`git clean`、push 或任何未经当前角色授权的 Git 提交。
- Character、HUD、Widget 或 ViewModel 的业务 Tick 轮询。

## 固定架构与 UE5.6 API 路径

1. `AHSRCharacterBase` 实现 `IAbilitySystemInterface`，`GetAbilitySystemComponent() const` 返回构造期 `CreateDefaultSubobject` 创建并由 `UPROPERTY`/`TObjectPtr` 保护的 ASC。
2. 当前单机固定调用 `UAbilitySystemComponent::InitAbilityActorInfo(this, this)`；文档和日志必须仍区分 Owner 与 Avatar 概念。
3. `BeginPlay` 是本任务唯一运行时总入口；它按固定顺序调用三段独立幂等步骤：
   - `InitializeAbilityActorInfo`：只初始化/核验 Actor Info；拥有独立状态与日志。
   - `ApplyInitialAttributes`：只在 Actor Info 有效且 GE 有效时应用一次；拥有独立“已成功应用”状态，失败不得误标成功。
   - `BindAttributeDelegates`：只绑定/切换一次观察目标，并先推送当前快照；拥有独立 Delegate handles/状态，解绑必须对称。
4. 初始化 GE 固定使用 `MakeEffectContext()` → `MakeOutgoingSpec(InitialAttributesEffect, 1.0f, Context)` → `ApplyGameplayEffectSpecToSelf(*Spec.Data.Get())`。必须保存 `ApplyGameplayEffectSpecToSelf` 返回的 `FActiveGameplayEffectHandle`，并且只用 `Handle.WasSuccessfullyApplied()` 判断 Instant GE 是否成功；**严禁使用 `Handle.IsValid()` 作为 Instant GE 的成功判据**。只有 `WasSuccessfullyApplied()` 为真后才能设置“初始化 GE 已成功应用”标志；失败必须保留未成功状态并记录明确日志。不得并行保留 Blueprint 隐式 Apply 路径。
5. Attribute 观察固定使用 `GetGameplayAttributeValueChangeDelegate(FGameplayAttribute)`；每项保存 `FDelegateHandle`，重复绑定前比较或解绑，teardown 时从原 ASC `Remove` 并 Reset。
6. 五项属性使用 `FGameplayAttributeData` 与标准 accessor 宏。P2-001 只建立基本安全底座：MaxHealth、MaxEnergy、Speed 不得小于 0，初始化后 Health/Energy 不得小于 0 或高于当时的 Max。
7. 必须如实说明 `PreAttributeChange`、`PreAttributeBaseChange`、`PostGameplayEffectExecute` 在本任务实际覆盖的写入路径，不得声称已证明所有聚合/Base/GE 变化。Max 降低后的 Current 收敛、动态 Effect、完整 Clamp 矩阵属于 P2-002，本任务不要求其专项运行证据。
8. 若本机 UE5.6 头文件或编译证据与上述签名冲突，保留第一处真实错误并停止申请 Coordinator 修订，不能发明兼容层或换用旧 API。

## 实现与验证步骤

1. 用户再次确认后，检查允许源码和只读 Build.cs；记录现状，不修改只读文件。
2. 实现 ASC、AttributeSet、Character 三段式初始化和最小 ViewModel/HUD 接线；保持 Phase 1 不变量。
3. 更新 `tasks/execution-result.md`，逐文件记录实际修改、首处错误、未验证项和 Editor 交接说明。
4. 运行本轮真实 `HSREditor Development Win64` 构建，必须触发新增文件的 UHT、C++ Compile 与 Link；记录命令、退出码和关键输出。旧 up-to-date 结果不能替代。
5. 将 Segment B 的逐步 Editor 操作交给用户，不得自己修改资产。
6. 用户保存并重开 Editor，核对 C++ 类、父类、GE、Character BP、Debug Widget 和 Exploration HUD 引用。
7. 用户在 `Map_Phase1_Exploration` 执行 PIE 主路径，并提供 HUD 五值与 ASC Owner/Avatar/Actor Info 证据。三段协调函数的幂等性以代码审查为最低门禁；仅当实现提供不污染正式 Gameplay 的合法受控入口时才补同实例最小运行证据，否则专项实证明确留给 P2-002。禁止使用 Level Blueprint 直接调用核心初始化函数。
8. 用户临时清空 Character BP 的初始化 GE 引用后执行失败路径：预期明确 Warning、角色/UI 安全存在、不崩溃、不伪装成功；随后恢复引用、保存并重新验证主路径。错误配置不得提交。
9. Reviewer 独立交叉核对任务卡、执行报告、实际 diff、构建与用户证据；Implementation Agent 不得自审或归档。

## UE Editor 手动操作（用户）

1. 在 Content Browser 确认现有资产准确路径：
   - `/Game/Blueprints/Character/Player/BP_HSRExplorationCharacter`
   - `/Game/UI/WBP_ExplorationHUD`
   - `/Game/Blueprints/UI/BP_HSRHUD`
   - `/Game/Maps/Map_Phase1_Exploration`
2. 创建 `/Game/GameplayEffects/BP_GE_InitializeCoreAttributes`，Duration Policy 使用 Instant；`BP_` 前缀是用户明确的 Blueprint 特殊类命名选择。为五项属性配置灰盒初值与明确 Modifier 运算，并回传实际数值。
3. 在 `BP_HSRExplorationCharacter` 的 C++ 暴露属性中绑定该 GE；不得在 Blueprint 另行 Apply GE。
4. 创建 `/Game/UI/WBP_AttributeDebug`，只显示 Health/MaxHealth、Energy/MaxEnergy、Speed；将其接入 `/Game/UI/WBP_ExplorationHUD`，不直接 Set Attribute、不启用 Tick。
5. 保存全部资产，关闭并重开 Editor，检查引用未丢失。
6. 执行 PIE 主路径与缺失 GE 失败路径，回传截图、Output Log 关键行和未验证项。不得用 Level Blueprint 直接调用 Actor Info、初始化 GE 或 Delegate 核心函数来伪造重复入口证据。

## 验收标准

- [ ] 实际 diff 只包含本卡当前角色允许的文件；用户资产作者与提交归属真实。
- [ ] `HSREditor Development Win64` 本轮 UHT、C++ Compile、Link 退出码为 0。
- [ ] Editor 重开后 C++ 类与四个允许资产的父类/引用保持。
- [ ] PIE 中 ASC 有效，Owner 与 Avatar 均为当前 Character，Actor Info 有效。
- [ ] `ApplyGameplayEffectSpecToSelf` 返回 handle 只以 `WasSuccessfullyApplied()` 判定 Instant GE 成功，未使用 `IsValid()`；只有成功后才设置初始化成功标志。HUD 五项值与 ASC/AttributeSet 直接读取一致。
- [ ] Actor Info、初始化 GE、Delegate 三个协调函数分别具有独立、可审查的同实例幂等保护，不以一个总布尔值掩盖失败或部分完成。若没有合法受控运行入口，P2-001 不要求重复调用专项 PIE 证据，并明确把该实证留给 P2-002。
- [ ] Character/HUD/Widget/ViewModel 没有新增业务 Tick。
- [ ] 缺失初始化 GE 时有明确 Warning、不崩溃、不伪装成功；恢复引用后主路径重新通过。
- [ ] 构建、Editor 重开、PIE、失败路径与未验证项分开如实记录。

## 审查清单

- [ ] 唯一目标未跨 Phase，未提前实现 P2-002 或 Ability/战斗。
- [ ] UE 反射、GC、UObject 引用、IAbilitySystemInterface 和 UE5.6 API 路径正确。
- [ ] Owner/Avatar 概念明确，当前实现确为 `self/self`。
- [ ] Actor Info、初始化 GE、Delegate 三段分别幂等且失败不污染成功状态。
- [ ] 基本 clamp 的覆盖声明与真实 UE 路径一致；没有把 Max 降低、动态 Effect 或完整 Clamp 专项误报为 P2-001 已验证。
- [ ] UI 单向观察且无 Tick；已创建的 ViewModel 对称解绑代码通过代码审查。Widget 重建/解绑专项运行证据留给 P2-002，不阻断 P2-001。
- [ ] Content 资产由用户创建/修改，路径、作者、Editor 重开和 PIE 证据完整。
- [ ] 无 Config、地图、模块、插件、第三方资产或越权 Git 操作。

## 执行后必须更新的文件

- Implementation Agent 只更新 `tasks/execution-result.md`。
- Reviewer 产出独立审查记录；不得由 Implementation Agent 代写。
- 仅 Coordinator 在审查后更新 `PROJECT_STATE.md`、`worklog.md`、`todo_plan.md`（仅真实进度）、学习/专项文档与任务归档。

## 执行结果

- 已修改：尚未实施。
- 已验证：仅完成 Coordinator 规划与现有路径只读确认。
- 未验证：全部 C++、构建、Editor 重开、PIE 主路径与失败路径。
- Editor 待办：等待 Segment A 通过构建后由用户执行。
- 阻塞/扩权请求：当前无；首次执行仍需用户单独确认。
- 审查结论：Reviewer 当前为 `REVISE`。用户事实说明已解除 `f03888e` 附带动画资产、`a58f385` 中 ViewModel 修复作者归属、初始化 GE 的 `BP_` 命名三项疑点；仍需最终源码真实构建证据，以及 Editor 重开、PIE 主路径和缺失 GE 失败路径的可核证材料。

### 执行后事实更正（2026-07-17）

- `f03888e` 中 `ABP_HSRExploration.uasset` 是用户本人主动修改并确认无问题的 P2-001 范围外附带修改；不计入 GAS 验收，不要求移出 Git 历史。
- `a58f385` 中 `Source/HSR/UI/HSRAttributeViewModel.cpp` 的行为修复实际由 Implementation Agent 完成，用户仅误将其与资产一起提交；该代码归 Segment A，资产仍归 Segment B。本记录不改写历史。
- 初始化资产实际路径追认为 `/Game/GameplayEffects/BP_GE_InitializeCoreAttributes`。

## Git 交付

- Implementation Agent 完成 Segment A 后，按项目规则仅提交自己的允许源码与执行报告；提交前检查 status、diff、允许范围与派生产物。
- 用户资产使用独立作者/提交；若要求代办，必须另有用户明确授权并记录真实作者。
- Reviewer、Coordinator 使用各自角色提交自己的审查/归档产物，不得冒名。
- Phase 2 中途不得 push；本轮 Coordinator 规划按用户明确要求不创建 Git 提交。
