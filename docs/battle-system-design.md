# Battle System Design

## 设计目标

第一阶段使用独立 Battle Map 实现确定性 1v1 回合战斗。探索地图通过 `UHSRBattleTransitionSubsystem` 保存仅含稳定 ID、地图和 Transform 的 Encounter/Return Context；Battle Map 重建参与者 Actor 与 ASC。系统应在没有正式动画、VFX 或网络的情况下独立完成规则验证。

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
