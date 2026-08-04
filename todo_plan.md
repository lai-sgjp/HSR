# HSR Todo Plan

> 当前状态：P17-005 已暂停并 checkpoint 于 `4ef49f7`；最终 PASS 不可用。P17-PATCH-03 Gate 0 已 PASS，03A～03E1 已完成；03E2 仅可准备 Task Gate，实施仍需用户单独确认。

## 已完成的规划工作

- [x] TASK-P17-PATCH-01A：ActiveStatus 通用化（Reviewer `PASS`，已归档）。
- [x] TASK-P17-PATCH-01B：可重复 Break（Reviewer `PASS`，已归档）。
- [x] TASK-P17-PATCH-01C：完整行动距离模型（Reviewer `PASS`=`28d3213`；Build、9 项 Battle Automation、diff-check 通过，已归档）。
- [x] TASK-P17-PATCH-01D：Patch 01 最终回归、证据汇总与归档（Reviewer `PASS WITH FOLLOW-UP`=`56380dc`，已归档）。
- [x] TASK-P17-PATCH-02：Behavior Tree/Blackboard 探索敌人迁移已完成最终复核；其用户资产改动仍按真实 provenance 隔离，不等同于 P17-005。
- [x] P17-005 当前进度 checkpoint：commit `4ef49f7`（仅表示暂停点，不表示任务完成或最终 PASS）。
- [x] P17-PATCH-03 Gate 0：四角色最终 PASS；已冻结八子系统所有权、稳定 ID、事务、生命周期、失败矩阵、Editor exercise 与串行包。
- [x] TASK-P17-PATCH-03A：Frontend Boundary Contract Reconciliation；Build、11/11 Automation 与用户 X 返回探索 PIE 通过，双分辨率为 `USER ACCEPTED / NOT VERIFIED`。
- [x] TASK-P17-PATCH-03B：Production Bootstrap and Character Identity（Code Gate、Automation、happy/unavailable PIE 与最终 Blueprint Save/reopen 均 PASS，已归档）。
- [x] TASK-P17-PATCH-03C：Interaction -> Encounter -> Battle Admission（Build、Automation、胜利/失败 PIE、resolved replay 与 defeat retry 均通过；最终 Reviewer `PASS`，已归档）。
- [x] TASK-P17-PATCH-03D1：Atomic Settlement Foundation（Build、13/13 Automation、Independent Reviewer `PASS`、Reward/Catalog Save/reopen 与胜利/战败 PIE 均通过，已归档）。
- [x] TASK-P17-PATCH-03D2：Battle Result Settlement Integration（RED/GREEN、29/29 回归、胜利/战败/重复确认/返回 PIE 与最终复核均 `PASS`）。
- [x] P17-PATCH-03E1：Equipment Instance Ownership Foundation；Build、Automation、Independent Review 与用户探索 PIE 兼容观察均 `PASS`，UI 驱动的装备检查和 Save/Load 因 UI 未完成而保留为 `NOT APPLICABLE / NOT VERIFIED`，已归档。
- [x] TASK-P17-PATCH-03R：恢复既有 Automation development seams；最终 `HSREditor` Build、受影响 Automation 与 Independent Reviewer `PASS`，已归档，不改变业务行为。
- [ ] P17-PATCH-03E2：Inventory/Equipment 原子移动、OperationId/双 revision、ASC 与 UI 投影；mapping contract RED checkpoint 已提交，03R 阻断已关闭，恢复已授权 TDD 实现。
- [ ] P17-PATCH-03F～03G：按计划串行整合 Map travel、Save restore。
- [ ] P17-PATCH-03H：clean-save 端到端 closeout；不得以其结果倒推 P17-005 已完成。

