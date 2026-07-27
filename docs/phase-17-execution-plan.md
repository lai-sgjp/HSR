# Phase 17 详细执行计划：统一 UI 导航、前台菜单与战斗界面

> 计划基线：2026-07-27
> 前置阶段：Phase 16 版本化存档、备份与恢复已经通过工程门禁。
> 当前进度：P17-001～P17-004 已完成并归档；P17-004 的工程、自动化与用户 PIE 门禁为 `PASS`。
> 本次修订：根据用户提供的探索、暂停、角色养成、遗器、背包、队伍、挑战、对话、战前准备、战斗、地图、奖励、抽卡与存档参考，重新冻结 Phase 17 后续产品目标和工作包。
> 约束：本文件是规划，不授权任何后续 Source、Content、Config、资产删除或 Git 操作。

## 1. 当前事实与证据边界

### 1.1 已完成基础

- P17-001：纯值 Screen Stack、受保护 Exploration Root、单调 Request Token、Input/Focus Policy 与 Automation 已通过。
- P17-002：探索 Pause Modal、UIOnly/GameOnly、鼠标、焦点、Exploration IMC 与 Pause 所有权闭环已通过。
- P17-003A：Character Detail 接入统一 UIManager；Party/Save 数据深度仍保留后续边界。
- P17-003B：Inventory Widget + InventoryReward ViewModel 按需创建、严格配对、Back、Reward Summary 隔离与支持地图战斗返回已通过。
- P17-004：LocalPlayer UIManager 持久化、旧 World UI 原子 teardown、新 Host + ArrivalCommitted 双门槛恢复已通过。
- P17-004 生产证据：9 组 Freeze/Arrival/Consume 精确配对；普通旅行 7 次、战斗返回 2 次，均 `Result=0, Stack=1`；用户确认每次返回后的 HUD、W/A/S/D、鼠标正常，两轮 Reward Summary 各出现一次。

### 1.2 真实 UE 生命周期校准

项目中的 `UGameplayStatics::OpenLevel` 会令旧 HUD 收到 `EndPlay(Destroyed)`，而不是稳定收到 `LevelTransition`。生产捕获规则已经校准为：

- `EndPlay(LevelTransition)`；或
- `EndPlay(Destroyed)` 且 MapSubsystem/BattleTransition 存在已授权 pending travel。

PIE Stop、普通 HUD 销毁、手动 HUD rebuild 和无授权 pending 的 `Destroyed` 不创建旅行恢复描述符。

### 1.3 尚未完成

- 当前 Pause 仍是功能验证型 Modal，不是完整的前台功能中心。
- Character Detail 和 Inventory 只有基础纵向切片，没有完整角色养成 Shell、分类背包和多级详情。
- Party、Map、Challenge Directory、Save、Dialogue、Gacha 尚未进入统一前台导航。
- 战前编队/Buff、战斗目标选择、战斗只读详情、战斗暂停与关卡控制尚未按本计划统一。
- 当前 UI 视觉仍以原创灰盒和功能验证为主；正式美术、动效、音频和 VFX 属于 Phase 18。

### 1.4 版权边界

参考截图只用于理解信息架构、交互类别和页面关系。不得复制参考作品的角色、专有名词、图标、插画、UI 版式细节、字体组合、纹理、动画、音效、数值表或其他受保护表达。项目必须使用原创命名、原创布局、原创占位图形和已授权素材。

## 2. Phase 17 最终产品结果

玩家能够在探索、前台菜单、世界交互、战前准备、战斗和战后返回之间使用一套一致的 UI 导航系统：

