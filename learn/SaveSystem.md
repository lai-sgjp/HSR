# 游戏存档系统

> HSR 项目的存档系统设计学习笔记
> 涵盖：事务写入、Binary 序列化、规范排序、版本迁移、恢复流程、Cloud Save

---

## 业界主流方向

### 数据格式

| 格式 | 代表游戏 | 特点 |
|---|---|---|
| **Binary** | 大多数 3A（GTA、巫师、老滚） | 体积小、加载快、不可读、版本敏感 |
| **JSON / XML** | 独立游戏、配置型存档 | 人类可读、体积大、解析慢、容错高 |
| **Protocol Buffers / FlatBuffers** | 手游、跨平台 | 强类型 schema、向后兼容、体积介于 binary 和 JSON 之间 |
| **SQLite** | 复杂数据关系游戏（文明、P 社） | 查询灵活、增量写入、事务安全 |

### 序列化机制

- **UE 内置 `USaveGame`** —— 全自动反射序列化，简单但缺乏版本控制和校验，不适合复杂项目
- **自定义 Binary Serializer** —— 手动控制每字段的读写，配合版本号和迁移函数，大多数 3A 的选择
- **混合 JSON + Binary** —— JSON 存元数据（角色名、等级），Binary 存大块数据（地图状态、物品）
- **Database 驱动** —— SQLite 存结构化数据，支持增量更新，不写整个文件

### 架构层次

```
① Save Subsystem（入口）
   → 谁调用 Save/Load
   → 自动存档触发（切图、定时、事件）
   → 存档槽管理（多档位、自动/手动）

② Save Data（数据容器）
   → 平整的数据结构，不引用 UObject
   → 只存"值"，不存"对象"
   → 版本号字段

③ Serializers（序列化器）
   → 每个子系统有自己的 Serializer
   → InventorySerializer、QuestSerializer
   → 职责单一：struct ↔ byte stream

④ Migrators（版本迁移）
   → 存档版本号 → 从旧版本迁移到新版本
   → 支持跳过版本（V1 → V3 直接迁移）
```

### 增量存档 vs 全量存档

- **全量存档**：每次保存写整个游戏状态，实现简单、加载直接，但大存档保存卡顿
- **增量存档**：只记录变化，写入快、支持频繁自动存档，但需要"基线 + 增量链"

### 业界趋势

1. **Binary + Schema 版本控制** —— 绝大多数 3A 自研引擎的选择
2. **Cloud Save + 增量同步** —— 手游/服务端，需要差异 diff、冲突解决、多设备漫游
3. **Database 驱动** —— 大型沙盒游戏（Minecraft、P 社），增量写入、局部加载

---

## HSR 项目存档系统架构

### 系统文件结构

```
Source/HSR/Save/
├── HSRSaveSubsystem.h/.cpp     — 存档入口（GameInstanceSubsystem）
├── HSRSaveGame.h               — USaveGame 包装（兼容旧格式）
├── HSRSaveTypes.h              — 数据类型定义（DTO）
├── HSRSaveVersion.h/.cpp       — 版本控制 + Binary 序列化器
```

### 设计定位

你项目的设计处于**业界三维度地图**的如下位置：

```
格式:     JSON ────────────●── Binary (自定义)
序列化:   反射 ──────●──────── 手动 (FReader/FWriter)
恢复策略: 单文件 ──────────●── 事务 (Staging + Backup)
```

### 磁盘文件布局（Envelope 格式）

```
┌─────────────────────────────────────┐
│ Magic: "HSRSAVE\0"                   │  8 bytes
│ FormatVersion / HeaderSize           │  4 bytes
│ SchemaVersion / MinCompatible        │  8 bytes
│ Payload size                         │  8 bytes
│ SaveId (GUID)                        │ 16 bytes
│ Generation (uint64)                  │  8 bytes
│ UTC timestamp                        │  8 bytes
│ SlotIdentity (FNV-1a)               │  8 bytes
│ SHA-256 checksum                     │ 32 bytes
├─────────────────────────────────────┤
│ Canonical Payload                     │
│   - Profiles（规范排序）               │
│   - PartySlots                       │
│   - Equipment（规范排序）              │
│   - Inventory                        │
│   - Rewards                          │
│   - Quests                           │
│   - Map                              │
└─────────────────────────────────────┘
```

