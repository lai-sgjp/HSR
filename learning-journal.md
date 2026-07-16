# HSR Learning Journal

## 使用方式

本文件沉淀可复习的 UE5.6 C++、GAS、回合制架构、数据驱动、UI 和存档知识。它不替代 `worklog.md`：

- `worklog.md` 记录协作过程、工程证据和项目决策。
- `learning-journal.md` 记录可以脱离当前任务复习和面试表达的知识。

每条学习记录建议包含：

1. 日期与主题。
2. 概念本质。
3. 在 HSR 中的职责和代码映射。
4. 端到端数据流。
5. 常见误区或踩坑。
6. 验证方法。
7. 一道练习题。
8. 一道面试题及回答要点。

## 2026-07-15｜GAS 与回合系统的职责边界

### 概念本质

GAS 适合表达角色拥有的能力、属性、效果和标签；它不应独自承担整个回合战斗状态机。

### HSR 中的映射

- GAS：基础攻击能否激活、对目标应用何种 Effect、属性如何变化。
- TurnSystem：当前行动者、行动顺序、输入等待、行动解析、胜负和回合边界。

### 关键误区

- GameplayEffect 的秒数持续时间不天然等于回合数。
- 动画或 GameplayCue 完成不能成为战斗规则唯一真源。
- 不应为了未来联机，第一天就引入 PlayerState ASC 和完整预测复制。

### 验证思路

基础攻击应能在不依赖正式动画和 VFX 的情况下改变目标 HP，并由 TurnSystem 明确推进到下一个行动者。

### 练习题

画出“玩家选择基础攻击 → Ability 激活 → Effect 应用 → HP Delegate → UI 更新 → TurnSystem 推进”的数据流。

### 面试题

**为什么不把回合状态机全部写进 GameplayAbility？**

回答要点：Ability 表达一次可激活行动；战斗阶段、行动者所有权、等待输入、结算和胜负属于更高层流程。分离可以避免单个 Ability 掌握全局状态，也便于测试和扩展不同技能。

## 2026-07-15｜Blank 模板与独立 Battle Map 的生命周期边界

### 概念本质

- Blank C++ 模板让 Phase 1 从零建立 Character、Controller、Enhanced Input 和 Camera，便于理解职责，而不是继承模板黑盒。
- `OpenLevel` 会销毁当前 World 中的探索 Actor、ASC、Widget 和 Runtime Effect；独立 Battle Map 不能沿用探索 Actor 指针。

### HSR 中的映射

- `UHSRBattleTransitionSubsystem` 只保存稳定 ID、探索地图、返回 Transform、Encounter 配置和 BattleResult。
- Battle Map 根据 DataAsset、Profile Runtime 和 SaveGame 重建 Battle Character 与 ASC。
- 返回探索地图后同样重建探索 Character，并消费一次 BattleResult。

### 常见误区

- GameInstanceSubsystem 生命周期跨地图，不代表可以安全保存旧 World 的 Actor 指针。
- SaveGame 不能保存 Active GameplayEffect Handle。
- 独立 Battle Map 的生命周期成本应在 Phase 4/5 就验证，而不是拖到后期。

### 练习题

画出 `EncounterRequest → OpenLevel → Battle Actor 重建 → BattleResult → OpenLevel → Return Transform` 数据流，并标出哪些对象在每次旅行时被销毁。

## 2026-07-15｜MVP、第一月成果和垂直切片的区别

### 概念本质

- **第一月成果**用于建立可靠技术基础，不要求可玩战斗。
- **MVP**是最小可玩产品，需要形成探索、战斗、奖励、返回和最小 Save 闭环。
- **垂直切片**用于展示接近最终质量的多系统组合，范围和表现要求高于 MVP。

### HSR 的范围选择

- 第一月只到 Phase 3 单对象交互。
- MVP 使用 1v1、静止 Encounter、基础攻击、固定奖励和单槽位最小 Save。
- Phase 20 才提供三角色、Boss、任务、成长、装备、状态和完整展示流程。

### 为什么主动缩小

- 1v1 能验证 TurnSystem，而不引入队伍切换复杂度。
- 静止 Encounter 能验证跨地图 Context，而不先解决 AI。
- 基础攻击能验证 GAS，而不先解决 Cost、Target 和复杂 Formula。
- 固定奖励能验证幂等事务，而不先建立完整 Inventory/Drop。
- 最小 Save 能验证数据边界，而不先保存所有系统。

### Scope Control 的学习价值

工程能力不仅是实现功能，还包括识别最小验证路径、控制依赖数量、定义完成标准，并拒绝不服务当前风险验证的功能。

### 练习题

需求新增“装备随机副词条”时，判断它属于第一月、MVP 还是垂直切片，并说明推迟它不会阻止验证哪些核心风险。

### 作品集面试题

