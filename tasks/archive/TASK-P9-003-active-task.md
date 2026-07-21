# TASK-P9-003：回合触发 DoT 与 Phase 8 Break Debuff 接入

状态：`COMPLETE — PASS WITH FOLLOW-UP`

## Role Lock 与前置

执行者仅为 Implementation Agent。P9-000～002 已归档；P9-002 最终为自身 `16 PASS/2 COMPLETE`、P9-001 回归 `28 PASS/2 COMPLETE`、零失败。不得扩大到免疫、驱散、UI、Save、网络或 P9-004。

## 唯一可见结果

正常 1v1 中，目标获得持续 2 个自身 TurnEnded 的原创 DoT：每次目标 TurnEnded 先通过 Phase 7 统一 Damage/Execution 入口结算一次可审计伤害，再递减回合，第二次触发后到期。Phase 8 BreakResult 只提交 Status Add 请求，使同一 Status Runtime 添加 2 回合 tag-only Break Debuff。死亡、Finished、Reset、EndPlay、重复事件和旧 Epoch 下均 exactly-once，无迟到伤害或残留 Handle。

## 冻结所有权与顺序

- StatusDefinition 静态保存 DoT/Break 配置；StatusComponent 保存实例、回合、Handle 与最后消费序号，不直接写 Health。
- DoT 必须调用现有统一 Damage Execution/Coordinator 事务入口并产生 DamageType/Breakdown；禁止直接 Set Health、手工 Clamp 或自建公式。
- Break/Ability/GE 只构造纯值 Status Add 请求；Coordinator 将 Phase 8 BreakResult 映射到 Break Definition 并请求 StatusComponent。Break 不持有实例/Handle、不直接递减。
- Target TurnEnded 顺序：验证 Epoch/Participant/Sequence → DoT trigger damage → 若死亡/Finished 则 production cleanup 并停止 → 否则 RemainingTurns-- → 归零精确移除。
- 同 Epoch+TurnSequence 每实例至多消费一次；重复、旧 Epoch、非目标、TurnStarted、死亡目标、Finished 后事件零副作用。
- 最终 tick 可致死：只发布一次 Damage/Defeat/Finish 并清理全部状态；不得再递减或伤害。Break Debuff 不触发伤害。

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
- `Source/HSR/Battle/HSRTurnManager.cpp` (user-authorized 2026-07-22, minimal synchronous-terminal `ResolveAction` fix only)
- `tasks/execution-result.md`

只读：`HSRTurnManager.h`、`HSRBreakTypes.h`、`HSRDamageTypes.h`、`HSRDamageExecutionCalculation.*`、`HSRCoreAttributeSet.*`。`HSRTurnManager.cpp` 仅授权上述同步终局最小修复。未列文件禁止修改；若统一 Damage 入口不能在白名单内安全复用，停止请求扩权，不得旁路。

## User Editor/Config 资产

Implementation 不创建资产。用户在实现者给出最终字段后创建并验证：

- Tags：`Status.Debuff.DamageOverTime`、`Status.Debuff.Break`、`Damage.Type.Status`。
- `Content/Data/Status/DA_Status_DamageOverTime_P9_003.uasset`：Duration=2、TargetTurnEnded、单层、DoT DamageType/Rule/统一 Damage GE 引用。
- `Content/GameplayEffects/GE_Status_DamageOverTime_P9_003.uasset`：Infinite、仅 Status Tag；无 Period、直接 Health Modifier或自有伤害 Execution。
- `Content/Data/Status/DA_Status_BreakDebuff_P9_003.uasset`：Duration=2、TargetTurnEnded、单层、Break Tag。
- `Content/GameplayEffects/GE_Status_BreakDebuff_P9_003.uasset`：Infinite、tag-only；无 Period/伤害/属性 Modifier。
- `BP_HSRBattleGameMode`：绑定两 Definition 与默认关闭 P9-003 harness。

