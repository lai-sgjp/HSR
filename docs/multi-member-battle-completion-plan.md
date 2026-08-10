# 多人战斗与可扩展性收尾规划

> 计划基线：commit `60673a7`（PR #1 已并入 main）
> 目标范围：多人战斗、拉条、子系统可扩展性与前后端解耦
> 约束：本文件是规划，不授权任何后续 Source、Content、Config、资产删除或 Git 操作。

配套文档：作者向的资产配置手册见 [`authoring-guide.md`](authoring-guide.md)。

本文所有状态判断均以代码 grep 为依据并附行号。跨会话传递的待办清单已被证明会失准，
以本文的证据列为准，若与 `PROJECT_STATE.md` 或 `todo_plan.md` 冲突，请重新 grep 核实。

## 本规划与既有路线图的关系

`phase-roadmap-0-20.md` 末尾的「跨阶段改进清单」列了 7 条延期项。核实代码后，
**前 3 条已经完成，但路线图文本尚未更新**，容易让后续开发者重做已完成的工作：

| 路线图条目 | 实际状态 | 证据 |
|---|---|---|
| 1. ActiveStatus 通用化（`Status.Buff.AttackUp` 硬编码） | **已完成** | 生产代码零引用，仅测试 fixture 用作示例标签：`Source/HSR/Tests/HSRCombatPatchTests.cpp:1019` |
| 2. Break 可重复触发（移除终身只发布一次） | **已完成** | `bBreakResultPublished` 在整个 `Source/` 下已不存在 |
| 3. 速度变更与拉条重排 | **已完成** | TurnManager 对每个参与者的 Speed 绑变更委托并重算行动距离：`Source/HSR/Battle/HSRTurnManager.cpp:40`（绑定）、`:69`（重算） |
| 4. 探索敌人 AI 迁移 Behavior Tree | 未开始 | — |
| 5. 存档 Sync Domain 分类 | 未开始 | — |
| 6. 背包/装备独立 Revision 流 | 未开始 | — |
| 7. Cloud Slot 自动降级 | 未开始 | — |

第 3 条值得特别说明，因为它是本任务「拉条」需求的严格版本。路线图要求
「TurnManager 不能只在初始化时读取一次 Speed」，当前实现满足：速度变化触发
可控重建，且**当前正在行动者不被重排取消**（`HSRTurnManager.cpp:78`：当变更目标
就是 `CurrentTurnIndex` 时只更新数值、不参与重排）。这条不变量必须在后续任何
排序改动中保持，否则会出现「行动中被抢掉回合」的问题。

**行动项 P0-1：** 更新 `phase-roadmap-0-20.md`，把前 3 条标注为已完成并附证据行号，
避免后续开发者重做。

## 已完成的部分（供后续开发者确认，不需重做）

多人战斗的后端已闭合。根因是参与者 ID 曾硬编码为 `TEXT("Player")`，导致整队共用
一份状态键；现按槽位生成（`MakeParticipantId(Team, RosterIndex)`）。随之落地的：

- 队伍容量 2 → 4，存档 schema v9 + v8→v9 槽位加宽迁移（`HSRPartyTypes.h:11`、
  `HSRSaveVersion.h:39-41`）
- 整支队伍随遇敌进入战斗，Coordinator 每侧生成 N 个参与者
- 每个参与者独立维护速度、行动距离、状态与装备投射
- 败北语义由「队长阵亡」改为「全队阵亡」
- 技能改为按角色的 DA loadout，per-participant override 回退到 default
- 行动值预测 `FHSRTurnForecastEntry` 发布到 `FHSRBattleCommandViewState::TurnForecast`

前后端解耦方面已落地的关键接缝：ViewState 保持无指针（UI 不持有 Actor/ASC）、
命令提交走 `IHSRBattleCommandSink` 而非反向抓取 GameMode、前端模块路由改为
keyed map 而非平行 switch。

## 待完成项

按建议顺序排列。P0 是达成本任务目标的必要项，P1 是目标内但可增量交付，
P2 是目标外的既有延期项，列出仅为衔接。

### P0-1　更新路线图改进清单状态

如上表。纯文档改动，但优先级最高，因为过期的状态记录会直接导致返工。
这一轮审查中已发生过同类问题：跨会话压缩后的待办清单失去准确性，
多条记录为「未做」的条目实际都已完成。**结论是以代码为准，不以清单为准。**

### P0-2　拉条：ViewModel 投影 + 控件（含 C++）

这一项比预期大。后端与测试都完整，但**从 ViewModel 往上整条链路都不存在**：