1. 探索 HUD 提供任务、队伍、交互、状态与奖励摘要。
2. `Esc` 打开暂停功能中心；`B/T/M/F4` 分别直达背包、队伍、地图和挑战目录。
3. 前台模块始终保持暂停；Back/Esc 逐级返回，`X` 从任意前台页面直接回探索。
4. Character、Inventory、Party、Map、Challenge、Save、Settings 与 Gacha 使用统一 Frontend Shell 和内部 Router。
5. 世界中的 `F` 继续走 InteractionComponent，不被菜单 Router 接管。
6. Encounter 或 Challenge 进入统一战前准备：临时编队、关卡 Buff、确认后产生纯值 Encounter Request。
7. Battle 使用独立 Battle UI Context：行动顺序、角色状态、指令、两阶段目标确认、只读详情和战斗暂停。
8. 战斗结束后返回探索，Reward Summary 每个奖励事务只展示一次。
9. 地图传送、存档、战斗命令、装备、强化和抽卡均由各自 Domain Authority 决定；Widget 只读取快照并提交纯值意图。
10. 所有页面在失败、重复输入、旅行、HUD 重建和 Editor 重开后保持正确输入、鼠标、焦点与所有权。

## 3. 冻结导航规则

### 3.1 探索快捷键

| 输入 | 目标 | 规则 |
|---|---|---|
| `Esc` | Pause Hub | 打开前台功能中心并暂停 |
| `B` | Inventory Module | 直接进入背包根页面并暂停 |
| `T` | Party Module | 直接进入队伍根页面并暂停 |
| `M` | Map Module | 直接进入地图根页面并暂停 |
| `F4` | Challenge Directory | 直接进入材料挑战/高难挑战目录并暂停 |
| `F` | World Interaction | 只由 InteractionComponent 处理 NPC、Teleport、Encounter、Dialogue 等 |

重复快捷键不得重复创建 Shell、重复 Push 或污染返回历史。Battle 状态下探索快捷键默认拒绝，除非任务卡明确为 Battle Context 定义行为。

### 3.2 返回语义

- `Esc` 或 Back：逐级返回。
- 右上角 `X`：关闭整个 Frontend Shell，直接回探索。
- Pause Hub 的 Resume：直接回探索。
- 模块内部切换页签不向全局 ScreenStack Push 新页面。
- Character 模块切换角色时保持当前页签；切换页签时保持当前角色。
- 遗器强化 → 遗器选择 → 角色遗器页 → Character Shell → Pause Hub → Exploration。
- 从快捷键直接进入模块时，Back 的来源仍为 Pause Hub；再 Back 才回 Exploration，保证输入模型一致。

### 3.3 暂停规则

- 所有 Frontend Shell 页面保持 UIManager 拥有的 Pause Token。
- 模块切换不得反复 Pause/Unpause。
- 只有关闭最后一个 Frontend Shell 才释放自身 Pause Token。
- 外部 Pause 不得被 UIManager 无条件解除。
- Battle Pause 使用独立 Battle Context 和所有权，不复用 Exploration Frontend 的 Pause Token。

## 4. UI 层级与 Router 架构

### 4.1 全局 Screen Stack

```text
Exploration Root
└─ Frontend Shell 或 Exploration Modal

Battle Root
└─ Battle Overlay / Battle Pause / Battle ReadOnly Detail
```

全局 ScreenStack 继续负责顶层上下文、输入策略、焦点策略和旅行生命周期，不保存完整业务页面历史。

### 4.2 Frontend Shell 内部 Router

```text
Frontend Shell
├─ Pause Hub
├─ Character Module
│  ├─ Detail
│  ├─ Weapon/LightCone
│  ├─ Traces/Skills
│  │  └─ Skill Detail + Upgrade
│  ├─ Relics
│  │  ├─ Relic Select
│  │  └─ Relic Enhance
│  ├─ Eidolon/Constellation
│  ├─ Information
│  └─ Outfit
├─ Inventory Module
│  ├─ Weapons
│  ├─ Relics
│  ├─ Consumables
│  └─ Materials/Other Categories
├─ Party Module
├─ Map Module
├─ Challenge Directory
├─ Gacha Module
├─ Save Module
├─ Quest Module
└─ Settings Module
```

Router 保存纯值 Route History，例如 `ModuleId/PageId/SelectedCharacterId/SelectedItemId/SourceRoute`，不保存 World、Actor、Widget、ASC 或 Slate 指针。页面实例仍由 Shell/模块工厂拥有。

### 4.3 Battle UI Context

Battle UI 与 Frontend Shell 分离：

