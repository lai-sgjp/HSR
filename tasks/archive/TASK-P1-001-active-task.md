# TASK-P1-001：第三人称探索角色骨架与相机组件

> 状态：已完成高级协调规划，等待低级执行模型首次复述与用户另行确认。规划不等于实现；当前尚未创建 Character 源码、构建或启动 Editor。

## Role Lock / 角色锁定

低级执行模型当前且仅能担任 `Implementation Agent / 低级执行模型`。不得切换为高级模型、Prompt Planner、Prompt Reviewer、Architect、Code Reviewer、Safety Reviewer、Learning Reviewer、Teacher 或其他更高权限角色；不得重写目标、扩大文件范围、改变 Phase 或验收标准。

发现风险、歧义、缺少上下文、需要新增文件或需要更高阶决策时，必须停止，说明超出权限的原因、所需角色和所需授权，并承诺获得授权前不修改文件。

## 任务编号

`TASK-P1-001`

## 任务名称

第三人称探索角色骨架与相机组件

## 当前 Phase

- Phase 0 已通过全部门禁并处于 `Ready`。
- 当前正式进入 Phase 1：第三人称探索角色。
- 本任务是 Phase 1 的第一个串行最小任务；它只建立角色类和相机组件骨架，不证明移动、输入或完整 PIE 已完成。

## 任务目标

只创建一个可编译、职责清晰且无自定义 Tick 的探索角色 C++ 骨架：`AHSRCharacterBase` 提供最小公共 Character 扩展点，`AHSRExplorationCharacter` 创建并持有 `USpringArmComponent` 与 `UCameraComponent` 默认子对象，并确定第三人称旋转策略。

## 规划审查摘要

### 1. 子 Agent 分析

- **Prompt Planner：** 将 Phase 1 首轮压缩为四个 Character 源文件和一份执行报告；输入、Controller、GameMode、HUD、动画和资产全部后移。
- **Prompt Reviewer：** 要求分别验证 UHT、实际 C++ 编译和 Link；必须检查反射 include 顺序、GC 引用、默认子对象创建、Tick 默认关闭及失败路径，不得用旧构建或 up-to-date 结果冒充本轮真实编译。
- **Architect：** `AHSRCharacterBase` 继承 `ACharacter`，当前不持有 GAS 或玩法状态；`AHSRExplorationCharacter` 在构造函数中用 `CreateDefaultSubobject` 创建 CameraBoom 和 FollowCamera。角色朝移动方向旋转，相机随 Controller 旋转：Character 不直接使用 Controller yaw，SpringArm 使用 Pawn Control Rotation，Camera 不重复使用 Pawn Control Rotation。
- **Safety Reviewer：** 禁止修改 Build.cs、Config、Content、`.uproject`、地图或现有用户文件；禁止启动 Editor 后保存资产；禁止删除、reset、clean、push 或提交派生产物。出现范围外依赖或持久文件变化时立即停止。

### 2. 是否适合当前 Phase

适合。Phase 0 的 Blank C++ 工程、Runtime Module、Development Editor、Editor 重开、空白 PIE 与 C++20 均有真实证据。Character 与 Camera 是 Phase 1 路线图中的第一个相邻实现边界，不依赖 Controller、输入资产或 GAS。

### 3. 是否需要拆分

本任务已经是最小可独立编译单元，不再拆分。以下内容明确推迟：

- PlayerController、ControlMode 与 Mapping Context 生命周期；
- Move、Look、Jump、Interact 的 Enhanced Input 资产和绑定；
- GameMode、派生 Blueprint、灰盒探索地图与 Possession；
- HUD、AnimBP、GAS、交互、Encounter、Battle 和 SaveGame。

### 4. 本轮最小任务

创建两个反射类和对应四个源文件，使 HSR Editor Target 经过本轮真实 UHT、C++ 编译和 Link 后成功；代码只表达角色继承、组件所有权和第三人称相机/旋转默认策略。

### 5. 风险审查

