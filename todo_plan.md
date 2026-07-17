# HSR Todo Plan

> 当前状态：Phase 0、Phase 1 与 Phase 2 均为 `Ready`。P2-003 以 `USER ACCEPTED` 收尾，Reviewer `REVISE` 原结论保留；Phase 3 尚未开始。

## 已完成的规划工作

- [x] 迁移 ChatBot SNS 的文档协作经验。
- [x] 迁移 Blaster 的 UE5.6 C++ 工程规范，排除多人射击业务。
- [x] 建立 `.agents/agents.md`、README、worklog、todo 和 learning journal。
- [x] 确认单 `HSR` Runtime 模块起步。
- [x] 确认 Blank C++ 模板，第三人称角色在 Phase 1 从零搭建。
- [x] 确认 GAS/TurnSystem、DataAsset/Runtime/SaveGame 和 UI 命令边界。
- [x] 确认探索战斗进入独立 Battle Map。
- [x] 完成 Phase 0–20 路线图和长期系统蓝图。
- [x] 收敛 MVP、第一月、第一周和第一轮最小文档任务。
- [x] 创建 `docs/mvp-first-month-first-week-plan.md` 作为早期范围真源。
- [x] 完成 Stage 0–10 的项目阶段化 GAS 学习路线。
- [x] 创建 `docs/gas-learning-roadmap.md` 作为 GAS 学习真源。
- [x] 创建 `docs/low-level-model-task-templates.md` 作为低级模型任务模板真源。
- [x] 完成模板 A：纯 Markdown 文档任务。
- [x] 完成模板 B：1–5 个明确文件范围内的小型 C++ 功能任务。
- [x] 完成模板 C：基于明确证据和复现步骤的 Debug/修复任务。
- [x] 固化 13 个任务占位符、文件允许清单硬边界和停止扩权规则。
- [x] 固化 Codex 与用户的 UE Editor/C++ 协作边界。
- [x] 归档可选 Loop Engineering 角色和复杂任务审查闭环。
- [x] 创建 `docs/phase-execution-workflow.md`，记录 Phase 0–20 通用处理流程和“下一步”判断模板。
- [x] 创建项目级可选 Skill `phase-next-steps`，提供阶段门禁和相邻任务建议。
- [x] 创建协作参考：Loop Engineering、UE Editor 边界、低级模型、作品集和面试文档。
- [x] 建立 CC-SWITCH 文件化上下文机制：`PROJECT_STATE.md`、单一活动任务卡、任务/审查模板、归档目录和模型切换 Prompt。
- [x] 明确高级模型维护全局快照、低级模型只以 `tasks/active-task.md` 为上下文入口、审查模型独立核验证据。

## 低级模型任务门禁

- [x] 交付任务前选择且只选择模板 A、B 或 C。
- [x] 替换模板中的全部 `【占位符】`。
- [x] `【允许修改的文件】` 使用明确路径，未列入的文件视为禁止修改。
- [x] 确认任务只包含一个可以独立验收的小目标。
- [x] 高级模型在交付前读取 agents、todo、worklog、对应 Phase 和专项设计文档，并把必要上下文写入 `tasks/active-task.md`。
- [x] 低级模型只以 `tasks/active-task.md` 为上下文入口，只接触任务卡明确列出的目标文件。
- [x] 发现需要扩大文件或功能范围时停止并请求授权。
- [x] 最终按真实结果区分已完成、已验证、未验证和 Editor 手动操作。

## 阶段 Skill 使用门禁（可选）

- [x] 只有用户询问阶段下一步、门禁或最佳实践时才调用 `phase-next-steps`。
- [x] 调用前读取当前 agents、todo、worklog、README、roadmap 和 Phase 文档。
- [x] Skill 只推荐一个相邻小任务，不自动修改文件或推进 Phase。
- [x] 推荐必须区分 Codex 文件工作、用户 Editor 操作和验证证据。
- [x] Skill 建议不替代用户当前回合的明确要求，也不替代实际编译/PIE 证据。