**为什么你的 MVP 没有战技、击破和装备，却仍能证明回合制/GAS 架构成立？**

回答要点：MVP 已覆盖输入到 Command、跨地图 Context、ASC 重建、Ability/Effect、Attribute Delegate、Turn 状态机、幂等奖励和 Save 分层；复杂系统是在这些稳定边界上的扩展，而不是验证基础架构所必需。

## 2026-07-15｜按项目风险学习 GAS，而不是按概念表学习

### 核心认识

GAS 概念之间存在依赖。学习顺序应服务于项目当前风险：先验证 ASC 和 Attribute，再验证 GE，然后才是 Ability、Tags、复杂公式、击破、状态、UI、装备和网络。

HSR 的实际顺序为：

`边界 → ASC/Attribute → GE → Ability → Tags → Damage Execution → Break → Status → UI → Equipment → Networking`

### 关键原因

- 没有正确 Actor Info，后续 Ability 和 Effect 报错都难定位。
- 没有独立验证 GE，Ability 失败时无法判断是激活还是属性问题。
- 没有稳定简单伤害，不应引入 ExecutionCalculation。
- 没有 Break Debuff 真实用例，不应先设计通用状态框架。
- 单机规则未稳定，不应学习预测和复制实现。

### GAS 与 TurnSystem 的时间边界

- GAS Duration/Periodic 使用世界秒数。
- HSR 的持续回合、行动条、共享战技点和 Turn Delay 属于 TurnSystem/Runtime。
- Infinite GE 只承担回合状态的 Modifier/Tag 表现，由 Runtime Status 保存剩余回合和 Handle。

### 练习题

为“持续 2 回合的中毒”画出 StatusDefinition、Runtime Status、Infinite GE、TurnStarted、Instant Damage GE 和 UI Delegate 的数据流，并解释为什么不能使用 `Duration=2.0`。

### 面试题

**你为什么没有在项目早期使用 ExecutionCalculation、PlayerState ASC 和 Mixed Replication Mode？**

回答要点：它们分别服务于复杂多属性公式、跨 Pawn 的网络玩家状态和 Owner 客户端复制。早期单机最小闭环不具备这些真实需求，先使用简单 GE 和 Character ASC 可以降低变量数量，同时通过 Command、稳定 ID 和集中 Actor Info 初始化保留迁移点。

## 2026-07-15｜任务契约、范围控制与证据驱动 Debug

### 概念本质

低级模型执行模板不是一份“建议清单”，而是一份任务契约。它在执行前明确输入、允许写入范围、禁止事项、完成标准和验证责任，使模型不能依靠模糊的“相关文件”自行扩展任务。

### HSR 中的三类契约

- 纯文档任务：只改变项目记忆和规划，不把文档完成误标为 Gameplay 完成。
- 小型 C++ 功能：只实现一个可独立编译和验收的数据流，通常限制为 1–5 个明确文件。
- Debug/修复：先收集完整证据并找到第一处真实错误，再修复根因，不把后续连锁错误或症状当成根因。

### 为什么文件允许清单是硬边界

- 文件范围是任务范围最容易验证的代理指标。
- 明确路径可以阻止无关重构、批量 Config 修改和跨 Phase 实现。
- 如果真正修复必须修改清单外文件，正确行为是停止并请求授权，而不是悄悄扩权。
- “未列入即禁止”能避免遗漏的敏感文件被默认视为可写。

### 证据驱动 Debug

推荐顺序为：

`完整错误与复现 → 第一处真实错误 → 1–3 个根因假设 → 排除错误假设 → 最小修复 → 原路径复验`

第一处真实错误通常会产生大量后续 UHT、C++、Link 或 Blueprint 连锁错误。只处理最后一行、删除缓存或同时尝试多个修改，会破坏因果证据并增加新的变量。

### 与完成状态的关系

- “已修改”不等于“已编译”。
- “已编译”不等于“PIE 主路径通过”。
- “正常路径通过”不等于失败路径、重复调用和生命周期安全已经验证。
- 无法执行的验证必须明确列为未验证，不能使用“应该可以”代替证据。

### 常见误区

- 把模板中的占位符留空，导致执行模型自行假设范围。
- 在允许文件中写“相关文件”，使硬边界失效。
- 一个任务同时包含功能、重构、资源导入和 Debug。
- 因为修复了一个 Bug 就顺手更新 agents，造成长期规则被短期噪声污染。
- 把 worklog 当 todo，或把规划完成状态当成工程完成状态。

### 练习题

一个“新增基础攻击 Ability，并顺便重构 AttributeSet、制作技能 UI、导入动画、修复旧编译警告”的任务为什么不能直接套用模板 B？请将它拆成至少四个独立任务，并为每个任务给出明确的允许文件范围和验收证据。

### 面试题

