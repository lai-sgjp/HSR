# TASK-P2-002：属性变化、边界与生命周期专项闭环

> 归档状态：`PASS`（2026-07-17）。保留 A1/A2/A3 与历史 `REVISE` 链；最终结论见 `TASK-P2-002-final-review.md`。

## Role Lock / 角色锁定

低级执行模型当前且仅能担任 `Implementation Agent`。不得切换为 Coordinator、Reviewer、Teacher 或资产作者，不得扩大范围、改变验收标准或提前进入 Phase 3。需要额外文件、Config、模块、插件、地图、输入或未列出的持久资产时，立即停止并申请 Coordinator 扩权。

## 任务状态

- Phase 2：进行中。
- P2-001：用户最终判定 `USER ACCEPTED` 并已归档；历史 Reviewer `REVISE` 与用户接受的证据边界保留，不伪造 Reviewer `PASS`。
- P2-002：仅完成 Coordinator 规划，尚未实施、构建或 PIE 验证。
- P2-003、Phase 3：不得开始。

## 唯一可验收结果

通过 Character 的开发期受控请求接口应用任务白名单测试 GE 后，目标属性进入明确的合法区间，ASC/AttributeSet 实值与 Debug HUD 一致，且每次预期的底层属性变化只产生一次对应 UI 更新；通过 Character 受控 Re-Possess 请求、HUD 安全重建请求和连续 PIE 证明不产生重复初始化 Effect、重复 Delegate、旧对象回调或失效引用。

## 前置事实与固定边界

- 当前单机 GAS 所有权保持 `Owner Actor = Avatar Actor = 当前 Character`，本任务不迁移 ASC。
- 初始化资产路径保持 `/Game/GameplayEffects/BP_GE_InitializeCoreAttributes`。
- P2-001 的 up-to-date 构建与用户 Editor/PIE 材料属于已接受证据边界，不能冒充 P2-002 新证据。
- 测试 Effect 仅服务属性与生命周期专项，不得演化为正式伤害、治疗或技能系统。

## Segment 与角色交接

1. Segment A — Implementation Agent：检查现有实现，建立最小受控测试入口，完善 Clamp、Delegate 与生命周期；只修改 C++ allowlist 和执行报告。
2. Segment B — 用户 / UE Editor 资产作者：创建测试 GE，并仅在 Coordinator 明确激活时修改允许资产；用户本人提供真实作者和路径证据。
3. Segment C — 用户：执行 Editor 重开、动态属性、边界、Re-Possess、Widget 重建和连续 PIE 验证，回传可核证截图/日志/计数。
4. Segment D — Reviewer：独立核对任务卡、diff、Build、Editor/PIE 证据并给出结论；Implementation Agent 不得自审。
5. Segment E — Coordinator：只在 Reviewer 结论与用户决定后归档或建立受控修订。

## Implementation Agent 首次门禁

首次工具调用只能读取 `tasks/active-task.md`，不得同批读取其他文件。读取后不得继续调用工具，必须先向用户复述：唯一结果、Segment A～E、C++ allowlist、只读/禁止范围、用户资产路径与作者边界、受控测试入口、Clamp 规则、Build/Editor/PIE 证据和停止条件。复述最后一行必须精确为：

`等待用户确认执行 TASK-P2-002。`

用户再次明确确认前不得实施。

## Segment A 可读取文件

- `Source/HSR/GAS/HSRAbilitySystemComponent.h/.cpp`
- `Source/HSR/GAS/Attribute/HSRCoreAttributeSet.h/.cpp`
- `Source/HSR/Character/HSRCharacterBase.h/.cpp`
- `Source/HSR/UI/HSRAttributeViewModel.h/.cpp`
- `Source/HSR/UI/HSRHUD.h/.cpp`
- `Source/HSR/UI/HSRUserWidget.h/.cpp`
- `Source/HSR/HSR.Build.cs`（只读）
- `tasks/execution-result.md`（实施后创建/更新）

## Segment A 精确修改 allowlist

