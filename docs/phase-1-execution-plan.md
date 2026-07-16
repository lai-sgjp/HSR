# Phase 1 详细执行计划：第三人称探索角色

> 状态：已规划，尚未实施。
>
> 当前前置门禁：Phase 0 已完成并判定为 `Ready`。
>
> 阶段内容真源：[`phase-roadmap-0-20.md`](phase-roadmap-0-20.md) 中的 Phase 1。本文件负责将其拆分为可串行执行、独立验收的任务，不扩大阶段范围。旧的 [`phase-1-project-baseline.md`](phase-1-project-baseline.md) 仅作为历史聚合资料，不作为本阶段实施真源。

## 1. 阶段目标

从 Blank C++ 项目建立完整但最小的第三人称探索控制闭环：

- 第三人称 Character 与相机组件。
- PlayerController、Possession 与控制模式。
- Enhanced Input 移动、观察、跳跃和交互占位入口。
- GameMode、派生 Blueprint 与独立灰盒探索地图。
- 单实例、无 Tick 的占位探索 HUD。
- 只消费 Gameplay 状态的最小 AnimBP。
- 对 UIOnly、重复 Mapping Context、重新 Possess 和缺失配置执行专项验证。

阶段可见结果：玩家进入灰盒探索地图后，可以稳定移动、观察和跳跃；切换到 UIOnly 后探索输入停止，恢复 Exploration 后输入继续工作；HUD 只创建一次；输入、UI 和角色逻辑均不依赖自定义 Tick 轮询。

## 2. 全阶段执行规则

1. 任一时刻只允许一张活动任务卡存在于 `tasks/active-task.md`。
2. 当前任务完成实施、执行报告、审查或用户证据、归档和状态同步后，协调者才可创建下一张任务卡。
3. 后续任务的文件、类和资产不得提前创建。
4. 未列入当前任务卡允许清单的文件和资产默认禁止修改；如需扩大范围，必须停止并请求授权。
5. 每个 C++ 任务必须取得本轮真实的 UHT、编译和链接证据；不得复用旧的 up-to-date 结果冒充新代码编译。
6. 每个 Editor 任务必须区分资产存在、Editor 重开、PIE 主路径和失败路径证据。
7. 计划、代码存在和编译成功均不能代替 PIE 行为证据。
8. C++ 负责组件、生命周期、输入数据流和控制模式不变量；Blueprint/UMG 只负责派生配置、资产绑定、动画与显示。
9. 禁止用 Character Tick、Controller Tick 或 Widget Tick 轮询输入和业务状态。AnimBP 的正常动画更新仅可用于轻量表现计算。
10. Phase 1 不接入 GAS、交互扫描、敌人、Encounter、战斗、SaveGame、网络复制或正式表现资产。
11. 每个人格完成本轮职责后，必须先提交自己的实际产物，再向下一人格执行显式工作交接；聊天中的口头说明不能代替仓库记录和角色 commit。
12. 每个子任务固定采用“协调者 → 执行者 → 审查者 → 协调者”的交接链。执行者不得自审，审查者不得代替协调者归档或创建下一任务。
13. Phase 1 全部工程子任务通过后，协调者必须把源码、diff、编译/PIE 证据和学习记录交接给 Teacher。Teacher 完成教学、源码机制复盘、练习与面试沉淀并提交自己的产物后，协调者才能执行 Phase 收尾 commit 与 push。

### 2.1 强制角色交接链

