# Phase 2 详细执行计划：GAS 基础接入

> 状态：Phase 2 进行中；P2-001 已由用户验收归档，P2-002 已创建任务卡但尚未实施。
>
> 当前前置门禁：Phase 1 已完成并判定为 `Ready`；P1-001～P1-006 已归档。
>
> 阶段内容真源：[`phase-roadmap-0-20.md`](phase-roadmap-0-20.md) 中的 Phase 2。本文件负责把阶段目标拆分为可串行执行、独立验收的最小连贯垂直切片，不扩大到 Ability、伤害或 Phase 3。

## 1. 当前阶段、门禁与证据边界

- `PROJECT_STATE.md`、`todo_plan.md` 和 `worklog.md` 已记录 Phase 1 最终状态为 `Ready`。
- `HSR.uproject` 已启用 `GameplayAbilities` 插件。
- `Source/HSR/HSR.Build.cs` 已声明 `GameplayAbilities`、`GameplayTags` 和 `GameplayTasks` 依赖；Phase 2 默认不再修改插件或模块配置。
- P2-001 已建立 `IAbilitySystemInterface`、ASC、Core AttributeSet、Actor Info、初始化 GE 与属性 Delegate/UI 初始化闭环。
- 当前状态是“P2-001 已以 `USER ACCEPTED` 归档，P2-002 仅完成规划”；计划、代码或资产存在不能代替 P2-002 专项 PIE 证据。

门禁判定：**Phase 1 → Phase 2 为 `Ready`；Phase 2 功能本身为 `Not verified`。**

## 2. 阶段目标与唯一阶段可见结果

建立角色自持 ASC 的最小 GAS 属性闭环：

- `AHSRCharacterBase` 持有 ASC 与核心 AttributeSet，并实现 `IAbilitySystemInterface`。
- 当前单机阶段明确使用 `Owner Actor = Character`、`Avatar Actor = Character`。
- 在正确运行期入口初始化 Ability Actor Info，并保证重复入口幂等。
- 使用一个初始化 GameplayEffect 写入 Health、MaxHealth、Energy、MaxEnergy、Speed。
- AttributeSet 维护 Health/MaxHealth 与 Energy/MaxEnergy 的合法边界。
- Attribute ViewModel 订阅 GAS Attribute Change Delegate，把属性投影给调试 HUD。
- Widget 不拥有 Gameplay 真源、不修改属性、不使用 Tick 轮询。

阶段唯一可见结果：

> 进入 `Map_Phase1_Exploration` 的 PIE 后，调试 HUD 显示五项 GAS 属性；一个测试 Effect 改变属性后 HUD 自动刷新；重新 Possess、重复初始化和 Widget 重建不会重复应用 GE、重复回调或访问失效对象。

## 3. 架构边界与数据流

### 3.1 所有权

- `AHSRCharacterBase`：ASC 与 AttributeSet 的 UObject/Actor 生命周期所有者。
- `UHSRAbilitySystemComponent`：GAS 运行入口、Actor Info、Active Effect 与 Attribute Delegate 的提供者。
- `UHSRCoreAttributeSet`：五项 GAS Runtime 属性及本阶段的边界约束；不推进战斗流程。
- 初始化 GE：只描述初始属性如何写入，不是 SaveGame、角色成长数据或跨场景持久真源。
- `UHSRAttributeViewModel`：观察 ASC 并把属性变化转换为 UI 可消费数据；不写回 Gameplay 状态。
- `AHSRHUD`：继续只负责探索 Widget 的单实例创建与销毁。
- `WBP_AttributeDebug`：只负责布局和显示。

### 3.2 主数据流

```text
Character 构造默认子对象 ASC + AttributeSet
→ 运行期 InitializeAbilityActorInfo(Character, Character)
→ ApplyInitialAttributes（至多一次）
→ GE Modifier 写入 AttributeSet
→ AttributeSet 执行合法边界处理
→ ASC 广播 Attribute Change Delegate
→ Attribute ViewModel 更新只读 UI 数据
→ WBP_AttributeDebug 刷新文本/进度条
```

### 3.3 失败数据流

- ASC、Actor Info 或初始化 GE 无效时：记录包含对象和入口信息的明确日志，不静默伪装成功。
- 重复初始化时：不得重复应用初始化 GE或重复绑定 Delegate。
- Widget/ViewModel 销毁或切换观察目标时：先解绑旧 ASC，再释放引用。
- Max 值下降或 Effect 写入越界值时：属性回到合法区间，UI 显示最终 Runtime 值。

