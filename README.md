# HSR

HSR 是一个全新的 Unreal Engine 5.6 C++ 单机 JRPG 学习与作品集项目，目标是逐步完成原创的探索和回合制战斗系统。

> `HSR` 仅为本地学习代号。项目不会复制任何商业游戏受版权保护的角色、剧情、美术、音乐、文本、UI、商标、专有名词或数值表；公开展示前将改用原创名称并完成资产授权审查。

## 当前状态

- [x] 建立 HSR 协作规范和规则迁移记录。
- [x] 建立第一阶段文档基线。
- [x] 创建 UE5.6 Blank C++ 项目并保持单 `HSR` Runtime 模块。
- [x] 完整验证 Visual Studio、UBT/UHT、Development Editor 构建和实际 C++ 标准。（用户通过 `_MSVC_LANG` 确认 C++20。）
- [x] 验证 Enhanced Input 的第三人称移动、镜头、跳跃及生命周期去重。
- [x] 完成 Phase 2 GAS 基础接入、属性观察链与生命周期专项。

当前仓库的 Phase 0～3 均为 `Ready`。Phase 4 已由 P4-001～004 完成收尾，最终状态为 `Ready with inherited follow-ups`；证据等级、用户接受和延期项均保留。Phase 5 尚未实施，必须等待本次 Phase 4 收尾 commit 与 push 结果记录后开始。

## MVP 范围

MVP 是一个灰盒 1v1 闭环：

`探索移动 → 静止 Encounter Actor → 独立 Battle Map → GAS 基础攻击 → 胜负 → 固定奖励 → 返回原位置 → 最小 Save/Load`

MVP 不包含完整技能、伤害公式、击破、状态、成长、装备、遗器、背包、任务、正式 UI/美术和多人。MVP 不等于 Phase 20 三角色垂直切片。

## 早期节奏

- 第一轮：只维护 Markdown。
- 第一周：只完成 Phase 0 工程初始化与验证。
- 第一月：Phase 0、Phase 1、Phase 2 最小 GAS、Phase 3 单对象交互；不开始 Phase 4 代码。

## 技术方向

- Unreal Engine 5.6
- Visual Studio 工具链（UBT 已记录 VS2022 14.51.36248 / MSVC 14.51.36231；项目文档中的 Community 2026 名称与实际构建日志仍需在补证任务中统一）
- C++20（用户通过 `_MSVC_LANG` 检查确认）
- Enhanced Input
- Gameplay Ability System
- UMG、Gameplay Tags、DataAsset、SaveGame
- Windows 优先，当前为单机

## 第一阶段目标

完成灰盒闭环：

`探索 → 遭遇 → 1v1 回合战斗 → 胜负结算 → 固定奖励 → 返回探索`

第一轮只创建并验证 UE5.6 Blank C++ 工程基线；第三人称角色从 Phase 1 开始搭建，不同时实现 GAS、战斗或存档。

## 项目文档

- [协作规范](.agents/agents.md)
- [规则迁移分析](docs/rule-migration-analysis.md)
- [第一阶段计划](docs/phase-1-project-baseline.md)
- [Phase 0–20 路线图](docs/phase-roadmap-0-20.md)
- [Phase 0 工程设置计划](docs/phase-0-project-setup.md)
- [MVP、第一月、第一周与第一轮计划](docs/mvp-first-month-first-week-plan.md)
- [任务计划](todo_plan.md)
- [工作日志](worklog.md)
- [学习日志](learning-journal.md)
- [GAS 学习笔记](docs/gas-notes.md)
- [结合项目阶段的 GAS 学习路线](docs/gas-learning-roadmap.md)
- [低级模型执行任务模板 A/B/C](docs/low-level-model-task-templates.md)
- [阶段执行与“下一步应该做什么”流程](docs/phase-execution-workflow.md)
- [Loop Engineering 协作流程](docs/loop-engineering-workflow.md)
- [Codex 与 UE Editor 协作边界](docs/codex-ue-editor-collaboration.md)
- [低级模型执行指南](docs/low-model-execution-guide.md)
- [可选阶段 Skill：phase-next-steps](.agents/skills/phase-next-steps/SKILL.md)
- [作品集记录](docs/portfolio-notes.md)
- [面试复盘记录](docs/interview-notes.md)
- [战斗系统设计](docs/battle-system-design.md)
- [数据资产规范](docs/data-asset-guidelines.md)
- [UI 规范](docs/ui-guidelines.md)
- [Debug 工具说明](docs/debug-tools.md)

## 版权与第三方资产

第一阶段只使用灰盒、引擎默认资源和原创占位内容。引入任何第三方资源前，必须记录来源、作者、许可证、署名要求、商用、修改与再分发范围。

Phase 1 已引入 Mixamo/Kachujin 角色与动画资产；许可证据、项目用途、public 仓库决策及 owner acceptance 记录于 `docs/asset-licenses/mixamo-kachujin.md`。未来正式发布、迁移资产或改变分发方式时复核届时官方条款，此项为非阻断 follow-up。