1. **Coordinator → Implementation Agent**：交付活动任务卡、允许文件、禁止范围、验收标准和协调者 commit hash。执行者只读复述并获得用户再次确认后才能开始。
2. **Implementation Agent → Reviewer**：交付执行报告、执行者 commit hash、实际 diff、构建/Editor/PIE 证据、第一处失败和未验证项。
3. **Reviewer → Coordinator**：交付独立审查结论、审查报告、审查者 commit hash，以及 PASS、REVISE 或 BLOCKED 后续决定。
4. **Coordinator → 下一任务执行者**：仅在当前任务完成审查、归档、状态同步并完成协调者 commit 后，创建下一张活动任务卡并正式交接。
5. **Coordinator → Teacher（Phase 末尾）**：交付 Phase 全部源码、任务与审查 commit、最终 build/Editor/PIE 证据、learning journal 和未验证风险。
6. **Teacher → Coordinator**：交付教学记录、源码机制讲解、用户复述/练习结果、面试沉淀和 Teacher commit hash。
7. **Coordinator → Phase 收尾**：确认全部子任务、审查、归档、角色提交和 Teacher 交接齐全后，创建 Phase 收尾 commit，并按规则推送远端。

## 3. 工作包总览

| 任务 | 目标 | 主要产物 | 核心验收 |
|---|---|---|---|
| P1-001 | Character 与相机骨架 | 两个 Character 类 | 真编译、组件层级、反射/GC、无 Tick |
| P1-002 | Controller 与控制模式 | PlayerController、ControlMode | Possession 安全、模式切换幂等 |
| P1-003 | Enhanced Input | IA、IMC、输入处理 | Context/Binding 不重复、引用持久 |
| P1-004 | 探索可玩闭环 | GameMode、派生 BP、灰盒地图、HUD 与首轮集成证据 | 正确 Spawn/Possess，Move/Look/Jump、UIOnly 恢复、Context/HUD 不重复 |
| P1-005 | 动画与最终回归 | 合法动画资源、AnimBP 与剩余生命周期/帧率/失败路径证据 | 动画只负责表现，重新 Possess、连续 PIE 与缺失配置稳定 |
| P1-006 | 阶段收尾工作包 | 工程归档、Teacher 教学提交、Coordinator 最终提交与 push | 工程 Gate、学习 Gate 和全角色 Git 交付齐全 |

## 4. P1-001：角色骨架与相机组件

### 目标

创建可独立编译的角色继承体系，以及稳定的第三人称相机组件层级。本任务只证明角色和相机骨架可以构建与配置，不证明角色已经可以移动。

### 允许文件

- `Source/HSR/Character/HSRCharacterBase.h`
- `Source/HSR/Character/HSRCharacterBase.cpp`
- `Source/HSR/Character/HSRExplorationCharacter.h`
- `Source/HSR/Character/HSRExplorationCharacter.cpp`
- 当前任务执行报告与明确列出的归档文件。

### Codex 工作

- 创建 `AHSRCharacterBase`，继承 `ACharacter`。
- 创建 `AHSRExplorationCharacter`，继承角色基类。
- 使用构造函数和 `CreateDefaultSubobject` 创建 CameraBoom 与 FollowCamera。
- 配置 Capsule、CharacterMovement、SpringArm、Camera 和角色朝向策略。
- 使用最小必要的 `UPROPERTY`、`TObjectPtr` 和 Blueprint 可见性。
- 保证 `*.generated.h` 是头文件最后一个 include，并正确使用 `GENERATED_BODY()`。
- 禁用自定义 Character Tick；不在构造函数访问 World 或运行时对象。
- CharacterBase 只保存真正共享的角色职责和未来扩展边界，不提前创建 ASC。

### 用户 Editor 工作

本任务原则上无需 Editor 资产操作。编译后可创建临时派生 Blueprint，仅用于检查 C++ 类可见性与组件树，不应提交未列入任务卡的资产。

### 验收证据

- `HSREditor Development Win64` 实际触发新增文件的 UHT、C++ 编译、lib/dll 链接，退出码为 0。
- C++ 类可以用于创建 Blueprint 派生类。
- 组件附件关系为角色根组件/胶囊体 → CameraBoom → FollowCamera。
- CameraBoom、FollowCamera 引用有效，CDO 默认值合理。
- 没有 Mesh 或 AnimBP 时角色仍可安全生成。
- `PrimaryActorTick` 未启用，没有自定义 Tick。

