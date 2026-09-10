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
| **多路缓存去重** | `ProcessedOperationIds`（状态操作 GUID）/ `ProcessedInvalidSources`（纪元\|来源）两路拦截 |

### 已知问题

- `ActiveStatus` 目前硬编码了 `Status.Buff.AttackUp`，不是通用方案，将来加新 Buff 时需要重构

---

## TurnManager 回合管理器

### 核心职责

`UHSRTurnManager`（UObject，战役本地）是事件驱动的**行动距离调度器**：不给角色排固定顺序，而是给每个参与者一条"行动距离"（Action Distance），谁的距离先耗尽谁行动。永不 Tick。

### 行动距离模型

```
基础行动距离 = MaximumBaseActionDistance / Speed   （速度越快，基准距离越短）
剩余行动距离 = 距下次行动的进度（行动一次后充值回基础距离）
行动顺序     = 剩余距离最小者先行动；全体减去该最小值 = 时间推进
```

- 速度由 ASC 的 `Speed` 属性实时读取（`MakeBaseActionDistance`），非法值直接初始化失败
- 初始化按 ParticipantId 字典序稳定排序 + 去重（同速时顺序可预测）；`InitiativeSpeed` 已降级为诊断快照
- **速度变化实时生效**：`BindSpeedDelegates` 为每个参与者绑定 Speed 属性变化委托，`HandleSpeedChanged` 按新旧基准等比例换算剩余距离，不打断当前进度

### 回合推进（AdvanceToNextValidTurn）

```
① 收集所有 IsParticipantTurnEligible 的参与者（唯一资格判定入口 = 存活 IsAlive）
② 找出剩余距离最小的 → 全体减去该最小值（时间推进到下一行动时刻）
③ 同速者按 ID 字典序打破平局
④ 设为当前回合 → State 置 PlayerTurn/EnemyTurn → TurnSequence++ → 广播 TurnStarted
```

剩余距离出现非有限值 → 数值错误，直接 Finished。

### 行动结算（ResolveAction）

```
ResolveAction(当前行动者)
  → 广播 TurnEnded
  → ApplyCurrentPendingAfterRecharge：剩余距离充值回基础距离 + 应用挂起的调整
  → AdvanceToNextValidTurn 推进下一回合
  → 广播 ActionResolved
```

### 行动距离调整（拉条 / 延后）

`RequestActionDistanceAdjustment(Request)` 是拉条/延后的通用入口，纯值 DTO（`FHSRActionDistanceRequest`）：

- `Advance`（推进/拉条）：剩余距离减去 `BaseActionDistance × Ratio`
- `Delay`（延后）：剩余距离加上 `BaseActionDistance × Ratio`
- **正在行动者**的调整不立即生效——挂进 `PendingPostActionOperations`，行动结束后随充值一并应用（先做有限性预演防数值爆炸）
- 带幂等（`ConsumedOperationIds` 按 OperationId 去重）与纪元校验，拒绝结果以 `FHSRActionDistanceResult` 返回（可验证/可重放）

### Break 延后

`ConsumeBreakDelay` 现在是 P8 兼容桥：把 `FHSRTurnDelayRequest` 转成 `Kind=Delay, Ratio=1.0` 的距离调整——破韧把目标**延后一整段基础行动距离**，不再有独立的"跳一回合"状态。另有 `ConsumeAdmittedBreakDelay` 处理"伤害即死亡 + 破韧同帧"的例外。

### 回合条预测（BuildTurnForecast）

`BuildTurnForecast(SlotCount)` 在**副本上模拟**未来行动顺序（只读，不改真实状态），产出 `FHSRTurnForecastEntry[]`（SlotIndex / DistanceUntilAction / bRepeatAction），供回合条 UI 消费。第 0 格固定为当前行动者；速度变化、拉条/延后会立刻反映在预测上。

### 事件

```
TurnStarted（携带 BattleEpoch + TurnSequence + ParticipantId）→ 状态系统消费
TurnEnded   → 行动结算后广播
ActionResolved → 每次行动成功结算后广播
BattleEpoch → 每次新战斗递增（纪元隔离：重置后旧事件不污染新战斗）
TurnSequence → 每次推进回合递增（防同一回合重复消耗）
```

### 状态机

```
Waiting → PlayerTurn / EnemyTurn（循环交替） → Finished
```

- `EHSRTurnManagerState`：Waiting / PlayerTurn / EnemyTurn / Finished
- `FinishBattle()`：清空挂起操作、解绑速度委托、置 Finished

### 已实现

- ✅ 速度变更实时生效（速度委托绑定，加速/减速即时调整行动距离）
- ✅ 拉条 / 立即行动（`RequestActionDistanceAdjustment` Advance）
- ✅ 回合条预测（`BuildTurnForecast`，纯值 DTO）

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
  └── 行动延后（TurnManager->ConsumeBreakDelay）
       → 转成 Delay, Ratio=1.0 的距离调整：目标延后一整段基础行动距离
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
