# HSR 总系统操作流程

> 状态：`PROPOSED / P17-PATCH-03 GATE 0`
> 基线：2026-07-28，P17-005 checkpoint commit `4ef49f7`。
> 范围：战斗、交互、角色养成、装备、背包、地图、存档与 UI 的单机可扩展协作合同。

## 1. 总原则

HSR 不建立一个可以任意修改所有数据的“万能管理器”。每个 Domain 保留唯一权威，跨系统协作只传递稳定 ID、纯值 Request/Result、单调 Revision 和类型化事件。Actor、Widget、ASC、GE Handle、Delegate Handle 与 World 指针不得跨地图或进入 Save。

统一操作形状：

```text
User/Input/Interaction
-> pure-value Intent
-> context and authority preflight
-> candidate construction and validation
-> one authoritative commit
-> typed Result + monotonic Revision/TransactionId
-> typed committed event
-> read-only ViewModel snapshot
-> Widget presentation
```

失败必须在 commit 前保持零污染；commit 后的展示失败不能回滚已经成功的业务事务，而应由 TransactionId/Revision 在 UI 重建后重新读取或恰好展示一次。

## 2. 生命周期与所有权

| 层级 | 典型对象 | 拥有什么 | 不拥有什么 |
|---|---|---|---|
| Static Definition | Character/Item/Equipment/Encounter/Map DataAsset | 原创静态规则、显示数据、软引用 | Runtime、Actor、存档实例 |
| GameInstance Domain | Profile、Party、Inventory、Equipment、Reward、Map、Save、BattleTransition | 跨 World Runtime、稳定 ID、Revision、事务状态 | Widget、地图 Actor、当前 ASC |
| World/Battle Runtime | InteractionComponent、Encounter Actor、BattleCoordinator、TurnManager、ASC | 当前 World 候选、Battle-local 状态、Actor 投影 | 持久化真源、跨 World 指针 |
| LocalPlayer UI | UIManager、ScreenStack、FrontendRouter、ViewModel | UI session、route、input/focus、只读快照订阅 | Gameplay commit、OpenLevel、Save DTO |
| Save Projection | Save DTO/envelope | 已提交 Domain 的纯值快照、schema、generation、checksum | Actor、Widget、ASC、Handle、pending UI route |

## 3. 稳定身份合同

- `CharacterId`：连接 CharacterDefinition、Profile、Party、Equipment owner、探索/Battle Actor 投影和 UI selection。
- `ItemDefinitionId` / `ItemInstanceId`：连接背包定义、实例、装备来源与奖励条目。
- `EquipmentSlot` / `EquipmentInstanceId`：连接装备事务和可逆属性来源。
- `EncounterId` / `EncounterRequestId` / `BattleId`：连接交互准入、Battle 创建、结果与返回。
- `RewardTransactionId`：奖励 exactly-once、Inventory mutation 与 Reward Summary receipt 的共同键。
- `MapId` / `TeleportId` / `ArrivalCommitId`：连接旅行请求、目标位置和新 World 到达确认。
- `SaveId` / `Generation` / domain revision：连接磁盘 lineage、恢复候选与幂等 Load。
- `OperationId`：只用于一次跨系统用户意图的去重和审计；不能替代任何 Domain 自己的事务 ID。

所有 ID 必须由权威 C++ 校验。Blueprint 只选择或展示 ID，不生成被当作成功依据的随机业务 ID。

## 4. Canonical Bootstrap

新档和读档必须共用一个确定性 Bootstrap 顺序：

```text
Load/register Definitions and catalogs
-> build or validate Profile candidate
-> build or validate Party candidate
-> validate Inventory + Equipment ownership candidate
-> validate Reward ledger and Map candidate
-> commit all accepted GameInstance domains
-> choose active CharacterId from Party
-> create/bind World Actor projection
-> rebuild ASC from Definition + Profile + Equipment/Relic sources
-> register Exploration HUD/UI host
-> ViewModels read committed snapshots
```

探索 Pawn 的存在不等于 Profile/Party 已存在。Pawn 是当前 World 中某个 `CharacterId` 的投影。空 Party、缺 Definition、错误 owner 或无效 Map 必须返回结构化 Bootstrap failure；不得由 Character Widget 硬编码 `Character.A` 修复。

## 5. 主操作流程

### 5.1 Exploration -> Interaction -> Battle

```text
Input F
-> InteractionComponent resolves current candidate
-> Interactable submits stable Encounter intent
-> BattleTransition validates no pending/resolved/travel conflict
-> stores pure-value EncounterRequest + return context
-> authorized map travel
-> BattleGameMode consumes request exactly once
-> rebuilds participants from CharacterId/Definition/Profile/Equipment
-> BattleCoordinator owns BattleId, turn and GAS resolution
```

目标销毁、候选过期、重复 F、旧 RequestId 或 pending travel 均不得创建第二场 Battle。

### 5.2 Battle -> Result -> Reward/Progression -> Return

```text
BattleCoordinator commits terminal BattleResult once
-> settlement preflight validates BattleId/ResultId/RewardDefinition
-> SettlementAuthority allocates RewardTransactionId and captures one immutable aggregate candidate
-> RewardSubsystem, InventorySubsystem and CharacterProfileSubsystem prepare candidate states without mutation
-> SettlementAuthority validates the complete candidate and freezes all resulting revisions/events/receipt
-> SettlementAuthority invokes only non-failing internal commit primitives for Reward, Inventory and Profile
-> after all three live states are installed, one publication point exposes revisions, committed events and receipt
-> BattleTransition publishes pure-value return context
-> Exploration arrival consumes return once
-> Reward Summary reads receipt; it does not grant reward
```

