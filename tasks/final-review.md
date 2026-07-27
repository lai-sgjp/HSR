# TASK-P17-PATCH-01C Task Gate Re-review

## 审查对象

- 任务编号：`TASK-P17-PATCH-01C`
- 任务名称：Complete Action-Distance Turn Model
- 审查角色：Prompt Reviewer + Safety Reviewer（仅实施前 Task Gate）
- 审查日期：2026-07-27
- Coordinator 修订提交：`7036aa5`
- 结论：`REVISE`

## 审查输入与证据等级

- 一级：`.agents/agents.md` 第 38～44 节、修订后的 `tasks/active-task.md`、`tasks/review-template.md`、Patch 计划、roadmap 改进方向 3。
- 一级：当前 `HSRBreakTypes`、BattleParticipant、TurnManager、Coordinator、Status consumer 与 `HSRCombatPatchTests` 代码事实。
- 一级：PATCH-01B 已归档的 repeatable Break、same-frame deferred defeat、Status/Delay exactly-once 边界。
- 本轮仍是实施前契约审查；没有 Implementation、Build、Automation 或 PIE 完成证据。

## 原六项 Blocking 复审

| 原问题 | 复审结果 |
|---|---|
| 当前 actor 的 Advance 会在 Remaining=0 时丢失 | 已补充 post-action pending、冻结绝对距离和 Speed 改变示例；方向通过，但 pending 表示仍有下述内部矛盾。 |
| DTO/result/OperationId 消费边界不完整 | 已冻结字段、结果分类、全局 battle-local 去重域、非法请求不消费、其余拒绝首次观察即消费，以及 Break 映射；通过。 |
| zero/negative/NaN/Inf Speed 与算术失败未定义 | 已冻结初始化原子失败、有限值 clamp、运行中非法值零变化和 arithmetic failure；通过。 |
| Speed delegate 身份、解绑、同步顺序与重入 | 已冻结 identity/epoch/handle、Initialize/Finish/Reset、旧 callback、防 lifecycle 重入和 TurnEnded 后使用最新 Base 的顺序；合同通过。 |
| epsilon、Tie-break、TurnSequence/回合预算 | 已固定 epsilon 和 lifecycle 预算；方向通过，但候选公式仍有下述变量时点矛盾。 |
| Break Delay 双机制与 deferred-defeat 边界 | 已要求删除旧集合/skip、窄转发不得存状态，资格只由 Coordinator 同步传入且不进入 DTO；通过。 |

## 剩余必须修订项

| 严重度 | 修订文本 | 问题 | 必须采取的精确动作 |
|---|---|---|---|
| Blocking | Current actor adjustments：单个“累计的纯值 `PendingPostActionOffset`”与“按请求顺序应用、每次 Advance clamp” | 单个累计标量不能保存操作顺序，而 clamp 使顺序可观察。例如 recharge=10：`Advance 25 → Delay 30` 得 30；`Delay 30 → Advance 25` 得 15；两者累计 offset 都是 +5。当前文字同时要求标量累计和逐项顺序，无法实现唯一结果。 | 二选一并冻结：推荐保存 battle-local 有序 pending operation 列表，每项在接收时固化 signed absolute distance，Resolve 按列表顺序逐项应用并在每个 Advance 后 clamp；结果快照记录 pending count 与 signed total 仅作诊断。若坚持单标量，则必须取消逐项顺序语义并冻结统一一次 clamp 的不同合同。Required matrix 增加上述低 recharge 的反序用例。 |
| Blocking | Candidate selection：先减去严格 Min，随后候选使用 `abs(Remaining-Min)<=epsilon` | 若 Min 是归一化前的值，减法后候选 Remaining 约为 0，再与旧 Min 比较会错误；若 Min 被改成 0，文字没有说明。 | 冻结无歧义两阶段算法：先保存 `MinBeforeNormalization`，以 `abs(OriginalRemaining-MinBeforeNormalization)<=epsilon` 建候选集合；再对全部 eligible 执行 `Remaining=max(0, Remaining-MinBeforeNormalization)` 并 snap，最后仅从已保存候选集合按 ParticipantId 选择。也可先归一化再以 `abs(Remaining)<=epsilon` 建集合，但必须只保留一种表述并测试 Min 明显非零的 tie。 |
| Blocking | Required matrix：Initialize 部分绑定失败原子回滚；allowlist 没有可自然制造 AddUObject 中途失败的输入 | 全量 participant/ASC 验证后，delegate `AddUObject` 通常没有可控“第 N 个绑定失败”条件；若无 seam，这条只能伪造或无法稳定执行。任务卡目前只明确允许距离 snapshot seam。 | 在现有 TurnManager h/cpp allowlist 内明确允许一个仅 `WITH_DEV_AUTOMATION_TESTS` 的绑定失败注入/fixture seam（非反射、非 Blueprint、非 Shipping、默认关闭），用于指定第 N 个绑定前失败并断言此前 handles 全部解绑；或删掉“部分绑定失败”矩阵，只保留可真实构造的 Initialize validation failure。无需新增文件或扩权。 |
| Risk | ArithmeticFailure 与已接受 current pending 的未来 Resolve | 请求接受时 finite，不代表最新 Base recharge 与 pending 逐项组合在 Resolve 时必然 finite；Resolve 没有结构化 request result 可返回。 | 卡中补一句：接受 current pending 前必须预检其有序序列在合法 Base 范围（Base 最大 10000）下可安全应用；Resolve 不得因已接受请求产生非 finite 或丢失行动。若实现采用 double 中间值再验证 float 存储，也须 Automation 覆盖接近上界的拒绝零变化。 |

## 已确认的范围与安全边界

- `GetOrderedParticipants()` 已明确为稳定 registry/诊断视图，不承诺未来行动顺序；现有 Coordinator/UI DTO 不需改变。
- 四类新增 Automation 已写入矩阵；除“部分绑定失败 seam”外，当前 allowlist 足够且可执行。
- 不需要新增生产文件、Build.cs、Config、Content、GameplayTag、GE、DataAsset、Widget、正式行动条快照或反射接口。
- GameMode 仍只允许 fixture/既有 harness；不得借 PIE 测试创建资产或开发开关。
- 工作树中的 `learn/SaveSystem.md` 和 `.claude/**` 是既存用户/本地变更，本 Reviewer 不触碰、不暂存、不提交。
- Coordinator commit `7036aa5` 的 message 未采用 agents 第 41 节推荐/固定的四段 `角色+人格+任务阶段+产物` 格式；不影响本次合同内容判断，但后续 Role Commit 必须恢复规定格式并保留该 provenance 事实，不得改写历史掩盖。

## 审查结论

`REVISE`

原六项 Blocking 的实质边界已经补齐，allowlist 仍然充分；但 pending 的数据表示与顺序语义互相冲突，候选归一化公式的 Min 时点不一致，部分绑定失败测试缺少合法受控入口。三项均可由 Coordinator 在当前任务卡内精确修订，无需用户扩权。

**Implementation Agent 仍不得开始只读契约复述。** Coordinator 修订后应自动交回本 Task Gate 复审；只有复审为 `PASS` 后，Implementation 才可进入只读复述，实际实现仍须用户对 `TASK-P17-PATCH-01C` 单独确认。

## Git 交付

- Reviewer 仅提交 `tasks/final-review.md`。
- 不提交生产代码、活动卡、计划、状态文档、用户文件或 `.claude/**`。
