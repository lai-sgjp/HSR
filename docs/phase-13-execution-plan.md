# Phase 13 详细执行计划：背包、奖励与幂等掉落

> 基线：2026-07-26，`HEAD=origin/main=7606235`；Phase 12 为 `Ready with inherited follow-ups`。
> Gate 0：`PASS`。Coordinator、Independent Reviewer、Teacher、Implementation 四角色规划完成；先冻结本计划，再开始 P13-001。

## 1. 当前事实、证据与 Gate 0

- `AGENT/BUILD VERIFIED`：Phase 12 最终 Development Editor Build、`HSR.Equipment`、`HSR.Save`、`HSR.UI.EquipmentDetail` Automation 与 `git diff --check` 通过。
- `USER PROVIDED`：Phase 12 PIE 覆盖 Setup、Detail、2->1->2、Save/Clear/重复 Load 与 Cleanup。
- `PASS WITH GUIDED CORRECTION`：Phase 12 Teaching Gate。
- `NOT VERIFIED / inherited`：Standalone Esc、详情页打开时终局自动返回 ResultView、完整 Phase 2/5～10 专项回归。
- `GIT VERIFIED`：Phase 12 已提交并推送，`HEAD=origin/main=7606235`。旧状态文档中的“等待 commit/push”按 Git 与归档证据校准为陈旧信息。
- 工作树中的 `.claude/settings.local.json`、`learn/AI.md`、`learn/CppEngineDepth.md` 属于用户现有改动，Phase 13 全程排除且不得认领。

四角色结论：Phase 13 可进入。正式实现前必须冻结 Inventory/Equipment 持有权、RewardTransaction 幂等、Drop seed、Save v3 和 UI 边界；本计划已完成冻结，Gate 0 判定 `PASS`。

## 2. 阶段唯一最终结果

一个原创固定奖励可由 Encounter 驱动的 Battle victory 或 Reward Chest 通过同一个 RewardSubsystem 事务入口发放：首次成功原子写入背包和领取账本；相同 ClaimId 重放、失败重试、Save/Load 后重放均不重复到账；固定 seed 的掉落可复现，背包与奖励 UI 只读显示物品、数量、实例和来源。独立 Enemy drop producer 未在 Phase 13 实现。

## 3. 所有权与数据流不变量

### 3.1 数据分层

- **Definition**：Item/Reward/Drop `UPrimaryDataAsset`，只保存静态规则、稳定 ID、显示数据和软引用；不保存数量、实例、领取状态或 RNG 状态。
- **Inventory Authority**：`UGameInstanceSubsystem` 保存未装备物品。堆叠物为 `ItemId -> Quantity`；唯一物品为 `InstanceId + DefinitionId`；首版容量按 slot 计，整笔事务 all-or-nothing。
- **Equipment Authority**：继续保存已装备 loadout。唯一实例在 Inventory 与 Equipment 之间只能显式转移；同一 `InstanceId` 不得同时存在。P13-001 先建立通用 Item 边界，不重写 Phase 12 Equipment 合约；跨系统转移只在 P13-002 精确扩权后实现。
- **Reward Resolver**：纯值解析 Reward/Drop Definition；输入显式 seed 和稳定排序后的候选，输出冻结的 grant proposal，不修改 Inventory。
- **Reward Transaction**：以稳定 `ClaimId` 为幂等键，执行 `Validate -> Resolve/Freeze -> Build Inventory Candidate -> Capacity Check -> Commit Inventory + Ledger -> Broadcast`。失败不写 ledger、不广播、不留下部分奖励。
- **Producer paths**：Encounter 驱动的 Battle victory 与 Reward Chest 只提交稳定来源 ID、ClaimId、Reward/Drop ID 和上下文；禁止直接 AddItem 或自行掷随机。探索 Enemy 只发起 Encounter，不是独立 Reward producer。
- **Save**：只保存稳定 ID、数量、唯一实例、已领取 ClaimId、必要 seed/冻结结果和 revision；不保存 DataAsset/UObject、Actor、ASC、GE Handle、Widget 或随机流对象。
- **UI**：只读 Snapshot/Source Breakdown 并提交命令；不发奖、不直接改库存、无 Tick。

### 3.2 ID 与随机契约