- **Blocking：** 需要修改任何未列文件、增加模块依赖、创建资产或修复工程范围问题时，停止并申请扩权。
- **Reflection/GC：** 每个 `UCLASS` 必须有 `GENERATED_BODY()`；对应 `*.generated.h` 是头文件最后一个 include；组件成员使用 `UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=..., meta=(AllowPrivateAccess="true"))` 和 `TObjectPtr`，不得缓存无保护 UObject 裸指针。
- **构造生命周期：** CameraBoom 和 FollowCamera 只能在构造函数中用 `CreateDefaultSubobject` 创建并建立 Attach 层次；不得在构造函数读取 World、LocalPlayer 或运行时 Controller 状态。
- **Tick：** `PrimaryActorTick.bCanEverTick = false`；不得添加 Tick、Timer 或轮询输入。
- **旋转：** `bUseControllerRotationPitch/Yaw/Roll` 保持 false；`CharacterMovement->bOrientRotationToMovement = true`；CameraBoom 使用 Pawn Control Rotation；FollowCamera 不重复使用 Pawn Control Rotation。旋转速度只设置一个保守默认值，不加入输入逻辑。
- **证据：** 必须触发四个新源文件的真实编译并保留 UHT/Compile/Link 与退出码；若失败，记录第一处真实错误，不清缓存、不掩盖问题。
- **非目标：** 不宣称角色可移动、可观察、可跳跃或已完成 PIE；不实现输入、Controller、UI、动画、GAS、网络或资产。

### 6. 最终低级模型 Prompt

你是 `Implementation Agent / 低级执行模型`，当前只执行 `TASK-P1-001`。首次进入任务时先且只读取本文件，不调用其他工具。完整复述任务编号、当前 Phase、唯一目标、五个允许修改文件、禁止事项、实现步骤、Editor 操作、编译/UHT/Link 验收、失败路径与风险；复述必须以精确句子 `等待用户确认执行 TASK-P1-001。` 结束并停止。只有用户在该复述后另发明确确认，才可调用工具、读取实施文件、创建源码、构建或提交。

获得确认后，只创建四个 Character 源文件：最小 `AHSRCharacterBase`；以及在构造函数内创建 CameraBoom/FollowCamera 默认子对象、配置 Attach、关闭 Actor Tick 并应用任务卡规定旋转策略的 `AHSRExplorationCharacter`。组件 UObject 引用必须由 UPROPERTY/TObjectPtr 保护，反射宏和 generated include 顺序必须正确。随后运行能触发本轮新文件 UHT、实际 C++ 编译和 Link 的 `HSREditor Development Win64` 构建，记录命令、Target、工具链摘要、退出码以及实际编译/链接证据。构建失败只记录第一处真实错误并停止，不得 clean/reset/删除缓存或扩权修复。

Editor 只在用户需要时由用户打开进行只读类可见性检查，不保存资产、不运行 PIE；没有用户回传就明确写未验证。完成后只更新 `tasks/execution-result.md`，检查 status/diff/允许清单/派生产物，提交且仅提交五个允许文件，commit message 使用 `低级执行模型+Implementation Agent+TASK-P1-001/Phase 1+创建角色与相机组件骨架`，不得 push。

## 执行模型

- **高级协调者：** 已获授权，仅负责本活动卡、项目快照和协调日志；不实施源码。
- **低级执行模型：** 未来经“首次复述后用户另行确认”授权，实施四个源文件、真实构建、执行报告和角色提交。
- **用户：** 在低级模型复述后决定是否确认执行；如选择 Editor 检查，只执行只读检查并回传现象。
- **审查模型：** 执行完成后独立核对 diff、源码和证据；不得由执行者自我放行。

## 执行前必须读取的文件

首次读取阶段：

- `tasks/active-task.md`（唯一上下文入口；首次只读并复述，用户确认前零工具调用）

用户另行确认后，实施阶段只允许读取：

- `Source/HSR/HSR.h`
- `Source/HSR/HSR.cpp`
- `Source/HSR/HSR.Build.cs`（只读，用于确认现有依赖；禁止修改）
- 本任务创建的四个目标源文件
- `tasks/execution-result.md`

## 允许修改文件

- `Source/HSR/Character/HSRCharacterBase.h`
- `Source/HSR/Character/HSRCharacterBase.cpp`
- `Source/HSR/Character/HSRExplorationCharacter.h`
- `Source/HSR/Character/HSRExplorationCharacter.cpp`
- `tasks/execution-result.md`

未列出的文件默认禁止修改；如需扩展范围，立即停止并请求用户授权。

## 禁止修改内容

- 禁止修改 `Source/HSR/HSR.Build.cs`、Target、模块入口、`.uproject`、Config、Content、地图、Blueprint、插件或任何文档（执行报告除外）。
- 禁止修改、删除、暂存或提交用户的 `check.cpp` 与 `docs/phase-1-execution-plan.md`。
- 禁止创建 Controller、GameMode、HUD、AnimInstance、Input Action、Mapping Context、GAS、交互或战斗代码/资产。
- 禁止构建之外的 Editor 实施、保存资产、PIE、批量生成项目文件、清缓存或安装工具链。
- 禁止 `git reset`、`git clean`、删除、覆盖历史、批量移动、force push 和任何 push。
- `Binaries/`、`Intermediate/`、`Saved/`、`.vs/`、DerivedDataCache 等派生产物不得暂存或提交。

