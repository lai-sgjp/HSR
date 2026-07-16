# TASK-P1-003：Enhanced Input C++ 接口与生命周期（P1-003A）

> 状态：Coordinator 规划完成，等待 Implementation Agent 首次只读复述与用户再次明确确认。首次复述前，执行者只能读取本文件；用户确认前必须保持零工具调用。

## Role Lock / 角色锁定

执行者当前且只能担任 `Implementation Agent / 低级执行模型`。不得切换为 Coordinator、Reviewer、Teacher 或 UE Editor 人格，不得创建或提交用户 Editor 资产。发现需要未列文件、Blueprint、Config 或 `.uasset` 时立即停止并申请扩权。

## 任务信息

- 编号：`TASK-P1-003`
- 计划分段：本卡是 P1-003A C++ 接口；后续 `TASK-P1-003B` 才创建和绑定 Editor 资产。
- Phase：Phase 1 — 第三人称探索角色。
- 唯一目标：建立 Move/Look/Jump/Interact 的 C++ 输入绑定入口，以及由 Controller 单一管理、对缺失资产安全的 Exploration Mapping Context 生命周期，并真实通过 UHT、编译和链接。
- 前置提交：P1-002 Implementation `7fef22f1afc400c9488ad36b1ae39a8c03435ec5`；P1-002 Coordinator 归档 `1cf7b57de3d04b785b60ce8c2af7951ea13c8832`。

## 拆分与产物归属

- 本任务只提交四个既有 C++ 文件与 `tasks/execution-result.md`；用户 Editor 操作为“无”。
- 本任务不得创建、保存、暂存或提交 `.uasset`，也不得宣称 IA/IMC 已存在、已绑定、Editor 重开保持或 PIE 输入可用。
- 后续 `TASK-P1-003B` 由用户在 UE Editor 创建且仅创建 `IA_Move`、`IA_Look`、`IA_Jump`、`IA_Interact`、`IMC_Exploration`，完成映射/引用绑定并回传证据。
- P1-003B 的五个资产是用户 Editor 产物，应由用户本人创建 Git commit 并回传 hash；Implementation Agent、Reviewer 或 Coordinator 不得冒充资产作者。Coordinator 只在收到用户 commit 和证据后归档。

## 允许读取

首次进入任务时：

- 只能读取 `tasks/active-task.md`，完整复述后停止。

用户再次确认后，可读取：

- `Source/HSR/HSR.Build.cs`（只读，已有 EnhancedInput 依赖，禁止修改）
- `Source/HSR/Character/HSRCharacterBase.h/.cpp`（只读）
- `Source/HSR/Character/HSRExplorationCharacter.h/.cpp`
- `Source/HSR/Player/HSRPlayerController.h/.cpp`
- `tasks/execution-result.md`

## 允许修改文件

- `Source/HSR/Character/HSRExplorationCharacter.h`
- `Source/HSR/Character/HSRExplorationCharacter.cpp`
- `Source/HSR/Player/HSRPlayerController.h`
- `Source/HSR/Player/HSRPlayerController.cpp`
- `tasks/execution-result.md`

未列文件默认禁止修改。特别禁止修改 Build.cs、Config、Content、`.uproject` 和任何 Blueprint/资产。

## 职责与实现要求

### Character：唯一绑定 Pawn 行为

1. 在 `AHSRExplorationCharacter` 中以 `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)` + `TObjectPtr<UInputAction>` 声明 Move、Look、Jump、Interact 四个资产引用，保持最小 Blueprint 暴露。
2. 覆盖 `SetupPlayerInputComponent`，先调用 `Super`，再安全转换为 `UEnhancedInputComponent`；转换失败记录可定位日志并返回。
3. 只在该入口绑定 Pawn 行为，Controller 不重复绑定 Action。
4. Move 使用 `FInputActionValue` 的 Axis2D，并只根据 Controller Yaw 计算世界 Forward/Right；无 Controller 时安全返回。
5. Look 将 Axis2D 分别传给 Controller Yaw/Pitch 输入。
6. Jump 使用 Started 调用 `Jump`、Completed 调用 `StopJumping`；不得使用 Tick 或持续轮询。
7. Interact 只进入可观察日志/空函数占位，不扫描 Actor、不创建 InteractionComponent、不产生 Gameplay 副作用。
8. 某个 Input Action 缺失时只跳过该项并输出明确日志，其他有效 Action 仍可绑定；不得解引用空资产。

### Controller：唯一管理 Mapping Context

1. 以 `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)` + `TObjectPtr<UInputMappingContext>` 保存 Exploration Context 配置引用。
2. 通过 `ULocalPlayer` 的 `UEnhancedInputLocalPlayerSubsystem` 添加/移除 Context；无 LocalPlayer、无 Subsystem、无 Context 均安全失败并给出可定位日志。
3. Context 的添加/移除必须对称且幂等；Controller 自己记录“是否由本实例添加”，避免重复 Add/Remove。
4. Exploration 模式且已 Possess 探索角色时添加 Context；进入 UIOnly/Battle、UnPossess 或 Possess 错误 Pawn 时移除 Context。
5. 重新 Possess 后最多恢复一个 Context；不得缓存旧 Pawn 裸指针，不得在 Character 中重复管理 Context。
6. 修正 P1-002 初始模式已等于 Exploration 时 `BeginPlay` 提前返回导致首次副作用未应用的边界：使用明确的“模式是否已应用”状态，使重复调用幂等，但首次调用一定应用 InputMode/鼠标/Context 协调。
7. 不创建 Tick、Timer、轮询、HUD 或战斗行为。