### 关键设计决策

| 决策 | 替代方案 | 原因 |
|---|---|---|
| 自定义 Binary | UE 反射序列化 | 完全的字段控制、块级校验、schema 独立于 C++ 类型 |
| 规范排序（Canonical） | 原序写入 | 相同内容 → 相同字节 → 确定性 Checksum，去重可靠 |
| SHA-256 校验 | CRC32 | 防碰撞需要，档间去重可靠 |
| 备份槽 + Staging | 单文件写入 | 写入崩溃 → 从备份恢复 |
| 子系统的 Export/Import | 单一大类 | 每个子系统负责自己的序列化逻辑 |

---

## 核心机制详解

### 事务写入（Step 1）

为什么需要事务写入？根本问题：**存档写到一半电脑断电了怎么办？**

```
常见方案（单文件直接覆盖）：
  Save → 打开文件 → 写入数据 → 关闭文件
  ① 如果写入到一半断电 → 文件损坏 → 存档丢失

事务方案（HSR）：
  Step 1: 写入 Staging 文件（.__hsr_staging_v1）
  Step 2: 读回 Staging 验证（内容 = 预期？）
  Step 3: 旧文件 → 备份文件（.__hsr_backup_v1）
  Step 4: 写入 Primary 文件（SlotName）
  Step 5: 读回 Primary 验证
  Step 6: 删除 Staging 临时文件
```

每个阶段的失败场景：

| 场景 | 后果 | 如何恢复 |
|---|---|---|
| Staging 写入时崩溃 | 只有临时文件损坏 | 下次加载走 Primary 或 Backup |
| Primary 写入时崩溃 | 已有 Backup | 加载时用 Backup 恢复 |
| Staging 验证失败 | 磁盘满了 / 数据不对 | 不会覆盖旧存档，数据安全 |
| Primary 验证失败 | 同上 | 同上 |

这就是 `HSRSaveSubsystem.cpp` 中 `SaveToSlot()` 函数的完整逻辑链。

### Binary 序列化（FReader / FWriter）（Step 2）

你项目的序列化不依赖 UE 的反射（UPROPERTY），而是**手写每个字段的编码/解码**。

核心所在：`HSRSaveVersion.cpp` 第 46–69 行的 `FWriter` / `FReader`。

#### FWriter —— C++ 变量 → 字节

```cpp
struct FWriter {
    TArray<uint8>& B;          // 目标字节数组

    // 写 1 个字节
    void U8(uint8 V) { B.Add(V); }

    // 写多字节整数（小端序 Little-Endian）
    template<typename T>
    void I(T V) {
        for (int32 I = 0; I < sizeof(T); ++I)
            U8(static_cast<uint8>((V >> (I * 8)) & 0xff));
    }
    // 例：写 int32 0x12345678 → 字节序列 78 56 34 12

    // 写 GUID（四个 uint32 共 16 字节）
    void Guid(const FGuid& V) {
        I<uint32>(V.A); I<uint32>(V.B);
        I<uint32>(V.C); I<uint32>(V.D);
    }

    // 写 FName → UTF-8 字符串（长度前缀 + 内容）
    bool Name(const FName& V) {
        FString S = CanonicalName(V);      // 统一小写
        FTCHARToUTF8 Utf8(*S);
        I<uint32>(Utf8.Length());          // 先写长度（4 字节）
        B.Append(Utf8.Get(), Utf8.Length()); // 再写 UTF-8 字节
        return true;
    }

    // 写 double（先转 uint64 再写，检查有限性防止 NaN）
    bool D(double V) {
        if (!FMath::IsFinite(V)) return false;  // NaN/Inf 拒绝写入
        uint64 Bits;
        FMemory::Memcpy(&Bits, &V, sizeof(Bits));
        I<uint64>(Bits);
        return true;
    }
};
```

