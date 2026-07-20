# TASK-P7-004：P7-003 伤害事务 Follow-up 闭环

## Role Lock / 状态

低级执行模型当前且仅能担任 `Implementation Agent / 低级执行模型`；不得切换角色或扩大 Phase。P7-003 已由 Reviewer 判定 `PASS WITH FOLLOW-UP` 并归档；本任务为 `PLANNED — 等待用户独立确认执行`。首次读取后只读复述，必须以“等待用户确认执行 TASK-P7-004。”结束。

## 唯一目标

只补齐 P7-003 审查遗留的可复核证据与最小诊断缺口，不新增正式战斗规则：优先修复 `DamageResult/Breakdown.AppliedDamage` 与真实 `HealthBefore-HealthAfter` 对账，其次建立受控 Development 注入覆盖 CaptureFailed/InvalidCapturedValue、Cost 已提交后 ApplyFailure→Refund exactly-once，最后补 lethal、overkill 与真正 `Coordinator::Reset()` 后 ActionId/cache/RNG 生命周期。

## 严格依赖顺序

1. **P7-004A Breakdown 对账**：先修唯一 Result/Context 写回路径，要求 `AppliedDamage == HealthBefore - HealthAfter`；FinalDamage 与 AppliedDamage 在 overkill 时明确分离。未通过不得进入失败注入。
2. **P7-004B Capture/InvalidValue 注入**：只在 `WITH_EDITOR` Harness 注入可控失败，不修改正式 Capture 业务语义；失败 HP/Energy/SP/Turn/Result/RNG 不变。
3. **P7-004C Cost/ApplyFailure/Refund**：在 Spec Apply 前后提供仅测试用的失败 seam；验证 Cost committed→Apply failure→Refund 一次→Energy before==after，Refund 失败必须报告事务阻断，不能静默继续。
4. **P7-004D lethal/overkill/Reset**：三技能致死、Final>Applied overkill、真正 `Coordinator::Reset()` 后同 ActionId 不命中旧 cache 且 RNG 从新边界开始。

各段严格串行，同一时间只有一个活动段；任何一段发现需要修改正式规则、资产所有权或跨 Phase 文件，立即停止。

## 精确 Implementation allowlist

- `Source/HSR/GAS/Damage/HSRDamageTypes.h`
- `Source/HSR/GAS/Damage/HSRDamageExecutionCalculation.h`
- `Source/HSR/GAS/Damage/HSRDamageExecutionCalculation.cpp`
- `Source/HSR/GAS/Attribute/HSRCoreAttributeSet.h`
- `Source/HSR/GAS/Attribute/HSRCoreAttributeSet.cpp`
- `Source/HSR/Battle/HSRBattleCoordinator.h`
- `Source/HSR/Battle/HSRBattleCoordinator.cpp`
- `Source/HSR/Battle/HSRBattleGameMode.h`（仅 editor harness 声明，如确需）
- `Source/HSR/Battle/HSRBattleGameMode.cpp`（仅 editor harness/失败注入）
- `tasks/execution-result.md`

`HSRGameplayAbilityBase.*`、Basic/Skill/Ultimate、SkillDefinition、EffectContext、AbilitySystemGlobals、TurnManager、UI、Build.cs 与所有旧 GE/Skill/Refund/Rule 资产默认只读；如 P7-004A 确实必须扩大范围，停止并由 Coordinator 另发精确修订，不得自行修改。

## 用户/Editor 资产边界

- 默认无新增用户资产、无 Config 变更；复用现有 `BP_HSRBattleGameMode` 的 editor-only Harness 配置。
- 如注入开关或失败 seam 必须持久化 Blueprint 字段，只允许用户修改 `Content/Blueprints/Framework/BP_HSRBattleGameMode.uasset`，先由 Coordinator 书面扩权；Implementation 不编辑资产。
- 不修改三 Skill DataAsset、P7 Execution GE、Ultimate Refund GE、初始化 GE、Rule、旧固定 GE、Gameplay Tags 或地图。

## 必须保持的非目标

- 不改 Basic +1 SP、Skill -1 SP、Ultimate Energy Cost。
- 不实现 Wait/Pass、Basic/Skill/受击回能、Team SP 池拆分、敌方资源 AI；这些另立任务。
- 不迁移 Heal、不修改 Phase 8/9/10、AoE/多段、表现、SaveGame、网络/复制/Prediction。
- 不删除/移动旧 GE，不重写 Git 历史，不使用全局 RNG/Tick，不把诊断字段当规则真源。

## 验证证据

- 每段必须运行 fresh `HSREditor Win64 Development -Rebuild`（若只改 Harness 仍需记录实际构建边界），连续两轮 PIE，保留首个错误、exit code、日志和证据等级。
- 每条失败日志包含 ActionId/BattleId、Source/Target、HP/Energy/SP/Turn/RNG before/after、Result/Failure、RefundApplied 与 terminal 状态。
- Breakdown：正常、overkill、duplicate 三组均证明 FinalDamage/AppliedDamage/HP delta 对账。
- Failure：CaptureFailed、InvalidCapturedValue、ApplyFailure/Refund exactly-once；失败不得改变声明状态。
- Lifecycle：三技能 lethal exactly-once、终局后拒绝、真正 Reset 后旧 ActionId 不命中且新 RNG 序列可解释。
- P5/P6 smoke 与 Heal 回归保持通过；无 Error/Ensure/GC/Blueprint Runtime Error。

## 交接与门禁

- 执行者只写 `tasks/execution-result.md`，不得自判 PASS；用户完成 Editor/PIE 后交 Reviewer。
- Reviewer 必须先写 `tasks/final-review.md`；未独立放行不得归档 P7-004 或进入 P7-005。
- 当前不自动实施、不 Build/Editor/PIE、不执行 Git。
