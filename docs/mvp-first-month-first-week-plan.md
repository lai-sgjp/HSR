# HSR MVP、第一月、第一周与第一轮最小任务计划

> 状态：规划已确认，尚未创建 UE 工程或实施任何 Gameplay Phase。本文件是 MVP 与早期执行范围的唯一详细真源。

## 1. MVP 定义

### 1.1 一句话定义

在一张灰盒探索地图中接近敌人，进入独立 Battle Map，使用 GAS 基础攻击完成可复现的 1v1 回合战斗，获得一次性占位奖励，返回探索原位置，并通过单槽位 SaveGame 保存最小进度。

### 1.2 玩家体验流程

```text
启动或读取存档
→ 灰盒探索移动
→ 接近静止 Encounter Actor 并显示提示
→ 触发 Encounter
→ 进入独立 Battle Map
→ 根据稳定 ID 重建双方 Battle Actor 与 ASC
→ 按 Speed 决定行动顺序
→ 玩家点击基础攻击
→ 敌人执行确定性基础攻击
→ 胜利或失败
→ 胜利时获得一次性固定占位奖励
→ 返回探索地图原位置
→ 保存并重新读取最小进度
```

### 1.3 MVP 必须包含的功能

#### 探索

- 一张灰盒探索地图。
- 第三人称移动、镜头和跳跃。
- Enhanced Input。
- 一个静止 Encounter Actor；复杂巡逻、感知和追击不进入 MVP。
- 交互范围、交互提示和单次 Encounter 请求。
- Exploration、UIOnly、Battle 三种 ControlMode。

#### Encounter 与关卡切换

- `FHSREncounterRequest`。
- 独立 Battle Map。
- Context 保存玩家角色 ID、敌人/Encounter ID、先手类型、探索 Map ID 和 Return Transform。
- Pending Encounter 最多消费一次。
- BattleResult 最多消费一次。
- 跨地图禁止保存 Actor、Component、ASC、Widget、TurnQueue 或 Active GameplayEffect Handle。

#### 战斗

- 一名玩家角色和一名敌人。
- 玩家队伍和敌人队伍使用数组结构，但 MVP 各启用一名。
- Health、MaxHealth、Speed。
- AbilitySystemComponent 和 AttributeSet。
- 初始化属性 GameplayEffect。
- 基础攻击 GameplayAbility。
- 瞬时伤害 GameplayEffect。
- Speed 降序行动顺序。
- 同速使用稳定 Participant ID 裁决。
- 敌人执行确定性基础攻击。
- ActionResolved、死亡、胜利、失败和战斗清理。
- 返回探索地图和 Return Transform。

#### 奖励与存档

- 胜利发放一次固定占位奖励。
- Reward Transaction 使用唯一 ID，避免重复发放。
- 单槽位版本化 SaveGame。
- 保存当前探索地图、返回位置、已击败 Encounter ID、占位奖励数量和当前演示角色 ID。
- 不保存中途战斗。

### 1.4 MVP 必须有的 UE 类

| 类型 | MVP 职责 |
|---|---|
| `AHSRCharacterBase` | ASC、AttributeSet 和角色公共生命周期 |
| `AHSRExplorationCharacter` | 探索移动、镜头和 InteractionComponent |
| `AHSRPlayerController` | Enhanced Input、ControlMode、HUD 和命令提交 |
| `AHSRGameModeBase` | 探索地图默认类 |
| `AHSRHUD` | 根 UI 入口 |
| `UHSRInteractionComponent` | 候选 Encounter、提示和交互请求 |
| `IHSRInteractableInterface` | 交互协议 |
| `AHSREncounterActor` | 静止敌人入口，不承担复杂 AI |
| `UHSRBattleTransitionSubsystem` | Pending Encounter、BattleResult 和返回上下文 |
| `AHSRBattleGameMode` | Battle Map 启动和参与者生成 |
| `UHSRBattleCoordinatorSubsystem` | 战斗生命周期、命令验证和胜负 |
| `UHSRTurnManager` | 行动队列、阶段和当前行动者 |
| `UHSRBattleParticipantComponent` | 战斗身份、队伍、ASC 和存活查询 |
| `AHSRBattleCharacter` | Battle Map 内玩家/敌人实体 |
| `UHSRAbilitySystemComponent` | GAS 统一入口 |
| `UHSRCoreAttributeSet` | Health、MaxHealth、Speed |
| `UHSRBasicAttackAbility` | 无 Cost 的基础攻击 |
| `UHSRRewardSubsystem` | 固定幂等奖励事务 |
| `UHSRSaveSubsystem` | 唯一 Save/Load 入口 |
| `UHSRSaveGame` | 最小版本化存档 |

