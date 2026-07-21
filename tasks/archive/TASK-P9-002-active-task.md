# TASK-P9-002：叠层、刷新与替换策略纵切

状态：`COMPLETE — PASS WITH FOLLOW-UP`

## Role Lock

执行者当前且仅能担任 `Implementation Agent / 低级执行模型`。不得切换角色、扩大范围、推进 P9-003 或宣称 Phase 9 完成。首次只读复述后必须停止等待用户单独确认。

## 前置与唯一可见结果

P9-001 已由 Reviewer 判定 `PASS WITH FOLLOW-UP`，最终 USER PROVIDED 两轮 PIE 为 `28 PASS / 2 COMPLETE / 0 FAIL`。P9-002 只扩展既有 Status Runtime，不新建第二套容器。

当前硬阻断：UE5.6 公开 Runtime API 与现有 P9-001 资产契约不足以安全配置/验证目标聚合、StackLimit=2 和每层 Attack 计算。不得通过修改 P9-001 GE 绕过；P9-001 sentinel 与精确移除证据依赖其独立 Handle 语义。P9-002 必须先由用户完成下述独立资产 Gate，Implementation 暂停。

历史处置：上述阻断促成两段 Gate，原 `BLOCKED` 历史保留。用户现已明确确认 Gate A 全部列出的 GE/DA 字段与 Editor 重开保持，Gate A 结论为 `USER PROVIDED PASS`；这只解除实现前资产阻断，不是 Gameplay、Build、Gate B 或 PIE 完成。

唯一可见结果：同一目标连续获得同一 `StatusId` 的 Attack Buff 时，Runtime Instance 按冻结策略从 1 层增加到最多 2 层；每层使用同一逻辑剩余回合，达到上限后再次 Add 只刷新剩余回合；显式 Replace 在新 GE 成功后精确移除旧 Handle 并提交新 Handle；Apply/Refresh/Replace 任一失败均保留旧 Instance、旧层数、旧属性和旧 Handle。

本任务不实现 DoT、Break Debuff、免疫、驱散、UI、Save、网络或新的状态容器。

## 冻结规则与所有权

- 身份键：`TargetParticipantId + StatusId` 唯一 Instance；不同 Source 不创建第二 Instance，SourceParticipantId 记录最新成功来源并可审计。
- `MaxStacks=2`；同一状态每次 AddStack 只增一层，超过上限保持 2 层并刷新剩余回合为 Definition 的 DurationTurns。
- `RefreshDuration`：不改变层数、不 Apply 新 GE、不换 Handle，只刷新 RemainingTurns。
- `AddStack`：同一 Handle 上通过可审计方式更新层数与属性；失败不得改变旧值。若 UE/GAS 不支持安全修改，必须停止请求替代契约，不能隐式 Replace。
- `Replace`：先验证新 Definition/GE 并成功 Apply 新 Handle；再移除旧 Handle；最后原子提交新 Instance。新 Apply 失败时旧状态完整保留；旧 Remove 失败时不得丢失可追踪旧/新 Handle，记录结构化失败并停止进一步组合。
- StatusDefinition 仍为静态 DataAsset；StatusComponent 仍是 Runtime Instance/Handle 唯一所有者；Coordinator 只负责接线、清理和 harness；TurnManager 只发布事件。

## 精确允许范围

- `Source/HSR/Status/HSRStatusTypes.h`
- `Source/HSR/Status/HSRStatusComponent.h`
- `Source/HSR/Status/HSRStatusComponent.cpp`
- `Source/HSR/Data/Definitions/HSRStatusDefinition.h`
- `Source/HSR/Data/Definitions/HSRStatusDefinition.cpp`
- `Source/HSR/Battle/HSRBattleCoordinator.h`
- `Source/HSR/Battle/HSRBattleCoordinator.cpp`
- `Source/HSR/Battle/HSRBattleGameMode.h`
- `Source/HSR/Battle/HSRBattleGameMode.cpp`
- `tasks/execution-result.md`

禁止修改 TurnManager、CharacterBase、AttributeSet、Build.cs、`.uproject`、Config、Content、用户资产和根历史审查文件。需要扩权立即停止。

## Two-stage User Asset Gate

P9-001 的 `DA_Status_AttackUp_P9`、`GE_Status_AttackUp_P9` 和 BP 绑定必须保持原样；禁止修改或复用该 GE 作为叠层资产。用户创建：

- `Content/Data/Status/DA_Status_AttackUpStack_P9_002.uasset`
- `Content/GameplayEffects/GE_Status_AttackUpStack_P9_002.uasset`

Definition 使用 `StatusId=Status.Buff.AttackUp`，与 P9-001 保持同一 Runtime 身份语义；P9-002 harness 必须在干净目标上只选择新 Definition，不得让两个 Definition 在同一 Target+StatusId 上并存。GameplayTag 复用 `Status.Buff.AttackUp`，不新增第二个 Tag。

### Gate A：实现前资产骨架与专用 GE

Gate A：`USER PROVIDED PASS`（2026-07-21）。实际资产：`Content/Data/Status/DA_Status_AttackUpStack_P9_002.uasset`、`Content/GameplayEffects/GE_Status_AttackUpStack_P9_002.uasset`。