- [x] P7-000：只读校准 Phase 6→7 门禁，冻结 CoreAttributeSet 原位扩展、原创公式、CritDamage/Capture/RNG/单一伤害入口和结构化失败协议。
- [x] P7-001：战斗属性、原创规则与初始化基线（Reviewer `PASS WITH FOLLOW-UP`，已归档，follow-up 保留）。
- [x] P7-002：Attribute Capture、统一 Execution 与确定性暴击（Reviewer `PASS WITH FOLLOW-UP`，已归档）。
- [x] P7-003：普攻、战技、终结技迁移到统一伤害管线（Reviewer `PASS WITH FOLLOW-UP`，已归档）。
- [x] P7-004：P7-003 follow-up 闭环（Reviewer `PASS WITH FOLLOW-UP`，已归档，底层故障/teardown follow-up 保留）。
- [x] P7-005：阶段收尾、教学、独立审查与归档（Gate 0 PASSED，归档完成）。
- [ ] P8-001：Element / Weakness / Toughness 数据契约闭环（已创建活动卡，等待执行确认）。
- [x] P8-001：Element / Weakness / Toughness 数据契约闭环（Reviewer `PASS WITH FOLLOW-UP`，已归档）。
- [ ] P8-002：弱点匹配与削韧入口（已创建活动卡，等待执行者复述与用户确认）。
- [x] P8-002：弱点匹配与削韧入口（Reviewer `PASS WITH FOLLOW-UP`，已归档）。
- [ ] P8-003：一次性 BreakResult 结算（已创建活动卡，等待执行者复述与用户确认）。
- [x] P8-003：一次性 BreakResult 结算（Reviewer `PASS WITH FOLLOW-UP`，已归档）。
- [x] P8-004：Break Turn Delay（Reviewer `PASS WITH FOLLOW-UP`，已归档）。
- [x] P8-005：只读 Weakness/Toughness/Break/Delay 表现与收尾（`USER ACCEPTED`，已归档）。
- [x] P8-006：Independent Reviewer 最终 `PASS WITH FOLLOW-UP`；Build、Teacher、64/64 provenance、分角色提交、归档、Phase closeout 与远端交付均已完成；`HEAD == origin/main == 2c2abc2`。
- [x] P9-000：TurnStarted/TurnEnded 纯值事件、顺序、exactly-once、Finished/Reset/死亡/Delay 语义与 Development 矩阵（Reviewer `PASS WITH FOLLOW-UP`；Build 成功；USER PROVIDED `24 PASS / 2 COMPLETE / 0 FAIL`；三项 follow-up 保留）。
- [x] P9-001：两回合 Attack Buff 生命周期纵切（Reviewer `PASS WITH FOLLOW-UP`；两轮 USER PROVIDED `28 PASS / 2 COMPLETE / 0 FAIL`；Config/资产归属与 follow-up 保留）。
- [x] P9-002：叠层/刷新/替换纵切（Reviewer `PASS WITH FOLLOW-UP`；P9-002 `16 PASS/2 COMPLETE` + P9-001 回归 `28 PASS/2 COMPLETE`；零失败；三个 follow-up 保留）。
- [x] P9-003：DoT 与 Break Debuff 接入（Reviewer `PASS WITH FOLLOW-UP`；最终 P9-003/P9-002/P9-001=`36/16/28 PASS`，各 2 COMPLETE，零失败）。
- [x] P9-004：免疫/驱散/来源/清理（Reviewer `PASS WITH FOLLOW-UP`；最终 `38 PASS/2 COMPLETE`，P9-001～003 全回归零失败）。
- [x] P9-005：只读状态 UI/Debug（Reviewer `PASS WITH FOLLOW-UP`；最终五套矩阵 `28/38/36/16/28 PASS`，各2 COMPLETE，零失败）。
- [x] P9-006：独立验收、教学与阶段归档（Reviewer `PASS WITH FOLLOW-UP`；Teacher 六题掌握；provenance、Config EOF/global diff-check、User `2a2eb3d`、Implementation `a996475`、Teacher `39e0449`、Reviewer `db383b3` 和三件套归档均闭合）。
- [ ] 独立资源后续：Wait/Pass、Basic/Skill/受击回能，以及按 Team 拆分共享 SP 池（敌方独立或无池，禁止误做每角色 SP）；不得塞入 P7-003。

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

