# 装备系统（Equipment）

> HSR 项目的装备系统学习笔记
> 涵盖：装备类型定义、子系统管理、Stat 聚合、Effect Bridge、圣遗物套装、存档集成、Progression 联动

---

## 1. 系统全览

装备系统是项目的第二大完整模块，连接了三个子系统：

```
装备子系统 (EquipmentSubsystem)
  │  管理装备实例、定义注册、Equip/Unequip/Replace
  │
  ├── Stat Aggregator → Effect Bridge → GAS (属性应用)
  │      装备的 stat 怎么加起来、怎么 Apply 到角色
  │
  └── Save/Restore ─ 存档 (持久化)
          装备状态怎么存、怎么恢复
```

### 装备分类

```
EHSREquipmentKind:
├── Equipment (武器 + 防具)    → EHSREquipmentSlot (5 个槽位)
└── Relic (圣遗物)             → EHSRRelicSlot (6 个槽位)
```

### 5 个 Equipment 槽位

```cpp
enum class EHSREquipmentSlot : uint8
{
    Weapon,   // 武器
    Head,     // 头部
    Hands,    // 手部
    Body,     // 身体
    Feet      // 脚部
};
```

### 6 个 Relic 槽位

```cpp
enum class EHSRRelicSlot : uint8
{
    Head,          // 头部
    Hands,         // 手部
    Body,          // 身体
    Feet,          // 脚部
    PlanarSphere,  // 位面球
    LinkRope       // 连结绳
};
```

### 4 种 Stat

```cpp
enum class EHSREquipmentStat : uint8
{
    MaxHealth,  // 最大生命
    Attack,     // 攻击力
    Defense,    // 防御力
    Speed       // 速度
};
```

---

## 2. 装备实例模型

### FHSREquipmentInstance

```cpp
struct FHSREquipmentInstance
{
    FGuid InstanceId;                // 全局唯一实例 ID
    FName DefinitionId;              // 指向 EquipmentDefinition / RelicDefinition
    EHSREquipmentKind Kind;          // Equipment 还是 Relic
    int32 EnhancementLevel;          // 强化等级 (0~15)
    TArray<FHSREquipmentModifier> Modifiers;  // 副词条列表
};
```

### FHSREquipmentModifier

```cpp
struct FHSREquipmentModifier
{
    EHSREquipmentStat Stat;   // 哪种 stat
    float Value;              // 数值（始终 ≥ 0）
};
```

一把武器实例 = DefinitionId（"什么武器"）+ InstanceId（"这把是唯一的"）+ 强化等级 + 副词条。

### FHSREquipmentLoadout

```cpp
struct FHSREquipmentLoadout
{
    TMap<EHSREquipmentSlot, FHSREquipmentInstance> Equipment;  // 5 件
    TMap<EHSRRelicSlot, FHSREquipmentInstance> Relics;        // 6 件
};
```

每个角色 `FGuid` 对应一个 `Loadout`。当前版本中 11 个槽位全部可以独立装备。

---

## 3. Subsystem 核心逻辑

### 内部存储

```cpp
// UHSREquipmentSubsystem 内部状态
TMap<FName, FDefinitionRule> Definitions;  // 所有已注册的装备定义
TMap<FGuid, FLoadoutState>   Loadouts;     // 每个角色的装备
TMap<FGuid, FGuid>           InstanceOwners; // 实例 ID → 角色 ID
```

### 操作接口

| 方法 | 作用 | 返回值 |
|------|------|--------|
| `RegisterDefinition(EquipmentDef)` | 注册武器/防具定义 | `Success` / `DuplicateDefinitionId` |
| `RegisterDefinition(RelicDef)` | 注册圣遗物定义（含 SetId） | `Success` / ... |
| `Equip(CharacterId, Instance)` | 装备一件，槽位必须空 | `SlotOccupied` / `Success` |
| `Replace(CharacterId, Instance)` | 替换槽位上现有的装备 | `TargetNotFound` / `Success` |
| `Unequip(CharacterId, Kind, Slot, ExpectedInstanceId)` | 卸下装备（需确认实例 ID） | `InstanceMismatch` / `Success` |
| `SetEnhancementLevel(CharacterId, InstanceId, NewLevel)` | 强化装备 | `NoOp` / `InvalidEnhancementLevel` / `Success` |
| `GetLoadout(CharacterId, &OutLoadout, &OutRevision)` | 读取角色装备 | `true` / `false` |

### Equip 的完整验证链

