# TASK-P7-003：Basic、Skill、Ultimate 迁移到统一 Execution

## Role Lock / 角色锁定

低级执行模型当前且仅能担任 `Implementation Agent / 低级执行模型`。不得切换角色、扩大范围、实现延期资源系统或删除资产。发现必须修改未列文件、无法满足事务原子性、现有未提交变更无法归属或 UE API 不支持冻结方案时立即停止。

## 当前 Phase 与确认门禁

- P7-001、P7-002：Independent Reviewer 均为 `PASS WITH FOLLOW-UP`，三件套已归档。
- P7-003：`PLANNED — 等待用户对 TASK-P7-003 的独立执行确认`。
- 首次读取只允许复述；复述前、复述中和用户独立确认前零工具调用。必须以“等待用户确认执行 TASK-P7-003。”结束。

## 唯一目标

将正式 Basic、Skill、Ultimate 从各自直接 Health Modifier GE 迁移到 P7-002 的同一个 `BP_GE_P7_DamageExecution`，使用 SkillDefinition 的 DamageType、AbilityMultiplier 与 DamageRule；每个成功 ActionId 最多产生一次 RNG、一次伤害、一个 Damage Breakdown、一次资源提交、一个 Resolution 和一次 Turn/终局结果。Heal 保持 P6 直接治疗路径。

旧固定伤害资产只解除运行时引用，不删除、不移动、不改写历史。P7-003 不实现新资源规则或 Phase 8/9/10 内容。

## 已知资源现状与延期边界

- 当前 Basic 成功 `+1 SP`，Skill 成功 `-1 SP`；没有 Wait/Pass。
- Energy 当前只作为 Ultimate GAS Cost；Basic、Skill 和受击均不回 Energy。
- 当前只有一个 `FHSRTeamResourceState`，存在未来敌我共池风险。正确长期边界是**每个 Team 一个共享池**，敌方拥有独立池或明确无池；不是每角色独立 SP。
- Wait/Pass、Basic/Skill/受击回能、每 Team SP 池拆分必须另建后续任务，不得塞入 P7-003，不得借迁移改变当前数值行为。

## 统一职责与调用顺序

1. Coordinator 完成 Battle/Action/Turn/Definition/Target/终局/资源与 Damage 静态数据 preflight。
2. Ability Base 构建统一 Damage Context/Spec；三个伤害 Ability 只提供各自 Cost/资源语义并调用公共 Apply，不复制公式或 RNG。
3. Coordinator 是正式 battle-local RNG 与 ActionId→Resolution/DamageResult 缓存唯一所有者。用 RandomStream **副本预览** CritRoll；只有伤害 Apply 成功才提交副本状态与 consume count。
4. Execution 捕获/计算，CoreAttributeSet 消费 IncomingDamage；Ability 返回结构化 DamageResult。
5. 成功后 Coordinator 提交 SP、保存不可变 Resolution/Breakdown，并由既有 Health delegate 优先处理致死；若未终局再推进 Turn。
6. 重复 ActionId 在所有验证、Cost、RNG、GE、Turn 前返回第一次缓存结果。

## 事务与失败硬门禁

- 所有 Rule/Tag/GE/Context/Spec/Target/Capture 可前置验证必须在 Cost、RNG 或 HP 副作用前完成。
- SP 继续 Reserve→成功 Commit / 失败 Rollback；失败不得改变共享 SP。
- Ultimate Energy 必须继续由 GAS Cost GE 修改，不允许 Coordinator/Widget 直接写 Energy。
- Ultimate 在 Damage Apply 前提交 Cost 时，必须具备**GAS 补偿路径**：Damage Apply/Context Result 失败时，通过单独的通用 Energy Refund GE 恢复本次确切 Cost，验证 Energy before==after；禁止直接 `SetNumericAttributeBase` 退款。
- RNG 使用 stream copy 预览并注入 Spec；Apply 成功才提交正式 stream。任何失败或重复 Action 不推进正式 RNG 序列。
- 若退款 GE 自身无法应用、Damage 已改变 HP 后才报告失败、或致死 delegate 在事务完成前产生不可逆 BattleResult，立即停止并报告 `BLOCKING TRANSACTION GAP`；不得用 ensure 或“同步路径理论不可达”代替正确性。
- `CaptureFailed`、`InvalidCapturedValue`、`EffectApplicationFailed` 必须有受控 Development 注入点和动态证据。失败时 HP/Energy/SP/Turn/Result/RNG 均不变。