### 学习目标

- 理解 `AActor → APawn → ACharacter` 的继承关系。
- 理解 Character 是可被控制的“身体”，Controller 是“驾驶者”。
- 能解释 Capsule、CharacterMovement、SpringArm 和 Camera 的职责。
- 能预测 `bUseControllerRotationYaw` 与 `bOrientRotationToMovement` 冲突时的表现。

### 明确非目标

Controller、Enhanced Input、GameMode、HUD、AnimBP、GAS、资产绑定和 PIE 移动验证。

## 5. P1-002：PlayerController、Possession 与 ControlMode

### 目标

建立本地玩家控制协调层，明确 Controller 与 Pawn 生命周期，并提供幂等控制模式切换。暂不创建 Input Action 或 Mapping Context 资产。

### 允许文件

- `Source/HSR/Player/HSRPlayerController.h`
- `Source/HSR/Player/HSRPlayerController.cpp`
- 仅在任务卡证明编译依赖确有需要时修改 `Source/HSR/HSR.Build.cs`。
- 当前任务执行报告与明确列出的归档文件。

### 主要类型与接口

```cpp
enum class EHSRPlayerControlMode : uint8
{
    Exploration,
    UIOnly,
    Battle
};
```

- `SetControlMode`。
- `BeginPlay`、`OnPossess`、`OnUnPossess` 中职责清晰且不重复的生命周期入口。
- Mapping Context 的添加/移除入口可以预留，但本任务不创建 IA/IMC 资产。

### Codex 工作

- 创建 HSR PlayerController。
- 将本地输入模式、Mapping Context 生命周期和 UI 协调边界集中到 Controller。
- 对 Pawn、LocalPlayer、Enhanced Input Subsystem 为空进行保护。
- 确保重复设置同一 ControlMode 不叠加副作用。
- 确保 UnPossess、重新 Possess 和地图重建不会访问过期 Pawn。
- Battle 只保留枚举和职责边界，不实现战斗模式。

### 用户 Editor 工作

无。

### 验收证据

- 本轮实际编译与链接成功。
- Pawn 为空、Pawn 类型不匹配、UnPossess 和重新 Possess 不崩溃。
- 重复调用 `SetControlMode(Exploration)` 不重复添加状态或副作用。
- UIOnly 可以安全进入没有探索 Context 的状态。
- Controller 不保存无保护的旧 Pawn 裸指针，也不承载长期玩家数据。
- 没有自定义 Tick。

### 学习目标

- 理解 GameMode 创建 Controller/Pawn、Controller Possess Pawn 的数据流。
- 区分“设置输入模式”和“添加 Mapping Context”。
- 能解释 Controller 为什么不等于 Character，以及为什么 Possession 可以重复发生。

### 明确非目标

IA/IMC 资产、移动绑定、完整 UI 输入焦点、Battle 行为和网络复制。

## 6. P1-003：Enhanced Input 资产与角色输入绑定

### 目标

建立 Move、Look、Jump 和 Interact 的完整 Enhanced Input 数据流。Interact 只保留可观察、无副作用的占位入口。

### 允许文件与资产

C++：

- `Source/HSR/Character/HSRExplorationCharacter.h`
- `Source/HSR/Character/HSRExplorationCharacter.cpp`
- 必要时修改 `Source/HSR/Player/HSRPlayerController.h/.cpp`。

Editor 资产：

- `Content/Input/IA_Move.uasset`
- `Content/Input/IA_Look.uasset`
- `Content/Input/IA_Jump.uasset`
- `Content/Input/IA_Interact.uasset`
- `Content/Input/IMC_Exploration.uasset`

### 职责边界