`SettlementAuthority` 是 PATCH-03D 的单次结算事务所有者，不是新的全局业务真源。聚合候选至少包含原 Reward ledger、Inventory snapshot、Profile snapshot、三者目标 snapshot、预分配 revisions、`RewardTransactionId`、`BattleResultId` 与待发布 receipt/events。Prepare 或最终聚合验证任一步失败时，三个 live Domain 均保持不变，TransactionId 不进入已处理 ledger，也不发布 receipt/event。

Commit 阶段开始后不得再执行可能失败的查找、分配、Definition 解析、GE 应用或外部回调；这些工作必须在 candidate 阶段完成。三个 Domain 仍拥有并校验各自状态，但只向该 authority 暴露受限的内部 `CommitPreparedCandidate` 原语。固定安装顺序为 Inventory -> Profile -> Reward ledger，三个安装操作必须是移动/交换已准备纯值状态且不可失败；全部安装完成后才统一提高三个 revision，记录已处理 TransactionId，发布 committed events 和 receipt。任何实现无法满足“不失败 commit”时，03D 必须停在 Task Gate 修订，不能以补偿、`Inconsistent` 或 UI 调用顺序替代 AC-003 的零部分提交。

### 5.3 Inventory -> Equipment -> Character Read Model

```text
Inventory UI selects ItemInstanceId
-> Equip intent(CharacterId, InstanceId, Slot, OperationId)
-> Inventory ownership + Equipment slot preflight
-> Equipment builds replace candidate and source-effect delta
-> commit authoritative equipment mapping
-> rebuild exact ASC equipment source handles
-> Equipment/Profile revisions notify Character ViewModel
-> UI renders new stat/source breakdown
```

失败不得删除旧装备、丢失 Inventory ownership、重复应用 Infinite GE 或更新 UI 为乐观成功。

### 5.4 Map -> Travel -> UI Rebuild

```text
Map UI selects stable TeleportId
-> MapSubsystem validates unlock/current context/pending state
-> UIManager atomically tears Frontend to Exploration root
-> MapSubsystem starts authorized travel
-> old World/HUD/Actor teardown
-> new World arrival validates MapId + ArrivalCommitId
-> Map Runtime commits location
-> new Actor projection and HUD host rebuild
-> UI reads committed map/profile/inventory state
```

Widget 禁止直接 `OpenLevel`。旅行失败保持旧 Map Runtime 和可操作 World；旧 host、Widget callback 和 arrival token 不得复活。

### 5.5 Save/Load

```text
Save intent
-> reject battle/return/travel/reentry conflict
-> capture only committed Domain snapshots
-> cross-domain validation
-> canonical encode + checksum
-> staging write/readback -> backup rotation -> primary write/readback
-> structured result

Load intent
-> decode Primary/Backup candidate
-> schema migration + all-domain PrepareRestore
-> equipment projection validation
-> one global commit
-> typed restore events/revisions
-> UI and Actor projection rebuild from committed state
```

Load 不由 Widget 发起旅行，不保存 pending Frontend route，也不以磁盘数据直接逐个修改 live subsystem。重复 Load 同一状态为 no-op。

## 6. UI 合同

- `UHSRUIManagerSubsystem` 只拥有 ScreenStack、Router、pause、input/focus、Widget/ViewModel lifecycle 和 travel teardown。
- Domain ViewModel 可聚合多个只读 snapshot，但不能调用 mutation API。
- Command facade 只接受纯值 intent，返回结构化 Result；UI 根据 Result 和后续 committed snapshot 展示。
- Widget 不直接调用 `AddToViewport`、`SetGamePaused`、`OpenLevel`、Save serialization、Inventory mutation、Equip GE 或 Battle resolution。
- 无 Tick 轮询业务状态；使用 typed delegate，并在 teardown 解除绑定。

## 7. 可扩展性边界

- 新角色、物品、装备、Encounter 和 Map 优先通过 Definition/Catalog 增加，不修改核心分支。
- 单机阶段保留稳定 Request/Result/OperationId 与 authority boundary；不提前实现 RPC、Prediction 或复制。
- 若未来引入跨系统 saga coordinator，它只能拥有 operation step、stable IDs、timeout/cancellation policy 和审计 snapshot；不得成为 Domain 数据真源。
- Cloud Save、独立 Inventory 存档流、多人、账号和 Gacha Authority 不属于 PATCH-03。

## 8. 统一诊断快照

每次集成操作至少可输出：

```text
OperationId, Step, Result
CharacterId, PartyRevision, ProfileRevision
InventoryRevision, EquipmentRevision, RewardTransactionId
EncounterRequestId, BattleId, BattleResultId
MapId, TravelRequestId, ArrivalCommitId
SaveId, Generation, RestoreRevision
UI route, ScreenStack depth, host generation
```

日志不得把 UObject 地址当稳定身份。自动化与 PIE 以相同 ID/Revision 证明 exactly-once、失败零污染和旅行后重建。
