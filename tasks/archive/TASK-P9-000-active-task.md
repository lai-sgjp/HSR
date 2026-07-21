# TASK-P9-000：TurnStarted / TurnEnded 纯值回合事件契约

状态：`COMPLETE — PASS WITH FOLLOW-UP`

## Role Lock / 角色锁定

执行者当前且仅能担任 `Implementation Agent / 低级执行模型`。不得切换为 Coordinator、Architect、Reviewer、Teacher 或其他角色；不得重写目标、扩大文件范围、推进 P9-001 或宣称 Phase 9 已实现。发现范围、生命周期或 UE5.6 API 歧义时立即停止，请求 Coordinator/用户扩权。

## 当前 Phase 与前置证据

- Phase 8 为 `Ready with inherited follow-ups`；P8-006 Reviewer 原始结论为 `PASS WITH FOLLOW-UP`。
- Git 校准证据：`HEAD == origin/main == 2c2abc200b683b31fd2337dcfa84edad16317a9e`。
- 用户已授权进入 Phase 9。该授权允许创建本契约，不替代执行者首次复述后的单独执行确认。
- 当前 `UHSRTurnManager::ResolveAction` 在推进队列后广播 `OnActionResolved(ResolvedId)`；没有语义明确的 TurnStarted/TurnEnded 公共纯值契约。

## 唯一目标

为现有 TurnManager 建立可审计、battle-local、exactly-once 的 `TurnStarted` / `TurnEnded` 纯值事件契约，并通过 opt-in Development harness 证明两轮顺序和失败路径；不实现 StatusComponent、Buff、Debuff、DoT、GE、GameplayTag、UI 或 Editor 资产。

## 冻结契约

1. `TurnStarted` 表示一个存活参与者已被选为当前行动者并可接收合法命令；初始化成功和每次合法推进后各广播一次。
2. `TurnEnded` 只在当前参与者的合法 `ResolveAction` 被接受后广播一次。非法、重复、非当前、无当前行动者、Finished 后调用均不广播。
3. 合法行动顺序固定为：验证当前行动者 → 广播该行动者 `TurnEnded` → 推进/选择下一位 → 广播下一位 `TurnStarted`。
4. 死亡/失效参与者和 Break Delay 跳过不是已完成回合，不广播其 `TurnStarted` 或 `TurnEnded`；选到下一位合法行动者时只广播其 `TurnStarted`。
5. `FinishBattle` 不伪造 TurnEnded；进入 Finished 后不再广播回合事件。`Reset` 清空本轮身份/序号，不广播生命周期事件。
6. 事件载荷必须是纯值，至少包含 BattleEpoch、ParticipantId、TurnSequence 与事件类型；不得携带 Actor、ASC、Component、UObject 指针或可变引用。
7. 每次成功 `Initialize` 创建新的非零 BattleEpoch 并从确定的 TurnSequence 起点开始；旧 Epoch 事件不能被新一轮误认。使用本地递增 `uint64`，不得依赖 wall-clock。
8. 现有 `OnActionResolved` 若保留，必须明确为兼容通知且不作为 Phase 9 状态生命周期真源；若无法安全兼容，停止并请求审查。

## 执行前必须读取

- `tasks/active-task.md`
- `Source/HSR/Battle/HSRTurnManager.h`
- `Source/HSR/Battle/HSRTurnManager.cpp`
- `Source/HSR/Battle/HSRBattleGameMode.h`
- `Source/HSR/Battle/HSRBattleGameMode.cpp`
- `Source/HSR/Battle/HSRBattleTypes.h`（只读）

不得自行读取全局规划以补充隐含需求。

## 允许修改文件

- `Source/HSR/Battle/HSRTurnManager.h`
- `Source/HSR/Battle/HSRTurnManager.cpp`
- `Source/HSR/Battle/HSRBattleGameMode.h`
- `Source/HSR/Battle/HSRBattleGameMode.cpp`
- `tasks/execution-result.md`

未列出的文件默认禁止修改。需要新增类型文件、修改 Coordinator、Build.cs、Config 或资产时必须停止并请求扩权。

## 禁止范围