## 4. 全阶段执行规则

1. 任一时刻只允许一张活动任务卡存在于 `tasks/active-task.md`。
2. 每个工作包必须完成实施、执行报告、独立审查或用户明确验收、归档和状态同步后，才能创建下一张任务卡。
3. 未列入当前任务卡允许清单的文件和资产默认禁止修改；扩大范围必须停止并请求授权。
4. 不按类或单文件机械拆卡；C++、Editor 资产、PIE 与文档可以为了同一个可见结果组成一个工作包。
5. C++ 任务必须取得本轮真实 UHT、C++ 编译和 Link 证据；旧的 up-to-date 结果不能冒充新代码验证。
6. Editor 验证必须区分资产存在、引用保存、Editor 重开、PIE 主路径和失败路径。
7. Blueprint/UMG 只负责派生配置、GE 资产配置、布局和显示；ASC 生命周期、Actor Info、幂等保护、属性边界和 Delegate 生命周期不得隐藏在蓝图中。
8. 禁止使用 Character Tick、HUD Tick、Widget Tick 或 ViewModel Tick 轮询属性。
9. Runtime 属性只存在于 ASC/AttributeSet；Widget、Blueprint 默认值、构造硬编码和初始化 GE 不得形成多个互相竞争的真源。
10. 当前保持单机且不复制；不得为了未来联机提前把 ASC 迁入 PlayerState，或实现 RPC、Prediction、RepNotify。
11. 实施前若 UE5.6 GAS API 行为不确定，必须检查实际引擎头文件、官方文档或编译错误，不能猜测接口。
12. 每个工作包使用“Coordinator → Implementation → Reviewer → Coordinator”的显式交接链；Phase 收尾再交接 Teacher。

### 4.1 本阶段固定的 UE5.6 API 路径

- `IAbilitySystemInterface::GetAbilitySystemComponent() const` 是角色对外提供 ASC 的统一入口。
- Actor Info 使用 `UAbilitySystemComponent::InitAbilityActorInfo(OwnerActor, AvatarActor)`；本阶段两个参数都明确传当前 Character。
- 初始化 GE 固定使用 `MakeEffectContext` → `MakeOutgoingSpec` → `ApplyGameplayEffectSpecToSelf` 路径；保存返回 handle，并且只以 `WasSuccessfullyApplied()` 判断 Instant GE 应用成功，严禁以 `IsValid()` 作为成功判据；只有成功后才能设置初始化成功标志。不再并行保留 Blueprint 隐式初始化路径。
- 属性观察使用 `GetGameplayAttributeValueChangeDelegate(FGameplayAttribute)`；不使用 UE5.6 已建议替代的旧注册入口。
- Actor Info 初始化、初始化 GE 应用、Attribute Delegate 绑定是三个独立步骤，各自拥有独立的状态、日志与幂等判断。

## 5. 工作包总览

| 任务 | 唯一目标 | 主要产物 | 核心验收 |
|---|---|---|---|
| P2-001 | 建立初始化后可见的 GAS 属性闭环 | ASC、Core AttributeSet、Actor Info、初始化 GE、Attribute ViewModel、Debug HUD | PIE 显示五项初始化属性；重复初始化不叠加；无 Tick |
| P2-002（`PASS`） | 证明属性变化、边界和生命周期闭环可靠 | 测试 GE、Delegate/解绑完善、重建与失败路径证据 | Effect 改值后 UI 恰好刷新一次；Clamp、Re-Possess、Widget 重建和连续 PIE 已补证 |
| P2-003（规划中） | 完成工程、教学与阶段收尾 | 资产作者提交、最终回归、Teacher 产物、文档与 Coordinator 收尾 | 工程 Gate、学习 Gate、角色交接和 Git 交付齐全 |

## 6. P2-001：最小 GAS 属性初始化可见闭环

### 6.1 唯一可验收结果

进入探索地图 PIE 后，当前探索角色具有有效 ASC/Actor Info，并通过初始化 GE 获得五项核心属性；Debug HUD 显示与 ASC 直接读取一致的初值。重复调用初始化入口不会叠加数值或重复绑定。

### 6.2 建议允许的 C++ 文件

