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

## 3. 任务总览

| 任务 | 目标 | 主要产物 | 核心验收 |
|---|---|---|---|
| P1-001 | Character 与相机骨架 | 两个 Character 类 | 真编译、组件层级、反射/GC、无 Tick |
| P1-002 | Controller 与控制模式 | PlayerController、ControlMode | Possession 安全、模式切换幂等 |
| P1-003 | Enhanced Input | IA、IMC、输入处理 | Context/Binding 不重复、引用持久 |
| P1-004 | GameMode 与灰盒地图 | GameMode、派生 BP、地图 | 正确 Spawn/Possess、移动可用 |
| P1-005 | 探索 HUD | HUD、UserWidget、WBP | 单实例、空配置安全、无 Tick |
| P1-006 | 最小 AnimBP | AnimBP 与角色绑定 | Idle/Move/Jump 表现正确 |
| P1-007 | 专项集成验证 | 测试与失败证据 | UIOnly、重新 Possess、Context 不重复 |
| P1-008 | 工程门禁与归档 | Phase 文档与状态记录 | Phase 1 工程 Gate 判定 |
| P1-009 | Teacher 教学与源码复盘 | 教学、练习、面试沉淀 | 用户完成复述，Teacher 独立提交 |
| P1-010 | Phase 最终收尾 | 收尾 commit 与 push 记录 | 全角色交接和提交齐全 |

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

## 7. P1-004：GameMode、派生 Blueprint 与灰盒探索地图

### 目标

让正确的 Character 和 PlayerController 在独立灰盒探索地图中生成并建立唯一 Possession，形成首条可玩的 PIE 路径。

### 允许文件与资产

C++：

- `Source/HSR/Framework/HSRGameModeBase.h`
- `Source/HSR/Framework/HSRGameModeBase.cpp`

Editor 资产：

- `Content/Blueprints/Character/BP_HSRExplorationCharacter.uasset`
- 必要时创建 `Content/Blueprints/Player/BP_HSRPlayerController.uasset`
- `Content/Blueprints/Framework/BP_HSRGameMode.uasset`
- `Content/Maps/Map_Phase1_Exploration.umap`

只有当前任务卡明确列出时，才允许修改 `Config/DefaultEngine.ini` 的目标默认地图或 GameMode 键。

### Codex 工作

- 创建 HSR GameMode 基类并提供清晰的默认类配置边界。
- 保证 GameMode 不承担输入绑定、HUD 重复创建或跨地图持久状态。
- 编译并审查 DefaultPawnClass、PlayerControllerClass 和未来 HUDClass 的职责。

### 用户 Editor 工作

1. 创建 Character、Controller 和 GameMode 派生 Blueprint。
2. 在角色 BP 中绑定本阶段输入资产。
3. 创建独立灰盒地图、地面和 PlayerStart。
4. 在 GameMode BP 或任务允许的位置配置默认 Pawn 与 Controller。
5. 在 World Settings 中设置正确的 GameMode Override。
6. 仅在任务明确允许时配置项目默认地图。

### 验收证据

- Editor 重开后地图、GameMode 和类引用保持有效。
- PIE 只生成一个目标玩家 Pawn，并由正确 Controller Possess。
- Move、Look、Jump 可用。
- 缺少 PlayerStart、默认类错误或 Pawn 生成失败时能够由日志和 World Settings 定位。
- 连续 PIE 不出现重复 Pawn 或重复 Context。
- Level Blueprint 不承载核心规则。

### 学习目标

- 理解 GameMode 是当前 World 的开局规则与默认类工厂，不是全局持久状态。
- 能从 PIE 启动追踪 GameMode → Controller → Pawn → Possession。
- 能区分 GameMode、GameInstance 和 World Settings Override。

### 明确非目标

HUD、动画、美术、Encounter、Battle Map、存档和跨地图状态。

## 8. P1-005：探索 HUD 占位层

### 目标

创建生命周期安全、单实例、只负责显示的探索根 HUD，并完成 UIOnly 与 Exploration 的最小输入模式协作。