- 禁止修改 `Config/`、`Content/`、`.uproject`、模块依赖、现有资产或 GameplayTags。
- 禁止创建 StatusComponent、StatusDefinition、StatusInstance、Buff、Debuff、DoT、Infinite GE、免疫、驱散、叠层、刷新或 UI。
- 禁止改变伤害、Break、Delay 数值规则、行动排序、技能、资源、胜负或 Save/网络语义。
- 禁止 Tick、Timer、world scan、Actor/ASC 指针事件载荷、静态跨 PIE 状态或 CDO 可变状态。
- 禁止无关重构、删除/覆盖历史、批量格式化、`git reset`、`git clean`、stage、commit 或 push。

## 实施与验证

1. 只读确认 TurnManager 当前 Initialize/Resolve/Advance/Delay/Finish/Reset 顺序和 GameMode 现有 opt-in harness 形式。
2. 在 TurnManager 内定义最小纯值事件、只读 delegates、BattleEpoch/TurnSequence 与明确广播点。
3. 保持现有排序、Break Delay、`OnActionResolved` 和调用方兼容；无法兼容则停止。
4. 在 GameMode 增加默认关闭的 `Development|P9` opt-in harness；不得要求创建新 Editor 资产。
5. 运行 fresh `HSREditor Win64 Development` Build，头文件改变时必须有真实 UHT、HSR C++ 与 Link 证据；保留首个真实错误。
6. 用户在 Editor 中 opt-in 运行 harness 并回传两轮日志；执行者不得冒充用户 PIE 证据。
7. 仅将实际结果写入 `tasks/execution-result.md`。

## 必须失败矩阵

| Case | 预期 |
|---|---|
| 初始化两名存活参与者 | 首位仅一次 TurnStarted；无伪造 TurnEnded |
| 当前行动者合法 Resolve | 当前一次 TurnEnded，随后下一位一次 TurnStarted，顺序严格 |
| 非当前参与者 Resolve | 返回失败；事件计数和序号不变 |
| 同一参与者重复 Resolve | 第二次拒绝；无重复 TurnEnded |
| 当前参与者失效/死亡后推进 | 被跳过者无 Started/Ended；下一合法者一次 Started |
| Break Delay 跳过候选 | 被跳过者无 Started/Ended；下一合法者一次 Started |
| FinishBattle 后 Resolve/Delay | 保持既有拒绝语义；无生命周期事件 |
| Reset | 不广播；重新 Initialize 使用新 Epoch、序号重启 |
| 两轮连续 Initialize/Resolve/Reset | Epoch 可区分，无重复绑定或迟到广播 |
| 空参与者 Initialize | 失败且无事件 |

若现有 API 无法安全构造死亡/Delay/两轮用例，只允许使用现有 Development seam；不得暴露 Shipping 作弊接口。

## 验收标准

- [ ] 事件载荷纯值且包含非零 BattleEpoch、ParticipantId、TurnSequence、事件类型。
- [ ] 广播顺序与冻结契约一致，合法事件 exactly-once。
- [ ] 失败矩阵逐项输出结构化 PASS/FAIL 和关键计数，不以总 PASS 代替。
- [ ] 原有排序、Resolve、Break Delay、Finish、Reset 行为无回归。
- [ ] Fresh Build 覆盖真实 UHT/C++/Link；Editor/PIE 未运行前明确记为 pending。
- [ ] 无白名单越界、Config/Content 变更、Tick/Timer 或 Git 写操作。

## 执行者首次响应门禁

首次读取本卡后，执行者只能只读复述任务编号、唯一目标、拟修改文件、事件顺序、失败矩阵、Build/Editor 验证、禁止范围和风险，然后停止。最后必须原样写：

`等待用户确认执行 TASK-P9-000。`

在用户随后单独明确确认前，不得调用任何实现或写入工具；确认只适用于 `TASK-P9-000`，不得外推到 P9-001。

## Git 交付

- 本任务当前未授权执行者 stage/commit/push。
- 后续如获得角色提交授权，必须先核对 status、diff、allowlist 与派生产物；Phase 中途不得 push。
