# PHASE5 详细执行计划：1v1 回合战斗闭环

## 1. 阶段定位与门禁

PHASE5 的唯一用户可见结果是：玩家从 PHASE4 的 Encounter Context 进入独立 Battle Map，系统按稳定 ID 重建 1 名玩家与 1 名敌人及其 ASC，按确定性 Speed 顺序交替行动，执行一次固定伤害普攻，在死亡后只结算一次胜负与 `BattleResult`，并返回探索地图的原始 Transform。

最终状态：P5-001～P5-004 已完成并归档，Phase 5 为 `Ready with inherited follow-ups`。P5-002～P5-004 的 PIE/运行证据主要为 `USER PROVIDED`；P5-005 当前 Build 命令 exit 0 但目标 up-to-date，未升级为新的 fresh UHT/C++/Link 证据。未由 Reviewer 亲自运行的失败/生命周期矩阵、历史 mixed commit 和 `Map_Battle.umap` 资产归属继续作为 follow-up。

## 2. 不变量与边界

- 只支持单机、1v1；数据结构可容纳队伍，但本阶段不实现队伍 UI 或多人。
- `DataAsset` 只保存静态定义；Battle runtime 状态由 Coordinator/Participant/TurnManager 持有；Transition 只保存纯值 DTO。
- 不跨 World 保存 Actor、ASC、GameplayEffectHandle、Widget 或裸 UObject 指针。
- 核心规则由 C++ 事件/状态机驱动，不使用 Tick、每帧扫描或 Level Blueprint 作为规则源。
- GAS 负责能力激活与效果应用，TurnManager 负责谁能行动、何时结算和回合推进。
- Encounter、BattleResult、ReturnContext 均必须 exactly-once consume；失败路径必须可恢复，不得留下 Pending 卡死。
- Phase6+ 的复杂技能、目标系统、Cost/Cooldown/Energy、AI、战斗 UI、SaveGame、RPC/Replication/Prediction 均为非目标。

## 3. 工作包与顺序

### P5-000：门禁复核与任务卡锁定

依赖：PHASE4 收尾证据。产出：当前状态快照、分支/回滚点、每个工作包的精确 allowlist 与非目标清单。核对 `todo_plan.md`、`worklog.md`、README、roadmap、PHASE4 文档及 Build/UHT/PIE 证据。若 P4 未完成，先补 P4 收尾，不进入实现。

### P5-001：战斗运行时骨架与 Context 重建

产出：`BattleParticipant` 纯运行时 DTO/对象、`BattleCoordinator`、`BattleGameMode`（以及确有必要的 Transition 扩展）。消费 Encounter Context，保存返回 Map/Transform，创建 1v1 Actor 与 ASC，完成 `InitAbilityActorInfo`，清理旧 World 引用。

验收：同一 RequestId 只能进入一次；缺失定义/地图加载失败可回退；双方 Actor/HP/Speed 可读；连续 PIE 不复用旧对象。禁止在本包实现 Turn、Ability 或奖励。

### P5-002：TurnManager 与确定性排序

产出：参与者注册、Speed 降序队列、稳定 tie-break（initiative 后按 stable participant ID）、当前回合与 `ActionResolved` 事件；显式状态机 `Waiting → BuildOrder → PlayerTurn/EnemyTurn → Resolve`，无 Tick。

验收：Player/Enemy/Neutral、同速、多次重建均产生相同顺序；非当前行动者、重复提交、已死亡参与者的命令被拒绝且不改变状态。

### P5-003：最小普攻与固定伤害

产出：基础 `GameplayAbility`、合法目标检查、固定伤害 GameplayEffect/既有属性入口；Coordinator/TurnManager 只发起命令，GAS 完成能力与效果应用。

验收：一次输入只产生一次 ActionResolved；固定伤害正确扣减 HP；无效目标、能力未授予、重复激活均有可观测日志；不得直接从 UI/Level BP 修改属性。