```text
Battle HUD
├─ Turn Order
├─ Encounter/Wave Objective
├─ Party Status: Health/Energy/Buffs
├─ Command Panel
├─ Target Highlight
├─ C → ReadOnly Character Detail
├─ Esc → Battle Pause / Stage Information
└─ Result / Confirm Return
```

Battle Detail 只能读取角色、技能、属性和 Buff；禁止升级、换装、换遗器或改队伍。

## 5. 所有权与数据流

统一数据流：

```text
Domain Subsystem/Runtime
→ immutable snapshot / typed delegate
→ ViewModel
→ Widget
→ pure UI intent
→ UI Manager / Domain command
```

### 5.1 UI 所有权

- `UHSRUIManagerSubsystem`：LocalPlayer UI session、全局 ScreenStack、顶层 Widget、InputMode/Focus、旅行重建。
- `UHSRFrontendRouter`：纯值 Route History、模块来源、角色/页签/物品选择状态。
- `UHSRFrontendShellWidget`：模块容器和页面实例；不拥有业务 Runtime。
- 各模块 ViewModel：绑定对应 Domain 快照和 delegate；不成为业务真源。
- Widget：展示和提交意图；禁止直接 AddToViewport、SetInputMode、OpenLevel 或写 SaveGame。

### 5.2 Domain Authority

| 模块 | 权威 |
|---|---|
| Character/Progression | Character/Progression Runtime 与定义资产 |
| Weapon/Equipment/Relic | Equipment/Relic Runtime |
| Inventory/Consumable | InventorySubsystem |
| Party | Party Runtime/Subsystem |
| Reward | RewardSubsystem |
| Quest/Dialogue | Quest/Dialogue Runtime |
| Map/Teleport | MapSubsystem |
| Encounter/Return | BattleTransitionSubsystem |
| Battle Command/Turn | BattleCoordinator/TurnSystem/GAS |
| Save/Load | SaveSubsystem |
| Settings | GameUserSettings/明确授权的 Config 边界 |
| Gacha | 未来独立 Gacha Authority；不存在前不得由 Widget 模拟随机/保底 |

## 6. 模块产品合同

### 6.1 Exploration HUD

- 常驻显示任务追踪、队伍摘要、交互提示、角色状态和战后奖励收据。
- 不拥有 Pause/Character/Inventory 等全屏 Widget。
- Reward Summary 按 Reward Transaction ID 幂等显示，不因 HUD rebuild 重复展示。
- 小地图、技能图标和正式状态表现属于后续独立切片/Phase 18，不得用参考素材直接复制。

### 6.2 Pause Hub

- 是所有非战斗功能的入口中心。
- 功能 Tile 只提交 Route Intent。
- Resume 和 `X` 关闭到 Exploration Root。
- Back 在 Hub 等价于 Resume。
- 首选焦点必须稳定；鼠标和键盘/手柄导航都应有 fallback。

### 6.3 Character Module

- 顶部角色切换列表；左侧养成页签；中央展示；右侧属性/说明/操作。
- 当前 CharacterId 与 TabId 为纯值状态并跨页签/角色切换保持。
- Detail：属性、等级与只读说明。
- Weapon：当前装备、替换、强化入口。
- Traces/Skills：节点图、锁定/前置、节点说明、材料预览和升级意图。
- Relics：槽位总览、套装效果、替换、选择、对比和强化子流程。
- Eidolon：节点解锁与说明。
- Information/Outfit：首版可只读，任何外观替换必须由合法 Authority 提交。
- Widget 不直接修改属性、扣材料或应用 GameplayEffect。

### 6.4 Inventory Module

- `B` 或 Hub Tile 打开。
- 顶部分类；左/中为虚拟化/分页列表；右侧为选中物详情和合法操作。
- 支持 Weapon、Relic、Consumable、Material 等分类；具体分类由定义数据决定。
- 排序、筛选和选择为 UI 状态；Use/Enhance/Disassemble/Equip 为 Domain Command。
- 候选创建、快照、绑定、关闭和旅行 teardown 保持 P17-003B 的 Widget+ViewModel 严格配对。

### 6.5 Party Module

- `T` 或 Hub Tile 打开。
- 支持多个预设队伍、角色列表、加入、移除、换位和详情。
- 编辑采用 candidate-first；确认前不覆盖当前有效队伍。
- 重复角色、空位规则、队长规则和保存策略由 Party Authority 验证，Widget 不自行决定。

