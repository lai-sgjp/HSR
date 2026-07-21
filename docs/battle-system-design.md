# Battle System Design

## 设计目标

第一阶段使用独立 Battle Map 实现确定性 1v1 回合战斗。探索地图通过 `UHSRBattleTransitionSubsystem` 保存仅含稳定 ID、地图和 Transform 的 Encounter/Return Context；这些纯值只有在 GameInstanceSubsystem 等跨 World 服务中托管时才能跨 `OpenLevel` 暂存，不能把旧 World Actor 当作跨图状态。Battle Map 重建参与者 Actor 与 ASC。系统应在没有正式动画、VFX 或网络的情况下独立完成规则验证。

## 职责边界

| 子系统 | 职责 | 不负责 |
|---|---|---|
| Encounter | 从探索进入战斗，提供参与者 | 伤害公式和行动顺序 |
| TurnSystem | 状态、行动顺序、当前行动者、行动提交和结算 | 属性存储和 UI 表现 |
| GAS | Ability、Attribute、Effect、Tag | 完整回合流程 |
| Battle Participant | 暴露战斗身份、ASC 和可行动状态 | 全局战斗状态 |
| Enemy Decision | 为当前敌人选择确定性行动 | 修改回合状态真源 |
| UI | 显示状态、提交玩家选择 | 直接决定伤害和胜负 |
| Reward | 胜利后发放一次固定奖励 | 决定战斗胜负 |
| Battle Transition | 在探索/Battle Map 间传递纯数据 Context 和 Result | 保存跨地图 Actor、ASC 或 GE Handle |

## 第一阶段状态

1. `Exploration`
2. `EnteringBattle`
3. `BuildingTurnOrder`
4. `WaitingForPlayerAction`
5. `ChoosingEnemyAction`
6. `ResolvingAction`
7. `CheckingBattleResult`
8. `BattleWon` / `BattleLost`
9. `ReturningToExploration`

状态名是设计语义，不在创建代码前锁定为最终公开枚举名。

## 行动顺序

- 初始按 Speed 从高到低排序。
- 相同 Speed 使用稳定参与者 ID 打破平局，确保结果可复现。
- 第一阶段每个行动者完成一次行动后重新进入循环。
- 死亡参与者不能进入行动阶段。
- Ability 激活失败必须返回可恢复状态，不能永久停留在 ResolvingAction。

## 基础攻击数据流

1. TurnSystem 确认当前行动者。
2. 玩家 UI 或敌人决策提交目标与基础攻击意图。
3. TurnSystem 验证行动者、目标和战斗状态。
4. ASC 尝试激活基础攻击 Ability。
5. Ability 向有效目标应用瞬时伤害 Effect。
6. AttributeSet 处理 HP 边界。
7. HP Delegate 更新 UI；战斗事件通知 TurnSystem 行动完成。
8. TurnSystem 检查死亡和胜负，再推进顺序。

## 第一阶段不做

- 多人队伍、多目标技能、插队、行动延迟和复杂速度条。
- 复杂元素、弱点、韧性、击破和持续回合状态。
- 实时秒数冷却、客户端预测和网络权威。
- 依赖 Montage 或 GameplayCue 完成才能推进的规则。

## MVP 战斗子集

MVP 战斗只包含：

- 1 名玩家和 1 名敌人；队伍容器使用数组但不启用多角色。
- Health、MaxHealth、Speed。
- 初始化 GameplayEffect。
- 一个无 Cost 的基础攻击 GameplayAbility。
- 一个瞬时固定伤害 GameplayEffect。
- Speed 降序和稳定 ID 同速裁决。
- 确定性敌人基础攻击。
- ActionResolved、死亡、Victory/Defeat、清理和返回探索。

MVP 明确推迟 Attack、Defense、Crit、ExecutionCalculation、Skill、Ultimate、Element、Break、Buff/Debuff 和中途战斗存档。

## 不变量

- 同一时刻只有一个当前行动者。
- 非当前行动者不能提交行动。
- 每次有效行动最多结算一次。
- 胜负结算后不接受新行动。
- 奖励最多发放一次。
- UI 和表现对象不是战斗状态真源。
- 跨地图 Context 不保存 Actor、Widget、ASC 或 Active GameplayEffect Handle。
- Encounter 和 BattleResult 最多各消费一次。

## Phase 6 战斗命令与资源边界（收尾草案）

- Battle UI/ViewState 是事件驱动只读观察层；Widget 不 Tick、不持有权威 ASC/Actor，也不直接写 HP、Energy、战技点或 Turn。
- UI 提交稳定 ID Command；BattleCoordinator 重验当前行动者、SkillDefinition、目标和资源，并发布结构化 Resolution。
- TurnManager 只在成功 Resolution 后推进。Ability/GE 不直接拥有全局回合状态。
- Energy 属于单个 ASC，由 GAS Cost GE 管理；队伍战技点由 Coordinator battle-local 事务管理，使用 ActionId 保证幂等。
- 当前四类占位路径为 BasicAttack、Skill、Ultimate、Heal。SingleEnemy、Self 已有动态 Harness；SingleAlly 仅静态支持。
- P6-004A 已用真实 `WBP_BattleCommandPanel` 闭合构造、stable-ID 提交、NativeDestruct/解绑和重建无重复 Delegate 的阻断。
- 真实 Rollback/并发、SingleAlly 动态路径、Heal/Ability GE 失败、目标销毁、终局与异步路径仍未完整动态验证，继续作为 Phase 6 inherited follow-ups；最小 WBP 不代表 Phase 10 完整 UI。

## Phase 7 伤害事务收尾（2026-07-20）

- 正式 Basic/Skill/Ultimate 统一使用 Execution GE；Heal 保持旧治疗 GE；旧固定伤害 GE 资产保留但不再是正式运行时引用。
- Coordinator 通过 prepared damage、RNG stream-copy、ActionId exactly-once、SP Reserve/Commit/Rollback、Ultimate GAS Cost/Refund 和 terminal 延迟维护事务顺序；失败不得由 Coordinator 直接写 Energy/Health。
- P7-004 五项动态矩阵已通过，但底层 Aggregator capture false/真实 NaN、自然 GAS ApplyFailure、Refund 自身失败、Cost 后 HP 异常和多轮 teardown 仍是 follow-up，不能外推为已验证。
