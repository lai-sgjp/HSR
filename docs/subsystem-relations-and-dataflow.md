# HSR 子系统关系图与数据流动图

本文基于 `docs/subsystem-architecture-and-usage.md` 与 `docs/system-operation-flow.md`，描述当前单机架构中各子系统的依赖关系、权威边界和典型数据流。

## 1. 子系统关系图

```mermaid
flowchart TB
    Def["静态定义层\nCharacter / Item / Equipment / Reward / Quest / Map DataAsset"]

    subgraph GI["GameInstance 持久运行域"]
      Profile["Character Profile\n角色等级/经验/技能等级"]
      Party["Party\n队伍槽位与成员"]
      Inventory["Inventory\nStack/Unique/Revision"]
      Equipment["Equipment / Relic\n实例、槽位、装备映射"]
      Reward["Reward\n奖励定义、Ledger、Receipt"]
      Settlement["Settlement Authority\n跨域结算协调"]
      Quest["Quest\n任务状态与目标事件"]
      Dialogue["Dialogue\n节点、选项、分支转发"]
      Map["Map\n位置、解锁、旅行请求"]
      Transition["Battle Transition\nEncounter/Return admission"]
      Save["Save\nEnvelope、校验、迁移、恢复"]
    end

    subgraph WORLD["World / Battle 临时运行域"]
      Interaction["Interaction Component\n当前候选 Actor"]
      Battle["Battle Coordinator\nTurn、Action、GAS、BattleResult"]
      Actor["Actor / Pawn / ASC\n角色运行时投影"]
    end

    subgraph UI["LocalPlayer UI 读模型域"]
      UIManager["UIManager / Router / ScreenStack"]
      VM["ViewModel\n只读 Snapshot 聚合"]
      Widget["Widget / HUD\n展示与提交 Intent"]
    end

    Def --> Profile & Inventory & Equipment & Reward & Quest & Dialogue & Map
    Profile --> Party
    Profile --> Actor
    Party --> Battle
    Inventory <--> Equipment
    Equipment --> Actor
    Interaction --> Transition
    Transition --> Battle
    Battle --> Settlement
    Settlement --> Profile & Inventory & Reward
    Dialogue --> Quest
    Quest --> Reward
    Map --> Transition
    Map --> UIManager
    Save --> Profile & Party & Inventory & Equipment & Reward & Quest & Map
    UIManager --> VM --> Widget
    Widget --> UIManager
    Widget --> Interaction
    Widget --> Dialogue
    Widget --> Map
    Widget --> Equipment
```

图中箭头表示“调用或读取关系”，不表示被调用方失去自己的数据所有权。每个 GameInstance Domain 仍是自己的唯一权威；Actor、ASC、Widget 都是投影或读模型。

## 2. 统一数据流动模型

```mermaid
flowchart LR
    Input["用户输入 / Widget"]
    Intent["纯值 Intent\nStable ID + OperationId"]
    Preflight["Authority Preflight\n上下文、权限、Revision"]
    Candidate["Candidate 构造\n不修改 live state"]
    Validate["完整校验"]
    Commit["一次权威 Commit"]
    Result["Typed Result\nRevision / TransactionId"]
    Event["Committed Event"]
    Snapshot["只读 Snapshot"]
    VM["ViewModel"]
    View["Widget 展示"]

    Input --> Intent --> Preflight --> Candidate --> Validate
    Validate -->|失败：零污染| Result
    Validate -->|通过| Commit --> Result --> Event --> Snapshot --> VM --> View
    Result -->|失败| VM
```

约束：Commit 前可以失败并保持 live state 不变；Commit 后 UI 失败只影响展示，不能回滚已提交业务状态。

## 3. 典型业务数据流

### 3.1 探索交互进入战斗

```mermaid
sequenceDiagram
  participant W as Widget/Input
  participant I as InteractionComponent
  participant T as BattleTransition
  participant M as Map/World
  participant B as BattleCoordinator
  participant S as SettlementAuthority

  W->>I: TryInteract(F)
  I->>T: EncounterIntent(EncounterId, RequestId)
  T->>T: 校验 pending/travel/conflict
  T->>M: 授权切换战斗地图
  M-->>B: ConsumePendingEncounter()
  B->>B: 构建 CharacterId/Definition/Profile/Equipment participants
  B->>B: Turn + GAS + Damage + Status
  B-->>S: BattleResult(BattleId, BattleResultId)
  S-->>T: ReturnContext + RewardReceipt
  T-->>M: ArrivalCommitId
```

