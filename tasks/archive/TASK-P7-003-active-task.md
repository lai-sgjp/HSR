# TASK-P7-003：Basic、Skill、Ultimate 迁移到统一 Execution（归档）

## 最终状态

`PASS WITH FOLLOW-UP / Ready for archive`

## 已完成

- Basic、Skill、Ultimate 已通过 Coordinator-owned prepared transaction 迁移至 `BP_GE_P7_DamageExecution`；Heal 保持旧路径。
- Coordinator 预检、RNG stream-copy、SP Reserve/Commit/Rollback、Action cache、terminal 延迟和 Ultimate GAS Cost/Refund 合同成立。
- 旧 Basic/Skill/Ultimate 固定伤害 GE 资产保留但无正式运行时引用；三次正式伤害均走统一 Execution。
- fresh Rebuild 与两轮全绿 formal/P5/P6 smoke 证据由用户提供并经 Reviewer 检查。

## Follow-up

- `CaptureFailed`/`InvalidCapturedValue` 未动态注入。
- Cost 已提交后 ApplyFailure→Refund exactly-once 未动态注入。
- ReportedAppliedDamage 与真实 HP delta 诊断不一致，尚不能作为权威 Breakdown AppliedDamage。
- 三技能 lethal、overkill（Final>Applied）及真正 `Coordinator::Reset()` 后同 ActionId 未覆盖。
- Wait/Pass、Basic/Skill/受击回能和 Team SP 池拆分另行规划，不属于本任务。

## 归档指针

- 执行结果：`tasks/archive/TASK-P7-003-execution-result.md`
- 最终审查：`tasks/archive/TASK-P7-003-final-review.md`