```cpp
UHSREquipmentSubsystem::Equip(CharacterId, Instance)
{
    ① CharacterId 有效？
    ② InstanceId 有效？
    ③ Instance 的 Modifier 合法？
    ④ DefinitionId 已注册？
    ⑤ 槽位匹配（Kind + Slot）？
    ⑥ EnhancementLevel 在区间 [0, EnhancementCap]？
    ⑦ 这个 Instance 已被别人装备了（InstanceAlreadyEquipped）？
    ⑧ 目标槽位已被占用（SlotOccupied）？
    ⑨ 通过 → CommitLoadout()
}
```

### CommitLoadout——唯一的写入点

所有的更新操作最终都落在 `CommitLoadout` 上。

```cpp
void UHSREquipmentSubsystem::CommitLoadout(const FGuid& CharacterId, const FHSREquipmentLoadout& Candidate)
{
    // 1. 从 InstanceOwners 中移除旧装备的实例
    for (旧装备) InstanceOwners.Remove(InstanceId);
    // 2. 写入新 Loadout
    State.Loadout = Candidate;
    // 3. 在 InstanceOwners 中注册新装备的实例
    for (新装备) InstanceOwners.Add(InstanceId, CharacterId);
    // 4. Revision++
    ++State.Revision;
    // 5. 广播事件
    LoadoutChanged.Broadcast(CharacterId, State.Revision);
}
```

`CommitLoadout` 是原子的——同一角色一次完成所有变更。

---

## 4. Stat 聚合器

### 作用

将角色 Loadout 中所有装备的 Modifier 累加起来，得到角色的**装备提供的总 stat**。

### 实现

```cpp
// HSREquipmentStatAggregator.cpp — 极为简洁，总共 4 行
// 核心逻辑：
//   遍历 Loadout 中所有 Equipment + Relics
//   对每个装备的 Modifier 数组，按 Stat 类型累加：
//     MaxHealth: sum, Attack: sum, Defense: sum, Speed: sum

FHSREquipmentAggregate
├── float MaxHealth = 0.0f  // 装备提供的总 HP
├── float Attack = 0.0f     // 装备提供的总 ATK
├── float Defense = 0.0f    // 装备提供的总 DEF
├── float Speed = 0.0f      // 装备提供的总 SPD
└── int64 Revision = 0      // 聚合时的 Loadout Revision
```

确保数值有效性：每个 Modifier 的非负性、有限性、累加不溢出。

**聚合是单向的（加法）**。没有乘算、没有百分比——所有装备 Modifier 都是固定数值加法。百分比 buff 在 GAS 层通过其他 GameplayEffect 处理。

---

## 5. Effect Bridge——连接 GAS 的桥梁

### 需要 Bridge 的原因

装备聚合出来的 stat 是 `FHSREquipmentAggregate`（纯数值结构体）。要让这些 stat 在战斗中生效，需要通过 GAS 的 GameplayEffect 应用到角色身上。但：

- 装备子系统和 GAS 没有直接关联——装备不知道 ASC 是什么
- 装备子系统的 `LoadoutChanged` 事件由 BattleCoordinator 监听
- BattleCoordinator 通过 `EquipmentEffectBridge` 把 stat 变成 GE

### Apply 方法

```cpp
bool UHSREquipmentEffectBridge::Apply(
    const FGuid& Key,               // 装备实例 ID
    UAbilitySystemComponent* ASC,   // 目标角色的 ASC
    TSubclassOf<UGameplayEffect> C, // 装备 Stat GE 类
    const FHSREquipmentAggregate& A // 聚合后的 stat 值
);
```

内部逻辑：

```
① 指纹比较（检查是否能跳过）：
    如果这个 Key 对应的 GE 已存在且 ASC/Class/Fingerprint 完全一致
    → 只更新 Revision，直接返回 true（跳过不必要的 GE 更新）

② 创建 Spec：
    ASC->MakeOutgoingSpec(C, 1.0f, ASC->MakeEffectContext())

③ 通过 SetByCaller 设值：
    SetByCaller(HSREquipmentTags::BonusMaxHealth) = A.MaxHealth
    SetByCaller(HSREquipmentTags::BonusAttack)    = A.Attack
    SetByCaller(HSREquipmentTags::BonusDefense)   = A.Defense
    SetByCaller(HSREquipmentTags::BonusSpeed)     = A.Speed

④ Apply：
    ASC->ApplyGameplayEffectSpecToSelf(Spec)

⑤ 如果已有旧 GE，移除旧的：
    旧 GE 在 EquipmentEffectBridge 中按 Key 索引
    移除旧 GE → Apply 新 GE → 更新 Sources 表
```

**指纹优化**：如果装备的 stat 完全没变（MaxHealth/Attack/Defense/Speed 都相等），Bridge 不会重新 Apply GE。这避免了每 Tick 或每次 Revision 增加时重复的 GE 操作。