### 3.2 战斗结算到奖励、背包和成长

```mermaid
flowchart LR
    BR["BattleResult"] --> SA["SettlementAuthority"]
    SA --> RC["Reward candidate / Receipt"]
    SA --> IC["Inventory candidate"]
    SA --> PC["Profile candidate\n经验/成长"]
    RC --> Commit["冻结并一次提交"]
    IC --> Commit
    PC --> Commit
    Commit --> Rev["Reward/Inventory/Profile revisions"]
    Rev --> Summary["Reward Summary ViewModel"]
    Rev --> Character["Character ViewModel"]
```

### 3.3 装备变更到 ASC 投影

```mermaid
flowchart LR
    Select["选择 ItemInstanceId"] --> Intent["Equip Intent\nCharacterId + Slot + OperationId"]
    Intent --> Preflight["Inventory ownership +\nEquipment slot + Expected revisions"]
    Preflight --> Candidate["Equipment replacement candidate\nsource-effect delta"]
    Candidate --> ECommit["Equipment authoritative commit"]
    ECommit --> ASC["重建 ASC equipment source handles"]
    ECommit --> Snap["Equipment/Profile snapshot"]
    ASC --> VM["Character ViewModel"]
    Snap --> VM
```

### 3.4 地图旅行与 UI 重建

```mermaid
sequenceDiagram
  participant W as Map Widget
  participant M as MapSubsystem
  participant U as UIManager
  participant L as Level/World
  participant V as ViewModels

  W->>M: RequestTeleportTravel(TeleportId)
  M->>M: 校验 unlock/current/pending
  M->>U: PrepareExplorationTravel()
  U->>U: teardown old HUD/route/focus
  M->>L: authorized travel
  L-->>M: MapId + ArrivalCommitId
  M->>M: commit current location
  M->>U: Register new exploration host
  U->>V: 重新读取 committed snapshots
```

### 3.5 Save / Load 全局恢复

```mermaid
flowchart LR
    SaveIntent["Save Intent"] --> Capture["捕获已提交 Domain snapshots"]
    Capture --> Encode["Canonical encode + checksum"]
    Encode --> Disk["staging -> readback -> backup/primary"]
    LoadIntent["Load Intent"] --> Decode["Primary/Backup decode + migration"]
    Decode --> Prepare["各 Domain PrepareRestore"]
    Prepare --> Global["SaveSubsystem 全局校验"]
    Global --> Commit["一次 CommitRestore"]
    Commit --> Rebuild["Map/Actor/ASC/UI 从 snapshot 重建"]
```

## 4. 跨系统数据对象

| 数据对象 | 主要流经系统 | 用途 |
|---|---|---|
| `CharacterId` | Profile → Party/Equipment/Battle/Actor/UI | 稳定角色身份 |
| `ItemDefinitionId` / `ItemInstanceId` | Definition → Inventory/Equipment/Reward | 区分物品规则与实例 |
| `OperationId` | UI/Interaction → Authority | 用户意图去重与冲突检测 |
| `EncounterRequestId` / `BattleId` | Interaction → Transition → Battle | 战斗准入与 exactly-once |
| `RewardTransactionId` / `ClaimId` | Battle/Quest → Reward/Settlement | 奖励幂等与收据 |
| `MapId` / `ArrivalCommitId` | Map/Transition → World/UI | 跨地图到达确认 |
| `Revision` | 各 Domain → ViewModel/Save | 并发校验与快照刷新 |
| `SaveId` / `Generation` | Save ↔ 全部持久 Domain | 存档代际、校验和恢复 |

## 5. 阅读要点

- Widget 不直接修改 Gameplay、Inventory、Equipment、Save，也不直接 `OpenLevel`。
- 跨两个以上 Domain 的原子业务由 `SettlementAuthority` 或 `SaveSubsystem` 协调。
- World Actor、Pawn、ASC 和 HUD 可销毁并重建；持久状态只通过稳定 ID、纯值请求和 Snapshot 传递。
- 所有失败路径都应保持零污染，并通过 typed result、revision 和事件让 UI 重读权威快照。