- Controller 是 Mapping Context 生命周期的唯一所有者。
- Character 在 `SetupPlayerInputComponent` 中绑定 Pawn 行为。
- Move 根据 Controller Yaw 计算世界前/右方向，不使用 Pitch 参与地面移动。
- Look 调用 Controller Yaw/Pitch 输入。
- Jump 使用适合按下/释放语义的触发事件。
- Interact 不创建交互组件、不扫描对象、不进入 Phase 3。

### Codex 工作

- 声明 Input Action 和 Mapping Context 的受保护 UObject 引用。
- 在单一位置绑定 Move、Look、Jump 和 Interact。
- 实现移动方向、镜头输入和跳跃入口。
- 对缺失 IA/IMC、非 EnhancedInputComponent、无 LocalPlayer Subsystem 等情况安全失败并输出可定位日志。
- 保持 Context 添加/移除对称且幂等，不使用 Tick 读取按键。

### 用户 Editor 工作

1. 创建 Move、Look、Jump、Interact 四个 Input Action。
2. 将 Move、Look 配置为 Axis2D。
3. 为 Jump、Interact 选择正确的值类型和触发事件。
4. 创建 `IMC_Exploration`。
5. 配置键盘、鼠标映射和必要的 Modifier。
6. 在任务允许的派生 Blueprint 中绑定资产引用，并回传资产路径和必要截图。

### 验收证据

- C++ 编译成功。
- Editor 重开后 IA、IMC 和引用不丢失。
- Character 与 Controller 没有重复 Action Binding。
- Mapping Context 添加/移除对称，重复模式设置不叠加 Context。
- 缺失资产时有明确日志且不崩溃。
- 重新 Possess 后一次输入只触发一次。
- Interact 触发不产生 Gameplay 副作用。

### 学习目标

- 能复述“输入设备 → IMC → IA → EnhancedInputComponent → 处理函数 → Movement/Camera”的数据流。
- 理解 Action 语义、Context 设备映射和 BindAction 执行入口的区别。
- 能解释 Jump 使用 Started/Completed 与持续 Triggered 的差异。

### 明确非目标

完整手柄适配、按键设置 UI、交互扫描、Gameplay Tags、GAS 和正式交互逻辑。

## 7. P1-004：探索可玩闭环

### 唯一可验收结果

从打开专用灰盒地图到生成并 Possess 探索角色，玩家可以 Move、Look、Jump，看到单实例探索 HUD，并完成 Exploration → UIOnly → Exploration 恢复；首次闭环中 Pawn、Action Binding、Mapping Context 与 HUD 均不重复。AnimBP 不属于本工作包，也不阻塞 Gameplay 闭环。

### C++ 范围

- `Source/HSR/Framework/HSRGameModeBase.h`
- `Source/HSR/Framework/HSRGameModeBase.cpp`
- `Source/HSR/UI/HSRHUD.h`
- `Source/HSR/UI/HSRHUD.cpp`
- `Source/HSR/UI/HSRUserWidget.h`
- `Source/HSR/UI/HSRUserWidget.cpp`
- 仅当活动任务卡逐项说明必要性时，允许调整 `Source/HSR/Player/HSRPlayerController.h/.cpp` 的 HUD/ControlMode 协调入口。

仅当活动任务卡证明现有模块依赖不足时才能修改 `Source/HSR/HSR.Build.cs`；仅当用户明确授权目标键时才能修改 `Config/DefaultEngine.ini`。不得借此扩展其他 Config。

### Editor 资产范围

- 用户已有候选：`Content/Blueprints/Character/BP_HSRExplorationCharacter.uasset`
- 用户已有候选：`Content/Blueprints/Input/BP_HSRPlayerController.uasset`
- `Content/Blueprints/Framework/BP_HSRGameMode.uasset`
- `Content/Blueprints/UI/BP_HSRHUD.uasset`（若 C++ HUD 需要派生配置）
- `Content/UI/WBP_ExplorationHUD.uasset`
- `Content/Maps/Map_Phase1_Exploration.umap`

两个现有未跟踪 Blueprint 是用户产物；规划阶段不得修改、暂存或提交。进入工作包后仍由资产作者按任务卡独立提交。

