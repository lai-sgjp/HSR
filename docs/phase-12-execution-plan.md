# Phase 12 详细执行计划：装备、遗器与来源可逆属性

> 基线：2026-07-25，`HEAD=0da4660`；Phase 11 功能状态为 `Ready with inherited follow-ups`。
> Gate 0：`PASS`（2026-07-25）。四角色首轮均为 `REVISE`；随后完成 Phase 11 provenance/allowlist 审计、删除误跟踪 `vision_actions.cpython-311.pyc`、加入缓存忽略规则，并将 `main` 推送至 `origin/main=42c32e0`，阻断已闭合。

## 1. 当前事实、证据与 Gate 0

### 1.1 Phase 11 前置证据

- `AGENT/BUILD VERIFIED`：Development Editor Build、8 项 `HSR.*` Automation、`git diff --check`、Progression GE Apply/Remove 故障注入。
- `USER PROVIDED / LOG INSPECTED`：Editor 重开 Load、ResultView 消费与返回、Closeout cleanup、C 键和蓝图按钮往返。
- `STATIC REVIEW ONLY`：Character Detail 打开时终局自动返回 ResultView。
- `NOT VERIFIED / USER ACCEPTED`：Standalone `Esc`。
- `NOT RUN`：完整 Phase 2/5～10 专项回归。

这些 follow-up 不阻塞 Phase 12 设计，但不得改写为已验证。

### 1.2 Gate 0 四角色结论

| 角色 | 结论 | 核心意见 |
|---|---|---|
| Coordinator | `REVISE` | 先冻结整阶段契约并闭合 Phase 11 交付链。 |
| Independent Reviewer | `REVISE` | `main` ahead 3；净差异含已跟踪 `Plugins/.../__pycache__/vision_actions.cpython-311.pyc`，其余本地噪声不得认领。 |
| Teacher | `REVISE` | 所有权、事务、来源 Handle、Save 重建和 Editor 资产合同必须先冻结。 |
| Implementation | `REVISE` | 现有 Progression 只支持单一成长来源；必须建立独立 Equipment authority 和来源投影，不能复用 Progression revision/handle。 |

### 1.3 Gate 0 转 `PASS` 条件

1. 对 `origin/main..HEAD` 完成 Phase 11 provenance/allowlist 审计。
2. 经用户授权处置误跟踪的 `Plugins/UnrealMCPython/Content/Python/UnrealMCPython/__pycache__/vision_actions.cpython-311.pyc`；不得认领或提交 `.claude/settings.local.json` 和其余未跟踪 `__pycache__`。
3. Phase 11 closeout 状态文档与真实提交链一致，并按项目规则完成远端同步。
4. 本计划经复核；随后才能创建唯一活动任务 P12-001。

以上四项已于 2026-07-25 闭合。删除和忽略规则提交为 `42c32e0`，远端 `main` 已同步；P12-001 可以按本计划创建。

## 2. 阶段唯一最终结果

一个固定装备和一套固定遗器可按稳定实例 ID 装备、替换、卸下、强化、触发一个固定套装阈值，并通过各自来源可追踪的 Infinite GE 精确影响属性；Save/Load 和 Battle Actor 重建后效果恰好一份，卸下只移除自己的效果，UI 可只读显示比较、套装和属性来源。

## 3. 架构与所有权不变量

### 3.1 四层数据边界

- **Definition**：静态只读 DataAsset；保存规则、槽位、固定数值和显示资源，不保存运行状态。
- **Instance/Authority**：`UGameInstanceSubsystem` 生命周期的纯值真源；保存 `InstanceId`、`DefinitionId`、槽位、强化等级、固定 authored 词条和 Character 绑定。
- **Aggregator**：纯函数；输出确定性数值、来源分解和 revision/fingerprint，不持有 ASC 或 Handle。
- **Runtime Projection**：Battle Actor/Coordinator 侧持有 ASC 与 `InstanceId/SetSourceId -> FActiveGameplayEffectHandle`；Handle 不跨 World、不进 Save。
- **UI**：只读 Snapshot/Breakdown 并提交命令；不写业务状态、不应用 GE、无 Tick。

### 3.2 属性与来源顺序

首版只支持固定 additive modifier，顺序固定为：

`Character Base -> Progression -> Equipment -> Relic main/sub -> Relic Set`

禁止从最终 Attribute 反推来源；禁止按 GE Class、Tag 或批量查询误删其他来源；每个 Instance 或 Set threshold 都有独立 source identity 和 Runtime Handle。

### 3.3 Equip/强化事务

`Validate -> Candidate loadout/instance -> Aggregate -> Runtime projection -> Commit -> Broadcast`