MVP 不创建完整 Enemy AI、Skill、Inventory、Quest、Equipment 或 Relic 系统。

### 1.5 MVP 核心类型

- `EHSRPlayerControlMode`
- `EHSREncounterInitiative`
- `EHSRBattlePhase`
- `EHSRBattleTeam`
- `EHSRBattleOutcome`
- `FHSREncounterRequest`
- `FHSRReturnContext`
- `FHSRBattleParticipantHandle`
- `FHSRTurnEntry`
- `FHSRBattleActionCommand`
- `FHSRBattleResult`
- `FHSRRewardTransaction`
- `FHSRSaveHeader`
- `FHSRMVPSaveData`

### 1.6 MVP 必须包含的 GAS 概念

- AbilitySystemComponent。
- `IAbilitySystemInterface`。
- Owner Actor 与 Avatar Actor。
- `InitAbilityActorInfo`。
- AttributeSet。
- Health、MaxHealth、Speed。
- 初始化 GameplayEffect。
- 基础攻击 GameplayAbility。
- 瞬时伤害 GameplayEffect。
- Gameplay Tags：
  - `Ability.Attack.Basic`
  - `State.Battle.Active`
  - `State.Battle.CurrentTurn`
  - `State.Dead`
  - `Event.Battle.ActionResolved`
- Attribute Change Delegate。
- Ability 成功、失败和取消的区别。
- 独立 Battle Map 中的 ASC 重建。

MVP 不使用 ExecutionCalculation、Cost、Cooldown、GameplayCue、Prediction、复制和回合状态 GE。

### 1.7 MVP 必须有的 DataAsset

| DataAsset | 内容 |
|---|---|
| `UHSRCharacterDefinition` | Character ID、Battle Actor 类、初始 Health/Speed、基础攻击 |
| `UHSREnemyDefinition` | Enemy ID、Battle Actor 类、初始 Health/Speed、基础攻击 |
| `UHSREncounterDefinition` | 敌方编组、Battle Map、先手和 Reward ID |
| `UHSRRewardDefinition` | 固定占位奖励 |

GameplayAbility 和 GameplayEffect 资产也是 MVP 配置，但不属于普通 DataAsset。

### 1.8 MVP 必须有的 UI

#### 探索 UI

- 交互提示。
- Save/Load 状态提示。
- 可选的占位奖励数量。

#### 战斗 UI

- 玩家和敌人的 Health/MaxHealth。
- 当前行动者。
- 一个基础攻击按钮。
- Ability 拒绝占位反馈。
- 胜利、失败和奖励结果。
- 返回探索提示。

UI 禁止直接扣血、推进 TurnManager、发奖励、写 SaveGame 或使用 Tick 轮询 Attribute。

### 1.9 MVP SaveGame

保存：

- SaveVersion。
- 当前探索 Map ID。
- 玩家 Return Transform。
- 已击败 Encounter ID 集合。
- 占位奖励数量。
- 当前演示角色 ID。

不保存：

- Actor、Component、Widget、ASC、AttributeSet 对象。
- Active GameplayEffect Handle。
- TurnQueue。
- 当前 Battle Map 中途状态。
- Ability 实例和 UI 状态。

### 1.10 MVP 不包含的内容

- 抽卡、概率和保底。
- 多角色队伍实际战斗。
- 战技、终结技和被动。
- Attack、Defense、Crit 和 ExecutionCalculation。
- 元素、弱点、韧性和击破。
- Buff/Debuff。
- 完整敌人 AI。
- 装备、遗器、随机词条和背包。
- 成长、等级、突破和技能升级。
- NPC 对话、任务、商店和地图 UI。
- 正式主菜单和复杂 UI。
- 正式模型、动画、音频和 VFX。
- 多人、Steam、Session、RPC、复制和预测。
- 中途战斗存档和正式数值平衡。

### 1.11 MVP 演示流程

1. 从干净存档启动。
2. 在灰盒探索地图移动和观察。
3. 接近 Encounter Actor，显示提示。
4. 触发 Encounter，进入独立 Battle Map。
5. 显示双方 HP 和当前行动者。
6. 玩家与敌人轮流基础攻击。
7. 展示胜利或失败。
8. 胜利时获得固定奖励。
9. 返回探索地图原位置。
10. 保存、退出、重新加载。
11. 验证位置、Encounter 和奖励进度。