### Bridge 用 DurationGE 而非 InstantGE 的原因

Bridge Apply GE 时使用的是 **Duration/Infinite GE**（无限持续，由 Bridge 手动管理移除），而不是 Instant GE。这和 GAS 的 Aggregator 行为有关：

| GE 类型 | 是否改 BaseValue | 移除 GE 时 | 适用场景 |
|---------|-----------------|------------|----------|
| **Instant** | ✅ 直接修改 BaseValue | 不消失（永久改变） | 一次性的伤害、治疗、消耗 |
| **Duration / Infinite** | ❌ 不进 Aggregator，不改 BaseValue | ✅ Aggregator 重新评估，值自动回退 | Buff、装备 stat 等临时叠加 |

具体到装备系统：

```
角色基础 ATK = 100（Instant GE 设的 BaseValue）

装备一件武器 +50 ATK：
  → Bridge 创建一个 +50 的 Infinite GE
  → Aggregator: ATK = 100(Base) + 50(Equipment) = 150

卸下武器：
  → Bridge::Remove(InstanceId)
  → 移除 Infinite GE
  → Aggregator 移除 +50 Modifier，重新评估
  → ATK 回到 100 ← 自动回退，不需要手算差值
```

如果 Bridge 用了 Instant GE：

```
装备武器时直接把 BaseValue 改成 150
卸下武器时需要：
  ① 读取旧值（150）
  ② 减去拆下装备的 stat（+50）
  ③ 手动 Apply 一个 -50 的 Instant GE 把 BaseValue 改回 100
  ④ 如果还有其他装备在身，这个计算更复杂
```

所以 **Bridge 用 DurationGE + Remove() 而非 InstantGE，是在利用 GAS Aggregator 的自动回退特性**。装备系统不需要知道 stat 的"旧值"是什么——移除 GE 时 Aggregator 自己就知道了。

### Remove / RemoveAll

```cpp
Remove(Key)      → 按实例 ID 移除对应的 GE
RemoveAll()      → 清空所有 Bridge 管理的 GE
RemoveSetSource() → 按套装 SourceId 移除套装 GE
```

### 为什么 Bridge 维护自己的 Sources 表

```cpp
TMap<FGuid, FSource> Sources;  // Key = 装备实例 ID
// → FSource 包含: ASC, GE Class, Fingerprint, ActiveGameplayEffectHandle
```

Bridge 持有 GE 的 Handle，因为：
- 需要在不通知 EquipmentSubsystem 的情况下直接更新/移除 GE
- BattleCoordinator 可以独立管理装备 stat GE 的生命周期
- 战斗结束时要调用 `RemoveAll()` 清理所有装备 GE

---

## 6. 圣遗物套装系统

### 套装判定流程

```
角色 Loadout 中的 Relics (6 个槽位)
  ↓
遍历每个 Relic 的 DefinitionId
  ↓
查找 DefinitionId → SetId（通过 HSRRelicDefinition）
  ↓
统计每个 SetId 的出现次数
  ↓
SetId 出现 ≥ 2 次 → 激活套装效果
```

### FHSRRelicSetResolver

```cpp
// 输入：角色 Loadout + 所有 RelicDefinition + 所有 RelicSetDefinition
// 输出：每个 SetId 的统计结果

struct FHSRRelicSetResolution
{
    FName SetSourceId;  // 激活时 = SetId，不激活时 = NAME_None
    int32 Count;        // 已装备的同套装 Relic 数量
    bool bActive;       // Count >= Threshold（当前 hardcode = 2）
};
```

### 套装效果 Apply

在 ProjectEquipmentRestore 中，套装的 stat 通过和单件 stat 相同的 Bridge Apply 路径：

```
RelicSetCounts 中 Count >= 2 的 SetId
  → 构建一个空的 FHSREquipmentAggregate（Revision 有效，stat 全是 0）
  → 通过 EquipmentEffectBridge::ApplySetSource() 应用到一个 keyed GE
```

注意套装 GE 的 stat aggregate 目前是空值（全部为 0）。套装效果本身（如"攻击力+12%"）在 GE 资产中通过 Modifier 直接定义，不需要 Bridge 传数值——Bridge 只做 Apply/Remove 生命周期管理。

---

## 7. 存档集成

### 7.1 ExportSaveData

从 Loadout 中导出存档 DTO：

```cpp
// FHSREquipmentSaveDto (存档格式)
struct FHSREquipmentSaveDto
{
    FName DefinitionId;
    FGuid InstanceId;
    FGuid CharacterId;        // 谁装备的
    int32 Kind;               // 0 = Equipment, 1 = Relic
    int32 Slot;
    int32 EnhancementLevel;
    TArray<FHSREquipmentModifier> Modifiers;
    FName SetId;              // 只有 Relic 有这个值
    int32 AuthorityRevision;  // 保存时的 Loadout Revision
};
```

