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

### 架构分层

```
① 韧性伤害计算（在 RequestActionCore 中，HP 伤害确定后才执行）
② Break 判断（是否触发 Break）
③ Break 效果（挂状态 + 行动延后）
```

### 韧性计算（后处理）

韧性计算是独立的后处理——即使失败也不回滚 HP 伤害。

```
检查技能元素（Element.Arc）
  → 构建对应的弱点 Tag（Weakness.Arc）
  → 检查目标 WeaknessTags 是否有精确匹配
    → 匹配 → 韧性伤害生效
    → 不匹配 → 韧性伤害为 0
  → 应用韧性伤害 GE（SetByCaller 传递伤害值）
  → 记录 ToughnessResult（Before, Damage, After, bReachedZero）
```

### Break 判断

同时满足所有条件才触发：

```
ToughnessResult.bReachedZero == true    // 韧性打到 0
&& ToughnessResult.Before > 0           // 之前有韧性
&& After ≈ 0                            // 几乎为 0
&& !Target->bBreakResultPublished       // 还没触发过 Break（整场只能一次）
&& 战斗还在进行中
&& 目标有效
```

### Break 效果

```
发布 BreakResult
  ├── 挂 Break 状态（RequestBreakStatus → Status.Debuff.Break）
  └── 注册行动延迟（TurnManager->ConsumeBreakDelay）
       → 目标下次轮到时不行动（跳过一次）
```

### 元素匹配过程

```
技能 ElementTag = "Element.Arc"
  ↓
构建弱点 Tag = "Weakness.Arc"
  ↓
检查 HasTagExact("Weakness.Arc") → 命中与否
```

元素和弱点是两套独立的 Tag 树，通过字符串替换（`Element.` → `Weakness.`）来匹配。

### 已知问题

- `bBreakResultPublished` 限制每个角色整场战斗只能 Break 一次
  但 HSR 原版设计中韧性可以恢复并再次 Break——这是未实现的

---

## TransitionSubsystem（地图往返）

### 完整流程

```
探索世界                         TransitionSubsystem              战斗世界
  │                                    │                            │
  ├─ RequestEncounter(定义) ──────────►│                            │
  │   生成 RequestId                    │                            │
  │   保存 ReturnTransform             │                            │
  │   记录 ExplorationMapPath          │                            │
  │                                    │                            │
  │  OpenLevel(BattleMap) ────────────────────────────────────────► │
  │                                    │                            │
  │                                    │  ├─ ConsumePendingEncounter
  │                                    │  ├─ BuildParticipants
  │                                    │  ├─ 战斗...
  │                                    │  └─  CombatResult
  │◄─ RequestBattleReturn(Result) ─────│◄───────────────────────────┤
  │  (ResolvedEncounterIds.Add)        │                            │
  │                                    │                            │
  │  OpenLevel(ExplorationMap)         │                            │
  │  ├─ ConsumeReturnContext            │                            │
  │  └─ 放回原来位置                    │                            │
```

### 为什么是 GameInstanceSubsystem

```cpp
UCLASS()
class UHSRBattleTransitionSubsystem : public UGameInstanceSubsystem
```

GameInstanceSubsystem 跨地图存活。如果放在 GameMode 或 World 上，OpenLevel 一调用就被销毁了。

### 状态机

```
Empty → Pending → Traveling → Consumed
```

| 状态 | 含义 | 操作 |
|---|---|---|
| Empty | 无请求 | RequestEncounter |
| Pending | 已请求，等待旅行 | 不可操作 |
| Traveling | 旅行中 | ConsumePendingEncounter |
| Consumed | 已消费，幂等保护 | 不可重复消费 |

### 返回数据流

```
RequestEncounter 时保存：
  → ExplorationMapPath（当前地图路径，Strip PIE 前缀）
  → ReturnTransform（角色的位置和朝向）

RequestBattleReturn 时使用：
  → OpenLevel(ExplorationMapPath)
  → ConsumeReturnContext 拿到 ReturnTransform
  → 放回原来位置
```

### 故障恢复

`HandleTravelFailure` 处理 OpenLevel 失败：

```
引擎 OnTravelFailure → 匹配 TravelTargetMap
  → 匹配 → 清除状态，允许重试
  → 不匹配 → 忽略（可能是其他系统的旅行）
```

### ResolvedEncounterIds 防重复

```cpp
TSet<FName> ResolvedEncounterIds;
```

战斗成功返回后，EncounterId 被加入此集合。同一次游戏中同一个遭遇不可重复触发。