### 6.6 Challenge Directory

- `F4` 打开材料获取与高难挑战目录。
- 列表展示稳定 Challenge/Encounter ID、消耗、奖励预览、完成度、锁定条件和进入按钮。
- 材料挑战与高难挑战可以共享目录 UI，但定义、消耗和结算规则必须数据驱动。
- 点击进入只产生挑战选择意图，随后进入统一战前准备；不得直接 OpenLevel。

### 6.7 Map Module

- `M` 或 Hub Tile 打开。
- 区域、楼层、传送点、当前位置和右侧地点详情均来自 Map 快照/定义。
- 选择传送点后调用 MapSubsystem 的稳定 TeleportId 请求。
- Widget 不直接 OpenLevel、不写当前位置、不自行解锁传送点。
- 旅行开始时 Frontend Shell 走 P17-004 teardown；到达后按冻结规则恢复或回 Root。

### 6.8 Dialogue/Interaction

- `F` 继续由 InteractionComponent 选择当前 Candidate。
- NPC 对话显示说话者、正文和多个选项；选项提交稳定 Dialogue Node/Choice ID。
- 对话 Overlay 可覆盖 Exploration HUD，但不进入 Pause Hub Router。
- 对话导致 Encounter、Quest 或 Reward 时只调用对应 Authority，不直接改状态。

### 6.9 Pre-Battle Preparation

统一来源：Challenge Directory 或 NPC/World Encounter。

```text
Encounter Selected
→ Candidate Party
→ Optional Stage Buff
→ Confirm
→ pure Encounter Request
→ BattleTransition Authority
```

- 临时队伍确认前不得覆盖永久探索队伍。
- Buff 只保存稳定 BuffId；进入 Battle 后由 Authority 验证和应用。
- 返回上一步保持候选选择；`X` 取消整个战前事务。
- UIOnly 页面不得自行添加 GE、消耗体力或标记挑战完成。

### 6.10 Battle HUD 与目标确认

- 固定视角属于 Battle/Camera Authority，不由 Widget Tick 驱动。
- 左上：行动顺序与波次。
- 左下：己方角色 Health、Energy、Buff/Debuff 和当前行动者。
- 右下：基本指令、技能、终结/特殊指令和资源。
- 中央/顶部：当前敌方、Health、Weakness/Status 和合法目标提示。
- `C` 打开只读 Character Detail。

战斗指令采用两阶段状态机：

```text
Select Command
→ Select/Confirm Target
→ Submit Battle Command
```

- 第一次选定只创建本地候选，不扣资源、不推进回合。
- 第二次合法确认后才向 Battle Authority 提交命令。
- 敌方技能只允许合法敌方目标；治疗/辅助只允许合法己方目标。
- 不需要目标的命令是否仍需第二次确认，必须在对应任务卡冻结，不能由 Widget 临时决定。
- 取消目标选择返回指令选择，不污染 TurnSystem/GAS。

### 6.11 Battle Pause / Stage Information

- Battle 中 `Esc` 打开独立 Battle Pause。
- 展示关卡目标、评分/星级条件、当前节点、关卡 Buff 和说明。
- 提供 Continue、Restart、Exit。
- Restart/Exit 是权威事务：必须验证状态、清理命令/Turn/Result、处理 Reward 和 Return Context；Widget 不直接 OpenLevel。
- 失败、胜利结算或 Return Pending 时的按钮可用性必须显式定义。

### 6.12 Reward Presentation

- Battle Result 负责确认结算；返回探索后 Reward Summary 在 HUD 左侧逐项展示。
- 每个 Reward Transaction ID 只展示一次。
- UI 重建、重复 delegate、重复 Confirm 和 Save/Load 不得重复领取或重复显示同一收据。
- 展示完成不等于奖励 Authority 成功；只有 RewardSubsystem 的成功事务可生成收据。

### 6.13 Save Module