- [x] P3-001：交互协议、弱候选组件与单灰盒对象闭环（Reviewer commit `e99078d` 为 `PASS WITH FOLLOW-UP`；最终修订 commit `64ac977`，follow-up 与历史流程偏差保留）。
- [x] P3-002：事件驱动 Prompt 观察层与生命周期专项（Reviewer `d93dbe8` 为 `PASS WITH FOLLOW-UP`；最终 Implementation/A4 `20ab555`，已归档；Build/UHT、用户 PIE、归属笔误及历史流程边界保留）。
- [x] P3-003：完成最终回归、Teacher、用户原始作答、Reviewer 与阶段归档（最终 Reviewer `3b3fbeb` 为 `PASS WITH FOLLOW-UP`；Phase 3 为 `Ready`；三个工程 `USER ACCEPTED` 缺口、`OutOfRange`、学习与历史流程 follow-up 全部保留，不自动进入 Phase 4）。

> Phase 3 当前执行边界为一个灰盒交互对象。早期路线图中的 NPC、宝箱和传送点三类占位对象仅作为共享协议的历史扩展依据，不是当前可勾选任务；真实对话、奖励、持久化和传送继续推迟。

## Phase 4：敌人探索表现与遇敌触发

- [x] P4-001：稳定 Encounter 合同、Transition Subsystem、灰盒触发与空 Battle Map 单次消费（Reviewer `PASS WITH FOLLOW-UP`；A2 后 Editor/PIE 缺失由用户 `USER ACCEPTED`，历史 Git/同身份偏差保留）。
- [x] P4-002：数据驱动探索敌人、AI Perception/NavMesh 事件驱动巡逻/发现/追击并只复用 `RequestEncounter`（Reviewer `PASS WITH FOLLOW-UP`；A1 Build 为报告级、02:07 PIE 为用户证据，范围/Git/生命周期 follow-up 保留）。
- [x] P4-003：Player/Enemy/Neutral 三种 initiative、重复触发/旅行失败恢复、纯值 Return Context、空 Battle Map 测试返回及 P4-002 组合回归（Reviewer `PASS WITH FOLLOW-UP`；A4c 一次可定位 Build，1/4 组合动态，其余用户接受/延期）。
- [x] P4-004：Phase 4 最终回归、Teacher、Independent Reviewer 与阶段文档归档（最终 Reviewer `PASS WITH FOLLOW-UP`；已归档；等待阶段收尾 commit/push 记录）。
- [ ] 创建 EnemyCharacter、AIController、Enemy/Encounter DataAsset。
- [ ] 实现巡逻占位、AI Perception、发现和追击。
- [ ] 创建 EncounterRequest 和先手类型。
- [ ] 创建 BattleTransitionSubsystem，Context 只保存稳定 ID、Map 和 Transform。
- [ ] 验证进入空 Battle Map 和返回流程。

## Phase 5：回合制战斗最小闭环（Ready with inherited follow-ups）

- [x] 创建 BattleCoordinator、BattleParticipant、BattleGameMode 和 TurnManager（P5-001/P5-002；Reviewer PASS/PASS WITH FOLLOW-UP）。
- [x] 在 Battle Map 根据 Context 重建玩家/敌人 Actor 与 ASC（P5-001；用户 PIE 证据）。
- [x] 实现 Speed 排序和稳定同速裁决（P5-002；两轮用户 PIE）。
- [x] 实现最小普攻 GameplayAbility 和固定伤害 GE（P5-003；两轮用户 PIE）。
- [x] 实现 ActionResolved、死亡、胜负和单次 BattleResult（P5-004；两轮 Victory/Defeat 用户 PIE）。
- [x] 返回探索地图和原 Transform（P5-004；exactly-once 与防重入证据）。

## Phase 6：GAS 技能系统基础（P6-005 收尾审计中）