## 致死与 exactly-once

- 致死 Damage 只产生一次 BattleResult；终局优先于 Turn 推进、SP/RNG/Resolution 的最终发布顺序必须一致且可审计。
- 终局后新 Action 返回 BattleTerminal；不扣 Cost、不掷 Crit、不伤害、不推进 Turn。
- duplicate Action 返回同一 Resolution/Breakdown；不得再次扣 Energy/SP、再次伤害或再次触发死亡/返回。
- 补 P7-002 follow-up：overkill 必须证明 `FinalDamage > AppliedDamage`；同 ActionId across Reset 必须不命中旧 Battle cache；Harness FAIL 使用 Error verbosity。

## Implementation 精确 allowlist

- `Source/HSR/GAS/Ability/HSRAbilityTypes.h`
- `Source/HSR/GAS/Ability/HSRGameplayAbilityBase.h`
- `Source/HSR/GAS/Ability/HSRGameplayAbilityBase.cpp`
- `Source/HSR/GAS/Ability/HSRBasicAttackAbility.h`
- `Source/HSR/GAS/Ability/HSRBasicAttackAbility.cpp`
- `Source/HSR/GAS/Ability/HSRSkillAbility.h`
- `Source/HSR/GAS/Ability/HSRSkillAbility.cpp`
- `Source/HSR/GAS/Ability/HSRUltimateAbility.h`
- `Source/HSR/GAS/Ability/HSRUltimateAbility.cpp`
- `Source/HSR/GAS/Damage/HSRDamageTypes.h`
- `Source/HSR/Data/HSRSkillDefinition.h`
- `Source/HSR/Battle/HSRBattleCoordinator.h`
- `Source/HSR/Battle/HSRBattleCoordinator.cpp`
- `Source/HSR/Battle/HSRBattleGameMode.h`（仅 Development Harness/失败注入）
- `Source/HSR/Battle/HSRBattleGameMode.cpp`（仅 Development Harness/失败注入）
- `tasks/execution-result.md`

P7-002 的 AttributeSet、Execution、EffectContext、Globals、RuleDefinition 只读；若发现必须修订，停止请求 Coordinator 扩权，不得自行加入。

## 用户 Editor 资产白名单

由用户在 C++ 首次构建成功且收到精确指引后完成并单独提交：

- `Content/Data/Skills/DA_BasicAttack_Single_Test.uasset`
- `Content/Data/Skills/DA_Skill_Single_Test.uasset`
- `Content/Data/Skills/DA_Ultimate_Single_Test.uasset`
- `Content/GameplayEffects/BP_GE_P7_UltimateEnergyRefund.uasset`（新增通用 Instant GAS 退款 GE；精确值/SetByCaller 与 Cost 对账）
- `Content/Blueprints/Framework/BP_HSRBattleGameMode.uasset`（仅 P7-003 Harness/失败注入绑定）

三类 Skill Definition 统一指向 `BP_GE_P7_DamageExecution`、`DA_DamageRule_Default`、`Damage.Type.Physical`，分别配置冻结的 AbilityMultiplier。Heal DataAsset/GE 不修改。

旧资产 `BP_GE_P5_BasicAttackDamage`、`BP_GE_P6_SkillAttackDamage`、`BP_GE_P6_UltimateDamage` 只解除引用；不允许修改、删除、移动。使用 Reference Viewer/运行日志证明正式路径不再引用它们。

如退款需要新 SetByCaller Tag，仅允许用户在 `Config/DefaultGameplayTags.ini` 新增一个精确 `Damage.Data.EnergyRefund`；不得更改其他 Config。若无需该 Tag 则禁止产生 Config diff。