## 本轮文档协作落地

- [x] 记录 Codex 与 UE Editor 的职责分工。
- [x] 记录 Loop Engineering 的可选角色闭环。
- [x] 记录所有 Phase 的通用执行流程和下一步输出结构。
- [x] 创建 `phase-next-steps` Skill 骨架、元数据和阶段矩阵引用。
- [x] 运行真实 HSR Phase 0 工程任务并取得编译/Editor/PIE 证据。（P001-P007 已执行并归档；实际 C++ 标准仍单独未验证。）
- [x] 将本轮文档变更提交到本地 Git。
- [x] 将本轮文档变更推送到远程仓库 `origin/main`。

## 全局执行门禁

- [ ] 每次只实施一个 Phase 或其中一个可独立验收的小节。
- [ ] 开始前列出将修改的文件和职责。
- [ ] 完成后执行编译、PIE、专项测试和文档更新。
- [ ] 没有验证证据时不得将任务标记为完成。
- [ ] DataAsset、Runtime、SaveGame 的字段归属必须明确。
- [ ] UI 只提交命令和显示状态，不直接修改业务真源。
- [ ] 不为未来联机提前实现 Steam、会话、RPC 或复制。
- [ ] 第三方资源导入前完成授权审查。

## GAS 学习门禁

- [ ] Stage 0：能解释 GAS/TurnSystem/Data/Save/UI 边界，不创建 GAS Gameplay 类。
- [ ] Stage 1：完成 ASC、Owner/Avatar、Core AttributeSet、初始化 GE 和 Attribute Delegate。
- [ ] Stage 2：完成 Instant Damage/Healing GE、Meta Attribute、Pre/Post 和 Clamp。
- [ ] Stage 3：完成 Ability 激活、目标、Cost/Commit、Resolution 和 TurnManager 通知。
- [ ] Stage 4：建立 Tags 命名、生产者/消费者、Block/Cancel/Cooldown 规则。
- [ ] Stage 5：完成 MMC/ExecutionCalculation 选择、捕获属性、原创公式和 Damage Breakdown。
- [ ] Stage 7：完成 Weakness/Toughness/Break 与 Turn Delay 的职责分离。
- [ ] Stage 6：完成 Infinite GE + Turn Status Runtime、叠层、免疫和命中抵抗。
- [ ] Stage 8：完成无 Tick 的 Attribute/Tag/Effect UI 和 Command 提交。
- [ ] Stage 9：完成装备/遗器 Infinite GE、来源追踪、卸下和 Save 重建。
- [ ] Stage 10：只在单机系统稳定后学习 Character/PlayerState ASC、Full/Mixed/Minimal、预测和权威。
- [ ] 每个 Stage 均更新 gas-notes、learning journal、worklog、测试证据和面试复盘。
- [ ] 不能脱离代码解释数据流时，不得将 Stage 标记为完成。

## 第一轮文档任务门禁

- [x] 完整定义第一轮只维护 Markdown。
- [x] 明确第一轮不等于 Phase 0 已开始。
- [x] 提供可直接复制的文档维护 Prompt。
- [x] 实际执行第一轮 Prompt 时只审核、合并和补缺现有 Markdown。
- [x] 不创建 `.uproject`、Source、Config、Content 或构建目录。
- [x] 不运行 shell、构建、Editor、插件操作或 Git。

## 第一周门禁

- [x] 只实施 Phase 0，不创建 Character、Controller、ASC、AttributeSet、Blueprint 或 UMG。
- [x] Day 1–7 按文档审核、Blank 工程、工具链、插件/Tags、地图、回归、复盘推进。
- [x] Phase 0 Build、Editor 重开和空白 PIE 均有真实证据。
- [x] 第一周结束前不进入 Phase 1。

## 第一月门禁