- `Source/HSR/GAS/HSRAbilitySystemComponent.h`
- `Source/HSR/GAS/HSRAbilitySystemComponent.cpp`
- `Source/HSR/GAS/Attribute/HSRCoreAttributeSet.h`
- `Source/HSR/GAS/Attribute/HSRCoreAttributeSet.cpp`
- `Source/HSR/Character/HSRCharacterBase.h`
- `Source/HSR/Character/HSRCharacterBase.cpp`
- `Source/HSR/UI/HSRAttributeViewModel.h`
- `Source/HSR/UI/HSRAttributeViewModel.cpp`
- `Source/HSR/UI/HSRHUD.h`
- `Source/HSR/UI/HSRHUD.cpp`
- `Source/HSR/UI/HSRUserWidget.h`
- `Source/HSR/UI/HSRUserWidget.cpp`
- `tasks/execution-result.md`

只允许为本任务暴露的专项问题做最小修改。若现有代码无需修改，必须如实报告，不为制造 diff 而改代码。

## 固定的可执行开发期接口

Implementation Agent 在 `AHSRCharacterBase` 中规划并实现以下最小接口；签名与名称固定，不得替换为通用作弊接口：

```cpp
UFUNCTION(BlueprintCallable, Category = "GAS|Development", meta = (DevelopmentOnly))
bool RequestApplyPhase2TestEffect(TSubclassOf<UGameplayEffect> TestEffect);

UFUNCTION(BlueprintCallable, Category = "GAS|Development", meta = (DevelopmentOnly))
bool RequestPhase2Repossess();
```

- 两个函数即使因 UHT/构建要求存在于所有配置，也必须在实现内以 `#if UE_BUILD_SHIPPING || UE_BUILD_TEST` 明确记录 Warning 并返回 `false`；仅 Debug/Development 可用，不得进入 Test、Shipping 或正式 Gameplay 调用路径。`meta = (DevelopmentOnly)` 只约束 Blueprint 节点烘焙/使用语义，不能替代该 C++ 配置边界。
- `RequestApplyPhase2TestEffect` 只接受下述五个精确软包路径对应的 class；空类、其他 GE、路径不符或无有效 ASC/ActorInfo 时拒绝并记录 Warning。不得使用 SetByCaller、新 Gameplay Tags 或 Widget 直接 Apply GE。
- 允许路径：`/Game/GameplayEffects/BP_GE_Test_HealthBelowZero`、`BP_GE_Test_HealthAboveMax`、`BP_GE_Test_LowerMaxHealth`、`BP_GE_Test_EnergyBounds`、`BP_GE_Test_SpeedBelowZero`（以完整 `/Game/GameplayEffects/...` 包路径核对）。
- 该接口只调用现有 ASC 的标准 GE 应用链；不得直接写 `FGameplayAttributeData`。每次请求记录 Character 实例 ID、ASC 指针、GE 路径、成功判据和调用前后值。
- `RequestPhase2Repossess` 在函数内部先保存 `AController* OriginalController = GetController()`，核验本角色当前被该 Controller Possess，再依次执行 `OriginalController->UnPossess()` 与 `OriginalController->Possess(this)`。它不得生成新 Pawn、迁移 ASC 或修改 Controller 类。
- Re-Possess 请求记录前后 Character 实例 ID、Controller、ASC、Owner、Avatar、ActorInfo、`InitialAttributesApplySuccessCount`；成功计数必须保持 `1`。
- 禁止在 Level Blueprint 中调用 Actor Info、初始化 GE、Delegate 核心函数或直接改属性。Widget 只调用上述 Character 请求函数，绝不直接 Apply GE、Set Attribute 或承担去重。
- 若这些接口无法在当前 allowlist 和 UE5.6 API 内实现，保留首处真实 UHT/编译错误并停止请求 Coordinator，不得改输入、Controller、GameMode 或地图。

`AHSRHUD` 可在现有 `ShowExplorationHUD` / `RemoveExplorationHUD` 基础上增加：

```cpp
UFUNCTION(BlueprintCallable, Category = "HUD|Development", meta = (DevelopmentOnly))
void RequestRebuildExplorationHUDForPhase2Test();
```

- HUD 重建函数同样必须在实现内以 `#if UE_BUILD_SHIPPING || UE_BUILD_TEST` 记录 Warning 后直接返回；仅 Debug/Development 可用。`meta = (DevelopmentOnly)` 不替代 C++ 边界。
- HUD 函数持有自身生命周期，在内部调用现有 Remove 路径，再通过安全的 next-tick/弱 HUD 引用调用 Show 路径；不得让正在销毁的旧 Widget 在 `RemoveExplorationHUD` 后继续执行或访问自身成员。
- 重建对象是整个 `ExplorationHUD`，其销毁连带旧 Debug 子 Widget；新 ExplorationHUD/Debug Widget 重新取得 ViewModel 当前快照。

