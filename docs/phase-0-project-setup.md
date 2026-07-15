# Phase 0：项目初始化与规范落地

> 当前状态：仅完成规划，尚未创建 `.uproject`、Source、Config、Content 或构建产物。

第一轮文档维护不等于 Phase 0 已开始。只有用户明确授权创建 UE 工程后，才可以执行本文件的工程步骤。

完整路线见 [Phase 0–20 路线图](phase-roadmap-0-20.md)。

Phase 0 的七个串行任务、四角色规划审查和总门禁见 [Phase 0 详细执行计划](phase-0-execution-plan.md)。

## 阶段目标

- 使用 Unreal Engine 5.6 Blank C++ 模板创建 `HSR`。
- 保持单 `HSR` Runtime 模块。
- 验证 Visual Studio 2026、UBT、UHT、Windows SDK 和实际 C++ 标准。
- 确认 Enhanced Input、GameplayAbilities、GameplayTags 和 GameplayTasks。
- 建立基础 Gameplay Tags、目录和文档基线。
- 不实现 Character、ASC、Ability、UI 或其他游戏功能。

## 计划文件

- `HSR.uproject`
- `Source/HSR.Target.cs`
- `Source/HSREditor.Target.cs`
- `Source/HSR/HSR.Build.cs`
- `Source/HSR/HSR.h/.cpp`
- `Config/DefaultEngine.ini`
- `Config/DefaultInput.ini`
- `Config/DefaultGameplayTags.ini`

现有 Markdown 必须合并更新，禁止覆盖历史。

## 初始 Tag 命名空间

- `Ability.*`
- `State.*`
- `Status.*`
- `Event.*`
- `Damage.*`
- `Element.*`
- `Cooldown.*`
- `UI.*`

Phase 0 只建立命名空间和配置入口，不创建 GameplayAbility 或 GameplayEffect。

## UE Editor 计划

1. 创建 Blank C++/Desktop 项目，不包含 Starter Content。
2. 确认 GAS 与 Enhanced Input 插件。
3. 检查 Gameplay Tags 设置。
4. 创建并保存 `Map_ProjectSetup`。
5. 设置 Editor Startup Map 和 Game Default Map。
6. 重启 Editor 验证插件和配置。

## 编译与测试计划

1. 生成项目文件。
2. 构建 `HSREditor Development Win64`。
3. 打开项目并确认主模块加载。
4. 打开 `Map_ProjectSetup` 并执行空白 PIE。
5. 检查 Output Log、插件和 Tags。
6. 记录实际 UE、VS、Compiler、SDK、BuildSettings、IncludeOrder 和 C++ 标准。

## 完成标准

- `.uproject` 可打开。
- Development Editor 构建通过。
- Enhanced Input 和 GAS 依赖可加载。
- Gameplay Tags 可在编辑器中查询。
- 空白 PIE 可运行。
- 没有自定义游戏功能代码。
- README、todo、worklog、learning journal 和本文件记录真实证据。

## 风险

- Visual Studio 2026 或 SDK 与 UE5.6 不兼容。
- 插件已启用但 Build.cs 缺少模块依赖。
- Tags 配置未被加载。
- 误把工程创建与 Phase 1 Gameplay 开发混为一轮。

## 第一周 Day 1–7 节奏

| 日 | 内容 | 完成标准 |
|---|---|---|
| Day 1 | 审核规范、路线图和 Phase 0 范围 | 文档一致，不把规划标为实施 |
| Day 2 | 创建 UE5.6 Blank C++ `HSR` | 单 Runtime 模块，历史 Markdown 保留 |
| Day 3 | 验证 VS2026、SDK、UBT/UHT 和 Build | 成功或形成单一可复现阻塞 |
| Day 4 | 确认 Enhanced Input、GAS 和根 Tags | 插件/模块/Tags 有真实证据 |
| Day 5 | 按需目录和 `Map_ProjectSetup` | 地图保存并设为默认 |
| Day 6 | 重开、增量 Build 和空白 PIE | Build、重开、PIE 均通过 |
| Day 7 | 缓冲和阶段复盘 | Phase 0 完成或阻塞记录完整 |

## Phase 0 禁止项

- 不创建 Character、Controller、ASC、AttributeSet、GameplayAbility 或 GameplayEffect。
- 不创建 Blueprint、UMG 和 Input Action。
- 不导入模型、动画、音频和 VFX。
- 不创建 Battle Map、SaveGame 或 Gameplay 系统。
- 不拆多个模块，不配置多人和 Steam。