## 实现步骤

1. 首次只读本卡，完整复述并以指定句子结束；等待用户另行确认，确认前零工具调用。
2. 确认后检查 status；只读模块入口与 Build.cs，确认现有 Engine 依赖足够。发现额外依赖需求立即停止。
3. 创建 `AHSRCharacterBase` 的最小反射声明与实现，不提前加入 ASC、输入、状态或 Tick。
4. 创建 `AHSRExplorationCharacter`，构造 CameraBoom/FollowCamera 默认子对象，配置 Attach、旋转策略和保守相机默认值；组件引用使用 UPROPERTY/TObjectPtr。
5. 静态检查 UCLASS、GENERATED_BODY、generated include、include 最小化、构造生命周期、GC 和 Tick。
6. 运行 `HSREditor Development Win64` 真实构建；分别记录 UHT、四个新源文件编译、Link、退出码和第一处真实错误。
7. 用户如选择 Editor 只读检查，则记录类是否可见、是否产生加载错误；不得保存资产或运行 PIE。
8. 将真实改动、命令、证据、失败路径、未验证项和范围外需求写入 `tasks/execution-result.md`。
9. 检查 status/diff/允许清单/派生产物，只提交五个允许文件；记录 commit hash，不 push。

## UE Editor 手动操作

本任务无需 Editor 才能通过代码与构建验收。构建成功后，用户可选执行以下只读检查：

1. 打开 UE5.6 Editor，在 C++ Classes/HSR/Character 中确认两个类可见。
2. 检查 Output Log 是否出现目标类加载、模块、Error/Ensure/Assert 问题。
3. 不创建派生 Blueprint、不保存地图/资产、不运行 PIE；把现象或第一处真实错误回传。

若用户未执行，执行报告必须写“Editor 类可见性未验证”，不得伪造。

## 验收标准

- [ ] 只新增四个目标源码并只更新执行报告，没有越界持久修改。
- [ ] `AHSRCharacterBase` 与 `AHSRExplorationCharacter` 的 UCLASS、GENERATED_BODY 和 generated include 正确。
- [ ] CameraBoom/FollowCamera 在构造函数中由 `CreateDefaultSubobject` 创建，Attach 层次正确。
- [ ] 组件引用使用 UPROPERTY/TObjectPtr 保护，不缓存无生命周期保护的 UObject 裸指针。
- [ ] `PrimaryActorTick.bCanEverTick` 为 false，没有 Tick/Timer/输入轮询。
- [ ] 旋转策略符合：Character controller pitch/yaw/roll false、Movement orient-to-movement true、SpringArm 使用 control rotation、Camera 不重复使用 control rotation。
- [ ] 本轮构建真实触发 UHT、四个新源文件 C++ 编译与 Link，`HSREditor Development Win64` 退出码 0。
- [ ] 构建失败时只记录第一处真实错误并停止，没有 clean/reset/删缓存或越权修复。
- [ ] Editor 检查结果如实记录；未执行则明确未验证，不把编译等同于 PIE。
- [ ] 未宣称移动、镜头输入、跳跃、HUD、动画或 Phase 1 完成。
- [ ] 低级执行模型完成范围检查并使用指定四段格式提交，未暂存用户文件或派生产物，未 push。

## 审查清单

- [ ] 任务是否只有角色/相机骨架一个结果，没有跨到 P1-002 或 Phase 2？
- [ ] 实际 diff 是否只含五个允许文件，且排除 `check.cpp`、执行计划与派生产物？
- [ ] UE 反射宏、generated include、继承、默认构造和 include 是否正确？
- [ ] UObject 组件引用是否由 UPROPERTY/TObjectPtr 保护，默认子对象是否在构造期创建？
- [ ] Tick 是否关闭，代码是否没有 World/Controller 运行时构造访问、输入轮询或无关抽象？
- [ ] 第三人称旋转责任是否唯一，没有 Character、SpringArm 和 Camera 重复消费控制旋转？
- [ ] UHT、真实 Compile、Link 与退出码证据是否来自本轮；是否记录第一处失败？
- [ ] 是否没有 Config、Blueprint、资产、GAS、网络、删除、清理、越权 Git 或 push？
- [ ] Editor/PIE 未验证项和完成状态是否准确？