## Clamp 与计数规则

- `MaxHealth >= 0`、`MaxEnergy >= 0`、`Speed >= 0`。
- 最终保持 `0 <= Health <= MaxHealth`、`0 <= Energy <= MaxEnergy`。
- 必测：Current 低于 0、Current 高于 Max，以及 Max 降低后 Current 至少一条真实收敛路径。
- 必须区分 `PreAttributeChange`、`PreAttributeBaseChange`、`PostGameplayEffectExecute` 实际覆盖路径，不得用单一路径结果宣称完整矩阵。
- 每次测试 GE 造成的每个预期属性变化应产生一次对应 Delegate/UI 更新；若一个 GE 有多个 Modifier，必须按“每个预期属性变化”记录，不能笼统声称整个 GE 仅一个回调。
- 初始化 GE 的一次性保护不得阻止测试 GE；Re-Possess 不得重复应用初始化 GE。

## Delegate 与生命周期规则

- ViewModel 切换观察 ASC 前先从旧 ASC 对称解绑并 Reset 全部 handles；重复绑定同一 ASC 必须幂等。
- Widget 销毁后旧观察者不得收到回调；Widget 重建后应立即取得当前属性快照且只绑定一次。
- 同会话 `UnPossess → Re-Possess` 后 Owner/Avatar/Actor Info 仍正确，测试 GE 仍可生效。
- 连续开始/停止 PIE 后不得出现失效对象访问、残留 Delegate、重复 Effect、GC Warning、Error 或 Ensure。
- Character、HUD、Widget、ViewModel 不新增业务 Tick。

## 固定诊断计数与实例证据

- Character：`InitialAttributesApplySuccessCount`，只在初始化 GE 真正成功时递增；同一 Character Re-Possess 前后必须为 `1`。
- ViewModel：按 Health、MaxHealth、Energy、MaxEnergy、Speed 分别记录 ASC Attribute Delegate 回调次数；另记录 ViewModel `OnValuesUpdated` 广播总次数。
- 当前 Debug Widget：记录收到 `OnValuesUpdated` 的次数与 Widget 实例 ID。
- 旧 Debug Widget：在 Destruct 前保存实例 ID/接收次数并显式解绑；销毁后的新增接收次数必须为 `0`。
- 日志至少包含 Character 实例 ID、ASC 指针、ViewModel 指针、ExplorationHUD/Debug Widget 实例 ID、测试 GE 路径与四层计数快照。
- `WBP_AttributeDebug` 必须在 Construct 中对 Blueprint dynamic multicast 执行一次显式绑定，在 Destruct 中对同一 ViewModel 显式解绑；不得只依赖 GC，也不得每次刷新时重新 Bind。

## Segment B 用户 Editor 资产范围

用户是唯一 UE Editor 资产作者。默认只允许创建以下精确测试资产，不使用 SetByCaller 或新 Gameplay Tags：

- `Content/GameplayEffects/BP_GE_Test_HealthBelowZero.uasset`：从执行前 Health 基线施加足量负 Add，使原始结果低于 0；预期最终 Health=0，预期回调集合仅 Health。
- `Content/GameplayEffects/BP_GE_Test_HealthAboveMax.uasset`：从执行前基线施加足量正 Add，使原始结果高于当前 MaxHealth；预期最终 Health=MaxHealth，预期回调集合仅 Health。
- `Content/GameplayEffects/BP_GE_Test_LowerMaxHealth.uasset`：将 MaxHealth 降至低于当前 Health 的明确值；预期 MaxHealth 到目标值且 Health 收敛至不高于新 MaxHealth，预期回调集合为 MaxHealth 与发生收敛时的 Health。
- `Content/GameplayEffects/BP_GE_Test_EnergyBounds.uasset`：使用明确 Modifier 顺序覆盖 Energy 低于 0 与高于 MaxEnergy 的测试值；执行前必须记录该 GE 实际会触发的 Energy/MaxEnergy 变化集合，不能把多 Modifier 误计为一次回调。
- `Content/GameplayEffects/BP_GE_Test_SpeedBelowZero.uasset`：从执行前 Speed 基线施加足量负 Add；预期最终 Speed=0，预期回调集合仅 Speed。

