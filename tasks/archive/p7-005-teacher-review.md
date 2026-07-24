# P7-005 教师复盘：从一次伤害请求到可审计事务

本复盘只解释已经实现并由日志验证的概念，不新增战斗规则，也不把开发注入 seam 当作 Shipping 规则。

## 1. Execution Capture：先捕获，再计算

`UHSRDamageExecutionCalculation` 通过 Source Attack/CritRate/CritDamage 与 Target Defense 的 GAS Capture 得到公式输入。Capture 失败或值非 finite 时，结果必须是 `CaptureFailed` 或 `InvalidCapturedValue`，并且不能输出 `IncomingDamage`。对应开发矩阵日志为：

`P7-004 Matrix Case=CaptureFailed Result=PASS`

`P7-004 Matrix Case=InvalidCapturedValue Result=PASS`

这两类失败的审计重点是 HP、Energy、SP、Turn、RNG 和 terminal 全部不变。

## 2. Meta Attribute：伤害先进入 IncomingDamage

Execution 不直接改 Health，而是输出 `IncomingDamage` meta attribute。AttributeSet 在唯一结算点读取它、清零并将 Health clamp 到 `[0, MaxHealth]`。当前实现还把真实 `HealthBefore - HealthAfter` 写回 `DamageResult.Breakdown.AppliedDamage`，因此：

- `FinalDamage` 是公式结果；
- `AppliedDamage` 是实际生命值变化；
- overkill 时二者可以不同。

Reviewer 证据记录过 `Final=15, Applied=1, HP→0`，这正是 overkill 的可审计形态。

## 3. EffectContext：纯值诊断载体

`FHSRDamageEffectContext` 携带 ActionId、DamageType、Multiplier、CritRoll、Rule 数值以及 DamageResult。它不保存 Actor、ASC 或 UObject 生命周期。开发注入字段只在 `WITH_EDITOR` 下存在，默认 `None`，并通过 `Duplicate/NetSerialize` 在同一测试过程内传递。

一个重要故障教训是：`MakeOutgoingSpec` 可能复制 Context。修复后 Coordinator 会重新取得 Spec 内部 Context，显式写回 ActionId 与 injection；日志用 `P7-004 SpecContext` 和 `P7-004 AbilityApply Context` 对账。

## 4. RNG stream-copy：预览不等于提交

Coordinator 使用 battle-local `FRandomStream` 的副本预览 CritRoll。只有 Spec、Target、Rule、Tag 和 Apply 前置条件都通过，且正式伤害成功，才提交副本并递增 consume count。失败、重复 ActionId 和终局拒绝不得推进正式 RNG。

可核验字段包括 `RNGBefore`、`RNGAfter`、ActionId 和 Duplicate replay；P7-002 矩阵与 P7-004 Reset/Overkill 日志都采用这一格式。

## 5. Prepared transaction：Coordinator 准备，Ability 唯一 Apply

事务顺序是：Coordinator preflight → 创建 Context/Spec → prepared handoff → 资源 reserve → Ability activation → Ability Base 唯一 Apply → 资源/RNG/cache/Turn/terminal 提交。Basic、Skill、Ultimate 不再各自创建旧伤害 Spec。

这保证同一 ActionId 最多一次伤害、一次 Breakdown、一次资源提交和一次 Turn/terminal 结果。失败时 prepared 数据清理，SP rollback，RNG preview 丢弃。

## 6. Cost/Refund：GAS 扣费与补偿

Ultimate 的 Energy Cost 仍由 GAS `CommitAbility` 执行；Coordinator 不直接写 Energy。只有 Cost 已提交且 post-Cost ApplyFailure 注入命中时，Ultimate 才 self-apply 预构建 Refund GE 一次。

Reviewer 证据中的成功路径是：`CostCommitted=1 DamageApplied=1 RefundApplied=0 Energy 100→0`；P7-004 Refund 矩阵则要求 `CostCommitted=1 RefundApplied=1 Energy 100→0→100`，重复 ActionId 不得再次扣费或退款。

## 7. FinalDamage vs AppliedDamage

FinalDamage 描述执行公式，AppliedDamage 描述真实 Health delta。不能用 FinalDamage 代替生命值变化，也不能把 overkill 当作额外 HP 损失。正确断言同时检查 Final、Applied、HPBefore、HPAfter 与 terminal 状态。

## 8. Terminal 与 Reset

致死伤害只产生一次 BattleResult；事务提交前 Health delegate 只记录 pending defeat，提交后才 ResolveDefeat。Finished 后新 Action 必须拒绝，不能再次扣资源、推进 RNG 或产生伤害。

真实 Reset 不是简单重置 RNG：它清除 Participants、RequestId、Definitions、TurnManager、Action cache 与资源。开发 seam 以纯值 encounter request 执行 `Reset → SubmitBattleRequest → BuildParticipants`，不得直接写 Spawned 或恢复旧 Actor/ASC。新 BattleId 下复用旧 ActionId 必须作为新 Action 执行，而不是命中旧 cache。

## 9. 证据等级与后续边界

现有 Reviewer 结论为 `PASS WITH FOLLOW-UP`。已具备的证据包括 fresh Build、P7-002/P7-003/P7-004 矩阵、Ultimate Cost/Refund、Overkill、Reset/Rebuild 与 P5/P6/Heal smoke。后续若验证真实 GAS 自然 ApplyFailure、真实底层 Aggregator capture false 或 World teardown 后引用安全，应增加独立测试；开发注入结果不能冒充引擎自然故障证据。