- 从 Pause Hub 进入，保持暂停。
- 展示自动存档/手动槽位、时间、地图、队伍和版本摘要。
- Save、Load、Overwrite、Delete 均经 Save ViewModel → SaveSubsystem。
- Widget 不直接创建/写入 SaveGame，不自行迁移、不绕过 Primary/Staging/Backup。
- Overwrite 与 Delete 需要确认；Delete 为破坏性操作，必须单独授权和独立失败矩阵。
- Load 成功后 UI 必须处理 World/Host 重建；失败保持旧可操作状态并显示真实分类错误。
- 云存档、多 Profile、跨平台槽位同步不属于 Phase 17。

### 6.14 Gacha Module

- 可先建立 Route、卡池列表、资源摘要、详情、单次/多次意图和结果展示合同。
- 随机、保底、货币扣除、重复转化、奖池时效和 Save 持久化必须由未来独立 Gacha Authority 实现。
- Authority 未建立前，Phase 17 不得将抽卡按钮宣称为真实业务完成；只允许明确标注的 UI Prototype/Blocked Route。
- 禁止在 Widget 或 Blueprint 中使用随机节点模拟正式抽卡。

## 7. 串行工作包与唯一验收结果

每张任务卡必须再次冻结精确 allowlist、失败矩阵和用户 Editor 工作。本节仅定义后续顺序，不自动授权实施。

### 已完成

- `P17-001`：Screen Stack 与 Input Policy 合同 — `PASS`。
- `P17-002`：探索 Pause 与输入/焦点闭环 — `PASS`。
- `P17-003A`：Character Detail 接入 — `PASS WITH FOLLOW-UP`。
- `P17-003B`：Inventory 接入与 Reward Summary 隔离 — `PASS WITH FOLLOW-UP`。
- `P17-004`：Travel teardown/rebuild 与逻辑恢复 — `PASS`。

### P17-005：Frontend Shell、Router 与快捷导航

唯一结果：探索中 Esc/B/T/M/F4 打开正确模块；所有模块保持暂停；Esc/Back 逐级返回，`X` 直接回探索；重复输入零副作用；关闭后 W/A/S/D、鼠标、焦点和 Exploration IMC 恢复。

首版接入 Pause Hub、现有 CharacterDetail、现有 Inventory，并为 Party/Map/Challenge/Save 提供真实可导航模块根页面。模块根页面不能伪造业务成功。

### P17-006：Character 养成 Shell

唯一结果：玩家可在角色列表和 Detail/Weapon/Traces/Relics/Eidolon/Information/Outfit 页签间导航，切角色保持页签、切页签保持角色，Back/X 规则正确；数据至少来自真实 Character/Equipment/Relic 快照。

升级、替换和强化若 Authority 合同不足，分离成后续任务，不在 UI 内补造规则。

### P17-007：Relic/Equipment 多级选择与强化

唯一结果：从角色遗器页进入槽位选择、候选对比和强化，再逐级返回；合法事务通过 Equipment/Relic Authority exactly-once 提交，失败不扣材料、不改装备、不留下半开页面。

### P17-008：Inventory 分类、详情与合法操作

唯一结果：B 打开分类背包，筛选/排序/选择稳定，Consumable Use、Equip/Enhance/Disassemble 只在对应 Authority 支持时提交；任何失败保持完整快照和一致 UI。

### P17-009：Party 与战前编队/Buff

唯一结果：T 可编辑并确认永久队伍；Encounter/Challenge 可建立独立候选队伍和 Buff，确认后产生纯值 Encounter Request；取消不污染永久队伍、资源或 BattleTransition。

### P17-010：Map 与 Challenge Directory

唯一结果：M 可选择已解锁传送点并通过 MapSubsystem 旅行；F4 可选择材料/高难挑战并进入战前准备；锁定、无效、重复和旅行失败保持旧状态。

### P17-011：Save UI

唯一结果：从 Pause Hub 查看真实槽位摘要，执行 Save/Load/Overwrite 的安全事务，并在成功 Load 后重建正确 UI；失败保持旧 World 可操作。Delete 默认不进入首版，除非单独授权。

### P17-012：Dialogue Presentation

唯一结果：F 与 NPC 进入事件驱动对话，选项逐级推进或退出；Quest/Encounter/Reward 分支只提交稳定 ID，重复选择不重复领奖或重复触发 Encounter。

