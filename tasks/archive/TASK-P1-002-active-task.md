# TASK-P1-002：PlayerController、Possession 与 ControlMode 安全边界

> 归档状态（Coordinator，2026-07-16）：Implementation commit `7fef22f1afc400c9488ad36b1ae39a8c03435ec5` 已经用户人工验收；用户明确跳过独立 Reviewer，因此不存在 Reviewer 产物或 commit。

## Role Lock / 角色锁定

执行者只担任 `Implementation Agent / 低级执行模型`，不得自行改写目标、扩大文件范围、跨入 P1-003 或承担 Reviewer/Coordinator/Teacher 职责。发现必须修改未列文件、现有依赖不足或任务定义冲突时，立即停止并申请扩权。

## 任务信息

- 编号：`TASK-P1-002`
- Phase：Phase 1 — 第三人称探索角色
- 唯一目标：建立最小 `AHSRPlayerController`、`EHSRPlayerControlMode` 与幂等控制模式入口，并验证 BeginPlay、Possess/UnPossess 及错误 Pawn 的安全边界。
- 前置提交：P1-001 Implementation `14b3cc96592f65dd5a0db8c1ef2ffd3987345c32`；P1-001 Coordinator 归档 `02c150f669cf851afb37509d1b78f28176f79285`。

## 本任务职责边界

- Controller 负责本地输入模式、鼠标光标与 Possession 生命周期协调。
- Character 仍负责身体、移动组件与相机组件；Controller 不反向拥有 Character，不缓存无生命周期保护的 Pawn 指针。
- `Battle` 只作为枚举和未来边界存在，不实现战斗、Battle Map、GAS 或战斗输入。
- Mapping Context 的实际资产、Enhanced Input Subsystem 添加/移除与 IA/IMC 管理全部推迟到 P1-003。本任务不得因 Build.cs 已有 EnhancedInput 依赖而提前实现。

## 允许读取

首次进入任务时：

- 只能读取 `tasks/active-task.md`，完成复述后停止。

用户再次确认执行后，可只读：

- `Source/HSR/HSR.h`
- `Source/HSR/HSR.cpp`
- `Source/HSR/HSR.Build.cs`（只读确认现有依赖；禁止修改）
- `Source/HSR/Character/HSRCharacterBase.h`
- `Source/HSR/Character/HSRExplorationCharacter.h`
- 本任务两个目标 Player 源文件
- `tasks/execution-result.md`

## 允许修改文件

- `Source/HSR/Player/HSRPlayerController.h`
- `Source/HSR/Player/HSRPlayerController.cpp`
- `tasks/execution-result.md`

未列文件默认禁止修改。特别禁止修改 `Source/HSR/HSR.Build.cs`。

## 实现要求

1. 新建 `AHSRPlayerController : public APlayerController`，保持最小职责并关闭自定义 Tick。
2. 在 Controller 头文件中定义反射安全的 `EHSRPlayerControlMode : uint8`，至少包含 `Exploration`、`UIOnly`、`Battle`。
3. 提供明确的当前模式默认值和 `SetControlMode(EHSRPlayerControlMode)`；重复设置同一已应用模式必须直接返回，不重复设置 InputMode、鼠标或其他副作用。
4. `BeginPlay` 安全应用初始 `Exploration` 模式；不得依赖 Pawn 已存在。
5. `Exploration` 使用适合纯 Gameplay 的输入模式并隐藏鼠标；`UIOnly` 使用 UI 输入模式并显示鼠标。没有 Widget 时仍必须安全，不能持有临时 Widget。
6. `Battle` 只采用安全、明确的临时边界行为和注释/日志，不实现战斗系统或 Mapping Context。
7. 覆盖 `OnPossess(APawn*)` 与 `OnUnPossess()`：始终正确调用 `Super`；空 Pawn、非 `AHSRExplorationCharacter` Pawn 与重复 Possess/UnPossess 不崩溃；错误 Pawn 输出可定位但不过度刷屏的日志。
8. 不缓存 Pawn 裸指针；若没有持续使用需求就不新增成员引用。不得由 Character 拥有 Controller。
9. `UCLASS`、`GENERATED_BODY()`、`*.generated.h` 最后 include、前置声明/include 与 UE 反射规范必须正确。
10. 不创建 Tick、Timer 或轮询；不绑定输入 Action，不操作 LocalPlayer Enhanced Input Subsystem。