- [x] Week 1 只完成 Phase 0。
- [x] Week 2 只完成 Phase 1 第三人称探索角色。
- [ ] Week 3 只完成 Phase 2 最小 GAS 属性闭环。
- [ ] Week 4 只完成 Phase 3 单一灰盒交互对象。
- [ ] 第一月不编写 Phase 4 Enemy/Encounter 代码，只允许形成接口草案。

## MVP 验收清单

- [ ] 灰盒探索地图、第三人称移动、镜头和 Enhanced Input。
- [ ] 一个静止 Encounter Actor 和交互提示。
- [ ] 独立 Battle Map 与纯数据 Encounter/Return Context。
- [ ] Battle Map 重建 1 名玩家和 1 名敌人的 Actor/ASC。
- [ ] Health、MaxHealth、Speed、初始化 GE 和基础攻击 Ability。
- [ ] Speed 排序、稳定同速裁决、确定性敌人行动和胜负。
- [ ] 固定幂等奖励、返回原位置和单槽位最小 Save。
- [ ] Encounter、BattleResult 和 Reward 均最多消费一次。
- [ ] 无跨地图 Actor/ASC/Widget/GE Handle 引用。
- [ ] Build、PIE 主路径/失败路径和 Save 往返有证据。
- [ ] MVP 文档、README、worklog 和 learning journal 完整。

## Phase 0：项目初始化与规范落地

- [x] 使用 UE5.6 Blank C++ 模板创建 `HSR`，关闭 Starter Content。
- [x] 创建单 `HSR` Runtime 模块。
- [x] 验证 Visual Studio Community 2026、Windows SDK、UBT/UHT 和实际 C++ 标准。（实际构建工具链已记录；用户通过 `_MSVC_LANG` 确认 C++20。）
- [x] 确认 Enhanced Input、GameplayAbilities、GameplayTags、GameplayTasks。
- [x] 建立根 Gameplay Tags 和按需目录。（八个根 Tags 与按需 `Content/Maps` 目录均已有文件和 Editor 证据。）
- [x] 创建并设置 `Map_ProjectSetup`。
- [x] 构建 Development Editor、打开 Editor、执行空白 PIE。（P006：UBT 退出码 0，Editor 重开、默认地图与空白 PIE 启停均由用户确认；本轮构建为 up-to-date。）
- [x] 更新 Phase 0、README、worklog、learning journal 和本计划。（P007 协调归档已同步真实证据与未验证项。）
- [x] 不创建 Gameplay 类或资源。（截至 `f18269a` 的 Git 树仅有最小模块入口、Config 与 `Map_ProjectSetup` 基线资产，无 Gameplay 类、Blueprint、UI、输入资产或第三方资源。）

## Phase 1：第三人称探索角色

- [x] 从 Blank 项目创建 CharacterBase、ExplorationCharacter、PlayerController、GameMode 和 HUD。
- [x] P1-001：完成 CharacterBase、ExplorationCharacter、CameraBoom、FollowCamera 与 CharacterMovement 基础配置；UHT/编译/链接通过，用户人工验收通过并明确跳过独立 Reviewer。（不代表移动、输入、PIE 或 Phase 1 总项完成。）
- [x] P1-002：完成 PlayerController、ControlMode 幂等入口与 Possession 安全边界；UHT/编译/链接通过，用户人工验收通过并明确跳过独立 Reviewer。（Editor/PIE Possession 与输入集成仍未验证。）
- [x] P1-003A：完成 Enhanced Input C++ 接口、Action 绑定入口与 Mapping Context 生命周期；构建通过并由用户人工验收。
- [x] P1-003B：创建 Move、Look、Jump、Interact Input Actions 与 Exploration Context；用户确认 Value Type、IMC 配置及 Editor 重开持久性，五资产 commit 为 `7c71ae825fb840ace6d76fc6232883b807d395d1`。（Blueprint 引用绑定与完整 PIE 仍未验证。）
- [x] P1-004：完成 GameMode、派生 BP、灰盒地图、探索 HUD 与可玩闭环；A2 移除手工 PushInputComponent 和高频日志；用户验证 UIOnly 往返、同会话 Re-Possess、Context/Binding/HUD 去重；最终 Reviewer `6b19d179562f03c8cc50b94456d3a943478855c0` 为 `PASS`。
- [x] P1-005：完成基础 AnimBP、角色/动画资产接入与最终回归；用户补齐 Editor/PIE/失败路径证据，最终 Reviewer `af6b14898f589cd44fbd176488dcd5e82c309d4b` 为 `PASS WITH FOLLOW-UP`。
- [x] P1-006：工程归档、Teacher 技术/源码学习提交与 Coordinator Phase 1 最终收尾；Teacher commit `70efd6f24f5d8532f74d0994c8c551d9353d6204`，Phase 1 判定 `Ready`。