任一步失败时旧 authority、旧有效 GE 和 UI snapshot 不变。重复相同请求为明确 no-op；失败解除后可重试收敛。内存事务成功后才允许进入 Save 快照。

### 3.4 Save 边界

Save 只包含稳定 `DefinitionId/InstanceId/CharacterId`、槽位、强化等级、固定 authored modifiers 和 authority revision。禁止保存最终 Attribute、DataAsset/UObject、Actor、ASC、GE Handle 或 Widget。Load 必须 candidate-first 校验并在提交后幂等重建 Runtime；Phase 16 才完成通用迁移、备份和恢复体系。

## 4. 串行工作包

### P12-001：Definition、Instance 与纯值装备事务

**唯一验收结果：** 一个固定装备和一套固定遗器实例能生成并完成纯值 Equip/Replace/Unequip；未知/重复 ID、错误槽位、跨角色双占用、非法数值或强化等级失败时零副作用。

**Codex allowlist：**

- `Source/HSR/Data/Definitions/HSREquipmentDefinition.h`
- `Source/HSR/Data/Definitions/HSRRelicDefinition.h`
- `Source/HSR/Equipment/HSREquipmentTypes.h`
- `Source/HSR/Equipment/HSREquipmentSubsystem.h`
- `Source/HSR/Equipment/HSREquipmentSubsystem.cpp`
- `Source/HSR/Tests/HSREquipmentSubsystemTests.cpp`
- `tasks/active-task.md`、`tasks/execution-result.md`、`tasks/final-review.md`

**用户 Editor 资产：** 一个原创占位 Equipment Definition、六个固定 Relic Definition、一个固定 Relic Set Definition；本包不创建 GE。

### P12-002：属性来源分解与 Equipment Infinite GE

**唯一验收结果：** Equip 后属性精确增加，Replace 只替换目标实例来源，Unequip 精确恢复；同 revision 重建不叠加，ASC replacement 后不复用旧 Handle，Apply/Remove/rollback 失败可重试且旧有效层不丢失。

**Codex allowlist：**

- `Source/HSR/Equipment/HSREquipmentStatAggregator.h`
- `Source/HSR/Equipment/HSREquipmentStatAggregator.cpp`
- `Source/HSR/Equipment/HSREquipmentEffectBridge.h`
- `Source/HSR/Equipment/HSREquipmentEffectBridge.cpp`
- `Source/HSR/Progression/HSRCharacterDerivedStats.h`
- `Source/HSR/Battle/HSRBattleCoordinator.h`
- `Source/HSR/Battle/HSRBattleCoordinator.cpp`
- `Source/HSR/Battle/HSRBattleGameMode.h`
- `Source/HSR/Battle/HSRBattleGameMode.cpp`
- `Source/HSR/Progression/HSRProgressionGameplayTags.h`
- `Source/HSR/Progression/HSRProgressionGameplayTags.cpp`
- `Config/DefaultGameplayTags.ini`
- `Source/HSR/Tests/HSREquipmentEffectContractTests.cpp`
- `tasks/active-task.md`、`tasks/execution-result.md`、`tasks/final-review.md`

**用户 Editor 资产：** `GE_Equipment_P12` 和 `GE_Relic_P12`。两者各含四个 additive SetByCaller Modifier，DataTag/Modifier Tag 精确为 `Equipment.Bonus.MaxHealth`、`Equipment.Bonus.Attack`、`Equipment.Bonus.Defense`、`Equipment.Bonus.Speed`，分别写入 `MaxHealth/Attack/Defense/Speed`；必须核对 `Duration Policy=Infinite`、无 Period/Execution/Cue，并保存重开。Codex 不创建或修改 Content 资产。

### P12-003：遗器套装阈值与强化事务

**唯一验收结果：** 固定遗器完成 0->1->2->1->2 阈值和强化；套装 GE 始终为 0/1，换同套部件不重复，不同套装不串计数，非法强化或套装 GE 失败不半提交。

**Codex allowlist：**

- `Source/HSR/Equipment/HSRRelicSetResolver.h`
- `Source/HSR/Equipment/HSRRelicSetResolver.cpp`
- `Source/HSR/Equipment/HSREquipmentSubsystem.h`
- `Source/HSR/Equipment/HSREquipmentSubsystem.cpp`
- `Source/HSR/Equipment/HSREquipmentStatAggregator.h`
- `Source/HSR/Equipment/HSREquipmentStatAggregator.cpp`
- `Source/HSR/Equipment/HSREquipmentEffectBridge.h`
- `Source/HSR/Equipment/HSREquipmentEffectBridge.cpp`
- `Source/HSR/Tests/HSRRelicSetResolverTests.cpp`
- `Source/HSR/Tests/HSREquipmentSubsystemTests.cpp`
- `tasks/active-task.md`、`tasks/execution-result.md`、`tasks/final-review.md`