### Codex/C++ 工作

- 建立 GameMode 默认类配置边界，不让 GameMode 承担输入绑定、Widget 业务状态或跨地图持久状态。
- 建立 HUD/根 Widget 的单一幂等创建入口，正确设置 Owning Player，对空 WidgetClass、非本地 Controller 与 CreateWidget 失败安全处理。
- 保持 Controller 是 Mapping Context 生命周期唯一管理者，Character 是 Pawn Action 唯一绑定者。
- 保证 Character、Controller、HUD 与 Widget 不用自定义 Tick 轮询输入或业务状态。
- 实际触发本工作包所改 C++ 的 UHT、编译与链接；失败时保存第一处真实错误。

### 用户 Editor 工作

1. 检查并保存现有 Character/Controller 派生 Blueprint，在正确属性中绑定 IA/IMC。
2. 创建 GameMode 派生 Blueprint，配置 DefaultPawnClass、PlayerControllerClass 与 HUDClass。
3. 创建 WBP（以及需要的 HUD 派生 BP），只放占位控制提示，不拥有 Gameplay 真源。
4. 创建独立灰盒地图、地面与 PlayerStart，并在 World Settings 设置 GameMode Override。
5. 保存全部资产，关闭并重开 Editor，重新打开地图核对所有类、IA/IMC、Widget 与 GameMode 引用。
6. 运行 PIE，观察 Pawn、Possession、Move/Look/Jump、HUD 和 Output Log。
7. 执行 Exploration → UIOnly → Exploration，并重复相同模式调用，确认探索输入和焦点可以停止与恢复。
8. 连续开始/停止 PIE，确认 Pawn、Context、Binding 与 HUD 不叠加。

### Build、Editor 重开与 PIE 证据

- `HSREditor Development Win64` 对本工作包修改实际执行 UHT/C++/Link，退出码 0。
- Editor 重开后地图及所有 Blueprint、IA、IMC、HUD/Widget 引用保持。
- PIE 只生成一个目标 Pawn，由正确 Controller Possess；Move、Look、Jump 各触发一次且行为正确。
- Interact 仍为无 Gameplay 副作用占位。
- HUD 只出现一次，没有 Widget Tick；连续 PIE 不访问销毁对象。
- UIOnly 阻止探索输入，恢复 Exploration 后输入正常；重复模式调用不叠加 Context 或 UI 副作用。
- Output Log 无未解释的 Error、Ensure、重复绑定或访问已销毁对象警告。

### 失败路径

至少真实验证并记录一条：空 WidgetClass、缺 PlayerStart、错误默认类、缺 IA/IMC、无 Pawn 或错误 Pawn 类型。失败必须安全且能由首条日志、World Settings 或资产引用定位。若验证暴露真实缺陷，记录证据并由 Coordinator 决定是否在同一工作包内创建受控修复交接；不得在验证中顺手越权修复。

### 明确非目标

AnimBP、正式美术、GAS、交互扫描、Encounter、Battle Map、SaveGame、第三方资源与 Phase 2 内容。

## 8. P1-005：动画与最终回归

### 唯一可验收结果

在不改变 Gameplay 真源的前提下完成合法的 Idle/Move/Jump 占位表现，并以最终回归证明动画缺失或配置失败不会破坏移动；重新 Possess、连续 PIE、不同帧率及剩余缺失配置路径稳定。

### 前置门禁与资产范围

- 必须先确认使用 UE 默认、项目已有、原创或已有明确授权且允许提交的 Skeleton、Mesh 与 Animation；没有合法资源时停止并报告，不得下载或临时导入第三方资源。
- `Content/Characters/Animation/ABP_HSRExploration.uasset`
- `Content/Blueprints/Character/BP_HSRExplorationCharacter.uasset` 的 Mesh/AnimClass 绑定。
- 默认 Blueprint-only；如确需 C++ AnimInstance，必须由 Coordinator 先扩展任务范围和文件清单。