| 层 | 状态 | 证据 |
|---|---|---|
| 预测结构 | 完整，5 字段全 `BlueprintReadOnly` | `Source/HSR/Data/HSRBreakTypes.h:101-122` |
| 构建函数 | 完整，纯读不扰动回合状态 | `HSRTurnManager.h:58`、`HSRTurnManager.cpp:228` |
| 发布进 ViewState | 完整，槽位数 8 | `HSRBattleCoordinator.cpp:1194`、槽数常量 `HSRBattleCoordinator.h:239` |
| 后端测试 | 完整 | `HSRCombatPatchTests.cpp:1175-1223` |
| **ViewModel 投影** | **缺失** | `HSRBattleCommandViewModel.h:26-52` 的 13 个展示属性里没有 forecast |
| **Widget 容器** | **缺失** | `HSRBattleCommandWidget.h:102-140` 只有 `TXT_TurnOrder` 一个 `UTextBlock`（`:131`） |
| **UMG 资产** | **缺失** | `Content/UI` 29 个 uasset 无一含 Turn/Forecast/ActionBar |

当前线上表现是 `TXT_TurnOrder` 里一行 `A -> B -> C` 文本（`HSRBattleCommandViewModel.cpp:67-71`
把旧的 `TurnOrderParticipantIds` join 起来，那是纯 ID 数组，**不是** forecast）。

因此工作分两段：

**a) C++：给 forecast 加展示层投影。** 现成可照抄的模式是技能列表那条数据驱动通路
（`HSRBattleCommandWidget.h:140-144` 的 `SkillListContainer` + `SkillEntryClass`）。
需要一个 entry 级展示 DTO 与稳定的列表重建。不要让 WBP 自己遍历
`GetStateCopy()` —— ViewModel 头部注释已明确禁止 WBP 自行格式化命令状态
（`HSRBattleCommandViewModel.h:25`），而且那样每次都整结构拷贝。

**b) 资产：建控件并接线。**

两点契约必须遵守：

- 该结构描述的是**预测**而非当前状态，任何速度变化或 advance/delay 都会使先前
  构建的预测失效（结构注释已写明）。控件必须在每次 ViewState 更新时整体重建，
  不能增量维护自己的副本。
- `bRepeatAction` 表示同一参与者在同一份预测里已占据更早的槽位，用于区分
  「同一人第二次行动」，UI 需据此避免重复渲染同一头像却不加区分。

建 Widget 时的两个已知坑：UMG 外模块父类会回退成 `UserWidget`，需用 reparent
修正且类名不带 `U` 前缀；改 Blueprint CDO 要用 `SetBlueprintCDOProperty`，
`CreateWidget` 不触发 `NativeConstruct`。

### P0-3　命令面板接上数据驱动技能列表

与 P0-2 同源且必须一起做。C++ 侧 `SkillListContainer` / `SkillEntryClass` 已就位，
但 `Content/` 里零引用 —— `WBP_BattleCommandPanel` 仍走
`BTN_Basic`/`BTN_Skill`/`BTN_Ultimate` 四按钮遗留路径。多人战斗下每个角色技能数量
和构成都不同，四个固定按钮直接撞墙。这是纯资产接线，不需要改 C++。

### P0-4　角色与技能资产补齐

这是「多人战斗每人不同」的真正阻塞项，而且**纯资产、零代码**——正好是本任务
「加实例只需加 DA」目标的验收方式。当前授权量不足，两重不足：

- **角色定义只有 2 个**（`Content/Data/Characters/DA_Character_P11_A`、`_B`），
  而 `HSRPartyCapacity = 4`（`Source/HSR/Party/HSRPartyTypes.h:11`）。满编还缺 2 个
  角色 DA 加 catalog 条目。
- **技能定义只有 4 个**，且**没有任何角色 DA 填了 `SkillDefinitions`**。这 4 个技能
  目前只作为 `BP_HSRBattleGameMode` 上的**全队共享 default loadout** 存在。

C++ 通路已经通了：`UHSRCharacterDefinition::SkillDefinitions`（`HSRCharacterDefinition.h:34`）
会被 `ResolveAuthoredLoadout` 解析并按参与者覆盖共享默认值
（`HSRBattleGameMode.cpp:1700-1756`，`:1750` 注释明确写着新技能只需改 DataAsset、
无需改 C++）。所以要做的是：为每个角色 DA 填自己的 `SkillDefinitions`，并补足
`DA_Skill_*` 数量。字段要求见 `authoring-guide.md`。