### 1.12 MVP 完成标准

- 完整流程可从干净 Save 重复完成。
- Encounter 和 BattleResult 都只消费一次。
- Speed 排序正确，同速结果稳定。
- 非当前行动者不能行动。
- Ability 失败不会锁死回合。
- Health 始终在合法范围。
- Reward 最多发放一次。
- Victory/Defeat 都能安全返回探索。
- 返回位置和输入正确。
- Save/Load 后最小进度正确。
- Development Editor 构建和 PIE 主路径/失败路径通过。
- 没有 Tick 轮询输入、属性或 UI。
- 没有跨地图失效对象引用。
- README、worklog、learning journal 和设计文档同步。

### 1.13 MVP 风险

- 独立 Battle Map 增加 Actor/ASC 生命周期难度。
- `InitAbilityActorInfo` 时机错误。
- Encounter 或 BattleResult 重复消费。
- Ability 失败导致 TurnManager 停滞。
- UI Delegate 未解绑。
- Reward 重复发放。
- SaveGame 保存了运行时对象。
- MVP 被 Skill、Break、Equipment 和正式 UI 持续扩大。

### 1.14 学习与作品集价值

学习价值：UE 模块、Enhanced Input、Game Framework、Interface、Component、GAS 最小闭环、状态机、跨地图 Context、Data/Runtime/Save 分层、Delegate UI、幂等事务和失败恢复。

作品集价值：证明能从 Blank C++ 项目建立第三人称控制、理解 GAS、设计可复现回合战斗、处理跨地图生命周期、实现数据驱动和最小版本化存档，并具有测试、Debug 和版权意识。

---

## 2. 第一月计划

### 2.1 第一月总体目标

完成 Phase 0、Phase 1、Phase 2 最小 GAS 和 Phase 3 单对象交互。第一月不开始 Phase 4 代码，只允许在最后补充 `FHSREncounterRequest` 设计草案。

| 周 | 范围 | 可见成果 | 学习重点 | GAS | 文档 | 测试 | 主要风险 | 完成标准 |
|---|---|---|---|---|---|---|---|---|
| Week 1 | Phase 0 | 空白工程可编译、打开、PIE | UBT/UHT/Module/Plugin/Config | 只确认插件和 Tags | phase-0、README、worklog、todo、learning | Build、重开、插件、Tags、空白 PIE | 工具链和范围偷跑 | 无 Gameplay 类且证据完整 |
| Week 2 | Phase 1 | 灰盒角色移动、观察、跳跃 | Character/Controller/Possession/Input/Camera | 不接入 | phase-1、输入数据流 | 移动、帧率、ControlMode、Possession | 输入职责和 Tick | 无自定义 Tick，职责清晰 |
| Week 3 | Phase 2 最小 | HUD 显示并修改 Health/Energy/Speed | ASC、Owner/Avatar、AttributeSet、Delegate | 最小属性闭环 | gas-notes、learning、worklog | 重复初始化、边界、Widget 解绑 | Actor Info 和重复 GE | 属性只初始化一次，事件 UI |
| Week 4 | Phase 3 子集 | 接近方块显示提示并交互 | UInterface、Component、Overlap、Weak Ptr | 只预留 Tags | 交互决策和 Encounter 草案 | 进入/离开、销毁、多候选、重复交互 | 同时做过多对象/AI | 单对象交互稳定 |

### 2.2 第一月结束演示

```text
启动 UE 项目
→ 进入灰盒地图
→ 移动、观察、跳跃
→ HUD 显示 GAS 属性
→ 使用测试入口改变 Health/Energy
→ 接近灰盒交互对象
→ 显示提示并完成交互
```

第一月不承诺 Encounter、Battle Map 或可玩战斗。

---

## 3. 第一周详细计划

### 第一周目标

只完成 Phase 0：建立可编译、可打开、可 PIE 的 UE5.6 Blank C++ 工程和可靠文档基线。