具体 Modifier 数值必须由 Segment A/B 交接时记录的 PIE 执行前基线决定，并在执行报告中写成“基线 → 原始目标 → Clamp 后目标 → 预期属性回调集合”。

另允许：

- 修改 `Content/GameplayEffects/BP_GE_InitializeCoreAttributes.uasset`，仅当专项测试暴露真实边界问题且 Coordinator 事先确认。
- 最小修改 `Content/UI/WBP_AttributeDebug.uasset`：加入五个测试 GE 请求按钮、Re-Possess 请求按钮、HUD 重建请求按钮与诊断文本；按钮只取得 Owning Player Pawn/Controller/HUD 后调用上述受控请求接口。
- Widget 重建按钮必须调用 `AHSRHUD::RequestRebuildExplorationHUDForPhase2Test`。若 Reviewer 接受无新增 HUD 包装函数的方案，则必须先把 OwningPlayerController/HUD 保存为局部引用，并确保 Remove 后不再访问旧 Widget；默认采用 HUD C++ next-tick 安全包装方案。

不得修改地图、Level Blueprint、Exploration Character BP、HUD BP、AnimBP、Input、Config 或第三方资产。Editor 自动产生额外持久文件时停止并报告。资产作者、实际包路径与提交归属必须如实记录。

## 禁止范围

- GameplayAbility、Ability 授予/激活、Target、Cost、Cooldown、GameplayCue。
- 伤害、治疗、Meta Attribute、ExecutionCalculation、正式战斗公式。
- Interaction、Enemy、Encounter、TurnSystem、SaveGame、复制、预测、PlayerState ASC。
- Phase 3 交互系统以及后续 Phase 的任何实现。
- 新模块、插件、Gameplay Tags、Config、地图、Level Blueprint、Input、Controller、GameMode、AnimBP、第三方资产。
- 业务 Tick、Widget 写真源、批量重构、删除/重命名历史、`git reset`、`git clean`、push 或越权 Git 操作。

## 执行与验证顺序

1. 用户确认后，Implementation Agent 读取 allowlist，记录基线，设计最小入口和诊断计数。
2. 实施最小修改并更新 `tasks/execution-result.md`，逐文件记录理由、首处错误和未验证项。
3. 若修改 C++，执行本轮真实 `HSREditor Development Win64` 构建，必须记录命令、退出码及 UHT/C++/Link 实际状态；up-to-date 不得写成重新编译。
4. 用户创建并保存五个精确测试 GE，最小修改 Debug Widget 按钮/诊断文本，关闭并重开 Editor，确认类、GE 与 Widget 引用保持。
5. 在 `Map_Phase1_Exploration` PIE：应用测试 GE，核对 ASC/AttributeSet 实值、HUD 值与 Delegate/UI 计数。
6. 验证 Current < 0、Current > Max、Max 降低后的 Current 收敛，以及 Speed 非负边界。
7. 由 Debug Widget 调用 Character 的 `RequestPhase2Repossess`；接口内部保存 Controller 后执行 `UnPossess → Possess(this)`，再应用测试 GE并核对 Actor Info、初始化 GE 成功计数与回调计数。
8. 由 Debug Widget 调用 HUD 的安全重建请求；HUD 内部 Remove 整个 ExplorationHUD 并在 next tick Show，新 Debug Widget 立即显示当前快照，旧 Widget 销毁后接收次数保持 0。
9. 连续至少两轮开始/停止 PIE，检查 teardown、残留对象、Error/Ensure/GC warning。
10. Reviewer 独立复审；Coordinator 才能归档。

## 必需证据

- 最终 C++ 树对应的构建命令、退出码及实际 UHT/Compile/Link 或 up-to-date 状态。
- 五个测试 GE 的实际包路径、Duration/Modifier 配置、基线/原始目标/Clamp 后目标/预期回调集合、用户作者说明及 Editor 重开结果。
- 每个测试用例的应用次数、预期底层变化数、实际 Delegate/UI 更新数、最终 ASC/AttributeSet/HUD 数值。
- Clamp 三类边界与 Max 降低后 Current 收敛的真实结果。
- Re-Possess 前后 Owner/Avatar/Actor Info、初始化 GE 去重和测试 GE 可用证据。
- Widget 销毁/重建前后的 ExplorationHUD/Debug Widget 实例 ID、Construct 绑定、Destruct 显式解绑、旧 Widget 销毁后零新增接收和新快照证据。
- 至少两轮连续 PIE 的启动/teardown 日志；无未解释 Error、Ensure、GC 或失效对象访问。
- Build、Editor 重开、PIE、代码审查与未验证项分开记录。