**小端序例子**：`int32 Value = 0x12345678`
```
sizeof(int32) = 4，循环 4 次：

I=0: Value >> 0  = 0x12345678 → & 0xff = 0x78 → 写入
I=1: Value >> 8  = 0x00123456 → & 0xff = 0x56 → 写入
I=2: Value >> 16 = 0x00001234 → & 0xff = 0x34 → 写入
I=3: Value >> 24 = 0x00000012 → & 0xff = 0x12 → 写入

序列化结果（文件中的字节）：78 56 34 12
```

#### FReader —— 字节 → C++ 变量

```cpp
struct FReader {
    const TArray<uint8>& B;   // 源字节数组
    int64 P = 0;              // 当前读取指针
    bool bOk = true;          // 读取过程是否有错误

    // 读 1 个字节
    uint8 U8() {
        return P < B.Num() ? B[P++] : (bOk = false, 0);
    }

    // 读多字节整数（小端序）
    template<typename T>
    T I() {
        U V = 0;
        for (int32 X = 0; X < sizeof(T); ++X)
            V |= static_cast<U>(U8()) << (X * 8);
        return static_cast<T>(V);
    }
    // 读取 78 56 34 12 → 拼回 0x12345678

    // 读 FName（先读长度，再读 UTF-8 字节 → FString）
    bool Name(FName& Out) {
        const uint32 N = I<uint32>();           // 读长度
        if (P + N > B.Num()) return bOk = false; // 越界检查
        FUTF8ToTCHAR Conv((ANSICHAR*)(B+P), N); // UTF-8 → TCHAR
        if (!IsCanonical(Conv)) return bOk = false;
        Out = FName(S);
        P += N;
        return true;
    }

    // 读 double
    bool D(double& Out) {
        const uint64 Bits = I<uint64>();
        FMemory::Memcpy(&Out, &Bits, sizeof(Bits));
        if (!FMath::IsFinite(Out)) return bOk = false;
        return true;
    }
};
```

FReader 和 FWriter 是**完全对称的**——写的时候怎么编码，读的时候就怎么解码。顺序必须一致：
```
写入：  I<int32>(Level) → I<int32>(Experience) → Name(CharacterId)
读取：  Level=R.I<int32>() → Experience=R.I<int32>() → R.Name(CharacterId)
```

#### 为什么不用 UPROPERTY 反射？

UE 内置的 `USaveGame` 反射序列化有什么问题？

1. **依赖 C++ 类型布局**：C++ 类里加个字段，旧存档自动读不出新格式
2. **无法做版本迁移**：没有 SchemaVersion 概念，`FCustomVersion` 只能做到"版本已知"，不能做"字段级别的转换"
3. **写入所有 UPROPERTY**：有些临时字段你也得跟着序列化，无法精确控制
4. **UObject 引用链复杂**：GC 引用、子对象、外链问题

手工 Binary 序列化的好处：

- **精确控制**：决定哪个字段写、以什么顺序写、怎么写
- **Schema 独立于 C++ 类型**：C++ 结构体改了，存档格式不变（显式 Migration 处理）
- **体积可控**：没有反射元数据，纯裸数据
- **确定性**：同一个数据写入 2 次产生完全一样的字节序列

### 规范排序（Canonical Order）

```cpp
// Profiles 按 CharacterId 字母序排序
P.Sort([](const auto&A, const auto&B) {
    return NameLess(A.State.CharacterId, B.State.CharacterId);
});

// 相同的数据 → 完全相同的二进制字节
// → 确定性 Checksum 比较
// → 可以判断两个存档是否内容一致（无需完整解析）
```

排序发生在序列化的那一步，而不是序列化之前。这样做的好处是：

1. **写入时验证重复**：排序后相邻元素比较，发现重复立即拒绝
2. **读取时验证排序**：读入时检查每个元素比前一个大，发现乱序则 `NonCanonical`

### 版本迁移（Migration）（Step 3）