输出后按 `CharacterId → Kind → Slot` 三字段规范排序——确保确定性字节序列，便于存档的 SHA-256 校验。

### 7.2 PrepareRestore

```cpp
bool UHSREquipmentSubsystem::PrepareRestore(
    const TArray<FHSREquipmentSaveDto>& In,
    FHSREquipmentRestoreMap& Out         // TMap<角色GUID, FHSREquipmentRestoreState>
) const;
```

验证链：

```
① 每条记录的 CharacterId、InstanceId 有效？
② InstanceId 不重复？
③ Slot 类型有效？
④ DefinitionId 已注册、Kind 匹配？
⑤ EnhancementLevel 在 [0, EnhancementCap] 内？
⑥ AuthorityRevision ≥ 0？
⑦ Modifier 合法？
⑧ 装备套装的 SetId 和 Definition 匹配？
⑨ 同一角色的所有记录的 AuthorityRevision 一致？
⑩ 槽位不冲突？
```

全部通过 → 构建 `FHSREquipmentRestoreMap`（角色 GUID → { Loadout, RelicSetCounts, Revision }）。

### 7.3 ProjectRestore（关键跨域路径）

ProjectRestore 是存档恢复中唯一需要跨域的步骤。它在 `SaveSubsystem LoadSnapshot` 中被调用。

为什么 Equipment 需要 ProjectRestore——以及它和 LoadoutChanged 路径的关系：

两种触发路径最终走的是 **同一个 Bridge**，不会重复 Apply：

```
路径 A：正常游戏
  换装备 → CommitLoadout → LoadoutChanged
    → BattleCoordinator 监听到
      → ApplyEquipmentSource() → Bridge::Apply() ← GE 生效

路径 B：存档恢复
  LoadFromSlot → PrepareRestore → ProjectRestore
    → BattleCoordinator::ProjectEquipmentRestore
      → ApplyEquipmentSource() → Bridge::Apply() ← 同一个 Bridge
```

那为什么不能省掉 ProjectRestore，只依赖路径 B 中的 LoadoutChanged？

```
假设没有 ProjectRestore：
  ① PrepareRestore（验证数据）
  ② CommitRestore  → Loadouts.Reset() ← 清空！
  ③ NotifyRestored → LoadoutChanged
       → 此时 GetLoadout 返回空 → Bridge Apply 0 stat
  ④ 然后 CommitRestore 再从 Candidate 写入真正的值
       → 又一个 LoadoutChanged → Bridge Apply 正确 stat

第 ③ 步和第 ④ 步之间有一个瞬态：stat 短暂变成 0。
```

ProjectRestore 提前在 Commit 之前就把 stat Apply 好了——用的是 Candidate 的数据（已验证但还没写入内存），而不是从 Loadout 读的。这样 Commit 完成后 NotifyRestored 时，Bridge 指纹检查发现 stat 已经 Apply 过了，直接跳过。**瞬态不一致就不存在了。**

### ProjectEquipmentRestore 完整实现

| 子系统 | 需要 ProjectRestore？ | 原因 |
|--------|----------------------|------|
| Party | 不需要 | 队伍数据不涉及 stat |
| Profile | 不需要 | 等级/经验是独立数值 |
| Equipment | **需要** | 装备的 stat 需要通过 GAS Apply 到角色 ASC |
| Inventory | 不需要 | 背包不直接影响战斗 |
| Quest/Map | 不需要 | 不涉及战斗 |

ProjectEquipmentRestore 在 BattleCoordinator 中实现（因为只有 Coordinator 知道怎么通过 GAS 应用 stat）：

```
ProjectEquipmentRestore(Candidate)
  │
  ├── 遍历 Candidate 中的每个角色
  │     ├── 找到对应的战斗参与者 (FHSRBattleParticipant)
  │     ├── 对每个装备实例：
  │     │     ├── UHSREquipmentStatAggregator::Aggregate → stat 聚合值
  │     │     └── ApplyEquipmentSource() → Bridge::Apply() → GE
  │     └── 对每个套装 (Count ≥ 2)：
  │           └── ApplyEquipmentSetSource() → Bridge::ApplySetSource() → GE
  │
  ├── 对比 OldStates vs DesiredStates
  │     ├── 旧的但新的没有 → RemoveEquipmentSource()
  │     └── 新旧共有的 → 跳过（指纹不变）
  │
  └── 更新 EquipmentProjectionStates
```