## 验收清单

- [ ] 实际 diff 严格匹配当前角色 allowlist，作者与提交归属真实。
- [ ] 测试 GE 通过受控入口应用；Level Blueprint/Widget 不写真源。
- [ ] 动态属性实值与 HUD 一致，每个预期属性变化恰好一次 UI 更新。
- [ ] Current/Max/Speed 边界与 Max 降低收敛符合规则。
- [ ] Re-Possess 后 Actor Info 正确、初始化 GE 不重复、测试 GE 正常。
- [ ] Widget 销毁后无旧回调，重建后立即快照且不重复绑定。
- [ ] 连续 PIE 无残留 Delegate、失效引用、Error、Ensure 或 GC warning。
- [ ] 无业务 Tick，无 Ability/伤害/Phase 3 或其他禁止范围实现。
- [ ] 若修改 C++，本轮最终源码构建入口成功且证据边界如实。
- [ ] Reviewer 独立结论已记录；Implementation Agent 未自审归档。

## 当前执行结果

- 已完成：Coordinator 创建 P2-002 契约并锁定范围。
- 未实施：全部 C++、测试 GE、Editor 重开、PIE 与专项验证。
- 当前交接：Coordinator → Implementation Agent 首次只读复述。
- 阻塞：无；等待用户单独确认执行。
- Git：本轮不提交、不 push。

## Segment A2：Reviewer REVISE 受控修订（2026-07-17）

### 状态与用户事实确认

- P2-002 当前状态保持 `REVISE`；不得勾选验收项、归档任务或进入 P2-003。
- 用户明确确认 `Content/GameplayEffects/BP_GE_InitializeCoreAttributes.uasset` 的当前修改由用户本人完成且没有问题。Coordinator 据此追认其真实作者与本任务中的资产事实；该资产无需回退、无需再次申请授权，此项不再构成阻塞。
- 现有未提交 Source/Content 改动均作为 A2 输入保留。A2 只允许在既有 allowlist 内做最小修订，不得扩大到伤害、Ability、Phase 3 或其他禁止范围。

### A2 必须修复的实现契约

1. **Max 降低后的 Current 收敛**：使用 UE5.6 `UAttributeSet` 合法路径，使 `MaxHealth` 降低后已有 `Health` 收敛到 `[0, MaxHealth]`，并对 `MaxEnergy/Energy` 实施同构规则。必须如实声明并验证实际覆盖路径；允许在 `PostGameplayEffectExecute` 中处理并抽取必要的私有/受保护 helper，但不得扩展为伤害、治疗、Meta Attribute 或正式战斗公式。不得只以 `PreAttributeChange`/`PreAttributeBaseChange` 的候选值 Clamp 冒充已修正现存 Current。
2. **精确 GE 白名单**：`RequestApplyPhase2TestEffect` 必须比较 class 的精确 package path（完整 `/Game/GameplayEffects/BP_GE_Test_*` 包路径），禁止 `Contains`、前后缀或模糊子串匹配。
3. **诊断广播边界**：`BroadcastCurrentValues()` 必须是 Development-only 受控能力。若继续暴露为 Blueprint callable，添加 `meta=(DevelopmentOnly)` 并在 Test/Shipping 明确拒绝；更优地可收紧为 protected/private 的受控内部快照调用。不得成为正式 Gameplay 或任意 Widget 可调用的广播入口。
4. **Re-Possess 成功判定**：操作前后均判空；完成后必须验证 `OriginalController->GetPawn() == this`，ASC 与 ActorInfo 非空，且 ActorInfo 的 Owner Actor、Avatar Actor 均为 `this`，再返回成功。日志不得以仅调用了 `Possess` 代替成功证据。
5. **HUD 安全性**：所有 Owning PlayerController、Pawn、HUD 与 next-tick 弱引用路径必须判空/判有效；旧 Widget Remove 后不得继续访问自身成员。修复所有 `UE_LOG` format placeholder 与实参数量不一致或多余参数。
6. **单一快照路径**：一次 HUD 重建只能由一条明确路径向新 Widget/ViewModel提供当前值快照，避免 Construct/Show/手动广播叠加造成同一次重建双广播。报告须指出唯一快照触发点。
7. **四层诊断证据**：保留并输出 Character/ASC、逐属性 ASC Delegate、ViewModel 广播、当前 Widget 接收四层计数及对应实例 ID；旧 Widget 在 Destruct 前记录基线，销毁后新增接收增量必须为 `0`。计数必须能区分初始化快照与测试 GE 变化，不能以总数笼统替代单次预期变化。