| 日 | 建议任务 | 可见成果 | 学习点 | 文档更新 | 风险 | 完成标准 |
|---|---|---|---|---|---|---|
| Day 1 | 审核 agents、roadmap、todo、README、worklog 和 Phase 0 | 范围和禁止项统一 | 文档职责和阶段门禁 | 补缺失但不覆盖历史 | 把规划标成实施 | 文件职责和验收一致 |
| Day 2 | UE5.6 创建 Blank C++ `HSR`，关闭 Starter Content | `.uproject` 和单模块 | Project/Target/Build.cs | 记录生成文件 | 误用 Third Person/覆盖 Markdown | 工程骨架存在、历史保留 |
| Day 3 | 验证 VS2026、SDK、UBT/UHT 和首次 Build | Development Editor Build 结果 | 编译链 | 记录版本、首错和证据 | 同时改多个配置 | 成功或形成单一可复现阻塞 |
| Day 4 | 确认 Enhanced Input、GAS 和根 Tags | Tags 可在 Project Settings 查询 | Plugin 与 Module | 更新 gas-notes | 启用无关插件/提前写 GAS 类 | 插件和 Tags 有证据 |
| Day 5 | 建立必要目录和 `Map_ProjectSetup` | Editor 默认打开项目地图 | Content/Map 设置 | 记录按需目录 | 创建全部长期空目录/导入资产 | 地图保存并设为默认 |
| Day 6 | 重开、增量构建和空白 PIE | 工程可重复运行 | Editor/PIE 生命周期 | 写真实验证 | 只验证一次 | Build、重开、PIE 通过 |
| Day 7 | 缓冲、阶段检查和复盘 | 明确完成或阻塞结论 | 证据化验收 | README/worklog/todo/learning/phase-0 | 提前写 Character | 清单闭合或阻塞证据完整 |

### 第一周禁止内容

- Character、Controller、ASC、AttributeSet。
- Blueprint、UMG、Input Action。
- 模型、动画、音频、VFX。
- Battle Map 和 SaveGame。
- GameplayAbility 和 GameplayEffect。
- 多模块和联网配置。

### 第一周演示

打开 HSR → 展示单 Runtime 模块和 Blank Map → 展示 Enhanced Input/GAS 插件与 Tags → 空白 PIE → 展示 Phase 0 验证文档。

---

## 4. 第一轮最小文档任务

### 4.1 第一轮与 Phase 0 的区别

- 第一轮只维护 Markdown，发生在 Phase 0 之前。
- Phase 0 才允许创建 `.uproject`、Source、Config、Map 和运行构建。
- 当前所需 Markdown 已多数存在，第一轮应审核、合并和补缺，不得盲目覆盖或重复创建。

### 4.2 可直接复制的 Codex Prompt

```text
你现在负责 HSR 项目的第一轮最小文档任务。

本轮只允许维护 Markdown。禁止创建或修改 UE 工程、C++、Blueprint、Config、插件、Content 资产、构建产物和 Git 状态。

项目背景：
- HSR 是计划中的 UE5.6 Blank C++ + GAS 单机回合制 JRPG 学习项目。
- 第三人称系统在 Phase 1 从零搭建。
- 探索战斗采用独立 Battle Map。
- 核心逻辑 C++；蓝图只用于配置、动画、UI、派生类和资源绑定。
- 当前只维护规范、路线图和长期记忆，不开始工程实现。

执行规则：
1. 先使用非终端的只读文件浏览能力读取当前工作区。
2. 禁止调用 shell、terminal、PowerShell、cmd、构建工具、UE Editor 或 Git。
3. 如果环境只能通过命令读取文件，立即停止并报告限制，不要绕过。
4. 重点检查：
   - .agents/agents.md
   - README.md
   - worklog.md
   - todo_plan.md
   - learning-journal.md
   - docs/phase-roadmap-0-20.md
   - docs/phase-0-project-setup.md
   - docs/gas-notes.md
   - docs/battle-system-design.md
   - docs/data-asset-guidelines.md
   - docs/mvp-first-month-first-week-plan.md
5. 修改前先说明将创建/修改哪些文件、每个文件用途及必须保留的历史。
6. 使用受控文件编辑工具维护 Markdown；不得使用 shell 写文件。
7. 已存在文件必须合并更新，不得整文件覆盖历史。
8. 仅在缺失时创建上述 Markdown。
9. 文档必须统一包含或引用：
   - Phase 0 规划；
   - GAS 初学者路线；
   - Phase 0–20 摘要；
   - Blank C++ 与独立 Battle Map 决策；
   - GAS/TurnSystem 和 DataAsset/Runtime/SaveGame 边界；
   - UI 只提交命令；
   - 版权规则；
   - MVP、第一月、第一周和第一轮边界。
10. todo_plan.md 只能勾选真实完成的文档规划，不得将 Phase 0 或 Gameplay Phase 标为完成。
11. worklog.md 追加用户目标、检查、决策、修改、未验证和下一步，不覆盖历史。
12. learning-journal.md 只记录可复习概念。
13. 禁止创建 .uproject、Source、Config、Content、Binaries、Intermediate、Saved、C++、Blueprint、DataAsset 或 UMG；禁止启用插件、导入资源、运行命令、构建、Editor 或 Git。
14. 最后使用非终端只读能力检查标题、中文和一致性。
15. 最终回答列出已创建、已修改、保留历史、未验证内容，并明确没有创建 UE 工程或 Gameplay；下一步只能建议 Phase 0，不能执行。
```