- [x] 建立 GameplayAbilityBase、普攻、战技、终结技占位和治疗（P6-001～004；各包 Reviewer `PASS WITH FOLLOW-UP`）。
- [x] 建立 SkillDefinition DataAsset 和 SingleEnemy/SingleAlly/Self 目标类型（SingleAlly 仅静态验证）。
- [x] 使用 GAS GE 处理 Energy Cost、伤害和治疗（Heal GE 失败与异步路径未动态验证）。
- [x] 使用 BattleCoordinator battle-local 事务管理共享战技点（真实 Rollback 分支未动态覆盖）。
- [x] 区分 Ability 成功、取消、拒绝和失败 Resolution（动态证据主要为 `USER PROVIDED`）。
- [x] P6-004A：创建并验证真实 `WBP_BattleCommandPanel`、GameMode 绑定、stable-ID Command、NativeDestruct/重建与无重复 Delegate（Reviewer `PASS WITH FOLLOW-UP`；动态证据 `USER PROVIDED`）。
- [x] P6-005：完成 Coordinator/Teacher/Independent Reviewer、canonical 文档和阶段证据收尾；最终 Reviewer `PASS WITH FOLLOW-UP`，Phase 6 为 `Ready with inherited follow-ups`。

## Phase 7：属性、伤害公式与暴击

- [ ] 扩展 Attack、Defense、CritRate、CritDamage。
- [ ] 建立原创 DamageRule 和 ExecutionCalculation。
- [ ] 使用可复现随机并输出 Damage Breakdown。

## Phase 8：弱点、韧性与击破

- [ ] 建立 Element Tags、Weakness、Toughness/MaxToughness。
- [ ] 实现削韧、击破伤害、Turn Delay 和击破 Debuff。
- [ ] 创建弱点图标和韧性条。

## Phase 9：Buff、Debuff 与状态效果（Ready with inherited follow-ups）

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

- [x] 完成 Coordinator、Independent Reviewer、Teacher、Implementation 四角色 Gate 0 规划并创建 `docs/phase-12-execution-plan.md`。
- [x] Gate 0：Phase 11 provenance/allowlist 审计、误跟踪缓存处置与远端同步已闭合；`origin/main=42c32e0`，Gate 改判 `PASS`。
- [x] P12-001：Definition、Instance 与纯值 Equip/Replace/Unequip/强化事务；最终 Reviewer `PASS`，Build/Automation 通过。
- [ ] P12-002：属性来源分解与可逆 Equipment Infinite GE；四角色 Gate 0 与 User Editor Asset Gate 已通过，Runtime implementation 进行中。
- [x] P12-002：属性来源分解与可逆 Equipment Infinite GE；最终 Reviewer `PASS WITH FOLLOW-UP`，Build/Automation 通过。
- [ ] P12-003：遗器套装阈值与固定强化；先完成四角色 Gate 0 和 User Editor Asset Gate。
- [x] P12-003A：Relic Set schema 与纯值阈值 Resolver；Reviewer `PASS WITH FOLLOW-UP`，Build/Automation 通过。
- [x] P12-003A 归档边界：该项是 P12-003B 的内部前置段，没有独立三件套；证据随 P12-003B 与阶段收尾保留。
- [x] P12-003B：套装来源重建与固定强化；Reviewer `PASS WITH FOLLOW-UP`，Build/Automation 通过。
- [ ] P12-004：Save v2、装备来源快照与只读详情 UI；先完成四角色 Gate 0 和 User Editor Asset Gate。
- [x] P12-004：Save v2、原子装备来源恢复与只读 Equipment Detail UI；Reviewer `PASS WITH FOLLOW-UP`，纠偏 Harness P12-004C 最终 `PASS`。
- [x] P12-004C：Development PIE Harness 覆盖固定装备、2->1->2、Save/Clear/重复 Load、Detail 生命周期与 cleanup；Build/Automation/用户 PIE 通过。
- [x] P12-005：教学 Gate、阶段文档、最终 Independent Review `PASS WITH FOLLOW-UP` 与归档完成；等待 commit/push。
- [x] 建立 Equipment/Relic Definition、实例、槽位、词条、套装和强化。
- [x] 建立 StatAggregator 和属性来源分解。
- [x] 使用 InstanceId/SetSourceId 跟踪的可移除 Infinite GE 应用装备效果。

## Phase 13：背包、奖励与掉落