### 用户 Editor 与验证工作

1. 创建 AnimBP，从 Pawn/CharacterMovement 读取平面 Speed、方向与 IsFalling。
2. 建立最小 Idle/Move/Jump 状态机，绑定 Mesh 与 AnimClass，保存并重开 Editor 核对引用。
3. 在 PIE 中观察 Idle/Move/Jump 参数和状态；验证动画只消费 Gameplay 状态。
4. UnPossess 后重新 Possess，确认输入一次只触发一次且 Context/Binding/HUD 不重复。
5. 连续多次启动/停止 PIE，检查销毁对象访问和重复副作用。
6. 使用至少两个帧率限制验证移动距离、镜头与跳跃 Gameplay 不受 AnimBP 或帧率改变。
7. 验证缺 Mesh/AnimClass、错误 AnimInstance Owner 或缺输入配置中的至少一条安全路径，并记录第一处真实证据。

### 最终回归证据

- Editor 重开后 Mesh、AnimBP 与状态机引用保持。
- Idle/Move/Jump 表现与参数一致；动画缺失时 Gameplay 移动仍可用。
- 重新 Possess、连续 PIE 和不同帧率下无重复 Binding/Context/HUD、无访问销毁对象、无未解释 Error/Ensure。
- Character、Controller、Widget 无自定义 Tick；AnimBP 更新不推进输入、移动、结算或其他 Gameplay 规则。
- 最终 Development Editor 构建和完整 PIE 主路径均有新证据。

### 明确非目标

Montage、Root Motion Gameplay、战斗动画、大规模重定向、正式模型、动画驱动结算、自动化测试框架和任何 Phase 2 内容。

## 9. P1-006：阶段收尾工作包

### 唯一可验收结果

Phase 1 的工程证据、源码教学和 Git 交付全部闭合，阶段状态只能依据真实证据判定为 `Ready`、`Not verified` 或 `Blocked`。这是一个管理工作包，但内部三个人格步骤和提交不得合并。

### 内部强制顺序

1. **Coordinator 工程归档 commit**：核对全部工程工作包、源码 diff、角色/资产作者提交、最终 Build、Editor 重开、PIE 主路径与失败路径；同步 Phase 文档、PROJECT_STATE、todo、worklog、learning journal、README 和归档记录。不得把缺失证据写成通过。
2. **Coordinator → Teacher 交接**：交付 Character、Controller、Possession、Enhanced Input、GameMode、HUD、AnimBP 的最终源码与数据流，全部相关 commit hash、真实错误/修复过程及未验证风险。
3. **Teacher 技术与源码教学 commit**：讲解源码机制、反射/GC/Tick、输入与生命周期，完成用户复述、预测题、源码阅读题与真实 Debug 练习，并独立提交教学/学习/面试沉淀；Teacher 不修改 Gameplay C++、不 push。
4. **Teacher → Coordinator 交接**：提供教学产物、用户练习结果、未掌握项与 Teacher commit hash。
5. **Coordinator 最终 commit 与 push**：重新核对工程 Gate、学习 Gate、工作树、角色提交和远端目标，创建独立 Phase 收尾 commit 后按规则 push；不强制 push。

同一工作包可以统一管理上述步骤，但 Coordinator 工程归档、Teacher 教学、Coordinator 最终收尾必须是各自真实产物和独立 Git commit，不能由一个人格代交或压成一个提交。

## 10. Phase 1 最终门禁

只有同时满足以下条件，Phase 1 才可标记为 `Ready`：