### P17-013：Battle HUD、只读详情与两阶段指令

唯一结果：行动顺序、己方 Health/Energy/Buff、敌方状态和 Command Panel 事件驱动更新；C 打开只读详情；Command → Target → Confirm exactly-once，取消零副作用。

### P17-014：Battle Pause、Restart/Exit 与 Reward Presentation

唯一结果：Esc 打开关卡信息；Continue/Restart/Exit 通过权威事务工作；胜利返回后 Reward Summary 每个事务只显示一次，并恢复 Exploration HUD/input。

### P17-015：Gacha UI Contract

唯一结果：建立可导航的原创 Gacha Shell、Banner/Resource/Intent/Result DTO 与失败展示合同。若 Gacha Authority 尚未获得独立设计和授权，本任务只到 `UI CONTRACT PASS / BUSINESS BLOCKED`，不得宣称真实抽卡完成。

### P17-016：阶段收尾

唯一结果：Fresh Build、全量 HSR.UI 定向 Automation、两种分辨率、键鼠、可用时物理手柄、Editor 重开、旅行/战斗/存档回归、Teacher、Independent Review、版权/provenance 审核和文档归档全部完成。

## 8. Codex 与用户 Editor 分工

### 8.1 Codex 负责

- 经任务卡授权的 C++ Route DTO、Router、UIManager、ViewModel、Widget 基类和 Automation。
- 审查业务 Authority 的公开快照/命令合同；缺失时停止并申请精确扩权。
- Build、Automation、日志解析、失败矩阵和文档证据。
- 保持核心规则在 C++/Domain Authority，不将不可验证规则藏进 Blueprint。

### 8.2 用户负责

- UE Editor 中创建/派生 WBP、布局、动画、按钮、Is Focusable、首选焦点和 Class Reference。
- 创建 Enhanced Input Action/Mapping 资产和 Blueprint forwarding；任何新 Config 需单独授权。
- 保存、关闭并重开 Editor，验证资产引用持久化。
- PIE 中确认可见焦点、鼠标、键盘/手柄导航、布局、动画和业务结果。
- 对原创/第三方素材保留来源和授权记录。

## 9. 候选文件和资产结构

每个任务必须缩小为精确 allowlist。长期候选结构：

```text
Source/HSR/UI/
├─ Frontend/
│  ├─ HSRFrontendRouteTypes.*
│  ├─ HSRFrontendRouter.*
│  └─ HSRFrontendShellWidget.*
├─ Character/
├─ Inventory/
├─ Party/
├─ Map/
├─ Challenge/
├─ Dialogue/
├─ Save/
├─ Battle/
└─ Gacha/

Source/HSR/Tests/
├─ HSRFrontendNavigationTests.cpp
├─ HSRCharacterUITests.cpp
├─ HSRInventoryUITests.cpp
├─ HSRPartyPreparationUITests.cpp
├─ HSRMapChallengeUITests.cpp
├─ HSRSaveUITests.cpp
└─ HSRBattleUITests.cpp

Content/UI/P17/
├─ Frontend/
├─ Character/
├─ Inventory/
├─ Party/
├─ Map/
├─ Challenge/
├─ Dialogue/
├─ Save/
├─ Battle/
└─ Gacha/
```

不得因为本计划列出候选路径就批量创建空目录或扩张任务权限。

## 10. 自动化与失败矩阵

### 10.1 Router/输入

- Esc/B/T/M/F4 正确路由；Battle/Dialogue 等错误上下文拒绝。
- 重复按键、按键竞争、同帧 Back/X、缺失 Class、Create/Attach/Policy/Focus 失败。
- Back 逐级、X close-to-root、来源 Route、角色/页签保持。
- Frontend Pause Token exactly-once；外部 Pause 不被解除。
- 重建、旅行、旧 Host 回调和 stale Route Token 零副作用。

### 10.2 Character/Inventory/Equipment

- 缺 Definition、无角色、重复角色、无效页签、空列表。
- 半配对 Widget/ViewModel、候选创建失败、快照失败、delegate 重复/漏解绑。
- 升级材料不足、锁定节点、前置不足、重复请求、事务失败和补偿。
- 遗器槽位不符、已装备冲突、强化上限、锁定物品和并发选择。