### 允许文件与资产

C++：

- `Source/HSR/UI/HSRHUD.h`
- `Source/HSR/UI/HSRHUD.cpp`
- `Source/HSR/UI/HSRUserWidget.h`
- `Source/HSR/UI/HSRUserWidget.cpp`
- 如任务卡逐项列明，可修改 `Source/HSR/Framework/HSRGameModeBase.*` 和 `Source/HSR/Player/HSRPlayerController.*`。

Editor 资产：

- `Content/UI/WBP_ExplorationHUD.uasset`
- 必要时创建 `Content/Blueprints/UI/BP_HSRHUD.uasset`

### Codex 工作

- 实现单一且幂等的 `CreateExplorationHUD` 入口。
- 使用正确的 Owning Player 创建 Widget。
- 对 WidgetClass 为空、CreateWidget 失败和非本地 Controller 安全处理。
- 使用正确的强/弱引用语义维护 Widget 生命周期。
- 保证重复 BeginPlay、重新 Possess 或连续 PIE 不重复 AddToViewport。
- Widget 不保存或修改 Gameplay 真源，不使用 Tick 拉取状态。

### 用户 Editor 工作

- 创建 WBP，占位显示当前探索状态或控制提示。
- 绑定 WidgetClass 和 HUDClass。
- 验证 UIOnly 下鼠标、焦点和探索输入状态，并验证返回 Exploration 后恢复。

### 验收证据

- C++ 实际编译成功。
- HUD 在 PIE 中只出现一次。
- WidgetClass 为空时明确、安全失败。
- 地图重开、连续 PIE、重新 Possess 后不访问已销毁 Widget。
- UIOnly 能阻止探索输入，恢复 Exploration 后输入正常。
- 没有 Widget Tick。

### 学习目标

- 区分 HUD 的创建协调职责和 Widget 的显示职责。
- 理解 Owning Player、Viewport、焦点和输入模式的关系。
- 能解释未来属性 UI 为什么应使用 Delegate/ViewModel，而不是 Widget Tick。

### 明确非目标

正式美术、GAS 属性显示、战斗 UI、完整导航、响应式布局和业务状态所有权。

## 9. P1-006：基础动画参数与占位 AnimBP

### 目标

为探索角色提供最小 Idle、Move、Jump 表现参数和状态机。动画只消费 Gameplay 状态，不成为移动规则真源。

### 允许资产

- `Content/Characters/Animation/ABP_HSRExploration.uasset`
- 修改 `Content/Blueprints/Character/BP_HSRExplorationCharacter.uasset` 的 Mesh/AnimClass 绑定。

建议本任务保持 Blueprint-only。若需要创建 C++ AnimInstance，必须另开任务并明确新的文件允许清单。

### Codex 工作

- 提供 Editor 操作步骤和参数来源审查。
- 审查 AnimBP 只读取 Velocity、平面 Speed、方向、IsFalling 等表现数据。
- 检查动画更新没有推进移动、输入或其他 Gameplay 规则。

### 用户 Editor 工作

1. 确认使用 UE 默认、项目已有或原创且可提交的骨骼与动画资源。
2. 创建 AnimBP。
3. 从 Pawn/CharacterMovement 读取平面速度、方向和 IsFalling。
4. 建立最小 Idle/Move/Jump 状态机。
5. 将 Mesh 和 AnimClass 绑定到探索角色派生 BP。

若没有合法可用资源，应停止并报告，不得临时导入第三方资源。

### 验收证据

- PIE 中 Idle、Move、Jump 参数与状态可以观察。
- AnimInstance/Pawn 类型不匹配时可以通过动画调试定位。
- 动画或 Mesh 缺失时角色 Gameplay 移动仍然正确。
- 不同帧率下动画不改变 Gameplay 结果。
- Editor 重开后资产引用保持。

### 学习目标

- 理解 AnimInstance、TryGetPawnOwner、Velocity、Speed 和 IsFalling。
- 能解释动画更新与自定义 Actor Tick 的区别。
- 能说明为什么动画结束、Notify 或状态机不能成为 Gameplay 结算真源。

