# TASK-P0-006 活动任务卡

> 归档说明：执行者 commit `f18269a8f056c110f2e6cf673271cbd2201e19d1` 已完成。用户于 2026-07-16 明确确认执行结果无问题，并明确要求跳过本任务的独立审查；本归档只记录该用户决定，不代表或伪造高级审查者结论。

> 状态：待低级执行模型复述并等待用户单独确认。任务卡存在不等于已授权执行。

## Role Lock / 角色锁定

你当前且只能担任 `Implementation Agent / 低级执行模型`。未经用户明确授权或本活动任务卡明确授权，不得切换为高级模型、Prompt Planner、Prompt Reviewer、Architect、Code Reviewer、Safety Reviewer、Teacher 或其他更高权限角色，不得重写目标、扩大文件范围或改变 Phase。

发现风险、上下文缺失、持久文件变化或需要扩权时，必须停止并报告原因、所需角色与授权；获得授权前不得继续。

## 任务编号

`TASK-P0-006`

## 任务名称

验证 Development Editor、Editor 重开与空白 PIE 运行门禁

## 当前 Phase

- Phase 0：项目初始化与规范落地。
- 门禁：`Not verified`。
- 前置证据：P001-P005 已审查归档；P005 为 `PASS WITH FOLLOW-UP`，审查 commit 为 `dbf08cdf310b0dd68627aa498ad83ab322fbb5a0`。
- P003 已独立证明 `HSR.cpp`、生成代码、`UnrealEditor-HSR.lib` 和 `UnrealEditor-HSR.dll` 实际编译/链接成功；该证据不能被误写成本轮构建实际重新编译。
- 当前未验证：本轮 Development Editor 构建状态、Editor 重开总门禁、默认地图加载、插件与 Tags 可用性、空白 PIE，以及实际 C++ 标准。

## 任务目标

只执行 Phase 0 运行门禁：构建 `HSREditor Development Win64`，重开 Editor 并确认自动加载 `Map_ProjectSetup`，检查基础插件和八个根 Gameplay Tags，启动一次空白 PIE 并正常停止，同时记录第一处真实错误和未验证项。

本任务不修改工程实现，不修复发现的问题，不创建或保存资产、设置或 Gameplay 内容，不创建 P007。

## 规划审查摘要

### 1. 子 Agent 分析

- Prompt Planner：把 P006 限定为一次可独立验收的 Phase 0 运行门禁；执行者只记录证据，Editor 可视化步骤由用户完成。
- Prompt Reviewer：构建、Editor 重开、默认地图、插件、Tags、PIE 启动/停止和日志检查必须分别记录；P003 的实际编译/链接与本轮结果必须分开陈述。
- Architect：`HSR.uproject`、Build.cs、Config 和地图均为既有基线，本轮全部只读；运行门禁不需要新增 C++、Blueprint、资产或配置。
- Safety Reviewer：唯一允许持久写入为 `tasks/execution-result.md`；出现任何其他持久文件 diff、需要修复或需要清缓存时立即停止。

### 2. 是否适合当前 Phase

适合。P001-P005 已提供工程、模块、插件、Tags 和默认地图前置证据；P006 正是 Phase 0 进入最终归档前的相邻运行门禁。本任务不进入 Phase 1，也不判定 Phase 0 最终完成。

### 3. 是否需要拆分

不需要。构建、Editor 重开、基础配置核对和空白 PIE 共同组成同一个 Phase 0 运行门禁，且都不要求持久实现修改。

明确推迟：P007 阶段审查与文档归档、实际 C++ 标准缺证后的专项补证、任何构建或运行错误修复、Phase 1 Gameplay 工作。

### 4. 本轮最小任务

完成一次不改持久工程文件的 `HSREditor Development Win64` 构建与 Editor/PIE 手动验证，并把可追溯证据写入 `tasks/execution-result.md`。

### 5. 风险审查

- Blocking：Source、Config、Content、`.uproject`、地图或其他持久文件发生变化；必须停止并报告，不得保存或提交这些变化。
- Blocking：构建、Editor 启动或 PIE 出现第一处真实 Error/Missing/Assert/Ensure；保留完整上下文后停止，不得自动修复。
- Risk：本轮构建可能为 up-to-date；必须如实记录，不能据此声称本轮发生实际 C++ 编译或链接。
- Risk：MSVC 14.51.36248 不是 UE5.6 preferred 版本；若再次出现只记录为非阻断警告，除非它成为第一处真实失败。
- Risk：日志可能不给出实际 C++ 标准；保持 `未验证`，不得猜测，不得修改源码或清缓存强制触发编译。
- 非目标：破坏默认地图、插件或 Tag 配置来制造失败；修改 SDK、Target、Build.cs；创建/保存资产；Gameplay；P007；push。