- `Source/HSR/Character/HSRCharacterBase.h`
- `Source/HSR/Character/HSRCharacterBase.cpp`
- `Source/HSR/GAS/HSRAbilitySystemComponent.h`
- `Source/HSR/GAS/HSRAbilitySystemComponent.cpp`
- `Source/HSR/GAS/Attribute/HSRCoreAttributeSet.h`
- `Source/HSR/GAS/Attribute/HSRCoreAttributeSet.cpp`
- `Source/HSR/UI/HSRAttributeViewModel.h`
- `Source/HSR/UI/HSRAttributeViewModel.cpp`
- 仅在真实集成需要且任务卡明确授权时修改 `Source/HSR/UI/HSRUserWidget.h/.cpp` 或 `Source/HSR/UI/HSRHUD.h/.cpp`。
- 当前任务执行报告与明确列出的归档文件。

`Source/HSR/HSR.Build.cs` 和 `HSR.uproject` 默认为只读；现有依赖已满足 Phase 2。

### 6.3 Codex 工作

- 让 `AHSRCharacterBase` 实现 `IAbilitySystemInterface`。
- 在构造函数中以 `CreateDefaultSubobject` 创建 ASC 与 AttributeSet。
- 通过 `UPROPERTY`/`TObjectPtr` 保护 UObject 引用，并保持 Blueprint 暴露最小化。
- 提供集中且幂等的 `InitializeAbilityActorInfo` 与 `ApplyInitialAttributes` 入口。
- 当前 Owner=Avatar=self 模型以 `BeginPlay` 作为最小运行期初始化入口；如果任务实施确需同时覆盖 `PossessedBy`，必须先在任务卡写明调用顺序，并保证该入口只刷新/核验 Actor Info，不重复应用 GE 或绑定 Delegate。
- 明确本阶段 Owner/Avatar 都是当前 Character，但代码与文档不混淆两个概念。
- 定义 Health、MaxHealth、Energy、MaxEnergy、Speed 五项 `FGameplayAttributeData` 与访问器。
- 为初始化 GE 使用每个 Pawn 实例独立的明确初始化状态；不得依赖 Instant GE 的 Active Handle 去重。
- P2-001 只建立基本安全底座：`MaxHealth >= 0`、`MaxEnergy >= 0`、`Speed >= 0`，初始化后 `0 <= Health <= MaxHealth`、`0 <= Energy <= MaxEnergy`；不加入伤害、治疗或 Meta Attribute。
- P2-001 如实说明 `PreAttributeChange`、`PreAttributeBaseChange` 和 `PostGameplayEffectExecute` 实际覆盖的变化路径，不声称已证明完整 Clamp 矩阵。Max 降低后的 Current 收敛、动态 Effect 和完整变化路径专项由 P2-002 验证。
- 实现 Attribute ViewModel 的绑定、初始快照推送和解绑接口；每个属性保存 `FDelegateHandle`，重复绑定前检查或解绑，在明确 teardown/`EndPlay` 路径从原 ASC 对称 `Remove` 并 Reset。
- 为缺 ASC、缺 Actor Info、缺初始化 GE 和重复入口提供可定位日志。
- 不改变 Phase 1 的输入、控制模式、相机、动画和 HUD 单实例不变量。

### 6.4 用户 Editor 工作

1. 创建 `Content/GameplayEffects/BP_GE_InitializeCoreAttributes.uasset`；`BP_` 前缀是用户对 Blueprint 特殊类的命名选择。
2. 将 GE 设为适合初始化的 Effect，并为五项属性配置原创灰盒初值；Modifier 运算类型和预期结果必须记录。
3. 在 `BP_HSRExplorationCharacter` 的 C++ 暴露属性中绑定初始化 GE。
4. 创建 `Content/UI/WBP_AttributeDebug.uasset`，显示 Health/MaxHealth、Energy/MaxEnergy、Speed。
5. 将调试属性区域接入当前探索 HUD；不得复制或拥有属性真源。
6. 保存全部资产，关闭并重新打开 Editor，检查父类、GE、Widget 与角色引用。

任务卡创建时必须根据 Content Browser 的真实现有路径写出 `BP_HSRExplorationCharacter` 和当前探索 HUD Widget 的准确资产路径；本计划不凭历史文本猜测资产位置。

### 6.5 验证证据

