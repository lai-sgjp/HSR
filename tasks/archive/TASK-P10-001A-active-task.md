# TASK-P10-001A：敌方确定性基础攻击与技能展示文本边界

归档状态：`COMPLETE / PASS WITH FOLLOW-UP`（2026-07-23）

## 唯一可见结果

任一合法 Enemy 回合由 Coordinator 经既有唯一行动事务自动且恰好一次提交固定 BasicAttack；非终局结算后进入 Player 回合并发布新的 Command ViewState。技能按钮仅显示 authored 短 `DisplayName` 与成本；多行 `Description` 保存在 Skill Definition 并复制为纯值 ViewState，留给后续详情页。

## 冻结架构

- `TurnStarted` handler 只记录 pending key，禁止直接提交、生成 ActionId 或推进 Gameplay。
- canonical key 为 `BoundTurnManagerIdentity + BattleEpoch + TurnSequence + ParticipantId`；旧 manager 即使复用相同数值也拒绝。
- initial Enemy turn 在 Coordinator 完成 Spawned 组装后进入与 subsequent turn 相同的 queue/drain。
- 最外层行动事务完成校验、Commit/Rollback、Resolution、Turn advance、terminal、cache/LastResolution 和 ViewState publication 后，才允许非递归 drain。
- Drain 只进入唯一 `RequestActionCore` 事务，不复制 Ability、Damage、Resource 或 Turn 管线；同步失败不递归重试、不额外推进。
- Reset、Finished、manager replacement 和 teardown 成对解绑 TurnStarted handle，并清 pending/consumed 状态。
- formal action 成功推进且未终局时追加发布 post-turn ViewState；terminal 或 ResolveAction 失败不发布伪下一回合状态。
- 禁止 Tick、Timer、Delay、Widget/Blueprint 敌方提交、第二命令管线及 TurnManager 修改。

## Development-only seam

为验证 old-manager same-epoch 与 3+ participant/Delay 序列，只允许严格 `#if WITH_EDITOR`、非反射、非 Blueprint seam：显式 SourceManager + 纯值 lifecycle event 只能进入生产 record/reject 规则；getter 只读；隔离 manager 换绑必须在所有出口恢复生产 manager、唯一 delegate、pending 与 consumed 状态。Seam 禁止 drain、ActionId、RequestAction/Core、Turn 推进或 Gameplay mutation，默认 flag=false，Shipping 零 harness 符号。

## 文件与作者边界

Implementation 范围：

- `Source/HSR/Battle/HSRBattleCoordinator.h/.cpp`
- `Source/HSR/Battle/HSRBattleGameMode.h/.cpp`，仅默认关闭 Development harness
- `Source/HSR/Data/HSRSkillDefinition.h`
- `Source/HSR/UI/HSRBattleCommandWidget.h/.cpp`
- 根 `tasks/execution-result.md` 的 001A 记录

User Editor 范围：四个 Skill DataAsset、`BP_HSRBattleGameMode.uasset` 与 `WBP_BattleCommandPanel.uasset`。Reviewer、Teacher、Coordinator 各自只认领审查、教学和契约/归档 Markdown。P10-001 与 001A 的共享 Source/资产提交按角色整体交付，不强行拆分交织 hunk。

## 验证与结论

- Development harness 覆盖 dispatcher/core depth、旧 manager identity、duplicate key、生产绑定恢复、四参与者三个连续 Enemy turn 与真实 Break Delay skip，并由用户日志证明 `Harness=COMPLETE`。
- 最终 fresh Build 完成相关 C++、HSR lib/dll Link、metadata，exit `0`；历史 `UE_LOG` verbosity 首错与所有修订保留于 execution archive。
- harness=false 两局正常 PIE：Enemy queue/dispatch `16/16`；第一局 Victory、第二局 Defeat；两局 Widget 各 bind/unbind 一次，玩家连续提交 `8/9` 次，非终局 Enemy 后均恢复 Player 输入。
- 运行/资产证据保持 `USER PROVIDED / REVIEWER LOG INSPECTED`，不冒充 Reviewer 亲自运行 PIE或反序列化 `.uasset`。
- Independent Reviewer：`PASS WITH FOLLOW-UP`。
- Teacher：`tasks/p10-001a-teacher-review.md`，`USER ANSWERED / TEACHER ASSESSED`，`3/3 mastered`。

## Follow-up 与授权边界

既有 GameplayCue paths、NavMesh/CrowdManager、return/encounter guard warnings 继续保留；角色 commits 与 Phase 10 整体交付仍待后续闭合。本归档只关闭 P10-001A，不完成 P10-001，不授权 P10-002。