### 6. 最终低级模型 Prompt

你是 `Implementation Agent / 低级执行模型`，只执行 `TASK-P0-006`。首次只读取本活动卡，不读取其他全局文档，不调用任何工具。

首次回复必须完整复述：

1. 任务编号、当前 Phase 和唯一目标。
2. 唯一允许修改文件与全部只读工程文件。
3. 构建命令/Target、工具链、退出码、实际编译/链接或 up-to-date 的证据要求，以及 P003 历史证据与本轮证据的区别。
4. 用户执行的 Editor 重开、默认地图、插件、Tags、空白 PIE 和 Output Log 检查步骤。
5. 第一处真实错误、持久文件变化、扩权需求和禁止事项等停止条件。
6. 执行者 Git 提交范围与固定 commit message。

复述最后必须逐字写：

`等待用户确认执行 TASK-P0-006。`

写完后立即停止。只有用户在该复述之后单独明确确认当前任务编号，才可调用工具、执行构建或指导 Editor 验证；旧确认不适用于本任务。

## 执行模型与职责

- 低级执行模型：确认后执行只读预检、构建证据采集、用户 Editor 步骤协调、执行报告和自己的 Git commit。
- 用户：实际执行 Editor 可视化检查、PIE 启动/停止并回传现象或日志。
- 高级审查者：执行完成后独立核对范围、Git diff、构建与 Editor/PIE 证据。
- 当前授权状态：未授权执行；等待低级模型先复述，再等待用户单独确认。

## 执行前必须读取的文件

- 首次且只读：`tasks/active-task.md`
- 用户确认后，实施所需只读文件：
  - `HSR.uproject`
  - `Source/HSR/HSR.Build.cs`
  - `Config/DefaultEngine.ini`
  - `Config/DefaultGameplayTags.ini`
  - `Content/Maps/Map_ProjectSetup.umap`（只核对存在性，不解析或保存资产）
  - `tasks/archive/TASK-P0-003-review.md`（只核对历史编译/链接证据边界）

## 允许修改文件

- `tasks/execution-result.md`

这是唯一允许的持久文件改动。未列出的文件默认禁止修改。

## 只读与派生产物边界

- 只读：`HSR.uproject`、`Source/`、`Config/`、`Content/`、`Plugins/`、Target、Build.cs、地图及所有工程实现文件。
- `Binaries/`、`Intermediate/`、`Saved/`、`.vs/`、`DerivedDataCache/` 是构建/Editor 忽略的派生产物，可由工具运行产生变化，但不可作为实现改动提交。
- 构建或 Editor 导致任何非忽略持久文件变化时，停止并报告；不得保存、还原、删除或自行修复。

## 禁止事项

- 禁止 `git reset`、`git clean`、删除缓存、删除文件或覆盖历史。
- 禁止修改 SDK、Target、Build.cs、Source、Config、Content、`.uproject` 或插件声明。
- 禁止为了触发编译修改源码、touch 文件、清理 Binaries/Intermediate 或执行 Rebuild/Clean。
- 禁止保存地图、资产、Editor 设置或新建任何资产。
- 禁止 Character、Controller、Input、ASC、AttributeSet、Ability、Effect、Blueprint、UMG 或其他 Gameplay。
- 禁止创建、规划或执行 `TASK-P0-007`。
- 禁止 push；Phase 未收尾前只允许当前角色 commit。

## 实现步骤

1. 用户确认后，先执行只读预检：确认工作树在执行前无未解释持久改动；确认目标工程、Target、默认地图、插件声明、模块依赖和八个根 Tag 的现有文件仍存在。预检不改变这些文件。
2. 使用项目现有且可追溯的 UE5.6 构建入口执行一次 `HSREditor Development Win64` 增量构建。不得执行 Clean/Rebuild。
3. 在执行报告中记录完整命令或 IDE Target、UE 路径、Target/Configuration/Platform、UBT/UHT、Compiler、Windows SDK、退出码和第一处真实错误。
4. 明确记录本轮是实际编译/生成代码/链接，还是 `Target is up to date`。若本轮 up-to-date，只说明本轮未触发实际编译/链接，并引用 P003 已审查的历史实际编译/链接证据，不得把两者合并成同一轮结果。
5. 尽可能从真实 Target、UBT 参数或构建日志记录实际 C++ 标准。若没有证据，写 `实际 C++ 标准：未验证`；不得猜测或制造源码变更。
6. 构建成功后，请用户启动或完全关闭后重开 UE5.6 Editor，并按下方手动步骤验证。若构建失败，保留第一处真实错误并停止，不进入 Editor/PIE。
7. 用户验证后，将每项现象、日志来源、第一处 Error/Missing/Assert/Ensure 和未验证项写入 `tasks/execution-result.md`。不得把用户未执行的步骤写成已通过。
8. 结束前检查 `git status` 与 diff。若除执行报告和忽略派生产物外出现持久文件改动，停止并报告，不提交。
9. 仅提交 `tasks/execution-result.md`，commit message 固定为：`低级执行模型+Implementation Agent+TASK-P0-006/Phase 0+验证构建重开与空白PIE运行门禁`。记录 commit hash，不 push。