**错误恢复**：如果 Apply 过程中任何一步失败，`RestoreOld()` 会把所有东西回滚到旧状态。

### 7.4 CommitRestore

```cpp
void UHSREquipmentSubsystem::CommitRestore(const FHSREquipmentRestoreMap& Candidate)
{
    Loadouts.Reset();             // 清空所有角色装备
    InstanceOwners.Reset();       // 清空所有实例所有关系
    for (Candidate 中的每个条目)
    {
        Loadouts[角色GUID] = Loadout;
        InstanceOwners[实例ID] = 角色GUID;
    }
    // 不广播 LoadoutChanged——NotifyRestored 负责
}
```

### 7.5 NotifyRestored

```cpp
void NotifyRestored(const TSet<FGuid>& Changed)
{
    for (每个有变更的角色)
        LoadoutChanged.Broadcast(角色GUID, Revision);
}
```

这里广播的 `LoadoutChanged` 会触发 `BattleCoordinator` 的监听回调，重新计算 stat 并 Apply。

### 7.6 完整恢复链路

```
SaveSubsystem::LoadFromSlot
  │
  ├── HSRSaveVersion::DecodeEnvelope    （验证格式+校验和+版本）
  ├── MigrateToCurrent                   （版本迁移）
  ├── Validate                           （引用完整性）
  │
  ├── Equipment::PrepareRestore           （验证装备数据合法性）
  ├── 其他 6 个子系统的 PrepareRestore
  │
  ├── SaveSubsystem::LoadSnapshot
  │     ├── Equipment::ProjectRestore
  │     │     └── BattleCoordinator::ProjectEquipmentRestore
  │     │           ├── 逐件装备：Aggregate → Bridge::Apply → GE
  │     │           └── 逐套装：Bridge::ApplySetSource → GE
  │     │
  │     ├── 差异计算
  │     ├── CommitRestore（×7 子系统）
  │     └── NotifyRestored
  │
  └── LoadoutChanged.Broadcast → (如果有) 触发战斗内 stat refresh
```

---

## 8. Progression 联动

### 角色 stat 的两层结构

```
角色最终属性 = 基础值 + 等级成长 + 装备加成
```

```
FHSRCharacterDerivedStats（角色的最终战斗属性）
├── MaxHealth
├── Attack
├── Defense
└── Speed
```

### UHSRCharacterStatAggregator

```cpp
// 从角色定义 + 运行时状态 → ProgressionContext
bool BuildContext(
    const UHSRCharacterDefinition* Definition,  // 基础值 + 每级成长
    const FHSRCharacterRuntimeState& Runtime,     // 当前等级
    int64 Revision,
    FHSRCharacterProgressionContext& OutContext
);
```

公式：

```
L = (Level - 1)
ProgressionBonuses.MaxHealth = MaxHealthPerLevel × L
ProgressionBonuses.Attack    = AttackPerLevel    × L
ProgressionBonuses.Defense   = DefensePerLevel   × L
ProgressionBonuses.Speed     = SpeedPerLevel     × L

DerivedStats.MaxHealth = BaseMaxHealth + ProgressionBonuses.MaxHealth
DerivedStats.Attack    = BaseAttack    + ProgressionBonuses.Attack
DerivedStats.Defense   = BaseDefense   + ProgressionBonuses.Defense
DerivedStats.Speed     = BaseSpeed     + ProgressionBonuses.Speed

（最终值全部 Clamp 到 [0, MAX_flt]）
```

### HSRCharacterProgressionContext

```cpp
struct FHSRCharacterProgressionContext
{
    FName CharacterId;
    int64 RuntimeRevision;
    FHSRCharacterDerivedStats DerivedStats;      // 基础 + 等级成长
    FHSRCharacterDerivedStats ProgressionBonuses; // 等级成长部分（单独存用于 Apply）
};
```

### 两层 stat 如何合并到 GAS

```
战斗开始时：
  ① BattleCoordinator 读取角色等级
  ② ProgressionSubsystem 计算基础 stat
  ③ 基础 stat 通过 GE Apply 到 ASC（Progression GE）
  ④ 装备 stat 通过 EquipmentEffectBridge Apply 到 ASC（Equipment GE）
  → ASC 上的 Aggregator 自动合并所有 Modifier
  → 最终属性 = 基础 + ProgressionBonus + EquipmentStat
```

**两者走不同的 GE 路径**：

| stat 来源 | GE 路径 | 谁来管理 |
|-----------|---------|---------|
| 基础值 + 等级成长 | `ProgressionEffects` maps | `BattleCoordinator` |
| 装备 stat | `EquipmentEffectBridge` | `BattleCoordinator` (间接) |