- 新增源码触发真实 UHT、C++ 编译和 Link，`HSREditor Development Win64` 退出码为 0。
- Editor 重开后 C++ 类可加载，GE、角色 Blueprint、HUD/Widget 引用保持。
- PIE 中 ASC 有效，Owner/Avatar 都指向当前角色，Actor Info 有效。
- HUD 五项数值与 ASC/AttributeSet 直接读取一致。
- Actor Info、初始化 GE、Delegate 三个协调函数具有独立、可审查的同实例幂等保护；若没有合法受控入口，P2-001 不要求重复调用专项 PIE 证据，实证留给 P2-002，且禁止使用 Level Blueprint 直接调用核心初始化函数。
- 真正新建的 Character/Pawn 实例拥有自己的 ASC/AttributeSet，并独立初始化一次；不得继承旧 Pawn 的 Delegate 或 Effect 上下文。
- Character、HUD、Widget、ViewModel 没有新增业务 Tick。
- Output Log 没有未解释的 Error、Ensure、失效对象访问或重复绑定警告。

### 6.6 失败路径

至少真实验证一条：临时清空角色 Blueprint 的初始化 GE 引用后进入 PIE。预期为明确 Warning、角色与 UI 安全存在、不崩溃、不伪装初始化成功；恢复引用后重新验证主路径。临时验证改动不得作为错误配置提交。

### 6.7 明确非目标

- GameplayAbility、Ability 授予或激活。
- 伤害、治疗、护盾、Meta Attribute、ExecutionCalculation。
- Cost、Cooldown、GameplayCue 和新的 Gameplay Tags 语义。
- 战斗、TurnSystem、敌人、交互、SaveGame、复制与预测。
- 正式 UI、美术、动画或第三方资产。

## 7. P2-002：属性变化、边界与生命周期专项闭环

### 7.1 前置门禁

- P2-001 已完成实施、审查和归档。
- 初始化属性在 Editor 重开后的 PIE 中可见。
- Actor Info、初始化 GE 和初始 Delegate 绑定有真实证据。

### 7.2 唯一可验收结果

一个测试 GameplayEffect 改变 Health、Energy 或 Speed 后，底层属性进入合法区间，Debug HUD 恰好刷新一次；重新 Possess、重建 Widget 和连续 PIE 不会产生重复 Effect、重复回调或失效引用。

### 7.3 建议允许范围

- P2-001 创建的 GAS、Attribute 和 ViewModel 源文件，仅限修复专项验证暴露的问题。
- `Source/HSR/Character/HSRCharacterBase.h/.cpp`，仅限 Actor Info/生命周期完善。
- `Source/HSR/UI/HSRHUD.h/.cpp` 与 `Source/HSR/UI/HSRUserWidget.h/.cpp`，仅限 Widget/ViewModel 绑定和解绑生命周期。
- `Content/GameplayEffects/BP_GE_Test_HealthBelowZero.uasset`
- `Content/GameplayEffects/BP_GE_Test_HealthAboveMax.uasset`
- `Content/GameplayEffects/BP_GE_Test_LowerMaxHealth.uasset`
- `Content/GameplayEffects/BP_GE_Test_EnergyBounds.uasset`
- `Content/GameplayEffects/BP_GE_Test_SpeedBelowZero.uasset`
- `Content/GameplayEffects/BP_GE_InitializeCoreAttributes.uasset`，仅限经审查确认的边界测试修正。
- `Content/UI/WBP_AttributeDebug.uasset`
- 当前任务执行报告与明确列出的归档文件。

不修改 Input、GameMode、Map、AnimBP、Config、模块依赖或第三方资产。

### 7.4 Codex 工作