### 10.3 Party/Pre-Battle

- 重复角色、空队、非法位置、无 leader、已锁定角色。
- 候选队伍取消、确认失败、Buff 锁定/重复/缺失、资源不足。
- Encounter 被解决、Return Pending、普通 Map Travel Pending 和重复 Confirm。

### 10.4 Map/Challenge

- 未解锁区域/传送点、错误 World、错误 Arrival、重复/过期 Request。
- Challenge 锁定、消耗不足、无奖励定义、无战前配置。
- Travel failure/cancel/timeout 后旧 UI 和输入保持可操作。

### 10.5 Save

- 空槽、损坏、版本不兼容、定义缺失、空间/写入/替换失败。
- Overwrite 取消、重复 Save/Load、Load 期间旅行或 Battle Return Pending。
- Primary/Staging/Backup 分类和失败恢复；UI 不得早于 Authority 报告成功。

### 10.6 Battle

- 非当前行动者、非法 Command、资源不足、目标失效、目标类型错误。
- 第一次选择零资源修改；第二次确认 exactly-once。
- Cancel 回到 Command Selection；重复确认和旧 Target callback 零副作用。
- C 详情只读；Battle Pause 不泄漏探索输入。
- Restart/Exit 在行动执行中、Result、Return Pending 等状态的拒绝和清理。
- Reward/Return 重复回调不重复领取或展示。

## 11. 验证门禁

每个工作包至少需要：

1. Development Editor Build，记录 UHT/Compile/Link 结果。
2. 对应窄 Automation 全部明确 `Success`。
3. 一条用户可见 PIE happy path。
4. 一条真实或受控失败路径。
5. Editor Save All、关闭并重开后的 Class/Input/Asset 引用验证。
6. `git diff --check`；不触碰任务 allowlist 外文件。
7. 文档明确区分 Build、Automation、PIE、用户视觉观察和 `NOT VERIFIED`。

P17-005 起重点分辨率：1920×1080 与 1280×720。物理手柄不可用时写 `NOT VERIFIED`，不得用键盘焦点自动化替代物理手柄证据。

## 12. 文档更新

每个任务完成后更新：

- `tasks/active-task.md`
- `tasks/execution-result.md`
- `tasks/final-review.md`
- `tasks/archive/TASK-P17-xxx.md`
- `worklog.md`
- `todo_plan.md`（只写真实进度）
- `learning-journal.md` 或相关专题学习文档

Phase 收尾再更新 `PROJECT_STATE.md`、README 和 Phase 17 最终总结。规划存在不等于任务完成。

## 13. 风险与停止条件

出现以下情况必须停止并申请授权：

- 新模块、插件、CommonUI、外部依赖或 Config。
- 批量创建/移动/删除 Content 资产。
- 修改 Map/Battle/Save/Inventory/Party/Equipment/Gacha 等业务权威。
- 新建真实随机抽卡、保底、货币或商业活动系统。
- 删除存档、覆盖真实用户槽位或执行破坏性失败注入。
- 复制参考游戏的受保护 UI、图标、插画、文本、角色或音频。
- 任务需要超出已冻结 allowlist。

## 14. 明确非目标

- Phase 18 的正式美术、动画、音频、GameplayCue 与 VFX。
- 登录、账号、云存档、多 Profile、多人、split-screen。
- 限时运营后台、支付和在线抽卡服务。
- 使用 Tick/任意 Delay 轮询 UI、旅行或战斗状态。
- 由 Widget 决定 Gameplay、Travel、Save、Reward、Battle 或 Gacha 结果。
- 把参考截图做成像素级复制品。

## 15. 下一步

唯一建议的下一任务是：

`P17-005 — Frontend Shell、内部 Router 与 Esc/B/T/M/F4 统一快捷导航`。

开始实现前必须创建独立任务卡，冻结精确 Source/Content allowlist、路由 DTO、Pause 所有权、输入资产边界、WBP Editor 步骤、Automation 和 PIE 验收；未经用户明确授权不得开始实现。