用户必须保存、关闭重开 Editor、确认字段/引用持久后运行 PIE。Config EOF 空行保留为提交前 User follow-up，本任务不代改。

### Asset Gate result

`USER PROVIDED PASS`（2026-07-22）。用户确认以下配置已保存，并在关闭/重开 Editor 后保持：

- Tags：`Status.Debuff.DamageOverTime`、`Status.Debuff.Break`、`Damage.Type.Status`；
- DoT marker GE：Infinite，仅授予 DoT Status Tag；无 Period、直接 Health Modifier 或自有伤害 Execution；
- Break marker GE：Infinite、tag-only；无 Period、伤害或属性 Modifier；
- DoT Definition：Duration=2、TargetTurnEnded、单层、Damage.Type.Status、现有统一 Damage Rule/GE 引用与 DoT marker GE；
- Break Definition：Duration=2、TargetTurnEnded、单层、Break Tag 与 Break marker GE；
- `BP_HSRBattleGameMode`：DoT 与 Break 两个 Definition 引用均已绑定并在重开后保持。

证据等级为 `USER PROVIDED`；Coordinator/Reviewer 未反序列化二进制资产。Asset Gate PASS 不代表 Implementation 矩阵、Build 或 PIE 完成。

当前所有 P9-003、P9-002、P9-001 及其他 Development harness 开关必须保持关闭。只有 Implementation 补齐活动卡矩阵、fresh Build 成功并由 Reviewer/Coordinator 明确允许后，用户才可开启对应 harness 运行两轮 PIE；此前任何 PIE 不计入验收。

## 失败与回归矩阵

| Case | 预期 |
|---|---|
| DoT Add | 单实例/Handle，Remaining=2，无立即伤害 |
| NonTarget/Duplicate/OldEpoch | 零伤害、零递减、零计数变化 |
| FirstTargetTurnEnded | 统一 Damage 一次，Breakdown 可审计，2→1 |
| FinalTargetTurnEnded | 统一 Damage 一次，随后到期，无第三次伤害 |
| DoT Lethal FinalTick | 一次 Damage/Defeat/Finish，全清，无迟到副作用 |
| Invalid Definition/GE/Rule/DamageType/ASC/Target | 结构化拒绝、零副作用 |
| Damage ApplyFailure | Remaining/Handle/实例保持，零假成功；重试语义明确 |
| BreakResult Request | 一次 Add 请求、单 Break Debuff 实例/Handle |
| Duplicate Break/Action | 不重复实例/Apply/回合消费 |
| Death/Finished/Reset/EndPlay/ManagerReplacement | 两状态清理、解绑、旧事件零副作用 |

完整回归 P9-001 十四 Case 与 P9-002 八 Case；两轮全部 PASS/COMPLETE，零 FAIL/INCOMPLETE/SKIPPED。

## Build 与证据 Gate

- Fresh HSREditor Win64 Development：真实 UHT、Status/Coordinator/GameMode C++、HSR lib/dll Link、metadata、exit 0；保留首错/warnings。
- 静态审查统一 Damage 唯一入口、trigger-before-decrement、失败原子性、Handle、重复/旧 Epoch、死亡/终局、无 Tick/Timer/GE Period/直接 Health 写入、allowlist。
- 用户重开 Editor 后两轮 PIE：P9-003 矩阵 + P9-002 `16 PASS/2 COMPLETE` + P9-001 `28 PASS/2 COMPLETE` 全回归；证据严格分级。

## 首次响应门禁

执行者首次只读复述任务编号、唯一结果、Damage/Status/Break 所有权、trigger/递减顺序、死亡/终局/幂等、白名单、User 资产、失败矩阵、Build/两轮回归 Gate，然后停止并原样写：

`等待用户确认执行 TASK-P9-003。`

用户已确认继续 P9-003。执行者仍必须按活动卡完成首次/修订复述；当前授权仅进入 Implementation matrix revision 与 Build，不授权 PIE、P9-004 或 Git 写操作。
