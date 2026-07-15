# Phase 0 详细执行计划

> 状态：规划完成，实施未开始，门禁为 `Not verified`。
>
> 本文件是 Phase 0 分步顺序、任务边界和验收证据的详细真源；每次只把其中一个任务写入 `tasks/active-task.md`。规划存在不代表任务已授权或已完成。

## 1. Phase 0 最终目标

建立一个可编译、可启动、可执行空白 PIE 的 UE5.6 Blank C++ 项目，并取得以下彼此独立的真实证据：

1. 工程创建成功，路径没有形成 `HSR/HSR` 嵌套。
2. 项目只有一个 `HSR` Runtime 模块，没有 Gameplay 类。
3. Visual Studio Community 2026、Windows SDK、UBT、UHT 和实际 C++ 标准有日志证据。
4. Enhanced Input、GameplayAbilities、GameplayTags、GameplayTasks 的插件与模块依赖可加载。
5. 根 Gameplay Tags 和 `Map_ProjectSetup` 可在 Editor 中确认。
6. `HSREditor Development Win64` 构建、Editor 重开和空白 PIE 均通过。
7. README、Phase 文档、worklog、todo 和 learning journal 只记录真实结果。

Phase 0 不创建 Character、Controller、Input Action、ASC、AttributeSet、GameplayAbility、GameplayEffect、Blueprint、UMG、Battle、SaveGame、多人功能或第三方资源。

## 2. 高级模型四角色联合审查

### Prompt Planner

- 将 Phase 0 拆成七个串行任务，每个任务只有一个主要验收结果。
- 每张活动卡必须自包含：当前证据、精确读取/修改范围、用户 Editor 操作、失败路径和验收标准。
- 任务只在上一任务通过审查并归档后生成；不得预先授权后续任务。

### Prompt Reviewer

- 检查任务是否停留在 Phase 0、是否只有一个目标、是否存在占位符或隐含文件范围。
- 检查“已创建、已编译、Editor 已打开、PIE 已运行”是否分开记录。
- 缺少授权、实际日志、Editor 证据或精确路径时，结论必须是 `REVISE` 或 `BLOCKED`。

### Architect

- 项目保持单 `HSR` Runtime 模块；Phase 0 只有模块入口，不引入 Gameplay 类型。
- `.uproject` 负责项目、模块和插件声明；`HSR.Build.cs` 负责编译模块依赖；Config 负责 Tags 与默认地图。
- Editor 资产操作由用户完成；低级模型只执行活动卡明确授权的文件工作和验证命令。

### Safety Reviewer

- 禁止删除、覆盖历史、清理缓存掩盖错误、批量 Config、Git 操作和第三方资源导入。
- UE 自动产生的 `Binaries/`、`Intermediate/`、`Saved/`、`.vs/` 是派生产物，不是实现证据，也不应被提交。
- 发现路径嵌套、同名文件已存在、工具链不兼容或必须扩大允许范围时立即停止。

## 3. Phase 0 串行任务

### TASK-P0-001：创建 Blank C++ 工程基线

目标：由用户通过 UE5.6 Project Browser 创建 `HSR`，低级模型在确认后执行只读预检和生成结果核对。

主要输出：`HSR.uproject`、Game/Editor Target、`HSR.Build.cs` 和模块入口；UE 可能同时生成最小 Config 与派生产物。

用户操作：Games → Blank → C++ → Desktop/Console → Maximum Quality → Starter Content Off；项目位置必须直接落在仓库根目录。

验收证据：生成文件清单、项目实际绝对路径、`.uproject` 模块声明、Editor 首次打开结果或第一处真实错误。

失败路径：发现 `E:/work/unreal_projects/HSR/HSR/` 嵌套、覆盖风险、非 UE5.6、非 C++ 或多 Runtime 模块时停止。若 Project Browser 拒绝使用已有的非空仓库目录，也必须停止，由高级模型另行规划临时生成与受控合并，不能自行搬运文件。

### TASK-P0-002：验证工具链和首次 Development Editor 构建

前置：`TASK-P0-001` 审查通过。

目标：只验证 `HSREditor Development Win64` 的首次构建链路，不修复尚未诊断的功能问题。

验收证据：完整构建命令/IDE Target、UE/UBT/UHT/Compiler/SDK 信息、实际 C++ 标准或 Target/BuildSettings 证据、退出码和第一处错误。

失败路径：若构建失败，保留第一处真实错误并结束任务；后续另建模板 C Debug 卡，禁止在本任务中随机改 SDK、Target、缓存或源文件。

### TASK-P0-003：配置并验证基础插件与模块依赖

前置：基础 Editor Target 可构建，或已有足以定位依赖问题的真实日志。