- [x] 完成 Coordinator、Independent Reviewer、Teacher、Implementation 四角色 Gate 0 规划并创建 `docs/phase-13-execution-plan.md`。
- [x] P13-001：ItemDefinition、ItemInstance 与 candidate-first InventorySubsystem 纯值事务；Build/Automation/Independent Review 和 User Editor 保存重开 Gate 通过。
- [x] P13-002：幂等 RewardTransaction、确定性 DropTable、资源上限和 User Editor 保存重开 Gate；最终 Build/7 项回归通过，保留修订后独立复审未重签 follow-up。
- [x] P13-003：Encounter/Battle victory 与 Reward Chest 统一接入 RewardSubsystem，producer exactly-once 与失败重试通过。
- [x] P13-004：Save v3、只读 Inventory/Reward UI、Editor 重开 Load；修订后 Build、17/17 Automation 和 Independent Review `PASS WITH FOLLOW-UP`。
- [x] P13-005：Phase 13 教学、provenance、最终 Independent Review `PASS WITH FOLLOW-UP` 与归档；Git delivery 随阶段收尾执行。

## Phase 14：任务与对话

- [x] P14-001～003：Quest/Dialogue/Save v4 核心链路与定向 Automation 已通过。
- [x] P14-004：HSREditor Build、Quest/Dialogue Automation、Save 8/8 Automation 和 NPC PIE 交互入口已通过。
- [x] P14-005：Teacher 复盘 `PASS WITH GUIDED CORRECTION`、Independent Review `PASS WITH FOLLOW-UP`、任务归档完成；分支选择/奖励 PIE 保留为 inherited follow-up。
- [ ] 建立 Quest/Dialogue DataAsset 和 Runtime State。
- [ ] 使用类型化领域事件推进任务。
- [ ] 完成一个 NPC、一个分支对话和一个任务奖励闭环。

## Phase 15：地图、传送与关卡流程

- [x] P15-000：四角色 Gate 0、所有权、旅行事务、失败矩阵、Save v5 与 Editor/PIE 门禁已冻结到详细执行计划。
- [x] P15-001：建立 Map/Teleport Definition、稳定 DTO 和纯值 MapSubsystem；Build、Automation、用户 Editor Gate 与 Independent Review `PASS`。
- [x] P15-002：完成 A↔B 普通地图旅行、authority World/Arrival 校验、失败取消/重试；Build、Automation、两轮双向 PIE、失败 PIE 与 Independent Review `PASS`。
- [x] P15-003：Battle Return 已接入稳定 MapId/Return Context；胜负、重试、resolved、失败回滚与 B→Battle→B exactly-once 均通过最终复审。
- [x] P15-004：Save v5 地图状态投影、v1-v4 保守迁移与 Editor 冷恢复已通过。
- [x] 保存区域解锁、当前地图、玩家位置和探索状态。
- [x] 验证两个探索地图与独立 Battle Map 稳定往返。
- [x] P15-005：fresh Rebuild、Automation、Teacher、Independent Review、归档与阶段 Git 收尾。

## Phase 16：存档系统总整合

- [x] P16-000：四角色 Gate 0；冻结所有权、磁盘合同、显式迁移、备份恢复、失败矩阵与证据边界。
- [x] P16-001～005：Envelope/schema v6、迁移、完整性、Primary/Staging/Backup、全 authority 冷恢复与独立复审完成。
- [x] 整合现有角色、队伍、背包、装备/遗器、奖励账本、任务与地图 authority；无独立 authority 的宝箱/敌人不虚构持久化。
- [x] 建立 SaveVersion、验证、备份、迁移和受控失败恢复。
- [x] 确保不保存 Actor、Widget、ASC 或 GE Handle。
- [x] Phase 16 closeout commit `2c11415`；保留 graphical Editor、Packaged/Shipping、真实断电/磁盘故障等证据边界。

## Phase 17：UI 总整合