- `DefinitionId` 标识静态规则；`InstanceId` 标识唯一物；`RewardDefinitionId`/`DropTableId` 标识静态奖励规则；`ClaimId` 标识一次可领取事实。显示名、数组下标、资产路径和 Actor 指针都不是业务 ID。
- 幂等只保证同一稳定 `ClaimId`；重试时生成新 GUID 不属于幂等。
- Drop 解析使用显式 seed、固定候选排序和版本字段。解析结果在事务首次尝试时冻结；容量或提交失败后的重试不得重掷。
- UE `FRandomStream` 的当前同平台复现证据不外推为永久跨平台兼容承诺。

### 3.3 Battle、Chest 与 Save 边界

- `FHSRBattleResult` 当前没有 RewardId；P13-003 只显式扩展纯 DTO/消费边界，BattleCoordinator 不依赖 Inventory 或 Widget。
- Encounter/Battle victory 上下文可携带 Reward 引用；探索 Enemy 仍只负责 Encounter 请求，不直接持有或发放掉落。
- BattleResult consumed-once、Chest opened 或单次 Delegate 都不能替代持久 Claim ledger。
- Phase 13 Save v3 只覆盖本阶段 DTO、candidate-first 恢复与 v2 明确兼容策略；Phase 16 的通用迁移、备份和损坏恢复不在本阶段。

## 4. 串行工作包

### P13-001：Item Definition 与 Inventory 纯值事务

**唯一验收结果：** 一个固定可堆叠材料和一个固定唯一物品可完成 Add/Remove；空/重复 ID、非法数量、整数溢出、stack cap、容量不足和重复实例全部零副作用，重复 no-op 不广播。

**Codex allowlist：**

- `Source/HSR/Data/Definitions/HSRItemDefinition.h`
- `Source/HSR/Inventory/HSRItemTypes.h`
- `Source/HSR/Inventory/HSRInventorySubsystem.h`
- `Source/HSR/Inventory/HSRInventorySubsystem.cpp`
- `Source/HSR/Tests/HSRInventorySubsystemTests.cpp`
- `tasks/active-task.md`
- `tasks/execution-result.md`
- `tasks/final-review.md`

**用户 Editor Gate：** C++/Automation 通过后创建一个原创 stackable Item Definition 和一个 unique Item Definition；记录精确资产路径、字段、作者及保存重开证据。P13-001 不创建 Reward/Drop/GE/UI 资产。

### P13-002：确定性 Reward/Drop 与幂等事务

**唯一验收结果：** 同一 ClaimId 首次成功到账一次，任意重放为 no-op；相同 seed 和稳定候选顺序产生相同冻结结果，失败不 claim 且重试不重掷；多项 grant 任一失败时整笔零副作用。

**预期 Codex allowlist：**

- `Source/HSR/Data/Definitions/HSRRewardDefinition.h`
- `Source/HSR/Data/Definitions/HSRDropTableDefinition.h`
- `Source/HSR/Reward/HSRRewardTypes.h`
- `Source/HSR/Reward/HSRRewardResolver.h`
- `Source/HSR/Reward/HSRRewardResolver.cpp`
- `Source/HSR/Reward/HSRRewardSubsystem.h`
- `Source/HSR/Reward/HSRRewardSubsystem.cpp`
- P13-001 Inventory 文件的精确事务扩展
- `Source/HSR/Equipment/HSREquipmentSubsystem.h/.cpp`（仅在跨 authority 唯一性/转移确实需要时单独扩权）
- 对应 Automation 与任务三件套

**用户 Editor Gate：** 一个固定 Reward Definition 和一个带权重 Drop Table；固定 seed 策略、稳定 ID、保存重开与引用持久证据。

### P13-003：Battle Victory 与 Reward Chest 统一奖励入口

**唯一验收结果：** Encounter 驱动的 Battle victory 与 Reward Chest 两条路径都只构造 Reward request；胜利和宝箱首次成功到账，defeat、重复 Result、重复交互、地图返回和失败重试均不会重复发奖。独立 Enemy drop producer 不属于交付结果。

**预期 Codex allowlist：** Reward 入口、`HSRBattleTypes.h`、Battle 结果消费适配器、`HSREncounterDefinition.h`、必要的 Enemy/Chest 定义或 Actor 精确路径、对应 Automation/Development harness 与任务三件套。创建任务卡时必须逐文件校准，不允许泛化为整个 Battle/Enemy/Exploration 目录。

**用户 Editor Gate：** 为一个 Encounter/Battle victory 上下文和一个 Chest 绑定稳定 Reward/Drop ID；Blueprint 禁止直接 AddItem。保存重开后运行主路径与重复/失败路径。

### P13-004：Save v3 与只读 Inventory/Reward UI