用户确认 GE 为 Infinite、Aggregate by Target、Stack Limit Count=2、Attack Add 10.0 per stack、Tag 仅 `Status.Buff.AttackUp`，且无 Period/Execution/Cue/其他 Modifier/Tag；DA 为 DurationTurns=2、MaxStacks=2、TargetTurnEnded、同 Tag、新 GE 引用，保存并重开 Editor 后保持。

当前 `EHSRStatusRefreshPolicy` 尚无 `AddStack`，因此用户此时不能也不得伪造该枚举值。`DA_Status_AttackUpStack_P9_002` 在 Gate A 仅创建骨架：DurationTurns=`2`、MaxStacks=`2`、TriggerTiming=`TargetTurnEnded`、GrantedStatusTag=`Status.Buff.AttackUp`、InfiniteGameplayEffectClass 指向新 GE；RefreshPolicy 暂选现有 `RefreshDuration`，并明确标记为 **placeholder / NOT ACCEPTANCE CONFIG / DO NOT RUN**。

`GE_Status_AttackUpStack_P9_002`：Duration Policy=`Infinite`；Stacking Type=`Aggregate by Target`；Stack Limit Count=`2`；Attack Modifier=`Add`、基础幅度 `10.0`，并明确由 UE5.6 资产配置实现“每层 +10 / 按 Stack Count 聚合”；Granted Tag 仅 `Status.Buff.AttackUp`；无 Period、Execution、Cue、其他 Modifier 或额外 Tag。

Overflow 第三次 Apply 必须拒绝新增第三层；达到 2 层后的“刷新 duration”不依赖 GE 秒数，仍由 Status Runtime 将 RemainingTurns 重置为 2。不得启用以现实时间刷新/重置 Duration 的选项来模拟逻辑回合。

用户必须逐字段截图或文字回传实际 UE5.6 标签（若面板名称不同，记录实际名称和值），保存全部资产，关闭并重开 Editor，确认 Definition→GE 软类引用、Stacking Type、Limit、Modifier、Tag、无 Period/Execution/Cue 均保持，再提供资产路径与 Git 归属确认。

附件 `a316...` 仅包含 P9-001 运行日志，不是 P9-002 stacking 资产字段、保存重开或引用持久证据；不得用于关闭 Gate A，也不得外推为新 GE/DA 已正确配置。

`Config/DefaultGameplayTags.ini` EOF 空行仍为提交前 User follow-up；本 Gate 不代改 Config。

Gate A 已通过。执行者下一步只能重新只读复述并等待用户单独确认，之后才可实现 `AddStack` 枚举、Runtime 逻辑和 Development harness，并完成 fresh Build。先前复述/确认不自动延续；当前仍禁止 Build/harness/PIE。

### Gate B：实现与 Build 后的最终 Editor 配置

Implementation Build 成功并由代码暴露真实 `AddStack` 枚举后，用户必须关闭并重开 Editor，打开 `DA_Status_AttackUpStack_P9_002`，将 RefreshPolicy 从 placeholder `RefreshDuration` 明确切换为新增 `AddStack`，保存，再次关闭/重开并确认字段持久。

只有 Gate B 的 `RefreshPolicy=AddStack`、专用 GE 完整字段和引用持久证据齐全后，才允许运行两轮 PIE。Gate A 的 placeholder 绝不属于验收配置，任何使用 `RefreshDuration` placeholder 得出的 PIE 必须判为无效，不得计入 PASS。

## 失败矩阵与验证 Gate

Harness 默认关闭，14 个 P9-001 主路径必须回归，并新增：

| Case | 预期 |
|---|---|
| AddStack 1→2 | Instance=1、Stacks 2、属性按 Definition 规则可审计、Handle 身份不丢 |
| OverMax Add | Stacks 保持 2、Remaining 刷新、无第三层/第二 Instance |
| RefreshAt1 | 层数保持 1、Remaining 重置、Handle/ApplyCount 不变 |
| ExplicitReplaceSuccess | 新 GE/Handle 成功后旧 Handle 精确移除，新 Instance 可追踪 |
| NewApplyFailure | 旧 Instance/层数/属性/Tag/Handle 完整不变 |
| OldRemoveFailure | 新旧 Handle 均有结构化归属，不静默丢失或误删其他 GE |
| DifferentSources | 同 Target+StatusId 仍一 Instance，来源变更可审计，不重复 Apply |
| DuplicateEvent/Add | exactly-once，无重复层数或刷新 |
| Expire/Clear/Death/Finished/Reset/EndPlay/ManagerReplacement | 继承 P9-001 清理与解绑，无残留实例/Handle/回调 |

必须运行 fresh `HSREditor Win64 Development`（UHT/C++/Link/metadata）、静态 allowlist/GC/原子性检查和用户两轮 PIE。所有证据标注 `AGENT VERIFIED`、`USER PROVIDED` 或 `STATIC REVIEW`；失败不得以总 PASS 覆盖。

## 首次响应门禁

Gate A 通过后，执行者首次只读复述任务编号、唯一结果、身份/来源/层数/刷新/替换规则、Handle 事务、Gate A placeholder 边界、Gate B 最终切换、新资产实际字段、精确白名单、User follow-up、失败矩阵和 Build/PIE Gate，最后原样写：

`等待用户确认执行 TASK-P9-002。`

用户单独确认前不得调用实现或写入工具；确认不授权 P9-003 或任何 Git 写操作。