- [x] P17-000：四角色 Gate 0 规划与 Independent Reviewer 最终复核 `PASS`。
- [x] P17-001：纯值 Screen Stack、唯一 HUD root、Input/Focus Policy 与失败零污染合同；Build 11/11、Automation 3/3、Independent Review `PASS WITH FOLLOW-UP`。
- [x] P17-002～004：探索 Pause/Input/Focus、Character/Inventory 接入、Travel teardown/rebuild 与用户 PIE Gate 已完成。
- [x] PATCH-01A：ActiveStatus 通用化；Build、真实 runtime Automation、P9-001/002/003 用户 PIE 与 Independent Reviewer `PASS`。
- [x] PATCH-01B：移除终身 Break 闩锁；恢复后再次 Break、Replay/Reset/death/Finished、同帧致死、Automation 与用户 P9-003 PIE 全部通过。
- [x] PATCH-01C～01D：统一行动距离模型与回归收尾完成；Speed/Slow/Advance/Delay/Break Delay 已通过 Build、Automation 与独立复核。
- [x] PATCH-02：Behavior Tree/Blackboard 探索敌人迁移完成；巡逻、追击、丢失返回、Encounter admission、teardown、半径数据驱动及用户 PIE 返回 Gate 已通过最终复核。
- [ ] 完成主菜单、暂停、角色、队伍、背包、任务、地图和设置界面。
- [ ] 建立 Screen Stack、统一 InputMode 和导航。
- [x] 验证地图旅行后的 UI 重建（P17-PATCH-03F：A↔B UI Freeze/Restore、地图面板与 Pause 重开；HUD EndPlay 重复 teardown 日志保留 follow-up）。
- [x] TASK-P17-006 Quest Frontend：只读 DTO/ViewModel、统一 route、Empty/Unavailable、Back 与 A↔B 后重开通过 Build、Automation 和用户 PIE；Ready/Objective/Reward 生产任务展示保留 follow-up。
- [x] TASK-P17-009A Party 只读投影：固定两槽 DTO/ViewModel、Ready/Empty/Unavailable、Back/Close 与 A↔B 后重开通过 Build、Automation 和用户 PIE。
- [ ] P17-009 Party 与战前编队/Buff：009A 仅完成显示基础；永久队伍候选编辑/确认、独立战前候选队伍、Buff、Encounter Request 与取消零污染仍待完成。
- [x] P17-009B Party 永久候选编辑与确认：候选 Set/Clear/Swap、Confirm/Cancel、revision/duplicate 失败保护、用户 PIE 与回归 Automation 已通过；P17-009C 战前编队/Buff 仍待完成。

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

## Phase 20 完成后的可选存档演进

- [ ] 设计并迁移 `EHSRSyncDomain`：Global、LocalOnly、PlayerBound；冻结旧档默认、玩家身份、校验和云冲突策略。
- [ ] 评估 Inventory/Equipment 独立 Revision、写入队列和 immutable generation；以 manifest/commit marker 保证主档引用与背包数据一致。
- [ ] 为未来 Cloud Save 设计四级 Slot 自动降级：Slot 1 最新、Slot 2/3 历史、Slot 4 手动恢复；失败上传不得覆盖最后已知良好版本。
- [ ] 在真实云提供方确定后补齐离线/双设备冲突、ETag、配额、取消、部分失败、隐私/加密和删除 tombstone 的设计与自动化证据。
- [ ] 上述项目不属于 Phase 17-20 Gate；实施前必须建立独立任务卡、迁移方案、失败矩阵和平台证据。

# Phase 11 closeout（2026-07-25）

- [x] P11-001～004：Character Definition/EXP、Profile authority、唯一 Progression GE 与双角色 Party 主链完成。
- [x] P11-005：Save v1、原子 Load/Restore、ASC 成长层刷新与 Character Detail UI，归档为 `READY WITH FOLLOW-UP`。
- [x] P11-006：fresh Build、8 项 HSR Automation、失败注入、Editor 重开 Load、完整战斗返回、C/蓝图按钮 UI 切换与归档完成。
- [ ] Follow-up：Standalone `Esc` 返回；详情页打开时终局自动显示 ResultView；完整 Phase 2/5～10 专项回归。
- [ ] Phase 11 提交前 provenance/allowlist 审计及阶段收尾 commit；不得提交 `.claude/settings.local.json`、插件 `__pycache__`、Binaries、Intermediate 或 Saved。