### A2 执行报告与证据门禁

- `tasks/execution-result.md` 当前存在编码损坏且 Segment B/C 未完成。Implementation Agent 必须以 UTF-8 修复可读内容，保留可核对的历史事实，逐项区分“代码已实现”“已构建”“用户 Editor/PIE 已验证”“仍未验证”；不得自行写 `PASS`、完成归档或冒充 Reviewer。
- C++ 修订完成后，必须对最终源码树执行真实 `HSREditor Development Win64` 构建，记录完整命令、退出码以及 UHT/Compile/Link 的实际状态；up-to-date 只能如实写为 up-to-date。构建必须晚于并覆盖最终 C++ 修改。
- 用户需补齐并确认：五个测试 GE 的精确路径、Duration/Modifier 配置与作者；Editor 重开后引用保持；Current/Max/Speed Clamp（含降低 MaxHealth/MaxEnergy 后 Current 收敛）；Re-Possess；Widget 销毁/重建；至少两轮连续 PIE。
- 每个专项用例必须提供可核证的执行前后值、预期/实际 Delegate 与 UI 增量、Character/ASC/ViewModel/HUD/Widget 实例 ID；Widget 重建必须证明旧 Widget 零新增回调且新 Widget 仅经单一快照路径获得当前值。

### A2 角色交接门禁

Implementation Agent 的 A2 首次行动仍然只能读取本活动卡。读取后必须复述：剩余七项修复、精确 allowlist、最终 Editor 构建、用户证据边界、UTF-8 报告要求、禁止范围与停止条件；不得在同一首次回合继续读取或修改其他文件。复述最后一行必须精确为：

`等待用户确认执行 TASK-P2-002-A2。`

用户再次明确确认前，不得实施 A2。完成 A2 后交回 Reviewer 独立复审；只有 Coordinator 可在 Reviewer 结论与用户决定后归档。

## Segment A3：Reviewer REVISE 最小修订与补证（2026-07-17）

### 状态、输入与边界

- P2-002 继续保持 `REVISE`；A3 只修复下列剩余项并补齐证据，不得归档、不得自判 `PASS`、不得进入 P2-003。
- 已提交实现基线为 `a81277b`；现有未提交 Content 与文档均保留为 A3 输入。Implementation Agent 不得据此扩大既有 Source/Content allowlist。
- `Content/GameplayEffects/BP_GE_InitializeCoreAttributes.uasset` 的修改由用户本人完成并已明确接受，此项不再是问题，不要求回退或重新授权。

### A3 精确实现契约