### 明确非目标

Montage、Root Motion Gameplay、战斗动画、大规模重定向、正式模型和第三方资源导入。

## 10. P1-007：ControlMode、重复 Context 与重新 Possess 专项验证

### 目标

验证 Phase 1 最重要的生命周期和失败路径。本任务不增加功能；发现失败后记录第一处真实证据，由协调者另建 Debug 任务。

### 允许范围

- Source、Config 和 Content 全部只读。
- 只允许持久写入当前任务执行报告及任务卡明确列出的测试记录。

### 测试矩阵

1. Exploration → UIOnly → Exploration。
2. 连续多次设置相同 ControlMode。
3. UnPossess 后重新 Possess。
4. 连续启动和停止多次 PIE。
5. 在不同帧率限制下测试移动、镜头和跳跃。
6. 检查 Mapping Context 是否叠加。
7. 检查 Action Binding 是否重复触发。
8. 检查 HUD 是否重复创建。
9. 验证缺失输入资产、无 Pawn 或错误 Pawn 类型的安全失败路径。
10. 检查 Output Log 第一处 Error、Ensure、访问销毁对象或重复绑定警告。

### Codex 工作

- 提供逐项测试矩阵和预期结果。
- 审阅用户回传的 Editor、PIE、日志和失败证据。
- 区分已验证、失败和未验证，不在验证卡中顺手修复。

### 用户 Editor 工作

- 执行上述控制模式、Possession、连续 PIE 和帧率测试。
- 观察移动、镜头、跳跃、HUD、鼠标与焦点行为。
- 回传第一处真实错误、日志或明确的通过结果。

### 验收证据

- UIOnly 阻止探索输入，恢复后输入正常。
- 重复模式调用不叠加 Context 或 UI 副作用。
- 重新 Possess 后输入一次只触发一次。
- Mapping Context、Action Binding 和 HUD 均不重复。
- 不访问已销毁 Pawn 或 Widget。
- Character、Controller、Widget 无自定义 Tick 轮询。
- Output Log 无未解释的 Error、Ensure 或重复绑定警告。

### 明确非目标

功能修复、自动化测试框架、性能优化、完整手柄测试和任何 Phase 2 内容。

## 11. P1-008：工程门禁与文档归档

### 目标

只根据真实证据完成 Phase 1 工程文档归档，并判断工程门禁是 `Ready`、`Not verified` 还是存在具体阻塞。本任务通过后仍须交接 Teacher，不直接执行最终 Phase 收尾。

### 允许文件

- 新建 `docs/phase-1-exploration-character.md`
- `PROJECT_STATE.md`
- `todo_plan.md`
- `worklog.md`
- `learning-journal.md`
- `README.md`
- `tasks/execution-result.md`
- 当前任务对应的 `tasks/archive/` 文件。

除非另获明确授权，不修改旧的 `docs/phase-1-project-baseline.md`；其一致性问题应作为独立文档任务处理。

### Codex 工作

- 记录 Character → Controller → Enhanced Input Subsystem → Pawn 的输入数据流。
- 记录 Possession、ControlMode、Mapping Context 和 HUD 生命周期。
- 记录 C++/Blueprint/UMG/AnimBP 职责边界。
- 归档 Build、Editor 重开、PIE 主路径和失败路径证据。
- 只根据证据更新 todo、worklog、learning journal、README 和项目快照。
- 明确写出尚未验证内容、兼容性风险和 Phase 2 前置条件。

### 用户 Editor 工作

- 提供最终 PIE 观察结果。
- 在需要作品集证据时提供灰盒截图。
- 确认最终测试结果与未验证项记录准确。

### 验收证据

- 最终 Development Editor 构建成功。
- Editor 重开后地图、Blueprint、IA、IMC、HUD 和 AnimBP 引用有效。
- 最终 PIE 可以稳定移动、观察和跳跃，HUD 可见且不重复。
- UIOnly、重复 Context、重新 Possess、连续 PIE和不同帧率测试有记录。
- 至少一条缺失配置或生命周期失败路径有真实证据。
- 文档没有把计划、代码存在或单纯编译写成功能完成。