---

## 9. 测试架构

### 测试文件

```
Source/HSR/Tests/
├── HSREquipmentSubsystemTests.cpp           — 单元测试：Equip/Unequip/Replace/SetLevel
├── HSREquipmentSaveTests.cpp                 — 存档集成：Export → PrepareRestore → CommitRestore
├── HSREquipmentSaveProjectionTests.cpp       — ProjectRestore 事务测试
├── HSREquipmentEffectContractTests.cpp       — Effect Bridge 合约测试
├── HSREquipmentDevelopmentHarnessTests.cpp   — 开发 Harness 测试
└── HSREquipmentDetailViewModelTests.cpp      — ViewModel 测试（UI 层）
```

### 错误注入

`ProjectEquipmentRestore` 支持 `EquipmentRestoreFailureAfterOperations` 注入——在第 N 次 Apply 操作后模拟失败，验证 `RestoreOld()` 回滚是否完整。

### 开发 Harness

```cpp
FHSREquipmentDevelopmentHarness
├── SetupFixedLoadout(GameInstance)     — 在 PIE 中设置标准 Loadout
├── RemoveSecondRelic(GameInstance)     — 移除第二件 Relic（用于 save/load 测试）
├── RestoreSecondRelic(GameInstance)    — 恢复
├── ClearLoadout(GameInstance)          — 清空
├── Save/Load/CleanupSave              — 磁盘往返
└── SetupFixedLoadoutForTest/...       — 不依赖 PIE 的纯逻辑版本
```

---

## 10. 架构图

```
                                ┌──────────────────────────┐
                                │  UHSREquipmentSubsystem   │
                                │                          │
                                │  Definitions[]            │
                                │  Loadouts[角色]           │
                                │  InstanceOwners[]         │
                                │                          │
                                │  Equip/Replace/Unequip    │
                                │  SetEnhancementLevel      │
                                │  ExportSaveData           │
                                │  PrepareRestore           │
                                │  CommitRestore            │
                                └────────┬─────────────────┘
                                         │
                    ┌────────────────────┼────────────────────┐
                    │                    │                    │
                    ▼                    ▼                    ▼
        ┌───────────────────┐  ┌──────────────┐  ┌──────────────────────┐
        │ StatAggregator     │  │ 存档 (Save)  │  │ LoadoutChanged       │
        │                    │  │              │  │ (异步通知)            │
        │ 装备 stat 累加     │  │ DTO 编码/解码 │  │                      │
        │ Loadout → Aggregate│  │ Version 迁移  │  │ BattleCoordinator    │
        └────────┬──────────┘  └──────────────┘  │ 监听此事件           │
                 │                               └──────────┬───────────┘
                 ▼                                          │
        ┌───────────────────┐                               │
        │ Effect Bridge     │◄──────────────────────────────┘
        │                    │    Coordinator 调用
        │ SetByCaller → GE   │    ApplyEquipmentSource
        │ ASC::Apply GE      │
        │ 指纹缓存跳过重复   │
        └────────┬──────────┘
                 │
                 ▼
        ┌───────────────────┐
        │ ASC (战斗参与者)   │
        │                    │
        │ Health / Attack /  │
        │ Defense / Speed    │
        │ 装备 stat 在此生效  │
        └───────────────────┘
```

---

## 11. 关键设计模式

| 模式 | 实现 | 说明 |
|------|------|------|
| **纯值 DTO 边界** | `FHSREquipmentInstance` / `FHSREquipmentSaveDto` | 不引用 UObject，跨系统安全传递 |
| **单一写入点** | `CommitLoadout()` | 所有更新操作汇聚于此，保证 Revision 一致 |
| **指纹跳过** | `EffectBridge::Apply` 的 Fingerprint 比较 | stat 没变时跳过 GE 操作 |
| **原子错误恢复** | `ProjectEquipmentRestore` 中的 `RestoreOld()` | 任何一步失败 → 全部回滚 |
| **延迟投影** | `ProjectRestore` 委托 | 装备子系统不依赖 GAS，由 Coordinator 执行投影 |
| **规范排序** | ExportSaveData 按三字段排序 | 确定性序列化 + 存档校验 |
| **属性分离** | 基础属性 vs 装备属性 | 不同 GE 路径、不同生命周期管理 |

---

## 12. 数据定义（Data Assets）

装备的"模板"数据在 Editor 中通过 Blueprint 子资产创建，所有逻辑层代码只通过 `FName DefinitionId` 引用它们。

### 12.1 EquipmentDefinition