### P5-004：死亡、胜负、BattleResult 与返回

产出：HP<=0 的单次死亡处理、Victory/Defeat 判定、纯值 `BattleResult`、一次性消费标记、战斗对象清理、返回探索 Map/Transform；奖励只保留占位 DTO。

验收：玩家胜利、玩家失败、同时死亡竞态、重复 Resolve、重复返回、Actor 销毁、旅行失败均有明确结果；返回后无跨 World 引用且原 Transform 恢复。

### P5-005：阶段收尾与独立审查（已完成）

产出已归档于 `tasks/archive/TASK-P5-005-*`；Reviewer `PASS WITH FOLLOW-UP`，Teacher `PASS WITH FOLLOW-UP`。阶段标记为 `Ready with inherited follow-ups`，不把用户证据升级为 Reviewer 动态通过。

## 4. 每个工作包的固定执行节奏

1. 先写不变量、状态转移和数据所有权；2. 锁定 allowlist 与非目标；3. 实现最小 C++ 垂直切片；4. 在 UE Editor 完成必要的 Map/DataAsset/Blueprint 绑定；5. 做 Build/UHT/PIE 与至少一条失败路径；6. 记录首个真实错误、修复和证据；7. 由 Reviewer 复核后再进入下一包。

## 5. 教学与能力检查

必须掌握：World 生命周期、UObject Outer/GC、ASC Owner/Avatar/ActorInfo、TurnSystem 与 GAS 职责边界、稳定排序、DTO 与 exactly-once 消费。暂时了解：队伍扩展、复杂 AI、UI ViewModel、网络权威。后续深入：Cost/Cooldown、ExecutionCalculation、状态效果、复制与预测。

每包结束完成一个练习：画数据流/状态机、解释一个失败日志、写出一个边界测试，并更新 `learning-journal.md` 与 `interview-notes`。评分重点是能否说明“谁拥有状态、谁触发事件、谁负责验证、失败如何恢复”。

## 6. 验收矩阵

| 类别 | 必测场景 | 证据 |
|---|---|---|
| 构建 | Development Editor fresh Build、UHT、Link | 命令、退出码、首个 warning/error |
| 正常流 | Encounter→Battle→双方行动→胜负→返回 | 日志、PIE 录像/截图、Transform |
| 数据边界 | 缺失 ID、缺失地图、空 Context | 错误日志与可恢复结果 |
| 回合 | 同速、重复命令、非当前角色、死亡后命令 | 稳定顺序与状态断言 |
| GAS | 能力未授予、无效目标、固定伤害 | 激活/拒绝/效果日志 |
| 生命周期 | Actor 销毁、旅行失败、连续两轮 PIE、Editor 重开 | 无悬挂 Pending、无旧 World 引用 |

## 7. 风险与停止条件

重点风险是跨图 Actor/ASC 悬空、ASC ActorInfo 初始化时序、同速不稳定、死亡竞态、重复消费/返回、旅行失败悬挂和把 GAS/Turn/UI 责任混在一起。若需要新增模块、修改危险 Config、引入第三方资源、跨 Phase 扩展或超出 allowlist，立即停止并请求授权。任何“计划已列出”都不等于“代码已实现”或“阶段已通过”。

## 8. 文档与提交要求

每个工作包完成后更新 `worklog.md` 和真实进度对应的 `todo_plan.md`；可复用知识写入 `learning-journal.md`、`interview-notes` 或设计文档。阶段收尾前检查 `git status`、`git diff`、allowlist、派生产物与证据，记录 commit hash；PHASE 中途不 push，只有全部子任务完成并通过审查后才由协调者执行阶段收尾 push。

## 9. 下一步

现在唯一建议的下一任务是 **P5-000 门禁复核**：先确认 PHASE4 的 commit/push 与 Build/PIE 证据，再锁定 P5-001 的文件 allowlist。该任务完成前不开始 Battle C++ 实现。