**你如何约束 AI 编码代理在大型 UE 项目中避免范围漂移和无证据修复？**

回答要点：使用明确任务契约和文件允许清单；每轮单一目标；先读取长期规则和阶段文档；按反射、GC、GAS、Tick、UI、Save/Data 做专项门禁；Debug 从第一处真实错误建立证据链；编译、PIE 和未验证状态分开汇报；扩大范围必须重新授权。

## 2026-07-16｜阶段 Skill、Loop Engineering 与 Editor 协作

### 概念本质

阶段 Skill 的价值不是替代开发者做决定，而是把“下一步应该做什么”变成基于证据的可选建议。它必须知道当前 Phase、前置门禁、最后一次验证和未解决风险，然后只推荐一个相邻小任务。

### HSR 中的映射

- `phase-next-steps`：识别当前阶段、检查门禁、给出下一步和验证清单；
- `phase-execution-workflow.md`：维护 Phase 0–20 的通用循环和阶段矩阵；
- Loop Engineering：将规划、Prompt 审查、架构、实现、代码审查、学习复盘和安全审查分成可选角色；
- UE Editor 协作：Codex 修改可审查 C++/Markdown，用户完成需要视觉确认和资产绑定的 Editor 工作。

### 为什么 Skill 必须是可选的

- 用户当前回合的明确要求优先于默认工作流；
- 不同任务需要不同深度的审查，强制完整闭环会增加噪声；
- 阶段建议不能自动修改文件、启动工程或把计划标记为完成；
- 可选机制更适合学习项目：用户可以比较建议、理解取舍并主动授权。

### 练习题

如果 Phase 2 的 ASC 初始化尚未有编译或 PIE 证据，而用户询问“下一步做完整技能系统可以吗？”，阶段 Skill 应该如何回答？

参考要点：状态应为 Not verified 或 Blocked；先补齐 ASC/AttributeSet/Actor Info/Delegate 的最小验证，再推荐 Phase 3 或 Phase 6 的任务；不能因为路线图存在就跳过门禁。

### 面试题

**如何设计一个不会擅自推进项目的 AI 阶段助手？**

回答要点：使用只读证据确定阶段；将规划、实施和验证分开；每次只推荐一个相邻小任务；列出 Codex 与用户 Editor 职责；明确允许文件和非目标；将“Ready、Blocked、Not verified、Optional”分开；不自动写代码、改配置或提交 Git。

## 2026-07-16｜为什么模型切换需要文件化上下文

### 概念本质

聊天上下文是某个会话和模型实例的临时工作记忆。通过 CC-SWITCH 切换 API 源或模型后，新模型可能看不到之前的决策、授权边界、错误证据和未完成项。如果继续依赖“它应该知道我们刚才说过什么”，模型就会用猜测填补缺失信息，造成范围漂移和状态误报。

文件化上下文把关键记忆变成可审查、可版本化、与具体模型无关的项目状态。任何模型都从同一组文件恢复事实，而不是要求用户重复完整对话。

### HSR 中的分层

- `PROJECT_STATE.md` 是当前快照，回答“项目现在在哪里”。
- `tasks/active-task.md` 是执行契约，回答“这一轮只做什么、可以改什么、如何验收”。
- `tasks/archive/` 保存历史任务，回答“过去为什么这样做以及证据是什么”。
- `worklog.md` 保存过程与证据，`todo_plan.md` 保存进度，`learning-journal.md` 保存可复习知识。
- `.agents/agents.md` 保存长期不易变化的规则；专项设计文档保存各系统真源。

这些文件不能合并成一个巨型文档：快照需要短而新，任务卡需要窄而严格，日志需要保留历史，规则需要稳定。

### 为什么低级模型只读取活动任务卡

- 低级模型的优势是低成本执行边界清楚的小任务，不适合在大量全局文档中重新做架构判断。
- 如果让它同时读取 agents、todo、worklog 和所有设计文档，它可能把“未来计划”误当成本轮授权，或从过期记录中选择错误状态。
- 高级模型先把必要上下文压缩进自包含任务卡，低级模型只执行卡片，可以缩短输入、减少歧义并让文件越界易于审查。
- 实现时仍可接触任务卡明确列出的目标文件，但不能自行扩读全局上下文或扩大允许范围。

### 文件流的数据闭环

`全局证据 → 高级模型生成活动卡 → 低级模型最小执行 → 用户补充 Editor 证据 → 审查模型核验 → 高级模型更新快照与日志 → 任务归档`

每一步都有明确的输入、输出和责任人。即使中途更换模型，下一角色也可以从文件继续，不需要信任前一模型的隐藏思考或不完整摘要。

### 常见误区