```cpp
UCLASS(BlueprintType)
class UHSREquipmentDefinition : public UDataAsset
{
    UPROPERTY(EditAnywhere) FName DefinitionId;         // 唯一标识
    UPROPERTY(EditAnywhere) EHSREquipmentSlot Slot;     // 槽位
    UPROPERTY(EditAnywhere) int32 EnhancementCap = 0;   // 强化上限
};
```

最小定义——只有三个字段。为什么这么简单？

| 字段 | 作用 |
|------|------|
| `DefinitionId` | 和存档/代码中引用的 FName 匹配 |
| `Slot` | 决定这个装备能装到哪个槽位（Weapon/Head/Hands/Body/Feet） |
| `EnhancementCap` | 强化等级上限（武器一般 15，防具一般 15） |

**装备的基础 stat（如"这把刀基础攻击力 100"）不在 Definition 里。** 基础 stat 来自 `HSRCharacterDefinition`——角色本身有 BaseAttack/BaseMaxHealth 等基础值，装备提供的是 Modifier（副词条），不是基础值。

### 12.2 RelicDefinition

```cpp
UCLASS(BlueprintType)
class UHSRRelicDefinition : public UDataAsset
{
    UPROPERTY(EditAnywhere) FName DefinitionId;          // 唯一标识
    UPROPERTY(EditAnywhere) FName SetId;                 // 所属套装 ID
    UPROPERTY(EditAnywhere) EHSRRelicSlot Slot;          // 槽位（Head/Hands/Body/Feet/PlanarSphere/LinkRope）
    UPROPERTY(EditAnywhere) int32 EnhancementCap = 0;    // 强化上限
};
```

比 EquipmentDefinition 多一个 `SetId`——圣遗物所属的套装。同一个 `SetId` 的圣遗物装备 ≥2 件时触发套装效果。

### 12.3 RelicSetDefinition

```cpp
UCLASS(BlueprintType)
class UHSRRelicSetDefinition : public UDataAsset
{
    UPROPERTY(EditAnywhere) FName SetId;                              // 套装 ID
    UPROPERTY(EditAnywhere, meta = (ClampMin = "2", ClampMax = "2")) int32 Threshold = 2;  // 触发阈值（当前固定 2）
    UPROPERTY(EditAnywhere) TSubclassOf<UGameplayEffect> SetGameplayEffectClass;           // 套装效果的 GE
};
```

关键设计：

- **Threshold 固定为 2**：HSR 原版的"2 件套"设计。编辑器 meta 限制了 ClampMin=2, ClampMax=2，实际不可更改
- **套装效果是 GE**：`SetGameplayEffectClass` 是一个 GameplayEffect 资产，在被 ApplySetSource 时由 Bridge Apply 到角色身上
- **套装 GE 的 Modifier**：套装效果本身（如"攻击力+12%"）在这个 GE 资产中通过 Modifier 定义，不是通过 Bridge 的 Aggregate 数值传递——Bridge 只负责 Apply/Remove 生命周期

### 12.4 注册流程

所有 Definition 都需要在运行时注册到 EquipmentSubsystem：

```cpp
// 在游戏加载时调用
EquipmentSubsystem->RegisterDefinition(WeaponDef);     // UHSREquipmentDefinition
EquipmentSubsystem->RegisterDefinition(RelicDef);      // UHSRRelicDefinition
```

内部转换：

```
UHSREquipmentDefinition → FDefinitionRule { Kind=Equipment, Slot, EnhancementCap, SetId=NAME_None }
UHSRRelicDefinition     → FDefinitionRule { Kind=Relic, Slot, EnhancementCap, SetId=... }
```

注册后的 DefinitionId 被后续所有操作引用：Equip 检查、PrepareRestore 验证、存档校验。

---

## 13. UI ViewModel

### 13.1 架构角色

```
EquipmentSubsystem (数据层)
  │ LoadoutChanged 事件
  ▼
UHSREquipmentDetailViewModel (ViewModel)
  │ 持有 FHSREquipmentDetailSnapshot
  │ OnChanged() 广播 + OnSnapshotChanged (蓝图)
  ▼
UHSREquipmentDetailWidget (UMG Widget)
  │ 订阅 ViewModel，收到 Snapshot → OnDetailSnapshotChanged (BlueprintImplementable)
  ▼
蓝图 (具体 UI 布局)
```

### 13.2 FHSREquipmentDetailSnapshot

ViewModel 的核心输出——一个纯值结构体，包含装备详情页需要的所有数据：