- 完成或修正 ViewModel 对五项 Attribute Delegate 的句柄管理。
- 切换观察对象或销毁时先解绑旧 ASC；重复绑定同一 ASC 必须幂等。
- ViewModel/UI 不缓存无保护的 ASC 裸指针；采用与 UObject 生命周期一致的受保护或弱引用，并仍执行显式解绑。
- 确保初始化 GE 的“一生效一次”保护不阻止后续测试 GE 正常修改属性。
- 验证 Health/MaxHealth 与 Energy/MaxEnergy 在 Current/Max 变化后的合法关系。
- 在 `AHSRCharacterBase` 提供 development-only `RequestApplyPhase2TestEffect(TSubclassOf<UGameplayEffect>)` 和 `RequestPhase2Repossess()`；前者只接受任务卡五个精确包路径，后者内部保存当前 Controller 后执行 `UnPossess → Possess(this)`。
- 在 `AHSRHUD` 提供 development-only 安全重建请求，内部沿现有 Remove/Show 路径以 next-tick/弱引用重建整个 ExplorationHUD；旧 Widget 不在移除后继续访问自身。
- 三个受控接口仅允许 Debug/Development；实现必须以 `#if UE_BUILD_SHIPPING || UE_BUILD_TEST` 记录 Warning 并拒绝执行（`bool` 接口返回 `false`，HUD `void` 接口直接返回）。`meta = (DevelopmentOnly)` 不替代该 C++ 配置边界。
- 保持 UI 单向观察：Widget 按钮只调用 Character/HUD 请求接口，不直接 Apply GE、Set 属性或承担去重；禁止 Level Blueprint 作为测试真源。
- 固定诊断层级：每属性 ASC Delegate 次数、ViewModel 广播次数、当前 Widget 接收次数、旧 Widget 销毁后接收次数，并记录 Character/ASC/ViewModel/Widget 实例标识。
- 记录 Actor Info、Effect 应用、Attribute 实值和 Delegate 回调次数所需的最小诊断证据；不建立 Phase 19 级完整 Debug 框架。

### 7.5 用户 Editor 与人工验证

1. 创建任务卡列出的五个精确测试 GE；不使用 SetByCaller 或新 Gameplay Tags，Modifier 数值和预期回调集合由执行前基线决定。
2. 最小修改 `WBP_AttributeDebug`，加入测试请求、Re-Possess、HUD 重建按钮和诊断文本；Blueprint dynamic multicast 在 Construct 显式绑定、Destruct 显式解绑。
3. 使用 Character/HUD 受控请求接口，禁止 Level Blueprint 或 Widget 直接写真源。
3. 观察 ASC/AttributeSet 实值与 HUD 是否同步。
4. 执行同会话 UnPossess → Re-Possess，并再次施加测试 Effect。
5. 移除并重新显示探索 HUD，确认新 Widget 获得当前快照，旧 Widget 不再回调。
6. 连续开始/停止 PIE，检查对象销毁、Delegate、Effect 与 UI 是否残留。
7. 测试 Current 超过 Max、低于 0，以及 Max 降低后的至少一条边界路径。

### 7.6 必需证据

- 若修改 C++，再次取得本轮真实 UHT/C++/Link 成功证据。
- 测试 Effect 每次应用只产生预期的一次底层变化和一次 UI 更新。
- Delegate 回调次数、Widget 实例和 ASC 指针证据足以排除重复绑定或绑错对象。
- Re-Possess 后 Owner/Avatar/Actor Info 仍正确；初始化 GE 不重放，测试 GE 仍可工作。
- 若通过销毁并生成新 Pawn 验证角色重建，新 Pawn 必须独立初始化，UI 必须切换到新 ASC，旧 Pawn 不再产生回调。
- Widget 销毁后旧观察者不接收回调；重建后立即显示当前快照。
- 属性越界时最终值符合任务卡写明的规则。
- 连续 PIE 无未解释 Error、Ensure、访问已销毁对象或 GC 警告。

### 7.7 真实 Debug 练习

预设现象：HUD 一直显示 0，日志显示初始化入口调用两次，测试 Effect 后 UI 不刷新。

必须按以下层次定位，不能随机修改：

1. 记录 ASC、Owner、Avatar、Actor Info、初始化标志和 GE class。
2. 直接读取 AttributeSet 实值，区分“GE 没写入”和“UI 没观察到”。
3. 检查 Actor Info 是否先于 GE、AttributeSet 是否注册、GE 是否成功应用。
4. 检查 ViewModel 是否绑定同一个 ASC/Attribute、是否主动推送初始快照。
5. 检查旧 Widget 是否解绑、重复入口是否生成多个 Delegate。
6. 最小修复后沿原路径复验主路径、重复路径和缺失 GE 路径。

### 7.8 明确非目标

- 不把测试 GE 扩展成正式攻击、治疗或技能。
- 不加入 GameplayAbility、Target、Cost、Cooldown、伤害公式或回合制持续时间。
- 不创建通用调试控制台、自动化测试框架或正式战斗 UI。
- 不迁移 ASC 所有权，不实现 respawn 系统；只验证当前项目可真实执行的角色重用、Re-Possess、Widget 重建和 PIE 生命周期。

## 8. P2-003：教学、最终回归与阶段收尾