- 把 `PROJECT_STATE.md` 写成历史流水账，导致新模型仍无法快速恢复。
- 让多张任务卡同时处于 active 状态，造成多个模型并行修改同一范围。
- 任务卡只写目标，不写允许文件、禁止事项、Editor 操作和验收证据。
- 把任务卡存在当作用户已经授权执行。
- 审查模型只阅读执行总结，不查看实际文件和证据。
- 低级模型发现信息不足时自行搜索全仓库，而不是停止并要求高级模型补卡。

### 验证思路

随机选择一个全新模型，只给它角色对应的启动 Prompt。若高级模型能准确复述 Phase、阻塞和下一任务，低级模型能在不扩读全局文档的情况下判断执行/停止，审查模型能独立发现越界和缺证据，则文件化上下文闭环有效。

### 练习题

设计一张“修复一个已知 UHT 错误”的活动任务卡，使低级模型无需读取 worklog 也能获得第一处错误、复现命令、允许文件、禁止清缓存规则和验收标准。

### 面试题

**多模型 AI 编码工作流为什么需要 repository-native context，而不是只依赖聊天摘要？**

回答要点：聊天记忆与模型供应方和会话绑定，容易丢失且不可审查；仓库文件可以形成稳定的状态快照、任务契约、证据链和历史归档；角色按最小必要上下文读取，既降低输入成本，也约束权限；通过独立审查和真实验证避免把模型自述当成完成证据。

## 2026-07-16｜UE5 Project Browser 非空目录阻断与手动骨架文件创建

### 概念本质

UE5 Project Browser 要求在空目录或新目录中创建项目。如果仓库根目录已经有文档、Agent 配置和 Git 数据，Project Browser 会拒绝创建，提示"Project cannot be saved in a folder with existing files"。这不是错误，而是 UE 防止意外的保护机制。

### HSR 中的映射