```cpp
// HSRSaveVersion.cpp
EHSRSaveDecodeResult MigrateToCurrent(FHSRSaveData& D) {
    while (D.SchemaVersion < CurrentSchema) {  // 循环直到追上当前版本
        switch (D.SchemaVersion) {
            case 1:
                D.Equipment.Reset();           // V1 没有装备系统
                D.Inventory = FHSRInventorySaveData();  // V1 没有物品
                D.Rewards = FHSRRewardSaveData();       // V1 没有奖励
                ++D.SchemaVersion;             // 推进到 V2
                break;
            case 2:
                // V2 到 V3 数据结构不变，只是 SchemaVersion 数字增加
                ++D.SchemaVersion;
                break;
            case 3:
                D.Quests = FHSRQuestSaveData(); // V3 新增任务系统
                ++D.SchemaVersion;
                break;
            case 4:
                D.Map = FHSRMapSaveData();      // V4 新增地图系统
                ++D.SchemaVersion;
                break;
            case 5:
                ++D.SchemaVersion;              // V5→V6 无数据变更
                break;
        }
    }
}
```

这种迁移模式允许：

- **跳过版本升级**：V1 → V3 直接走两次循环
- **向后兼容**：旧存档被加载时自动升级到最新 Schema
- **反向不兼容**：最新版本写的存档不能被旧版本读（MinCompatible 控制）

### 恢复流程（Restore）（Step 4）

存档恢复并非简单的"把数值写回去"，而是：**验证 → 准备 → 投影 → 静默提交 → 通知**

```
LoadFromSlot(SlotName)
  │
  ├─ ① 从磁盘读取 Envelope + 解码 + 迁移
  │   （可能在 Primary 或 Backup 中选一个）
  │
  ├─ ② Validate(Candidate)     — 数据完整性检查（引用约束、重复、空值）
  │
  ├─ ③ CanPrepareSnapshot(Candidate) — 各子系统 PrepareRestore
  │   (验证存档中引用的所有 ID 在子系统定义中存在)
  │
  ├─ ④ LoadSnapshot(Candidate)
  │   │
  │   ├─ Profiles->PrepareRestore
  │   ├─ Party->PrepareRestore
  │   ├─ Equipment->PrepareRestore
  │   ├─ Inventory->PrepareRestore
  │   ├─ ← Equipment->ProjectRestore（装备投影）
  │   ├─ 比较新旧状态 → 计算 ChangedIds
  │   ├─ Profiles->CommitRestoreSilent
  │   ├─ Party->CommitRestoreSilent
  │   └─ ...其他 CommitRestore
  │
  └─ ⑤ 广播 RestoreCommitted → 各子系统 OnChanged 事件
```

为什么需要三步（Prepare → Project → Commit）？
- PrepareRestore：校验数据合法，转换存档格式为子系统内部格式
- ProjectRestore：处理跨域依赖（如装备 → 角色的引用完整性）
- CommitRestore：一步到位写入生效，如果中途失败则事务回滚

---

## 模块依赖图

```
UHSRSaveSubsystem（入口，负责协调）
  │
  ├─ UHSRCharacterProfileSubsystem — 角色经验、等级、命途
  ├─ UHSRPartySubsystem           — 队伍编成
  ├─ UHSREquipmentSubsystem       — 武器/圣遗物
  ├─ UHSRInventorySubsystem       — 物品背包
  ├─ UHSRRewardSubsystem          — 奖励领取记录
  ├─ UHSRQuestSubsystem           — 任务进度
  └─ UHSRMapSubsystem             — 地图探索状态
```

每个子系统负责自己的 ExportSaveData/PrepareRestore/CommitRestore 逻辑。SaveSubsystem 只做编排。

---

## 学习备忘

### 已学内容
- [x] 事务写入模型（Step 1）—— SaveToSlot 的 Staging/Backup/Primary 三级写入 + 读回验证
- [x] Binary 序列化（Step 2）—— FWriter/FReader 的小端序逐字段编码/解码
- [ ] 版本迁移（Step 3）
- [ ] 恢复流程（Step 4）
- [ ] 测试架构（Step 5）

### 待拓展方向
- Cloud Save：差异计算（diff）和合并策略（merge）
- 增量存档
- 多设备存档同步