## UE Editor 手动操作

1. 构建成功后，关闭已有 Editor 实例；通过 `HSR.uproject` 启动或重新启动 UE5.6 Editor。
2. 确认项目启动后自动打开 `/Game/Maps/Map_ProjectSetup`，不要保存地图或任何资产。
3. 在 Plugins/Project Settings 中只读确认 Enhanced Input 与 Gameplay Ability System 已启用且可用，不改变设置。
4. 在 Project Settings > Gameplay Tags 中查询八个根 Tag：`Ability`、`State`、`Status`、`Event`、`Damage`、`Element`、`Cooldown`、`UI`；不要新增、编辑或保存 Tag。
5. 在 `Map_ProjectSetup` 中启动一次空白 PIE，观察能够进入运行状态；保持短暂运行后使用 Stop 正常停止。
6. 检查 Output Log，搜索并记录启动到 PIE 停止期间第一处 `Error`、`Missing`、`Assert` 或 `Ensure`。若存在，立即停止后续动作并保留完整上下文；不要修复。
7. 回传自动加载地图、插件、Tags、PIE 启动、运行、停止和日志结果；未观察或无法确认的项目明确标为未验证。

## 失败与边界路径

- 只检查现有默认地图、插件、Tag 和 PIE 日志是否产生真实错误。
- 不通过破坏配置、重命名/删除地图、禁用插件、删除 Tag、改源码或清缓存来制造失败。
- 第一处真实错误即为本轮失败路径证据；记录后停止，避免后续级联错误掩盖根因。
- 需要修改任何持久工程文件才能继续时，停止并请求高级协调者创建独立修复任务。

## 验收标准

- [ ] 用户在低级模型复述后单独确认 `TASK-P0-006`，确认前零工具调用。
- [ ] 只有 `tasks/execution-result.md` 发生可提交的持久修改，无工程实现或资产改动。
- [ ] `HSREditor Development Win64` 的命令/Target、工具链、退出码和第一处错误有真实证据。
- [ ] 本轮实际编译/链接或 up-to-date 状态被准确记录，并与 P003 历史证据分开。
- [ ] 实际 C++ 标准有真实证据；若无则明确保持未验证。
- [ ] Editor 重开后自动加载 `Map_ProjectSetup`。
- [ ] Enhanced Input、Gameplay Ability System 与八个根 Tag 可用。
- [ ] 空白 PIE 可启动、运行并正常停止。
- [ ] Output Log 第一处 Error/Missing/Assert/Ensure 已记录；无匹配时记录检查范围和结果。
- [ ] 未创建/保存资产、设置、Gameplay 或 P007，未清缓存、reset、clean 或 push。
- [ ] 执行者仅提交执行报告并回报 commit hash。

## 审查清单

- [ ] 任务只验证 Phase 0 运行门禁，没有进入 P007 或 Phase 1。
- [ ] Git diff 只包含执行报告，派生产物未提交。
- [ ] 构建、Editor、插件、Tags、PIE 和日志证据彼此独立且来自真实操作。
- [ ] 未把 P003 历史实际编译或本轮 up-to-date 错报为本轮实际编译。
- [ ] 未通过破坏配置制造失败，第一处真实错误与未验证项均保留。
- [ ] 实际 C++ 标准没有被猜测。

## 执行后职责边界

- 低级模型只能写 `tasks/execution-result.md` 并提交自己的角色产物。
- 低级模型不得更新本活动卡、`PROJECT_STATE.md`、`worklog.md`、`todo_plan.md` 或归档文件。
- 审查通过后由高级审查者同步状态并归档；本任务不得自动创建 P007。

## Git 交付

- 角色：低级执行模型 / `Implementation Agent`
- 固定 commit message：`低级执行模型+Implementation Agent+TASK-P0-006/Phase 0+验证构建重开与空白PIE运行门禁`
- 提交前检查：`git status`、diff、唯一允许文件、派生产物排除、无持久工程文件变化。
- Phase 收尾 push：本任务禁止；等待 Phase 0 全部子任务审查、归档与阶段收尾。