- Phase 0 前置门禁保持有效。
- 最终 `HSREditor Development Win64` 构建成功，无 UHT、UBT 或 Link 错误。
- 从干净 Editor 会话打开专用灰盒探索地图后，正确生成并 Possess 探索角色。
- Move、Look、Jump 在 PIE 中稳定工作。
- Interact 只有无副作用占位入口。
- Mapping Context 由 Controller 单一管理，添加/移除对称且幂等。
- Action Binding 只在明确入口建立，重新 Possess 后不重复触发。
- UIOnly 可以阻止探索输入，恢复 Exploration 后输入正常。
- HUD 单实例、生命周期安全、无 Widget Tick。
- AnimBP 只消费表现状态，不决定 Gameplay 规则。
- Character、Controller 和 Widget 均没有自定义 Tick 轮询。
- Editor 重开后所有阶段资产引用有效。
- PIE 主路径与至少一条缺失资产或生命周期失败路径均有记录。
- `PROJECT_STATE.md`、todo、worklog、learning journal、README 和 Phase 1 文档与真实证据一致。
- 执行者、审查者、协调者和 Teacher 均完成自己人格的产物提交与显式交接。
- Teacher 已完成 Phase 1 技术点与源码复盘，用户复述和练习结果有真实记录。
- Coordinator 已完成 Phase 收尾 commit，并按项目规则记录 push 结果。

只有编译成功但缺少 Editor 或 PIE 证据时，阶段状态必须是 `Not verified`。只有存在具体、无法继续的依赖或授权问题时才判定为 `Blocked`。

## 11. 跨工作包审查清单

### UE 反射与 GC

- `*.generated.h` 为头文件最后一个 include。
- 所有反射类型具有正确宏和安全默认值。
- UObject/Component/Asset 引用使用符合生命周期的 `UPROPERTY`、`TObjectPtr`、弱引用或软引用。
- Blueprint 暴露保持最小，不允许配置破坏 C++ 不变量。

### Tick 与生命周期

- Character、Controller、HUD 和 Widget 不使用自定义 Tick 轮询。
- Possess/UnPossess、Context 添加/移除、Widget 创建/销毁和 Delegate 绑定/解绑路径成对。
- 重复 BeginPlay、模式设置、重新 Possess 和连续 PIE 不产生叠加副作用。

### C++、Blueprint 与 UI 边界

- C++ 管理组件、输入流、控制模式和生命周期不变量。
- Blueprint 只绑定 Mesh、Anim、IA、IMC、WidgetClass 和灰盒配置。
- AnimBP 只负责表现。
- HUD/Widget 只显示状态和协调本地输入，不拥有 Gameplay 真源。
- Level Blueprint 不承载核心规则。

### 验证与证据

- 每次失败保存第一处真实错误，不用清缓存、reset 或删除产物掩盖问题。
- 区分 UHT、编译、链接、Editor 加载、资产持久性、PIE 主路径和失败路径证据。
- 每项任务结束执行“1 分钟复述 + 1 个预测题 + 1 次真实 Debug 定位”。

## 12. 全阶段明确非目标

- ASC、AttributeSet、GameplayEffect 和其他 Phase 2 GAS 内容。
- Phase 3 的交互扫描、InteractionComponent 或交互对象。
- Enemy、Encounter、Battle Map、TurnSystem 和战斗。
- SaveGame、跨地图持久状态和奖励。
- 网络复制、RPC、预测和多人所有权。
- 正式 UI、美术、音频、VFX 和 Root Motion 表现。
- 未经授权的第三方资源。
- 完整手柄适配、按键重映射页面和无障碍完整方案。
- 为未来需求提前拆分新模块或进行大规模重构。

## 13. 下一步

P1-001～P1-005 已完成并归档。P1-005 最终 Reviewer commit `af6b14898f589cd44fbd176488dcd5e82c309d4b` 为 `PASS WITH FOLLOW-UP`；非阻断 follow-up 仅为未来正式发布、资产迁移或分发方式变化时复核届时 Adobe/Mixamo 官方条款。Phase 1 尚未完成，唯一下一步是 `TASK-P1-006：阶段收尾工作包`，由 Coordinator 核对工程 Gate 后直接交接 Teacher，不进入 Phase 2。