仓库根目录 `E:\work\unreal_projects\HSR\` 在工程创建前已存在 `.agents/`、`docs/`、`tasks/`、`.git/` 和多个 Markdown 文件。Project Browser 拒绝使用该目录。方案 A 绕过：手动创建 `HSR.uproject` + `Source/` 下的 Target/Build.cs/模块入口（共 6 个文件），然后右键 `.uproject` → Generate Visual Studio Project Files，结果与 Browser 创建完全等效。

### 关键要点

- `.uproject` 的 `EngineAssociation` 字段决定引擎版本，需与本地安装的 UE 版本精确匹配（如 "5.6"）。
- `BuildSettingsVersion.V5` 和 `EngineIncludeOrderVersion.Latest` 随 UE5.6 默认版本。
- Generate VS Project Files 会调用 UBT 生成 `.sln`、`Intermediate/` 和相关文件，等同于 Project Browser 的 Create 行为。
- UE 首次打开 `.uproject` 时，如果模块入口已存在且 `.Build.cs` 正确，不会自动触发 C++ 编译（仅加载反射数据）。
- `DefaultEditor.ini` 和 `DefaultGame.ini` 只有通过 Editor 修改了 Editor 或 Game 专属设置后才会被 UE 写入。

### 练习题

简述当 UE5 Project Browser 因非空目录拒绝创建时，有哪些可行的绕过方式？每种方式的优缺点和风险是什么？

参考要点：方案 A（手动骨架 + Generate VS Project Files）——零嵌套风险，结果等效，适合有文档/配置的仓库；方案 B（临时目录 + 搬运）——需手动搬运，存在遗漏和路径硬编码风险；方案 C（接受嵌套路径或改名）——产生 `Project/Project/` 嵌套或项目名不一致，需后续处理。

### 面试题

**你在一个已有大量文档和 Agent 配置的仓库中创建 UE 工程时遇到 Project Browser 拒绝，是如何处理的？**

回答要点：说明 Project Browser 的保护机制；解释为什么不能直接改路径或删除仓库内容；描述手动骨架文件方法的核心原理（`.uproject` + Target + Build.cs + 模块入口 = 合法 UE 工程）；强调 Generate VS Project Files 后的结果与 Browser 等效；说明这样避免了嵌套目录和文件搬运风险。

## 2026-07-16｜AI 协作中的角色锁定与教师角色

### 概念本质

角色锁定把“谁能决定什么、谁能修改什么”写成可审查的任务契约。低级模型的价值是稳定执行明确的小任务；它若擅自切换为规划、架构、审查或教学角色，就会把尚未授权的判断、文件和目标带入本轮，破坏范围与证据边界。

### 为什么权限边界重要

- 任务目标、文件允许清单、Phase 与验收标准都属于用户授权的一部分，不能由执行模型自行改写。
- “为了安全”“自我反思”或“更好完成任务”可以触发暂停和提问，不能成为自行扩权的理由。
- 正确的升级路径是：停止 → 说明超出权限的原因 → 指明所需角色与授权 → 等待用户决定；不是在同一轮偷偷兼任该角色。

### Teacher 如何帮助 HSR 学习

Teacher 是高级模型的教学角色。它基于 worklog、learning journal、真实 diff 与编译/PIE/Debug 证据，把 UE5.6 C++、GAS、Gameplay Tags、Enhanced Input、SaveGame、DataAsset、UI 与 TurnSystem 的工程实践解释为“直觉类比 → 真实机制 → 项目例子 → 常见错误 → Debug → 面试表达”。它不替用户完成练习，也不把未验证结论包装成事实。

### 学习沉淀与面试检验

- `learning-journal.md` 保存可复习的概念、项目映射、误区、验证和练习，证明学习过程而非只保留 AI 产物。
- `docs/interview-notes.md` 将真实取舍、失败路径和证据整理为可表达的工程经验。
- 面试题可反向检验学习质量：若不能解释角色边界、验证证据和 UE/GAS 原理，就说明仍需复习而非仅会调用 AI。

### 练习题

某低级模型发现任务卡未列出一个“看起来相关”的文件，并认为自己可以兼任 Architect 后修改它。请写出它应当输出的四项停止报告内容，并解释为什么不能直接修改。

参考要点：说明超出 `Implementation Agent` 权限的原因；指定需要 Architect 或高级模型；说明需要用户授权的文件范围或角色切换；承诺授权前不修改。文件是否相关是架构/范围判断，不能覆盖任务卡的允许清单。

### 面试题

**你如何避免 AI 辅助 UE 项目沦为“会调用 AI、但不能解释工程决定”？**

回答要点：以角色锁定保证规划、实现、审查和教学不混淆；用任务卡限制文件和验收；把真实 diff、编译/PIE/Debug 证据沉淀到工作日志；由 Teacher 将实践解释为 UE/GAS 机制并通过复述、练习和面试题检验理解；未验证内容明确标注。

## 2026-07-16｜UBT "Target is up to date" 与实际 C++ 编译的区别

### 概念本质

UBT 在构建时会比对源文件时间戳与中间产物。如果没有任何源文件发生变化，UBT 直接跳过 cl.exe 编译，输出 "Target is up to date"。这不代表编译失败或工具链不可用，而是表示增量构建系统正常工作。

### HSR 中的映射

TASK-P0-002 构建结果为 12 成功 0 失败，但 UBT 判定 "Target is up to date"——因为自 Generate VS Project Files 后 `Source/` 下的 `.cpp`/`.h` 未发生任何修改。要触发实际 C++ 编译，需要：
1. 修改一个 `.cpp` 或 `.h` 文件并保存
2. 或修改 `.Build.cs` 触发模块重建
3. 或清理 `Intermediate/` 强制全量编译

### 关键要点

- UBT 输出 "Target is up to date" 是正常行为，不是错误。
- 构建成功（退出码 0）证明 UBT 管道、.NET SDK、引擎路径、项目配置均正确。
- NuGet 还原失败/警告出现在 AutomationTool 引擎工具项目时，属于 UE 引擎自带的工具依赖问题，不阻塞 HSR 项目编译。
- "构建成功"和"实际 C++ 编译执行过"是两回事，报告时必须区分。

### 练习题

如何确认 UBT 确实能成功地编译 HSR 的 C++ 代码？至少列出三种方法。

参考要点：(1) 在 `HSR.cpp` 中添加一个无关紧要的注释或空行再构建；(2) 修改 `HSR.Build.cs` 添加一个空依赖再构建；(3) 删除 `Intermediate/Build/Win64` 目录再构建（全量）。

## 2026-07-16｜阶段门禁中的用户验收、独立审查与缺证据

### 概念本质

用户验收、执行报告和独立审查是三种不同证据。用户可以明确决定跳过某次独立审查，但协调者只能记录这个决定，不能补写一个不存在的审查者结论。阶段总门禁仍按每个 checkpoint 的真实证据分别判断。

### HSR 中的映射

P006 的构建入口、Editor 重开、默认地图、插件/Tags 和空白 PIE 均有执行报告与用户确认；因此运行门禁可以完成。该轮构建为 up-to-date，没有输出实际 C++ 标准，故工具链组合 checkpoint 仍未完成，Phase 0 保持 `Not verified`。

### 可复用判断

- 历史实际编译不能冒充本轮实际编译。
- 默认 BuildSettings 的推断不能冒充编译日志中的标准参数。
- 跳过审查不等于审查通过；归档必须注明证据类型和授权来源。
- 一个组合 checkpoint 只要仍有关键子证据缺失，就保持未完成。

### 面试题

**当用户明确跳过一次独立审查时，如何既尊重授权又保持工程证据可信？**

回答要点：记录用户决定和原始执行证据；不伪造 reviewer 结论；按细粒度 checkpoint 更新进度；未验证项保持开放；对剩余风险创建独立、最小且需重新授权的后续任务。

## 2026-07-16｜Phase 0 运行门禁：Build、Editor、PIE 与证据边界

### 概念本质

Phase 0 运行门禁是验证工程基础设施**可运行**而非**可编译**的一组真实操作。它的核心价值不是测试功能，而是确认 UBT 管道、Editor 生命周期、默认地图、插件与 Tags 加载，以及最基本的 PIE 启停没有阻塞性错误。

### HSR 中的映射

- P003 证明实际 C++ 编译和 DLL 链接通过。
- P005 证明地图存在于正确路径且 Config 配置指向该地图。
- P006 将三者串联：增量构建（up-to-date）、Editor 重开后自动加载 `Map_ProjectSetup`、打开空白 PIE 并正常停止。Output Log 无 Error。
- 实际 C++ 标准缺乏 cl.exe 日志证据，因此 Phase 0 不能宣布通过。从 `BuildSettingsVersion.V5` 推断为 C++20 不能替代真实编译命令输出。

### 关键要点

- **"已编译" 不等于 "Editor 可正常启动"**：P003 证明 DLL 链接成功，但未验证 Editor 在真实环境中加载该 DLL。
- **"Editor 可启动" 不等于 "PIE 可运行"**：P006 专门验证了 PIE 的启动/运行/停止，与 Editor 启动是分开的三个步骤。
- **"全部通过" 不等于 "阶段完成"**：Phase 0 总门禁还包括实际 C++ 标准证据，这是一个文档要求而非功能阻断。
- **实际 C++ 标准证据获取方式**：修改一个 `.cpp` 添加 `__cplusplus` 输出或 `static_assert`，触发一次实际 cl.exe 编译，从 UBT 日志中提取 `-std=c++20` 或等价标志。这是最小补证任务。

### 练习题

Phase 0 运行门禁中，哪些检查项必须有用户手动参与，哪些可以由 Codex 通过 Git 树和日志自动验证？

参考要点：Git 树可验证工程文件存在性、Config 内容和无 Gameplay 类；用户手动参与的需求包括 Editor 可视化验证（默认地图、插件启用、Tags 可查询）和 PIE 启停观察；编译日志可由 Codex 核对但需用户回传。

## 2026-07-16｜使用 `_MSVC_LANG` 确认实际 C++ 标准

- MSVC 下 `_MSVC_LANG` 用于报告当前编译采用的 C++ 语言标准值。
- 用户通过本地检查确认结果对应 C++20，补齐 Phase 0 的实际标准证据。
- `BuildSettingsVersion` 可以表达 UE 构建默认策略，但不应在缺少运行/编译证据时单独冒充实际标准；本次以用户实测结果完成门禁。

## 2026-07-16｜P1-001 角色与相机骨架

- `ACharacter` 已提供 Capsule、Mesh 和 CharacterMovement；探索派生类只需建立本阶段特有的 SpringArm/Camera 默认子对象与旋转策略，避免提前加入输入、Controller 或 GAS 职责。
- 默认组件应在构造函数中用 `CreateDefaultSubobject` 创建，并用 `UPROPERTY` + `TObjectPtr` 保存反射/GC 可见引用；附件关系为 Root → CameraBoom → FollowCamera。
- 第三人称旋转职责保持单一：Character 不直接消费 Controller pitch/yaw/roll，Movement 朝移动方向旋转，SpringArm 消费 Pawn Control Rotation，Camera 不重复消费。
- 关闭自定义 Actor Tick 不影响 CharacterMovement 组件自身工作；本阶段不存在每帧轮询需求。
- 同目录头文件使用局部 include 路径可避免模块 include 路径解析歧义。本轮首次构建暴露该问题，修正后 UHT、编译和链接通过。
- 编译成功只证明类和组件骨架可构建；Editor 类可见性、输入、Possession、移动和 PIE 仍需后续真实证据。

## 2026-07-16｜P1-002 Controller、ControlMode 与 Possession 边界

- Controller 与 Character 是控制者和被控制身体的关系；Controller 不应缓存缺乏生命周期保护的旧 Pawn 裸指针。
- `SetControlMode` 的幂等性意味着重复设置相同模式不重复产生输入模式、鼠标或未来 Mapping Context 副作用。
- `BeginPlay` 不应假设 Pawn 已存在；`OnPossess`/`OnUnPossess` 必须正确调用 `Super`，并对空 Pawn、错误 Pawn 类型与重复生命周期安全。
- 输入模式（GameOnly/UIOnly）与 Enhanced Input Mapping Context 是两层不同机制；P1-002 只建立前者和生命周期边界，后者必须在 P1-003 由 Controller 单一管理。
- UHT、编译与链接成功不等于真实 Possession 或重新 Possess 行为已通过；这些仍需后续 Editor/PIE 证据。

## 2026-07-16｜P1-003A Enhanced Input C++ 边界

- Pawn 行为绑定集中在 Character 的 `SetupPlayerInputComponent`；Mapping Context 生命周期集中在本地 PlayerController，避免双重绑定和多所有者管理。
- Move 的二维输入通过 Controller Yaw 转换为世界 Forward/Right；Look 传给 Controller Yaw/Pitch；Jump 用 Started/Completed 配对，Interact 仅是无副作用占位。
- Context 添加/移除必须对称、幂等，并覆盖无 LocalPlayer、无 Subsystem、无 IMC、错误 Pawn、UnPossess 与重新 Possess 路径。
- 独立的“模式是否已应用”状态解决了默认枚举值为 Exploration 时首次 `SetControlMode` 被错误幂等短路的问题。
- C++ 构建通过只证明接口与静态生命周期边界成立；五个输入资产、引用绑定、Editor 重开和 PIE 行为仍需 P1-003B/P1-004 的真实证据。

## 2026-07-16｜P1-003B Input Action 与 Mapping Context 资产边界

- Input Action 定义“输入语义和值类型”，Input Mapping Context 定义“设备按键如何产生该语义”；Move/Look 使用 Axis2D，Jump/Interact 使用 Digital。
- WASD 的四个一维按键通过 Negate/Swizzle Modifier 合成为二维 Move；Mouse XY 直接提供二维 Look；Space 与 E 分别映射 Jump、Interact。
- 用户确认 Editor 重开后资产配置保持，证明资产序列化持久性；Coordinator 未独立解析二进制，因此证据来源必须明确标注为用户回传。
- 资产存在与配置持久不等于 Gameplay 闭环已通过。Blueprint 引用、Possession、Mapping Context 去重以及 Move/Look/Jump/Interact 的完整 PIE 行为仍需后续真实验证。
- Git 提交可以由用户授权代办，但提交范围和产物作者必须分开记录：本次 commit 只有五个 Input 资产，两个 Blueprint 被明确排除。

## 2026-07-16｜Enhanced Input 静默失效：不要关闭 PlayerController Tick

- Enhanced Input 不只依赖原始按键事件。`InputKey` 负责接收设备事件，而 Input Action 的 Trigger/Modifier 求值与 Delegate 派发发生在 PlayerController 的每帧 Player Input 处理链中。
- `APlayerController` 上的 `PrimaryActorTick.bCanEverTick = false` 会造成典型假象：原始 W/Mouse/Space/F 日志存在，Pawn 已 Possess，IMC `HasContext=true`，Action 与 Binding 也正确，但所有 `Triggered/Started/Completed` 回调沉默。
- Character 的自定义 Actor Tick 可以关闭而不影响 CharacterMovement 组件；PlayerController Tick 则不能在使用标准输入处理链时随意关闭。优化 Tick 前必须先确认该 Actor 的引擎基类是否借 Tick 执行框架职责。
- 推荐排查顺序：Spawn/Possess → LocalPlayer/EnhancedPlayerInput → IMC 与按键 → Character Action 对象身份 → Pawn EnhancedInputComponent 是否在输入栈及绑定数量 → `ProcessPlayerInput/PostProcessInput` 是否每帧运行。
- `Transient` 适合标记 `CurrentControlMode`、`bControlModeApplied` 等运行时状态，可避免 Blueprint CDO 序列化污染；但必须区分“防御性生命周期修复”和“经证据确认的实际根因”。
- Skeletal Mesh 缺失只影响可见性和动画，不会阻止 Capsule、CharacterMovement、Camera 或 Enhanced Input；标准 GameMode 会在 PlayerStart 自动 Spawn DefaultPawn，不需要手动把玩家 Character 拖入关卡。

## 2026-07-16｜事后扩权与审查证据边界

- 紧急调试中发生的越出任务卡范围修改，必须记录真实指令、作者、diff 与验证；后续用户追认可以补齐授权链，但不能改写为“事前已授权”。
- Reviewer 的 `REVISE` 不会因一个授权问题得到追认而整体消失；输入栈、高频日志、构建和专项 PIE 等其余问题仍需逐项闭环。
- 二进制资产的未提交修改不能靠普通文本 diff 推断作者或用途。协调者应保护现场并向实际操作者确认，之后再决定独立提交或恢复，不能静默混入文档/源码提交。

## 2026-07-16｜P1-004 输入与生命周期闭环

- Enhanced Input 的 Context、Action 和 Binding 全部存在，仍不代表回调会触发；PlayerController 的标准每帧输入处理链必须运行。Character 可以无自定义 Tick，但不能为了“无 Tick”规则关闭承担引擎输入职责的 PlayerController Tick。
- Mapping Context 应沿 ControlMode 与 Possession 生命周期对称添加/移除，并用幂等状态防止同一生命周期重复添加；恢复 Exploration 或 Re-Possess 后的一次重新添加是正常行为，不等于叠加泄漏。
- 不应手工 Push Pawn InputComponent 来弥补生命周期问题；应依赖标准输入栈流程，并用同会话 UnPossess/Re-Possess 验证 Action 单次触发。
- HUD 创建入口要幂等，Widget 使用正确 Owning Player，并对空类与创建失败安全返回；UIOnly 往返必须同时验证 Gameplay 输入、鼠标/焦点和恢复能力。
- Reviewer 可以依据用户运行日志审查结论，但必须明确证据作者，不能写成 Reviewer 独立运行 Editor/PIE。

## 2026-07-17｜P1-005 动画接入与证据边界

- AnimBP 只消费 Velocity、Direction、IsFalling 等 Gameplay 状态用于表现；清空 Mesh/AnimClass 后角色仍可移动，是“动画不是规则真源”的关键失败路径证据。
- 二进制 UE 资产无法仅靠文本 diff 证明内部配置，必须明确区分用户 Editor/PIE 回传、Git 路径事实与 Reviewer 独立核对范围。
- 第三方资产要维护来源、用途、分发方式和 owner decision 台账。当前 Mixamo/Kachujin 风险为 `OWNER ACCEPTED`；未来正式发布、资产迁移或分发方式变化时复核官方条款，但不阻断 P1-005 归档。
- `Config/DefaultEditor.ini` 属于本地 AssetViewer/Editor 预览状态，已精确加入 `.gitignore`，不应作为项目 Gameplay 配置提交。

## 2026-07-17｜Phase 1 Teacher 复盘：真源、幂等与序列化边界

### Gameplay 真源

Gameplay 真源是某项游戏状态最终由谁决定。HSR Phase 1 中，位置和速度以 Character/CharacterMovement 为真源，控制模式以 PlayerController 为真源；AnimBP 和 HUD 只消费状态用于表现，不能各自保存副本并反向决定规则。GameMode 负责当前 World 的规则与默认 Pawn、Controller、HUD 类以及生成关系，不负责具体 UI 切换或 IMC 生命周期。

### ControlMode 幂等

`bControlModeApplied` 表示当前模式的输入、鼠标和 Context 等副作用是否已经真正执行，不能理解为“枚举有默认值所以程序正常”。它解决默认枚举为 Exploration 时首次调用被幂等短路的问题。`bExplorationContextAdded` 则记录 Context 的实际添加状态，使 Add/Remove 对称且不重复。

调用 `Exploration, Exploration, UIOnly, UIOnly, Exploration, Exploration` 时，Context 操作为 `无、无、Remove、无、Add、无`；模式依次为 `E,E,UI,UI,E,E`；Applied 始终为 true；ContextAdded 为 `T,T,F,F,T,T`。

### 资产/CDO、Transient 与 SaveGame

- 适合序列化进资产或 Blueprint CDO：设计期稳定默认配置、资源引用、可调参数，例如默认输入资产、WidgetClass、相机臂长度。
- 适合 `Transient`：只属于当前运行会话的状态，例如 CurrentControlMode、是否已经添加 Context、临时 Widget 实例、运行时 handle。
- 保存资产会把可序列化属性写入磁盘，并影响下次加载或由 CDO 创建的新实例；若运行态字段被误保存，可能污染默认值或把 PIE 临时状态带入后续实例。
- SaveGame 是另一条显式持久化通道，用于玩家进度等跨会话数据，不等同于保存 Blueprint 资产或 CDO。

### 用户掌握结果

用户已能用自己的话解释 Gameplay 真源、GameMode 边界、Context 幂等序列、UPROPERTY/GC 与 Widget 强引用。需要继续深化：`bControlModeApplied` 的精确定义、AnimBP 内部节点，以及重复输入时按 Context、Binding、InputComponent 栈和 Pawn 生命周期收集证据的完整顺序。

## 2026-07-17｜Phase 1 学习 Gate 结论

- Teacher commit：`70efd6f24f5d8532f74d0994c8c551d9353d6204`。
- 已掌握：Gameplay 真源、GameMode/Controller/Character 职责边界、ControlMode 与 Mapping Context 幂等、UPROPERTY/GC 和 Widget 强引用生命周期。
- 接近独立 Debug：从原始输入、Possession、LocalPlayer、IMC/IA/Binding 到 PlayerController 每帧输入处理链定位 Enhanced Input 静默问题。
- 非阻断深化项：AnimBP 内部节点、资产/CDO 序列化细节，以及重复输入时 Context、Binding、InputComponent 栈、Pawn/HUD 生命周期的完整证据排查顺序。
- 学习 Gate 已满足 Phase 1 收尾要求；这些深化项不代表 Phase 1 工程功能未完成。