1. **最终 Editor 构建**：所有 C++ 修订完成后，必须使用 Unreal `Build.bat` 对最终源码树真实执行 `HSREditor Win64 Development`。命令参数顺序固定为目标、平台、配置、项目路径及必要选项，例如：`Build.bat HSR Editor?` 不可使用；应按 UBT 支持形式记录为 `Build.bat HSREditor Win64 Development "<绝对路径>/HSR.uproject" ...`。报告必须保存完整命令、时间、退出码以及 UHT/Compile/Link 或 up-to-date 的真实输出；`HSR Development Win64` 不能替代。
2. **Current 双向收敛**：`PostGameplayEffectExecute` 对 Health 与 Energy 分别计算 `Clamped = FMath::Clamp(Current, 0.0f, Max)`；只要 `Current` 与 `Clamped` 不相等就通过合法 AttributeSet setter 写回。必须同时覆盖 `Current < 0` 与 `Current > Max`。该保证仅声明为本任务验证的 Instant GameplayEffect/base-value 执行路径；不得把未验证的 Duration/Infinite 聚合变化或全部 GAS 路径宣称为已覆盖。
3. **精确 package 白名单**：以 `TestEffect->GetOutermost()->GetName()` 或语义等价的精确 package API 取得包名，并与以下五个完整名称做相等比较，禁止比较生成类对象路径、`Contains`、前后缀或模糊匹配：`/Game/GameplayEffects/BP_GE_Test_HealthBelowZero`、`/Game/GameplayEffects/BP_GE_Test_HealthAboveMax`、`/Game/GameplayEffects/BP_GE_Test_LowerMaxHealth`、`/Game/GameplayEffects/BP_GE_Test_EnergyBounds`、`/Game/GameplayEffects/BP_GE_Test_SpeedBelowZero`。
4. **初始化日志修正**：修复 `ApplyInitialAttributes` 中 format placeholder 与实参数量不一致的多余日志参数；不得借机改变初始化语义。
5. **Re-Possess 安全与快照**：对 Controller、Pawn、ASC、ActorInfo、Owner、Avatar 逐个判空后才能比较或写日志，任何失败均安全返回且不解引用空对象。操作前后分别记录 Controller、Pawn、Character/ASC 实例 ID、ActorInfo、Owner、Avatar、`InitialAttributesApplySuccessCount` 与五项属性快照；成功必须证明 Controller 重新 Possess 当前 Character、Owner/Avatar 均为当前 Character、ActorInfo 有效且初始化成功计数仍为 `1`。
6. **HUD next-tick 验证**：`GetWorld()` 为空时记录明确日志并安全返回。next-tick 完成 `ShowExplorationHUD` 后验证 Owning PlayerController → Pawn → HUD 链仍有效并记录结果；不得为此增加第二次属性快照广播，旧 Widget Remove 后不得继续访问自身。
7. **四层诊断拆分**：新增独立 `SnapshotBroadcastCount`，且 `BroadcastCurrentValues()` 每次执行必须递增该计数。逐属性 ASC Delegate 次数、ViewModel 的 GE/属性变化广播次数、手动 snapshot 广播次数、当前 Widget 接收次数必须可区分，禁止用一个总数混算。Character 测试接口记录操作前后五属性、Character/ASC/ViewModel 实例 ID及相关计数。Widget 层由用户在 Blueprint 记录 Construct、Destruct、receive count 与实例 ID；旧 Widget 销毁后的 receive count 增量必须为 `0`。

### A3 用户 Editor 与 PIE 证据

- 用户须如实记录五个测试 GE 的实际作者、精确 package path、Duration、每个 Modifier 及数值；确认这些资产与 `WBP_AttributeDebug` 保存后关闭并重开 Editor，引用仍保持。
- 每个 GE 用例分别记录：执行前五属性、原始目标、期望 Clamp、执行后五属性、预期/实际逐属性 Delegate 增量、ViewModel 属性变化广播增量、`SnapshotBroadcastCount` 增量、当前 Widget UI receive 增量。GE 变化与 snapshot 必须分开，不得用累计总数替代本次增量。
- Re-Possess 证据必须包含前后 Controller/Pawn/Character/ASC/ActorInfo/Owner/Avatar/初始化计数/五属性；完成后再次应用允许的测试 GE，证明链路仍可用。
- Widget 重建证据必须包含旧/新 HUD 与 Widget 实例 ID、Construct/Destruct/receive count；旧 Widget 销毁后新增接收增量为 `0`，新 Widget 只经唯一快照路径取得当前值。
- 至少执行两轮连续 PIE 启动与停止，记录 teardown，并确认没有未解释的 Error、Ensure、GC warning、残留 Delegate 或失效对象访问。

### A3 报告、状态与交接门禁

- Implementation Agent 必须以 UTF-8 更新 `tasks/execution-result.md`，如实区分已实现、最终构建、用户 Editor/PIE 证据及未验证项，并同步任务状态材料；不得自判 `PASS`、替 Reviewer 勾选验收或进入 P2-003。
- A3 首次行动只能读取 `tasks/active-task.md`。随后必须复述：上述七项实现契约、最终 `Build.bat HSREditor Win64 Development`、用户五 GE/Editor/PIE 证据、UTF-8 报告、既有 allowlist、禁止范围与停止条件；同一首次回合不得读取或修改其他文件。
- 首次复述最后一行必须精确为：

`等待用户确认执行 TASK-P2-002-A3。`

用户再次明确确认前不得实施。A3 完成后只交回 Reviewer 独立复审；P2-003 仍不得开始。