```cpp
struct FHSREquipmentDetailSnapshot
{
    FGuid CharacterId;
    TArray<FHSREquipmentInstance> Items;           // 所有已装备（Equipment + Relic）
    TArray<FHSREquipmentSourceRow> Sources;         // 每一行 stat 的来源追溯
    TArray<FHSRRelicSetDetailRow> RelicSets;        // 套装状态
    float MaxHealth;                                 // 装备提供的总 MaxHealth
    float Attack;                                    // 装备提供的总 Attack
    float Defense;                                   // 装备提供的总 Defense
    float Speed;                                     // 装备提供的总 Speed
    int32 Revision;                                  // Loadout Revision
    bool bRestoring;                                 // 是否正在从存档恢复
    bool bIsValid;                                   // Snapshot 有效
    EHSREquipmentDetailResult FailureReason;         // 失败原因
};
```

### 13.3 ViewModel 生命周期

```cpp
// 创建并绑定
ViewModel->Initialize(EquipmentSubsystem, CharacterId);

// 内部：订阅 EquipmentSubsystem::OnLoadoutChanged
// 每次变更时自动调用 Rebuild

// 销毁
ViewModel->Shutdown();  // 在 BeginDestroy 中自动调用
```

### 13.4 Rebuild——Snapshot 的构造

当 LoadoutChanged 事件触发时，ViewModel 重新构建 Snapshot：

```
Rebuild(ChangedId, Revision)
  │
  ├── 对比 ChangedId → 只处理自己关心的角色
  │
  ├── 从 EquipmentSubsystem 读取 Loadout
  │     ├── GetLoadout(CharacterId) → 失败 → Empty result
  │     └── 成功 → 遍历所有 Equipment + Relic
  │
  ├── 构建 Items[]
  │     ├── 从 Equipment 取 5 件
  │     └── 从 Relics 取 6 件
  │     └── 按 InstanceId 排序
  │
  ├── 构建 Sources[]
  │     ├── 对每件装备的每个 Modifier
  │     │     ├── SourceId = "InstanceId.index"（唯一标识每一行 stat）
  │     │     ├── AuthoredValue = Modifier.Value
  │     │     ├── EffectiveValue = Modifier.Value
  │     │     └── 按 Stat 类型累加到汇总值
  │     └── 对激活的套装
  │           ├── 添加一条套装 Source（无数值）
  │           └── SetSourceId = 套装名
  │
  ├── 构建 RelicSets[]
  │     └── 从 EquipmentSubsystem::GetRelicSetSnapshots 读取
  │
  ├── 汇总四维 stat (MaxHealth/Attack/Defense/Speed)
  │     └── 这正是 StatAggregator 做的事——但 ViewModel 自己又算了一次
  │     └── 原因：ViewModel 需要"按行追溯"，StatAggregator 只输出总计
  │
  └── 广播 Changed + OnSnapshotChanged (蓝图)
```

### 13.5 Widget 绑定

```cpp
// UHSREquipmentDetailWidget (C++ 基类)
// 在 NativeConstruct 中订阅 ViewModel
// 收到 Snapshot → 调用 OnDetailSnapshotChanged (蓝图实现)

SetViewModel(ViewModel)
  → 订阅 ViewModel->OnChanged()
  → 每次 Snapshot 更新 → Handle()
    → 缓存 Current Snapshot
    → 调用 BlueprintImplementableEvent OnDetailSnapshotChanged
```

### 13.6 ViewModel 的 stat 计算 vs StatAggregator

| | ViewModel | StatAggregator |
|---|---|---|
| **计算** | 遍历所有装备手动累加 | 遍历 Loadout 累加 |
| **输出** | 汇总值 + 每行追溯 | 只有汇总值 |
| **用途** | UI 展示（"这件装备提供了 +50 ATK"） | GAS 应用（"装备 stat 一共 +200 ATK"） |
| **谁调用** | ViewModel::Rebuild | BattleCoordinator::ProjectEquipmentRestore |

两者做了相同的累加计算，但目的不同——ViewModel 需要追溯每一行的来源（"这个 stat 来自哪件装备的哪个副词条"），StatAggregator 只需要最终的总值。这是一个有意义的重叠：聚合器只关注结果，ViewModel 关注明细。

### 13.7 整体 MVVM 数据流

```
Subsystem::CommitLoadout
  → LoadoutChanged.Broadcast(CharacterId, Revision)
    → ViewModel::Rebuild(CharacterId, Revision)
      → 构造 FHSREquipmentDetailSnapshot
        → Items[] (11 件装备)
        → Sources[] (每件装备的每个副词条 → 追溯行)
        → RelicSets[] (套装状态)
        → 四维 stat 汇总
      → Changed.Broadcast(Snapshot)
        → Widget::Handle(Snapshot)
          → 缓存 Current
          → OnDetailSnapshotChanged(Snapshot) [蓝图]
            → UI 更新
```
