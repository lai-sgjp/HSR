# TASK-P9-004：免疫、驱散、来源失效与战斗清理

状态：`COMPLETE — PASS WITH FOLLOW-UP`

## 唯一可见结果

在 1v1 中，带 `Status.Immunity.Debuff` 的目标结构化拒绝 DoT/Break Debuff 且零副作用；对无免疫目标执行一次确定性 Dispel，只移除一个符合分类且可驱散的 Debuff 并精确移除其 Handle，不影响 Buff、不可驱散状态或 sentinel GE；来源死亡按 Definition 的 Keep/Remove 策略处理，目标死亡、Finished、Reset、EndPlay 均清理完整。

不实现效果命中/抵抗数值、随机驱散、Boss 阶段、UI、Save、网络或 P9-005。

## 冻结规则

- Definition 增加正面/负面分类、bDispellable、ImmunityTag、SourceInvalidPolicy(Keep/Remove)。静态资产不保存 Runtime。
- Immunity 在 Apply GE 前查询目标 ASC Tag；拒绝不得 Apply marker GE、创建 Instance、改变回合/Handle/计数。
- Dispel 候选只来自 StatusComponent 自有实例；按稳定 StatusId lexical 顺序选第一个 `Debuff && bDispellable`，按保存 Handle 精确移除。空候选/重复 Dispel 为零副作用。
- 不可驱散状态永不被通用 Dispel 移除。Buff、sentinel、非 Status Runtime GE 均不进入候选。
- SourceInvalidPolicy：来源死亡/无效时 Keep 保持至正常到期，Remove 立即精确清理；同一 source event exactly-once。
- Target death、Finished、Reset、EndPlay/ManagerReplacement 沿用 production Clear/unbind；失败必须保留可审计 Handle ownership，禁止按 Tag/类批量移除。

## 精确 Implementation 白名单

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

TurnManager、AttributeSet、Damage/Break、Config、Content、Build.cs、uproject 与根审查历史默认禁止修改；需要扩权时停止。

## User Editor/Config

用户在实现字段可见后负责：新增 `Status.Immunity.Debuff`；创建 Infinite tag-only `GE_Status_DebuffImmunity_P9_004`；配置 DoT 为 Debuff/Dispellable/RemoveOnSourceInvalid，Break Debuff 为 Debuff/不可驱散/Keep，AttackUp 为 Buff；在 GameMode BP 绑定 immunity GE/测试配置。所有资产保存、关闭重开并确认，无 Period/伤害/额外 Modifier/Cue/Execution。Implementation 不代建资产。

### Asset Gate result

`USER PROVIDED PASS`（2026-07-22）。用户确认 immunity GE 为 Infinite、tag-only、无 Modifier/Execution/Cue/Period；GameMode immunity GE 与 Status Definition 绑定在保存、关闭/重开 Editor 后均持久。PIE 现已授权，但最终 Reviewer 尚未落盘。

## 验证矩阵

- ImmunityRejectDoT、ImmunityRejectBreak：零 Instance/Apply/Handle/Tag/伤害。
- DispelOneDeterministic、EmptyDispel、DuplicateDispel：稳定选择、精确一次、零误删。
- UndispellableBreakPreserved、BuffPreserved、SentinelPreserved。
- MultiSourceSameStatus、SourceDeathRemove、SourceDeathKeep、重复/旧 source event。
- TargetDeath、Finished、ResetSecondBattle、EndPlay、ManagerReplacement：完整清理/解绑。
- Invalid Definition/classification/immunity Tag/ASC/target：结构化拒绝零副作用。
- 两轮完整回归 P9-003 `36 PASS/2 COMPLETE`、P9-002 `16 PASS/2 COMPLETE`、P9-001 `28 PASS/2 COMPLETE`。

## Build/PIE Gate

Fresh HSREditor Build 必须包含 UHT、Status/Coordinator/GameMode C++、HSR lib/dll Link、metadata、exit0；静态审查精确 Handle、候选排序、失败原子性、无 Tick/Timer/批量移除。用户资产重开证据后才运行两轮 PIE，所有矩阵零 FAIL/INCOMPLETE/SKIPPED。

Config EOF 空行继续作为提交前 User follow-up，本任务不代改。

## 首次响应门禁

执行者首次只读复述唯一结果、分类/免疫/驱散/来源策略、Handle 所有权、白名单、User 资产、矩阵和 Build/PIE Gate，最后原样写：

`等待用户确认执行 TASK-P9-004。`

用户已确认继续；当前只允许按矩阵运行 PIE并等待 Reviewer，禁止推进 P9-005 或 Git 写操作。