**用户 Editor 资产：** 一个固定两件套 Set GE；首版只有一个阈值，不实现 4 件套链或条件效果。

### P12-004：Save v2、只读 UI 与 Editor 重开闭环

**唯一验收结果：** Equip -> 强化 -> Save -> 改变/清空 -> Editor 重开 Load 后，装备映射、套装、ASC 属性和 UI 来源全部恢复；重复 Load/Rebuild 不叠层，坏数据不污染旧 Runtime。

**Codex allowlist：**

- `Source/HSR/Save/HSRSaveTypes.h`
- `Source/HSR/Save/HSRSaveGame.h`
- `Source/HSR/Save/HSRSaveGame.cpp`
- `Source/HSR/Save/HSRSaveSubsystem.h`
- `Source/HSR/Save/HSRSaveSubsystem.cpp`
- `Source/HSR/UI/HSREquipmentDetailTypes.h`
- `Source/HSR/UI/HSREquipmentDetailViewModel.h`
- `Source/HSR/UI/HSREquipmentDetailViewModel.cpp`
- `Source/HSR/UI/HSREquipmentDetailWidget.h`
- `Source/HSR/UI/HSREquipmentDetailWidget.cpp`
- `Source/HSR/Tests/HSREquipmentSaveTests.cpp`
- `Source/HSR/Tests/HSREquipmentDetailViewModelTests.cpp`
- `tasks/active-task.md`、`tasks/execution-result.md`、`tasks/final-review.md`

**用户 Editor 资产：** `Content/UI/WBP_EquipmentDetail_P12.uasset`。若需修改既有 Character Detail 或 Battle Command Panel，任务卡必须逐项扩权，不能泛化为整个 `Content/UI`。

### P12-005：阶段收尾

**唯一验收结果：** P12-001～004 的工程、资产、Build、Automation、PIE、失败注入、教学、provenance、Git 和归档证据可独立复核，Phase 12 正式 Gate 有明确结论。

仅允许收尾文档、任务三件套和归档；不得新增 Gameplay。必须运行 Development Editor Build、Phase 12 Automation、固定装备主路径、失败路径、Battle Actor 重建、Save/Load/重复 Load、Widget destruct/rebuild，以及 Phase 11 progression 回归。

## 5. 验证与失败矩阵

必须至少覆盖：空/重复 DefinitionId、空/重复 InstanceId、slot mismatch、未知 Character、实例双占用、NaN/overflow modifier、强化 cap/非法等级、candidate 失败、同 revision、2->1/1->2 套装阈值、Apply 失败、old remove 失败、new rollback 失败、secondary cleanup、ASC replacement、Reset/EndPlay、坏 schema、缺 Definition、重复 Save 实例、Load candidate 失败、重复 Load、Widget 重建。

证据顺序：Automation 纯值/事务 -> Development Editor Build -> Editor 资产保存重开 -> PIE 主路径 -> PIE 失败注入 -> Save/Load 重开 -> 独立审查。构建以第一处真实 UHT/C++/Link 错误为准，不用旧日志代替当前结果。

## 6. 教学 Gate

用户在收尾前应能解释：

1. Definition、Instance/Save、Aggregator、Runtime Handle 四层边界。
2. 一次换装的 candidate-first 时序。
3. 为什么 Handle 不能存档，最终 Attribute 不能反推装备。
4. 为什么必须按 Instance 精确移除而不能按 GE Class 批删。
5. 为什么套装奖励是独立来源。
6. Load 后如何幂等重建每个来源。

## 7. 风险、停止条件与非目标

- 若需删除/移动资产、修改 Config、增加模块/外部依赖或扩大任务 allowlist，必须停止并请求授权。
- 若 UE5.6 的 Active GE Handle 返回语义不确定，先查实际引擎头文件或以首次编译证据校准，不猜 API。
- 随机副词条、随机种子、背包/掉落/奖励、资源经济、重铸/分解、复杂乘区、条件套装、被动 Ability 授予、完整 Save 迁移/备份、统一 UI 栈、正式美术和联网均不属于 Phase 12。

## 8. Phase 12 完成定义

只有 P12-001～005 全部归档、每包唯一结果和失败路径均有证据、用户 Editor 资产有作者与保存重开记录、每来源 GE 可精确移除、Save/Load 重建不叠层、教学与 Independent Reviewer 放行、角色提交及阶段交付闭合后，Phase 12 才能标记 `Ready` 或 `Ready with inherited follow-ups`。

## 9. 当前唯一下一步

创建并执行 P12-001：Definition、Instance 与纯值装备事务；不得提前创建 GE、接入 Battle/Save/UI 或进入 P12-002。