## 失败路径与风险

- `BeginPlay` 时没有 Pawn：控制模式仍可安全应用。
- `OnPossess(nullptr)`：不解引用，记录必要信息后安全返回。
- Possess 非探索角色 Pawn：不强制转换、不缓存错误类型，输出可定位日志。
- 重复调用相同 `SetControlMode`：没有重复副作用。
- `OnUnPossess` 后再次调用或重新 Possess：不访问旧 Pawn，不产生悬空引用。
- 如果实现需要 IA/IMC、Mapping Context、Blueprint、Config、GameMode 或 Character 修改，立即停止并申请下一任务/扩权。

## 明确禁止

- 禁止修改 Build.cs、Character、Config、Content、`.uproject`、地图、Blueprint 或其他文档。
- 禁止创建 IA、IMC 或任何 `.uasset`；禁止添加/移除 Mapping Context。
- 禁止实现 Move/Look/Jump/Interact、GameMode、HUD、AnimBP、GAS、交互或战斗。
- 禁止启动 Editor、运行 PIE、保存资产、clean/reset/删除缓存、修改用户文件或 push。
- 构建派生产物不得暂存或提交。

## 实施步骤（用户确认后）

1. 检查 Git status 与允许清单，排除用户/派生产物。
2. 只读模块入口、Build.cs 和 P1-001 角色头文件，确认继承与依赖；不得修改它们。
3. 创建 Controller 头文件：枚举、反射类、控制模式 API、生命周期覆盖和最小状态。
4. 创建 cpp：实现幂等模式应用、InputMode/鼠标行为、BeginPlay、OnPossess/OnUnPossess 安全路径。
5. 静态检查反射、include、生命周期、无裸 Pawn 缓存、无 Tick、无 Enhanced Input/Mapping Context 实现。
6. 运行能真实触发本轮 UHT、两个新 cpp/header 编译与 Link 的 `HSREditor Development Win64` 构建。
7. 若失败，只记录第一处真实错误并停止；不得 clean/reset/删除缓存或越界修复。
8. 将真实命令、Target、工具链摘要、UHT/Compile/Link、退出码、失败路径与未验证项写入执行报告。
9. 核对 status/diff，仅暂存并提交三个允许文件；创建 Implementation Agent 独立 commit，不 push。
10. 向下一接收方交付实现产物、commit hash、验证证据和未验证项。

## 构建与验收

- `HSREditor Development Win64` 本轮真实处理新增反射类型。
- 两个 PlayerController 源文件得到实际编译，目标 `.lib/.dll` 完成 Link，退出码 0。
- `SetControlMode` 对相同模式幂等；Exploration/UIOnly 的 InputMode 与鼠标行为明确。
- BeginPlay 无 Pawn、空 Pawn、错误 Pawn 类型、UnPossess/重新 Possess 均安全。
- 没有 Pawn 裸指针缓存、Tick、Timer、输入绑定、IA/IMC 或 Mapping Context 管理。
- 未运行 Editor/PIE 时必须明确记录未验证，不得把编译等同于实际 Possession/UI 行为通过。

## Git 交付

- 角色：`Implementation Agent / 低级执行模型`
- 只提交三个允许文件。
- 建议 message：`低级执行模型+Implementation Agent+TASK-P1-002/Phase 1+建立控制器与控制模式安全边界`
- 不 push。

## 强制交接链

1. 当前 `Coordinator → Implementation Agent`：交付本任务卡、P1-001 归档提交和下述接收条件。
2. `Implementation Agent → Reviewer`：实现者提交后交付三个文件、commit hash、UHT/Compile/Link 证据及未验证项；除非用户届时再次明确决定跳过，否则不得自行省略 Reviewer。
3. `Reviewer → Coordinator`：Reviewer 独立检查并以自己的 commit 交付结论；不得由实现者自我放行。
4. `Coordinator → 下一任务`：接收正式验收后归档并提交 Coordinator commit，之后才可创建 P1-003。

## 首次执行者接收条件

首次进入时只能读取本文件，不调用其他工具。完整复述任务编号、Phase、唯一目标、三个允许修改文件、禁止事项、实现步骤、失败路径、UHT/Compile/Link 验收、Editor/PIE 未验证边界和 Git/交接要求；复述必须精确以下句结束并立即停止：

等待用户确认执行 TASK-P1-002。