## 失败路径与停止条件

- 缺 IA：该 Action 不绑定，日志清晰，不崩溃。
- 缺 IMC、无 LocalPlayer 或无 Enhanced Input Subsystem：不 Add Context，不崩溃。
- 非 EnhancedInputComponent：不绑定 Action，不崩溃。
- 空/错误 Pawn：不添加 Exploration Context；已有 Context 必须安全移除。
- 重复 Exploration、重复 Possess、UnPossess 后重新 Possess：Context 不叠加，Action 绑定不由 Controller 重复建立。
- 若必须创建资产、修改 Build.cs、GameMode、HUD、Anim、GAS、Interaction 系统或其他文件，立即停止并请求扩权。

## 明确禁止

- 禁止创建五个 IA/IMC 资产或任何 `.uasset`；禁止创建/修改 Blueprint、地图或 Config。
- 禁止 GameMode、HUD、AnimBP、GAS、Gameplay Tags、InteractionComponent、扫描/交互系统和 Battle 行为。
- 禁止使用 Tick/Timer 读取输入，禁止 Character 与 Controller 双重 BindAction，禁止多个对象管理 Context。
- 禁止启动 Editor、运行 PIE、保存资产、push、reset、clean、删除缓存或提交派生产物。

## 实施步骤（仅在用户再次确认后）

1. 检查 Git status 与允许清单；读取允许的现有 C++ 和 Build.cs。
2. 在 Character 建立四个 Action 引用、单一 `SetupPlayerInputComponent` 绑定与四类处理入口。
3. 在 Controller 建立 IMC 引用、Subsystem 获取、幂等添加/移除和 Possession/ControlMode 协调。
4. 静态检查 UCLASS/UPROPERTY/generated include、TObjectPtr、GC、include、无 Tick、无重复职责及所有失败路径。
5. 运行能真实触发本轮 UHT、修改后的四个目标文件编译和 Link 的 `HSREditor Development Win64` 构建。
6. 构建失败只记录第一处真实错误并停止；不得越界修复或清缓存。
7. 如实写入执行报告：命令、UHT/Compile/Link、退出码、静态失败路径、未验证的 Editor/资产/PIE 项。
8. 仅暂存五个允许文件，检查 staged diff 后创建 Implementation Agent commit；不 push。
9. 向用户/Coordinator 交接实际产物、完整 commit hash、已验证/未验证项与 P1-003B 接收条件。

## UE Editor 手动操作

无。本卡明确不让用户创建资产；资产创建、映射、引用绑定、Editor 重开和 PIE 验证全部留到 P1-003B。

## 验收标准

- [ ] 实际 diff 仅包含四个获准 C++ 文件与执行报告。
- [ ] Character 单一绑定 Move/Look/Jump/Interact；Controller 不绑定 Pawn Action。
- [ ] Controller 单一管理 Context，添加/移除对称且幂等。
- [ ] 缺 IA/IMC、无 LocalPlayer、无 Subsystem、错误 Pawn、重新 Possess 路径均有安全代码边界。
- [ ] Interact 无 Gameplay 副作用；Character 与 Controller 均无自定义 Tick/Timer 输入轮询。
- [ ] UHT、四个修改文件实际编译、链接退出码 0。
- [ ] 执行报告明确标注：资产不存在/未绑定、Editor 重开与 PIE 输入均未验证。
- [ ] Implementation Agent 创建自己的提交并回传完整 hash；未 push。

## 角色交接

- 当前交接：Coordinator → Implementation Agent。
- Implementation Agent 首次只读本卡后，必须复述任务编号、C++/Editor 拆分、文件范围、职责、失败路径、构建、Git 与未验证项，最后精确写：`等待用户确认执行 TASK-P1-003。`
- 用户在该复述后再次明确确认前，执行者不得调用任何工具或执行任何修改。
- 执行完成后：Implementation Agent → 用户/Reviewer；若用户再次明确人工验收并跳过 Reviewer，Coordinator 只能记录该事实，不得创建 Reviewer commit。
- P1-003A 归档后才可创建 P1-003B；P1-003B 用户资产 commit 和 Editor 证据完成前，不得宣称 Enhanced Input 功能完成。

## Git 交付

- 角色：`低级执行模型 / Implementation Agent`
- commit message：`低级执行模型+Implementation Agent+TASK-P1-003/Phase 1+建立Enhanced Input C++接口与生命周期`
- 提交前检查：status、diff、五个允许文件、无 `.uasset`、无派生产物。
- 不 push。