**唯一验收结果：** 领取 -> Save -> 篡改/清空 -> Editor 重开 Load 后，数量、唯一实例、Claim ledger、冻结掉落与来源 UI 恢复；重复 Load 不叠加/不重复广播，坏数据不污染旧 Runtime。

**预期 Codex allowlist：** `HSRSaveTypes/SaveGame/SaveSubsystem` 精确文件、Inventory/Reward Snapshot/ViewModel/Widget 文件、对应 Automation、Development harness 与任务三件套。

**用户 Editor Gate：** 创建 `WBP_Inventory_P13` 和 `WBP_RewardSummary_P13`，只绑定 ViewModel Snapshot/命令；保存关闭重开并提供 Save/Load、重复 Load、Widget rebuild 日志。消耗品 GAS 使用、可写装备 UI 和统一 Screen Stack 不阻塞本阶段 MVP。

### P13-005：阶段收尾

**唯一验收结果：** P13-001～004 的工程、资产、Build、Automation、PIE、失败注入、Editor 重开、教学、provenance、Git 和归档证据可独立复核；不新增 Gameplay。

## 5. 验证与失败矩阵

必须覆盖：空/重复 ItemId、显示名冲突、数量 0/负数/溢出、stack cap、容量恰满、部分 grant 超容、空/重复 InstanceId、跨 Inventory/Equipment 重复实例、未知 Reward/Drop、空 reward、重复 entry、非法权重/NaN/总权重零、同 seed 同结果、稳定排序、同 ClaimId 同 payload/no-op、同 ClaimId 不同 payload/reject、candidate/commit/ledger 失败、defeat 不发奖、重复 BattleResult/Chest 请求、坏 schema、缺 Definition、重复 saved item/instance/claim、Load candidate failure、repeated Load、Widget destruct/rebind。

证据顺序：纯值 Automation -> fresh Development Editor Build -> Editor 资产保存重开 -> PIE stack/unique/capacity -> Battle victory/Chest exactly-once 与失败重试 -> Save/Editor restart/repeated Load -> UI lifecycle -> Independent Review。每项标记 `AGENT VERIFIED`、`USER PROVIDED`、`STATIC ONLY` 或 `NOT RUN`。

## 6. 教学 Gate

用户在收尾前应能解释：

1. DefinitionId、InstanceId、Reward/Drop DefinitionId 与 ClaimId 的区别。
2. Inventory possession 与 Equipment loadout 的真源和唯一实例转移边界。
3. `Validate -> Resolve/Freeze -> Candidate -> Commit Inventory + Ledger -> Broadcast -> Save` 时序。
4. 为什么先写 ledger、先改背包或先广播都会制造半提交风险。
5. 为什么 Result consumed-once 不等于奖励幂等。
6. 为什么固定 seed 仍需稳定候选顺序，失败重试不能重掷。
7. Save 为什么不保存 UObject/Actor/Widget/ASC/GE Handle/RNG 对象。
8. UI 为什么只能读 Snapshot 和提交命令。

## 7. 风险、停止条件与非目标

- 需要删除/移动资产、Config、第三方依赖、新模块、跨系统大重构或扩大 allowlist 时停止并请求授权。
- 若要把 Phase 12 的全部装备/遗器实例统一迁移为 Inventory 单一 possession authority，视为独立合约迁移，不得顺手完成。
- 商店、货币经济、任务奖励 consumer、随机副词条、重铸/拆解/出售、邮件/地面溢出、复杂 pity/动态等级、Phase 16 通用迁移/备份、Phase 17 UI 栈、正式美术、联网/复制/预测均不属于 Phase 13。
- 固定消耗品 GAS 接入是 Optional；若扣除与 Ability 激活的原子/补偿语义未单独冻结，不进入阶段 Gate。

## 8. Phase 13 完成定义

P13-001～005 全部归档；所有 producer 只走统一 Reward 入口；同 ClaimId 在失败重试、重复事件和 Save/Load 后只成功一次；固定 seed 结果可复现；Inventory/Equipment 不重复持有实例；Save/Load 与 UI lifecycle 通过；用户 Editor 资产和教学有真实证据；Independent Reviewer 放行且 Git 交付闭合。满足前不得标记 Phase 13 Ready 或进入 Phase 14。

## 9. 当前唯一下一步

执行 P13-001：Item Definition 与 Inventory 纯值事务。不得提前创建 Reward/Drop、接入 Battle/Chest/Enemy、修改 Save/UI 或进入 P13-002。