## Phase 2：GAS 基础接入

- [x] P2-001：创建 ASC、Core AttributeSet 和初始化 GE，可在探索 PIE 中显示五项初始化属性。
- [x] P2-001：实现 Health、MaxHealth、Energy、MaxEnergy、Speed 的初始化可见闭环。
- [x] P2-001：正确初始化当前单机 Owner=Avatar=self 和 Actor Info。
- [x] P2-001：使用 Attribute Delegate 驱动调试 UI，无业务 Tick。
- [x] P2-002：验证动态属性变化、Max 降低后的 Current 收敛、Widget 重建/解绑、连续 PIE 和 Re-Possess 回归；最终 Reviewer `PASS`。
- [x] P2-003：完成资产、工程核对、教学与阶段收尾；最终处置 `USER ACCEPTED`，学习缺口作为非阻断复习项保留，Reviewer 结论仍为 `REVISE`。

## Phase 3：交互系统与探索对象

- [ ] 创建 Interactable Interface、InteractionComponent 和交互类型。
- [ ] 创建 NPC、宝箱、传送点占位对象。
- [ ] 使用 Overlap 事件维护弱引用候选，不使用 Tick 搜索。
- [ ] 创建交互提示 UI 和失败原因日志。

## Phase 4：敌人探索表现与遇敌触发

- [ ] 创建 EnemyCharacter、AIController、Enemy/Encounter DataAsset。
- [ ] 实现巡逻占位、AI Perception、发现和追击。
- [ ] 创建 EncounterRequest 和先手类型。
- [ ] 创建 BattleTransitionSubsystem，Context 只保存稳定 ID、Map 和 Transform。
- [ ] 验证进入空 Battle Map 和返回流程。

## Phase 5：回合制战斗最小闭环

- [ ] 创建 BattleCoordinator、BattleParticipant、BattleGameMode 和 TurnManager。
- [ ] 在 Battle Map 根据 Context 重建玩家/敌人 Actor 与 ASC。
- [ ] 实现 Speed 排序和稳定同速裁决。
- [ ] 实现最小普攻 GameplayAbility 和固定伤害 GE。
- [ ] 实现 ActionResolved、死亡、胜负和单次 BattleResult。
- [ ] 返回探索地图和原 Transform。

## Phase 6：GAS 技能系统基础

- [ ] 建立 GameplayAbilityBase、普攻、战技、终结技占位和治疗。
- [ ] 建立 SkillDataAsset 和目标类型。
- [ ] 使用 GAS GE 处理 Energy/HP Cost、伤害和治疗。
- [ ] 使用 BattleCoordinator 原子管理共享战技点。
- [ ] 区分 Ability 成功、取消和失败 Resolution。

## Phase 7：属性、伤害公式与暴击

- [ ] 扩展 Attack、Defense、CritRate、CritDamage。
- [ ] 建立原创 DamageRule 和 ExecutionCalculation。
- [ ] 使用可复现随机并输出 Damage Breakdown。

## Phase 8：弱点、韧性与击破