## 执行后必须更新的文件

- `tasks/execution-result.md`：低级执行模型唯一可写的执行报告。

低级执行模型不得更新 `tasks/active-task.md`、`PROJECT_STATE.md`、`worklog.md`、`todo_plan.md`、`learning-journal.md`、README 或 Phase 文档。执行后由独立审查模型先核对实际 diff 和证据；只有审查完成后，高级协调者才归档活动卡并按真实结果更新项目文档。不得自动创建 P1-002。

## 执行结果

- 已修改：待低级执行模型填写。
- 已验证：待低级执行模型填写。
- 未验证：移动、输入、Possession、PIE、HUD、动画和 Phase 1 门禁默认均未验证。
- Editor 待办：可选只读类可见性检查；不得保存资产。
- 阻塞/扩权请求：待低级执行模型填写。
- 审查结论：待独立审查模型填写。

## Git 交付

- 角色：`低级执行模型 / Implementation Agent`
- 固定 commit message：`低级执行模型+Implementation Agent+TASK-P1-001/Phase 1+创建角色与相机组件骨架`
- 提交前检查：`git status`、目标 diff、五个允许文件、`check.cpp` 与执行计划排除、派生产物排除。
- commit hash：待填写。
- Phase 收尾 push：P1-001 中途禁止 push；只有 Phase 1 全部串行任务完成、审查、归档并完成角色提交后，才由高级协调者执行阶段收尾与非强制 push。

## 角色交接链 / 当前交接

### 当前交接：Coordinator → Implementation Agent

- 交付物：本活动任务卡、`PROJECT_STATE.md` 记录的当前 Phase/任务状态，以及协调者提交 `ce37faaa27053afc4b64bfb34e717c79bde1ba89`。
- Implementation Agent 的接收条件：首次只能只读本活动卡，完整复述任务编号、当前 Phase、唯一目标、五个允许修改文件、禁止事项、实现步骤、Editor 操作、UHT/Compile/Link 验收、失败路径与风险，并以精确句子 `等待用户确认执行 TASK-P1-001。` 结束。
- 在该复述之后用户再次明确确认前，Implementation Agent 必须保持零工具调用，不得读取其他文件、创建源码、构建、启动 Editor 或执行 Git。

### TASK-P1-001 后续强制交接

1. **Implementation Agent → Reviewer：** Implementation Agent 完成允许范围内的实现、执行报告和验证后，必须先创建其角色 Git commit，再向 Reviewer 交付执行报告、implementation commit hash、UHT/Compile/Link 构建证据以及全部未验证项；不得自我审查或自我放行。
2. **Reviewer → Coordinator：** Reviewer 必须独立核对源码、diff、提交范围、构建证据、生命周期/反射/GC/Tick 约束和未验证项，完成审查产物并创建 Reviewer 角色 Git commit；随后向 Coordinator 交付审查结论、review commit hash，以及明确的返工或归档决定。
3. **Coordinator → 下一任务：** Coordinator 只有在收到 Reviewer 正式交接后，才能根据结论安排返工或归档活动卡；归档工作也必须创建 Coordinator 角色 Git commit。归档提交完成前，禁止创建或启动 P1-002。

> 归档例外记录（2026-07-16）：用户在 Implementation Agent 完成后亲自审查，明确确认没有问题并要求跳过额外独立 Reviewer。因此本次由 Coordinator 直接接收用户验收并归档；没有 Reviewer 产物或 commit，也不伪造其存在。该用户指令仅适用于本次 P1-001 交接。

### Phase 1 最终强制交接

- Phase 1 的全部子任务逐项通过 Implementation、Reviewer、Coordinator 交接并归档后，Coordinator 必须把 Phase 1 交接给 Teacher；不得跳过教学验收直接收尾。
- Teacher 必须基于实际源码、diff、编译证据、PIE 证据和 `learning-journal.md`，完成本 Phase 技术点讲解、源码机制分析、练习、用户复述验收与面试知识沉淀，并创建包含 Teacher 产物的角色 Git commit。
- 只有收到 Teacher 的正式交付与 commit hash 后，Coordinator 才能进行 Phase 1 最终文档收尾、创建 Coordinator 收尾 commit，并按规则决定或执行阶段 push。
- 每个角色每次完成其职责都必须留下清晰交接内容和独立 Git 提交；计划、口头结论或其他角色的提交不能替代该角色交付。