## 明确禁止

- 禁止修改 HealAbility、Heal DataAsset/GE、TargetPolicy、TurnManager、UI/ViewModel/Widget、AttributeSet、ExecutionCalculation、EffectContext、Globals、RuleDefinition、Build.cs、uproject、地图。
- 禁止 Wait/Pass、Basic/Skill/受击回能、每 Team SP 池、多角色/敌方资源 AI；禁止修改现有 Basic +1 SP、Skill -1 SP、Ultimate Energy Cost 行为。
- 禁止 Phase 8 弱点/抗性/韧性/击破、Phase 9 状态、Phase 10 完整 UI、AoE/多段/追击、动画/VFX/Cue、SaveGame、网络/复制/Prediction。
- 禁止直接写 HP/Energy、全局 RNG、FMath::Rand、Tick、公式复制、删除资产、reset/clean、stage/push 或夹带历史变更。

## 执行顺序

1. 首次只读复述任务、事务设计、allowlist、Editor 资产、延期资源边界与验证，停止等待确认。
2. 获确认后先建立公共 Prepared Damage/Result 接口与 Coordinator stream-copy 事务，编译通过后再迁移一个 Basic 垂直切片。
3. Basic 动态通过后迁移 Skill；保持 SP Reserve/Commit/Rollback。
4. 最后迁移 Ultimate，并实现/验证 GAS Energy Refund；任何半完成状态立即停止。
5. 加 Development-only 失败、overkill、Reset/duplicate 与终局 Harness，不污染 Shipping 正式入口。
6. 用户更新资产、关闭 Editor；运行 fresh `HSREditor Win64 Development -Rebuild`，再连续两轮 PIE 完整矩阵。

## 固定验证矩阵

- 三技能：各自非暴击、必暴击、正常非致死；日志证明都只使用 `BP_GE_P7_DamageExecution`。
- 资源：Basic 成功 SP +1；Skill 成功 SP -1；Ultimate 成功只扣一次 Energy；各失败/重复均不改变资源。
- Failure：Invalid Source/Target/Rule/Tag/Capture/Value/Spec/Apply；退款成功；HP/Energy/SP/RNG/Turn/Result 全对账。
- Exactly-once：每技能重复 ActionId；相同 Breakdown/Resolution，零二次副作用。
- Terminal：每技能致死、overkill、终局后提交、重复死亡回调；一次 BattleResult，终局后不推进 Turn。
- Lifecycle：同 ActionId across Reset、连续两轮 PIE、World teardown；无旧 cache/Context/ASC/Actor 引用。
- Regression：Heal 仍走 `BP_GE_P6_Heal`；P5/P6 Harness、命令面板、Energy/SP/Turn/返回路径无回归。
- 引用：Reference Viewer 与运行日志证明旧三个伤害 GE 已解除正式引用但资产仍存在。

证据必须包含 ActionId、BattleId、SkillId、Source/Target、seed/consume、captured values、roll/Crit、Raw/Final/Applied、HP/Energy/SP/Turn before/after、Resolution/Failure、BattleResult。

## 验收与交接

- [ ] 所有正式伤害只经统一 Execution；Heal 保持旧路径；旧伤害资产未删除。
- [ ] RNG stream-copy、SP 事务、Ultimate GAS refund、失败零副作用成立。
- [ ] duplicate/terminal/overkill/Reset follow-up 与三技能矩阵通过。
- [ ] fresh Rebuild 实际 UHT/C++/Link exit 0；连续两轮 PIE 无 Error/Ensure/GC/Blueprint Runtime Error。
- [ ] 资产作者、源码作者、Reviewer 与 Coordinator diff 分离。
- [ ] Reviewer 先落盘 `tasks/final-review.md`；未放行不得归档、创建 P7-004 或改变资源系统。

低级执行模型只更新 `tasks/execution-result.md`。固定提交消息：`低级执行模型+Implementation Agent+TASK-P7-003/Phase 7+迁移三类技能到统一Execution伤害`。当前只等待复述和用户独立确认，不自动实施。