注意一个 id 层面的陷阱：`FindSkillDefinition` 先搜默认列表再搜所有参与者覆盖
（`HSRBattleCoordinator.cpp:2606-2632`），按 `SkillId` 而非归属查找。两个角色若各建
一份资产却用了**相同的 `SkillId`**，行动时其中一个会解析到另一个的定义。

### P0-5　PIE 验收多人战斗

自动化测试覆盖纯值逻辑与存档契约，但队伍规模、速度差手感、击倒表现只能在
编辑器里判断。逐项确认：

- 四人队伍进入战斗后是否四人都出现在战斗场景
- 回合顺序是否随速度差正确变化（可临时改 Speed 属性验证重排）
- 单人被击倒后是否只有他退出、战斗继续；仅全队阵亡才判负
- 拉条推进是否与实际行动顺序一致

### P1-1　测试补三处空白

现有多人战斗测试的**队伍规模上限只有 2 人**
（`HSRTeamWipeSemanticsTests.cpp:74-76`），`HSRPartyCapacity = 4` 这条路径从未在
战斗里跑过。建议补：

1. 3-4 人满队战斗集成测试。
2. forecast 与速度变更 / advance / delay 的交互。现有 forecast 测试只验证静态快照的
   单调性与槽位连续性，没有验证速度改变后的重建 —— 而重排正是本任务的核心诉求。
3. 「4 个成员 4 套不同技能同场」的端到端断言。`SetParticipantSkillLoadout` 有单元
   覆盖，但没有多人各自 loadout 的集成验证。

### P1-2　TurnManager 不监听 Health 的脆弱契约

`HSRTeamWipeSemanticsTests.cpp:90-99` 自陈：TurnManager 只在
`AdvanceToNextValidTurn` 里重选行动者、不监听 Health，所以击倒**当前行动者**会按
设计死锁，生产环境依赖 Coordinator 在别人行动时结算伤害。多人战斗放大了这个
风险面（同时存在的行动者更多、AOE 更容易打到当前行动者）。建议至少升级为显式
测试把契约钉死，而不是留在注释里。

### P1-3　装备槽位的末位枚举硬编码

比队伍宽度脆弱得多，值得点名。`EHSREquipmentSlot`（5 个）与 `EHSRRelicSlot`（6 个）
的边界检查全部写成「`<=` 末位枚举」的硬编码，散落 6 处：
`HSREquipmentSubsystem.cpp:813`、`HSRSaveVersion.cpp:101`（encode）、
`HSRSaveVersion.cpp:115-116`（decode 的 schema>=7 与 legacy 两条分支各一份）、
`HSRRelicEquipmentViewModel.cpp:326,337`。加一个槽位要同时改这 6 处，**且没有
`static_assert` 兜底** —— 与队伍宽度那条有 assert 强制的情况正好相反。

**队伍宽度那条不应改动**：它属于存档 canonical 格式，`HSRSaveVersion.h:48` 的
`static_assert` 是有意的防线，加宽必须走新 schema 加迁移（详见
`authoring-guide.md` 的存档章节）。

### P1-4　清两处已知红灯

`PROJECT_STATE.md:38-45` 自陈 `HSR.UI.ScreenLifecycle` 3 个失败与
`HSR.UI.CharacterDetail.ViewModel` 5 个 Save fixture 断言失败。这两处与本任务目标
无直接关系，但会干扰后续任何回归判断 —— 红灯基线会让新引入的失败难以识别。

### P2　路线图既有延期项

第 4-7 条（Behavior Tree AI、存档 Sync Domain、背包/装备独立 Revision 流、
Cloud Slot 降级）不属于本任务目标，保持延期。若要动，第 4 条最独立、
风险最低；第 6、7 条涉及存档原子性，需要完整的事务设计而非增量修补。

## 工作方式约定

以下几条来自本轮实践，写在这里是因为违反它们会直接产生假结论：

- **构建与测试必须指向当前工作树的 `.uproject`**。在 worktree 里误用主检出的
  `.uproject` 会静默验证错误的代码，破绽是改完多个文件后立刻出现
  `Target is up to date`。本轮曾因此使一整批测试结果失效。
- **测试进程即使失败也返回 0**，必须 grep 日志里的 `Result={Success|Fail}`，
  不能相信退出码。
- **单次 `-ExecCmds` 只接受第一个 `RunTests`**，多套件要分开跑。
- **审查类工作以代码为准**，用契约级 grep 核实，而不是依赖跨会话传递的待办清单。
- **不把多条语句压成一行**。既有密集测试文件是遗留，不是本项目约定。