目标：最小修改 `HSR.uproject` 与 `Source/HSR/HSR.Build.cs`，确认 Enhanced Input、GameplayAbilities、GameplayTags、GameplayTasks。

架构边界：插件启用状态与 Build.cs 编译依赖必须分别检查；不初始化 ASC，不创建 Ability 或 Effect。

验收证据：文件差异、Development Editor 重建结果、Editor 插件加载结果和 Output Log 中第一处错误。

### TASK-P0-004：建立 Gameplay Tags 配置基线

前置：Gameplay Tags 模块与插件可加载。

目标：只建立 `DefaultGameplayTags.ini` 配置入口和约定命名空间：`Ability`、`State`、`Status`、`Event`、`Damage`、`Element`、`Cooldown`、`UI`。

架构边界：只定义 Tag 词汇与配置，不创建使用这些 Tags 的 Gameplay 类或资产。

验收证据：Config 差异、Editor Project Settings 中 Tags 可查询、项目重开后仍存在、无重复或冲突命名。

### TASK-P0-005：创建并设置 Map_ProjectSetup

前置：Editor 可打开且 Tags/插件配置没有阻断错误。

目标：由用户创建 `Content/Maps/Map_ProjectSetup.umap`，并将其设为 Editor Startup Map 与 Game Default Map。

目录边界：只按需要创建 `Content/Maps`；不批量创建未来系统目录，不创建 Blueprint、角色、输入或 Gameplay 资产。

验收证据：地图资产路径、Project Settings 配置、`DefaultEngine.ini` 实际差异、关闭并重开 Editor 后默认地图正确。

### TASK-P0-006：执行 Phase 0 运行门禁

前置：前五项任务均通过或无未解释阻断。

目标：执行一次干净的增量 Development Editor 构建、Editor 重开和空白 PIE。

正常路径：项目打开到 `Map_ProjectSetup`，模块、插件和 Tags 可用，PIE 启动并正常停止。

失败路径：至少验证一次明确边界，例如缺失/错误默认地图是否产生可解释日志；不得为了制造失败而破坏已通过配置。

验收证据：构建结果、Editor 启动日志、PIE 开始/停止现象、Output Log 未解释错误列表和用户截图/文字回传。

### TASK-P0-007：阶段审查、文档归档与门禁判定

前置：收齐前六项真实证据。

目标：只更新文档与任务归档，独立判断 Phase 0 为 `Ready`、`Blocked` 或 `Not verified`。

允许文档：`PROJECT_STATE.md`、`worklog.md`、`todo_plan.md`、`learning-journal.md`、`README.md`、`docs/phase-0-project-setup.md`、活动卡及归档卡；实际任务卡必须再次列出精确路径。

验收证据：todo 每个勾选项均可追溯到日志/文件/Editor 证据；未验证项保持未完成；审查结论写入归档卡。

## 4. 每个任务的统一执行流程

1. 高级模型读取全局状态与上一任务证据。
2. 高级模型模拟 Prompt Planner、Prompt Reviewer、Architect、Safety Reviewer，生成一张自包含活动卡。
3. 低级模型只读取 `tasks/active-task.md`。
4. 低级模型先复述任务编号、计划、文件范围、用户操作、验证和风险，然后明确写“等待用户确认”，并停止。
5. 用户另发一条明确确认后，低级模型才执行卡片；确认不自动扩展到后续任务。
6. 用户完成 Editor 手动步骤并回传日志、截图或现象。
7. 审查模型依据实际文件和证据给出 `PASS`、`PASS WITH FOLLOW-UP`、`REVISE` 或 `BLOCKED`。
8. 高级模型只在审查后更新状态、日志和真实进度，归档当前卡，再规划唯一下一任务。

## 5. Phase 0 总门禁

- [ ] UE5.6 Blank C++ 工程位于正确根目录。
- [ ] 只有一个 `HSR` Runtime 模块和最小模块入口。
- [ ] VS2026、SDK、UBT、UHT、Compiler 与 C++ 标准有真实证据。
- [ ] Enhanced Input、GameplayAbilities、GameplayTags、GameplayTasks 编译并加载。
- [ ] 根 Tags 在 Editor 中可查询且重开后仍存在。
- [ ] `Map_ProjectSetup` 是 Editor/Game 默认地图。
- [ ] Development Editor 构建通过。
- [ ] Editor 重开通过，空白 PIE 启停通过。
- [ ] 无自定义 Gameplay 类、Blueprint、UI、输入资产或第三方资源。
- [ ] 文档只反映真实证据，活动任务已审查并归档。

全部满足后才可建议 Phase 1；任一关键证据缺失时保持 `Not verified`，存在明确外部阻塞时标记 `Blocked`。