- [ ] 建立 Element Tags、Weakness、Toughness/MaxToughness。
- [ ] 实现削韧、击破伤害、Turn Delay 和击破 Debuff。
- [ ] 创建弱点图标和韧性条。

## Phase 9：Buff、Debuff 与状态效果

- [ ] 建立 StatusDefinition、StatusComponent 和 Runtime StatusInstance。
- [ ] 实现回合持续时间、叠层、刷新、免疫、驱散和触发时机。
- [ ] 使用 Infinite GE 表达属性/Tag，TurnSystem 管理剩余回合。

## Phase 10：战斗 UI 完整化

- [ ] 完成行动条、技能、目标、状态、Buff、伤害数字和结果界面。
- [ ] 使用 ViewModel/Delegate 更新，不使用 Widget Tick。
- [ ] 完成键鼠和手柄焦点基础验证。

## Phase 11：角色数据与成长

- [ ] 建立 CharacterDataAsset、成长曲线、等级、经验、突破、技能等级和被动。
- [ ] 建立 PartySubsystem 和角色详情 UI。
- [ ] 引入最小版本化 SaveGame/SaveSubsystem。

## Phase 12：装备与遗器

- [ ] 建立 Equipment/Relic Definition、实例、槽位、词条、套装和强化。
- [ ] 建立 StatAggregator 和属性来源分解。
- [ ] 使用可移除 Infinite GE 应用装备效果。

## Phase 13：背包、奖励与掉落

- [ ] 建立 ItemDefinition、ItemInstance 和 InventorySubsystem。
- [ ] 建立幂等 RewardTransaction 和掉落表。
- [ ] 接入敌人、宝箱、战斗奖励和背包 UI。

## Phase 14：任务与对话

- [ ] 建立 Quest/Dialogue DataAsset 和 Runtime State。
- [ ] 使用类型化领域事件推进任务。
- [ ] 完成一个 NPC、一个分支对话和一个任务奖励闭环。

## Phase 15：地图、传送与关卡流程

- [ ] 建立 Map/Teleport Definition 和 MapSubsystem。
- [ ] 保存区域解锁、当前地图、玩家位置和探索状态。
- [ ] 验证两个探索地图与独立 Battle Map 稳定往返。

## Phase 16：存档系统总整合

- [ ] 整合角色、队伍、背包、装备、遗器、任务、宝箱、敌人、地图和位置。
- [ ] 建立 SaveVersion、验证、备份、迁移和失败恢复。
- [ ] 确保不保存 Actor、Widget、ASC 或 GE Handle。

## Phase 17：UI 总整合

- [ ] 完成主菜单、暂停、角色、队伍、背包、任务、地图和设置界面。
- [ ] 建立 Screen Stack、统一 InputMode 和导航。
- [ ] 验证地图旅行后的 UI 重建。

## Phase 18：表现与模型导入

- [ ] 建立模之屋等资源的授权审查与隔离导入流程。
- [ ] 完成骨骼、动画、Montage、镜头、音效、VFX、UI 动效和受击反馈。
- [ ] 保证表现系统不成为 Gameplay 规则真源。

## Phase 19：Debug、GM 与测试

- [ ] 建立快速战斗、经验/角色/物品/装备、任务重置和地图传送命令。
- [ ] 可视化 ASC、Attributes、Active Effects、Turn Order 和 Damage Breakdown。
- [ ] 增加 Turn、Damage、Status、Reward、Inventory、Quest、Save 和 Transition 自动化测试。

## Phase 20：垂直切片 Demo

- [ ] 完成一张探索地图、NPC、任务、宝箱、传送点、普通敌人和 Boss。
- [ ] 完成 3 名角色，系统支持第 4 名。
- [ ] 完成技能、击破、状态、装备、遗器、背包、奖励和 Save/Load。
- [ ] 完成从主菜单到任务结束的可重复演示流程。
- [ ] 完成授权清单、README、架构图、视频、截图、测试和个人贡献说明。