> 当前状态（2026-07-17）：P2-002 最终 Reviewer `PASS` 并归档；P2-003 已建立唯一活动任务卡，但只完成规划。首个门禁是用户本人提交五个测试 GE、`WBP_AttributeDebug` 与已接受的 `BP_GE_InitializeCoreAttributes` 修改；不得由 Coordinator 冒充作者或未经另行授权代提交。

### 8.1 唯一可验收结果

Phase 2 的工程证据、源码机制理解、真实 Debug 练习、文档与角色 Git 交付全部闭合；只有全部门禁满足时，Phase 2 才可标记为 `Ready`。

### 8.2 强制顺序

1. 用户资产作者先提交 P2-003 卡列出的七个资产并回传 commit hash。
2. Coordinator 核对 P2-001/P2-002 的源码、资产作者、执行者与 Reviewer commits、Build、Editor 重开、PIE 主路径和失败路径，并将真实材料交接给 Teacher。
3. Teacher 只先完成概念教学、源码阅读和出题；不得在用户作答前评估掌握度或提交。
4. 用户保留原始复述、预测题和真实 Debug 练习答案。
5. Teacher 收到原始答案后纠正、评估掌握度、更新允许文档并创建独立 Teacher commit。
6. Reviewer 独立只读审查，独立创建 `TASK-P2-003-final-review.md` 并提交；Coordinator 不得代写或修改 Reviewer 结论。
7. 仅在 Reviewer `PASS` 后，Coordinator 复核工程 Gate、学习 Gate、文档与工作树，创建 Phase 收尾 commit，并按项目规则尝试 push，不 force push。Ready 与 push 交付状态分开记录；push 失败必须保留真实错误。

### 8.3 Teacher 教学范围

- ASC、AttributeSet、GameplayEffect、Attribute Delegate 的职责。
- Owner Actor、Avatar Actor 与 Ability Actor Info。
- 构造期默认子对象与运行期 Actor Info 初始化的区别。
- 初始化 GE 幂等与后续普通 GE 可重复应用的区别。
- AttributeSet Current/Max 边界与 UI 最终值。
- Delegate 初始快照、重复绑定、解绑与 GC 生命周期。
- UI 观察者与 Gameplay 真源的边界。

预测题：

> Health 初值为 100。重新 Possess 后初始化入口再次运行；初始化 GE 分别使用 Add 和 Override 时，没有幂等保护会出现什么结果？为什么 HUD 去重不能修复底层错误？如果 Owner 有效但 Avatar 为空，哪些对象可能看似存在，哪些后续行为仍不可靠？

用户完成后应能独立复述：

- ASC 是 GAS 运行入口，AttributeSet 保存受 GAS 管理的 Runtime 属性，GE 描述属性变化，Delegate 通知观察者。
- Owner 是逻辑拥有者，Avatar 是当前承载表现与交互的 Actor；本阶段相同不代表概念相同。
- 构造期创建默认子对象，运行期初始化 Actor Info；初始化 GE 必须只应用一次。
- HUD/ViewModel 不拥有或修正 Gameplay 真源，也不 Tick 轮询。
- 属性 UI 故障按 `ASC/Actor Info → GE → AttributeSet → Delegate → ViewModel/Widget` 分层定位。

### 8.4 Phase 2 最终门禁

只有同时满足以下条件，Phase 2 才可判定为 `Ready`：

- Phase 1 前置门禁仍保持有效。
- 最终 `HSREditor Development Win64` 构建成功，无 UHT、C++ 或 Link 错误。
- ASC 与 AttributeSet 由角色默认子对象持有并受反射/GC 正确保护。
- `GetAbilitySystemComponent()` 返回正确 ASC。
- Owner/Avatar/Actor Info 在当前生命周期路径中正确且重复入口幂等。
- 初始化 GE 只应用一次；缺失 GE 安全失败并有明确证据。
- Health/MaxHealth、Energy/MaxEnergy、Speed 初值正确，Current/Max 边界符合记录规则。
- 测试 Effect 可以改变属性，并由 Delegate 驱动 HUD 更新，无 Widget Tick。
- Re-Possess、Widget 销毁/重建和连续 PIE 不产生重复 Delegate、重复初始化或失效对象访问。
- Editor 重开后所有 C++ 父类、角色 BP、GE 与 Widget 引用保持。
- PIE 主路径与至少一条失败路径有真实证据，Output Log 无未解释 Error/Ensure。
- `worklog.md`、`todo_plan.md`、`learning-journal.md`、`docs/gas-notes.md`、`PROJECT_STATE.md`、`README.md` 与真实证据一致。
- Implementation、Reviewer、Coordinator、资产作者和 Teacher 均完成自己的真实产物、显式交接和独立 commit。
- Reviewer 独立完成 final-review 与审查 commit，且结论为 `PASS`。
- Coordinator 仅在 Reviewer `PASS` 后完成 Phase 收尾 commit；工程/学习 `Ready` 与 push 交付状态分开记录，并如实记录 push 成功或失败。

