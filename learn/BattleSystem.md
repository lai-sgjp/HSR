# 战斗系统架构

> HSR 项目的非 GAS 战斗系统设计笔记
> 涵盖：BattleCoordinator、TurnManager、Break/Toughness、TargetingPolicy、Transition

---

## Coordinator 架构

### 核心职责

`UHSRBattleCoordinator` 是战斗系统的**中枢状态机**，拥有战斗参与者的完整生命周期。

### 状态机

```
Idle → Consuming → Spawned → [循环战斗] → Finished
                    ↑          |
                    └── Failed ←┘
```

| 状态 | 含义 | 转换条件 |
|---|---|---|
| Idle | 等待遭遇请求 | SubmitBattleRequest → Consuming |
| Consuming | 已接收请求，准备生成参与者 | BuildParticipants 成功 → Spawned |
| Spawned | 参与者已就绪，战斗中 | 全灭/胜利 → Finished；失败 → Failed |
| Failed | 初始化失败 | 不可恢复 |

### RequestActionCore 事务管道

命令执行的完整流水线（`HSRBattleCoordinator.cpp`）：

```
① 幂等拦截
   ProcessedActionResolutions 缓存已处理 ActionId
   重复请求直接返回上一次结果

② 前置验证（零副作用）
   - Coordinator 状态必须是 Spawned
   - 当前回合必须是攻击者的回合
   - 技能定义有效
   - 攻击者和目标有效
   - 目标未败北
   - 治疗目标不能已满血

③ 资源预扣
   ReserveSkillPoints → 在 TMap 中预留战技点

④ 事务执行
   bFormalDamageTransactionOpen = true
   TryActivateAbility → GAS 执行
   成功：CommitSkillPoints + CommitActionEnergyGain
   失败：RollbackSkillPoints（退战技点）

⑤ 韧性计算（独立于 HP 伤害的后处理）
   即使韧性计算失败，也不回滚 HP 伤害

⑥ 表现事件记录
   PresentationEvents 记录所有结果供 UI 消费

⑦ 自动处理敌人 AI
   DrainPendingEnemyTurns → 玩家行动结束后自动执行敌人回合
```

### 关键设计模式

| 模式 | 实现 |
|---|---|
| **幂等** | `ProcessedActionResolutions` 按 ActionId 缓存结果 |
| **事务** | `bFormalDamageTransactionOpen` 事务边界，失败时 Rollback |
| **先验证再执行** | 所有检查在第 419 行前完成（零副作用），之后的才是写操作 |
| **多路缓存去重** | `ProcessedStatusOperations` / `ProcessedInvalidSources` / `ProcessedAddOperations` 三路拦截 |

### 已知问题

- `ActiveStatus` 目前硬编码了 `Status.Buff.AttackUp`，不是通用方案，将来加新 Buff 时需要重构

---

## TurnManager 回合管理器

### 核心职责

事件驱动的回合调度器，永不 Tick。

### 初始化——按速度排序

```cpp
OrderedParticipants.Sort([](...)
{
    return Left.InitiativeSpeed > Right.InitiativeSpeed;
});
```

只排一次序，整个战斗以固定 Round-Robin 循环。

### AdvanceToNextValidTurn 扫描逻辑

```
从 CurrentTurnIndex+1 开始，绕一圈
对每个候选：
  ① IsParticipantTurnEligible？
     → Actor 有效且 HP > 0
     → 无效 → 跳过；如果有 BreakDelay 记录 → 消耗掉
  ② PendingBreakDelayActionIds 中有此角色？
     → 有 → 跳过（消耗，目标"丢一回合"）
  ③ 找到 → 设为 CurrentTurn，广播 TurnStarted
```

### Break 延后机制

```cpp
ConsumeBreakDelay(Request)
  → PendingBreakDelayActionIds[TargetId] = ActionId

AdvanceToNextValidTurn 扫描到 TargetId 时：
  → 检查 PendingBreakDelayActionIds
  → 有 → 跳过（消耗）→ 下次再轮到就正常
```

### 事件

```
TurnStarted → StatusComponent 扣回合数
TurnEnded   → 广播给 Coordinator 推进下一回合
BattleEpoch → 每次新战斗递增（标记这场战斗的数据）
TurnSequence → 每步递增（防同一回合重复消耗）
```

### 待实现

- 速度变更效果（加速/减速不重新排序，目前只读一次）
- 拉条（立即行动）——当前没有"插入到队列头部"的机制

---

## Break / Toughness 系统

待补充