### 4.3 第一轮完成标准

- 所需 Markdown 全部存在。
- 既有历史未覆盖。
- Blank C++、独立 Battle Map 和阶段门禁一致。
- 没有 `.uproject`、Source、Config、Content 或构建目录。
- 没有 Gameplay Phase 被标为完成。
- 交接可直接进入未来 Phase 0。

---

## 5. 早期坚决不要做清单

### 产品范围

- 不做完整抽卡、概率、保底和抽卡动画。
- 不做完整剧情、大型开放世界和大量角色/敌人。
- 不做复杂 Boss 和多阶段商业级演出。
- 不追求完整商业游戏功能量。

### GAS 与战斗

- 不做“完整 GAS 架构”。
- 未理解 ASC/AttributeSet/GE 前不做复杂 Ability。
- 不做 GAS 预测、复制和多人。
- 不做复杂伤害乘区、完整元素/弱点/击破和大量状态。
- 不把 GAS 秒数 Duration 当回合数。
- 不把 TurnSystem 塞进 GameplayAbility。
- 不用动画或 GameplayCue 决定伤害和回合完成。

### 成长与物品

- 不做完整遗器随机词条。
- 不做复杂装备强化和随机生成。
- 不做完整成长、突破、天赋树、背包和商店。
- MVP 不做实际多角色队伍战斗。

### UI 与表现

- 不做复杂 UI 和全部养成页面。
- 不在 Widget 中写核心逻辑。
- 不使用 Widget Tick 轮询 Attribute 或状态。
- 不导入大量模型。
- 不把“免费”视为可商用、公开或再分发。
- 核心规则稳定前不做复杂镜头、音效和 VFX。

### 工程架构

- 不把所有功能塞入 Character。
- 不把所有 Manager 都做成 GameInstanceSubsystem。
- 不提前拆 HSRCore、HSRGAS、HSRBattle、HSRUI 等模块。
- 不创建大量空目录、空类和未出现共性的抽象。
- 不在 DataAsset 保存运行时进度。
- 不在 SaveGame 保存 Actor、Widget、ASC 或 GE Handle。
- 不在 GameInstanceSubsystem 保存跨地图 Actor 指针。
- 不用显示名称作为稳定 ID。

### 性能、网络和工作流

- 不默认开启自定义 Tick。
- 不在 Tick 中搜索 Actor、更新 UI、加载资源或计算战斗。
- 不做多人、Steam、Session、Lobby、RPC 或复制。
- 不一次实现多个 Phase。
- 不在未说明职责前生成大量代码。
- 不绕过阶段编译和 PIE 验收。
- 每个角色完成任务后按固定格式 commit；只有 Phase 全部子任务完成并通过门禁后，由高级协调者执行阶段收尾 commit/push。
- 不覆盖 worklog 历史。
- 不把文档规划完成当作 Gameplay 完成。
- 构建错误时不同时修改多个无关配置。

---

## 6. 范围层级与默认值

| 层级 | 定义 |
|---|---|
| 第一轮 | 只维护 Markdown，不创建 UE 工程 |
| Phase 0 | 创建和验证 Blank C++ 工程，不写 Gameplay |
| 第一周 | 只完成 Phase 0 |
| 第一月 | Phase 0、1、2 和 Phase 3 单对象交互 |
| MVP | Phase 0–5 加最小 Reward/Save 子集，形成 1v1 可玩闭环 |
| Phase 20 | 三角色、Boss、任务、成长和完整展示的垂直切片 |

MVP 不等于第一月成果，也不等于 Phase 20。