如果只有源码或资产存在，状态仍为 `Not verified`；如果只有编译成功但缺 Editor/PIE/失败路径或教学证据，也不能标记 `Ready`。

## 9. 文档归档要求

- `worklog.md`：记录每个工作包的真实 Build、Editor、PIE、失败路径、第一处真实错误与修复复验。
- `todo_plan.md`：只勾选已经产生验收证据的 Phase 2 子项。
- `learning-journal.md`：记录 Owner/Avatar、Actor Info、GE 幂等、Delegate 生命周期与用户练习结果。
- `docs/gas-notes.md`：记录最终采用的数据流、属性边界、初始化入口和真实 Debug 经验。
- `PROJECT_STATE.md`：记录当前活动任务、最新 Gate、commit 和未验证风险。
- `README.md`：只在 Phase 2 可见闭环真实成立后更新功能说明。
- `tasks/archive/`：保留活动任务、累计执行结果、审查与角色交接产物。
- `docs/phase-1-project-baseline.md` 中“Phase 1B 最小 GAS”的旧命名与当前 Phase 2 真源冲突；后续文档整理可将其明确标为历史资料，但该整理不得冒充 Phase 2 实施证据。

## 10. 全阶段风险

- **Actor Info 时序错误**：ASC/AttributeSet 对象存在不等于 GAS 上下文有效。
- **初始化重复**：Add 型初始化 GE 会直接叠加；Override 也可能掩盖重复调用和 Delegate 重绑。
- **多头真源**：构造默认值、Blueprint、GE 与 Widget 同时写属性会让问题不可解释。
- **AttributeSet 未正确注册或保护**：可能导致属性查询失败、GC 风险或 Editor/PIE 生命周期问题。
- **Delegate 生命周期**：漏解绑、重复绑定、绑错 ASC 或不推初始快照都会造成假 UI 故障。
- **Clamp 覆盖误判**：`PostGameplayEffectExecute` 不能代表所有 Duration/Infinite 聚合变化都已受保护；本阶段必须按实际入口声明覆盖边界。
- **Current/Max 顺序**：Max 变化后 Current 必须进入合法区间，UI 显示最终值而非中间值。
- **现有系统回归**：GAS 接入不得破坏移动、相机、输入模式、HUD 单实例、AnimBP 与 Re-Possess。
- **过度网络设计**：当前单机角色自持 ASC 是明确决策；PlayerState ASC、复制和预测另行设计。
- **测试入口污染**：临时 Level Blueprint 或 Widget 按钮不得成为正式 Gameplay 规则真源。

## 11. Phase 2 全阶段明确非目标

- GameplayAbility、攻击、治疗、Target、Cost、Cooldown、CommitAbility。
- Damage、Defense、Crit、Meta Attribute 与 ExecutionCalculation。
- GameplayCue、技能动画、音效、VFX 与正式战斗表现。
- TurnSystem、Encounter、Battle Map、敌人 AI 与 Phase 3 交互系统。
- Status、Buff/Debuff、回合持续时间、弱点、韧性或击破。
- DataAsset 角色定义、成长、装备、SaveGame、背包与奖励。
- PlayerState ASC、网络复制、RPC、Prediction 与多人权威。
- 新 Runtime 模块、第三方依赖、正式美术 UI 或未经授权资产。

## 12. 计划完成后的唯一下一步

当前唯一下一步是用户完成 `TASK-P2-003` Gate 0 的七个资产作者提交并回传 commit hash；随后严格执行 Coordinator 核对/交接 → Teacher 教学/出题 → 用户原始作答 → Teacher 纠正/评估/提交 → Reviewer 独立审查/提交 → Reviewer `PASS` 后 Coordinator 收尾。进入 Phase 3 以前必须满足 Ready Gate；push 失败不得伪造为已交付。