## 12. P1-009：Teacher 教学与源码复盘

### 目标

在 Phase 1 工程门禁通过后，由 Teacher 基于真实源码与验证证据完成本阶段技术教学、源码机制复盘、练习和面试沉淀。Teacher 不实施或修改 Gameplay C++。

### Coordinator → Teacher 交付物

- Phase 1 全部执行者、审查者和协调者 commit hash。
- 最终源码 diff、构建日志、Editor 重开和 PIE 主/失败路径证据。
- `learning-journal.md`、`worklog.md`、todo、Phase 文档和未验证风险。
- Character、Controller、Possession、Enhanced Input、GameMode、HUD、AnimBP 的最终数据流与职责边界。

### Teacher 工作

- 讲解 `AActor → APawn → ACharacter`、默认子对象、反射、GC 与组件附件关系。
- 讲解 Controller/Pawn/Possession 生命周期和 ControlMode 幂等设计。
- 逐源码追踪输入设备 → IMC/IA → EnhancedInputComponent → CharacterMovement/Camera。
- 讲解 GameMode 默认类、HUD/Widget 生命周期和 AnimBP 表现边界。
- 复盘本 Phase 的真实错误、Debug 路径、第一处证据和修复依据。
- 提供复述题、预测题、源码阅读题、Debug 练习与参考要点。
- 将可复用知识写入任务允许的 learning、interview 或 portfolio 文档。

### 验收与提交

- 用户完成一次 Phase 1 完整数据流复述。
- 用户至少完成一个配置预测题和一个真实 Debug 定位练习。
- Teacher 明确区分已掌握、需要复习、尚未验证和后续深入。
- Teacher 只提交自己的教学与复盘产物，commit message 使用 `高级模型+Teacher+Phase 1+...` 四段格式，不 push。
- Teacher 完成后显式向 Coordinator 交接教学产物和 Teacher commit hash。

## 13. P1-010：Phase 最终收尾

### 目标

由 Coordinator 核对 Phase 1 所有子任务、审查、归档、角色 commit 和 Teacher 教学交接已经齐全，然后创建 Phase 收尾提交并按规则推送。

### 最终检查

- P1-001～P1-008 均有执行者、审查者和协调者各自的交接记录与 commit hash。
- P1-009 有 Teacher 教学产物、用户复述/练习结果和 Teacher commit hash。
- 工作树没有混入派生产物、未授权资产或其他人格产物。
- 项目状态、todo、worklog、learning journal、README 和 Phase 文档一致。
- Phase 1 工程 Gate 仍为 `Ready`，没有被教学阶段发现的新阻断问题。

### Git 交付

- Coordinator 创建独立 Phase 1 收尾 commit。
- 推送前再次检查远端、分支、工作树和提交范围。
- 不使用强制 push；记录收尾 commit hash、远端、分支和 push 结果。

## 14. Phase 1 最终门禁

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

## 15. 跨任务审查清单

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

## 16. 全阶段明确非目标

- ASC、AttributeSet、GameplayEffect 和其他 Phase 2 GAS 内容。
- Phase 3 的交互扫描、InteractionComponent 或交互对象。
- Enemy、Encounter、Battle Map、TurnSystem 和战斗。
- SaveGame、跨地图持久状态和奖励。
- 网络复制、RPC、预测和多人所有权。
- 正式 UI、美术、音频、VFX 和 Root Motion 表现。
- 未经授权的第三方资源。
- 完整手柄适配、按键重映射页面和无障碍完整方案。
- 为未来需求提前拆分新模块或进行大规模重构。

## 17. 下一步

唯一推荐的相邻任务是由协调者创建 `TASK-P1-001` 活动任务卡，精确授权四个 Character 源文件、执行报告和验证范围。任务卡完成复述并获得用户单独确认前，不实施角色代码。
