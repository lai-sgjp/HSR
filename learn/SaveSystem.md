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

#### 单字母变量对照表

HSRSaveVersion.cpp 中的单字母变量含义：

| 字母 | 含义 | 出现位置 | 例子 |
|---|---|---|---|
| `D` | **Data** — 存档数据 | `MigrateToCurrent(D)`、`EncodeEnvelopeAtUtc(D)` | `const FHSRSaveData& D` |
| `O` | **Output** — 输出的字节 | 写出的目标 | `EncodeCanonicalPayload(D, O)` |
| `P` | **Payload** — 有效载荷字节 | 编码时中间变量 | `TArray<uint8> P` |
| `B` | **Buffer/Bytes** — 原始字节数组 | FWriter/FReader 成员、DecodeEnvelope 参数 | `TArray<uint8>& B` |
| `H` | **Header** — 信封头 | DecodeEnvelope 可选输出 | `FHSRSaveEnvelopeHeader* H` |
| `N` | **Number** — 数量/长度 | FReader::Name、Count 函数 | `const uint32 N = I<uint32>()` |
| `P` | **Position** — FReader 读取位置 | FReader 成员 | `int64 P = 0` |
| `V` | **Value** — 值 | FWriter::I | `void I(T V)` |
| `U` | **Unsigned** — 无符号类型 | FWriter::I 内部 | `using U = std::make_unsigned_t<T>` |
| `X` | **Index** — 循环索引 | FWriter::I / FReader::I | `for (int32 X = 0; ...)` |

注意同名异义：
- `D()`（FWriter 方法）= 写 double；`D`（函数参数）= FHSRSaveData
- `B`（FWriter 成员）= 写入缓冲区；`B`（DecodeEnvelope）= 读入的字节

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

排序发生在序列化那一步，而不是序列化之前。这样做的好处是：

1. **写入时验证重复**：排序后相邻元素比较，发现重复立即拒绝
2. **读取时验证排序**：读入时检查每个元素比前一个大，发现乱序则返回 `NonCanonical`

---

### EncodeEnvelope: 结构体 → 完整的磁盘文件（Step 3）

`HSRSaveVersion.cpp` 第 117 行的 `EncodeEnvelopeAtUtc` 是"数据→磁盘字节"的完整流程：

```cpp
bool HSRSaveVersion::EncodeEnvelopeAtUtc(
    const FHSRSaveData& D,      // [输入] 要保存的数据
    const FString& Slot,        // [输入] 槽名
    int32 User,                 // [输入] 玩家索引
    const FGuid& Id,            // [输入] SaveId
    uint64 Gen,                 // [输入] Generation
    int64 Utc,                  // [输入] Unix 毫秒时间戳
    TArray<uint8>& O            // [输出] 完整的文件字节
)
```

内部流程分 4 步：

**第 1 步：把 FHSRSaveData 编码成 Payload**
```cpp
TArray<uint8> P;
if (!EncodeCanonicalPayload(D, P)) return false;
```
调用 `EncodeCanonicalPayload`——用 FWriter 把 Profiles、Party、Equipment、Inventory 等所有子系统数据编成二进制裸数据。

**第 2 步：写信封 Header（104 字节）**
```cpp
O.Reset();
FWriter W(O);

const uint8 Magic[8] = {'H','S','R','S','A','V','E',0};
O.Append(Magic, 8);              // Magic（8 字节）
W.I<uint16>(1);                  // FormatVersion（2 字节）
W.I<uint16>(104);                // HeaderSize（2 字节）
W.I<uint32>(D.SchemaVersion);    // Schema 版本（4 字节）
W.I<uint32>(1);                  // MinCompatibleSchema（4 字节）
W.I<uint16>(1);                  // PayloadCodecVersion（2 字节）
W.I<uint16>(0);                  // Flags（2 字节）
W.I<uint64>(P.Num());            // Payload 大小（8 字节）
W.Guid(Id);                      // SaveId（16 字节）
W.I<uint64>(Gen);                // Generation（8 字节）
W.I<int64>(Utc);                 // 时间戳（8 字节）
W.I<uint64>(MakeSlotIdentity(Slot, User)); // 槽身份哈希（8 字节）
for (int32 I = 0; I < 32; ++I)
    W.U8(0);                     // SHA-256 占位（32 字节，先写 0）
```

至此 Header 写完，第 72–103 字节是 32 个 0（预留校验和位置）。

**第 3 步：追加 Payload**
```cpp
O.Append(P);
```

此时完整的文件布局：
```
[0-7]     Magic         "HSRSAVE\0"         8 字节
[8-9]     FormatVersion  1                   2 字节
[10-11]   HeaderSize    104                  2 字节
[12-15]   SchemaVersion                      4 字节
[16-19]   MinCompatible                      4 字节
[20-21]   CodecVersion   1                   2 字节
[22-23]   Flags                               2 字节
[24-31]   Payload Size                       8 字节
[32-47]   SaveId (GUID)                     16 字节
[48-55]   Generation                         8 字节
[56-63]   UTC Timestamp                      8 字节
[64-71]   SlotIdentity                       8 字节
[72-103]  SHA-256（已清零）                  32 字节
[104~]    Payload                            N 字节
```

**第 4 步：算 SHA-256，回填到 Header**
```cpp
TArray<uint8> Digest;
ComputeSha256(O, Digest);                     // 算整个文件的哈希
FMemory::Memcpy(O.GetData() + 72, Digest.GetData(), 32); // 覆盖第 72 字节处的 32 个 0
```

为什么先清 0 再算？SHA-256 覆盖整个文件，包括校验和字段本身。如果校验和字段一开始不是 0，算出来的值依赖"校验和位置的初始值"——鸡生蛋问题。所以先清 0 算，算完再写。

---

### DecodeEnvelope: 字节 → 结构体（反向流程）

`HSRSaveVersion.cpp` 第 119–136 行是 `EncodeEnvelope` 的逆操作。每一步都可能失败，返回 `EHSRSaveDecodeResult`（13 种错误码）。

读取的验证链分为 6 个阶段：

```
阶段 1：格式完整性
  ① 文件够 104 字节吗？           → TooShort
  ② Magic 对得上吗？              → BadMagic

阶段 2：字段合理性
  ③ FormatVersion 支持吗？        → UnsupportedFormat
  ④ SaveId 有效吗？               → InvalidHeader
  ⑤ Generation 不为 0 吗？        → InvalidHeader
  ⑥ Payload Size 合理吗？         → InvalidSize

阶段 3：身份验证
  ⑦ SlotIdentity 匹配吗？         → SlotMismatch
  ⑧ SHA-256 校验通过吗？          → ChecksumMismatch

阶段 4：版本兼容
  ⑨ SchemaVersion > 当前？        → FutureSchema
  ⑩ SchemaVersion < 1？           → TooOld
  ⑪ MinCompatibleSchema 合理吗？  → InvalidHeader

阶段 5：Payload 解码
  ⑫ Payload → FHSRSaveData        → 各种解码错误
  ⑬ Payload 内 SchemaVersion 匹配？→ NonCanonical

阶段 6：版本迁移
  ⑭ MigrateToCurrent()            → MigrationFailed
```

关键代码对应：
```cpp
// SHA-256 验证（第 130 行）：
TArray<uint8> Copy = B;
FMemory::Memset(Copy.GetData() + 72, 0, 32);   // 校验和位清零
ComputeSha256(Copy, Digest);                     // 重新算哈希
FMemory::Memcmp(B.GetData() + 72, Digest.GetData(), 32)  // 对比

// 版本检查（第 132–133 行）：
if (X.SchemaVersion > CurrentSchema) return EHSRSaveDecodeResult::FutureSchema;
if (X.SchemaVersion < 1)               return EHSRSaveDecodeResult::TooOld;

// 解码 + 迁移（第 135 行）：
DecodeCanonicalPayload(P, O);          // 二进制 → 结构体
MigrateToCurrent(O);                   // 旧版本 → 当前版本
```

额外保护（第 135 行的中间检查）：
```cpp
if (O.SchemaVersion != X.SchemaVersion)
    return EHSRSaveDecodeResult::NonCanonical;
```
Payload 内部的 SchemaVersion 必须和 Header 里的一致——防止 Header 改成一个版本、Payload 改成另一个版本的"混合攻击"。

---

### 版本迁移（Migration）

```cpp
// HSRSaveVersion.cpp 第 115 行
EHSRSaveDecodeResult HSRSaveVersion::MigrateToCurrent(FHSRSaveData& D) {
    if (D.SchemaVersion < 1)  return EHSRSaveDecodeResult::TooOld;
    if (D.SchemaVersion > CurrentSchema) return EHSRSaveDecodeResult::FutureSchema;

    while (D.SchemaVersion < CurrentSchema) {   // 循环升到最新
        switch (D.SchemaVersion) {
            case 1:
                D.Equipment.Reset();                     // V1→V2：新增装备
                D.Inventory = FHSRInventorySaveData();   // V1→V2：新增背包
                D.Rewards = FHSRRewardSaveData();        // V1→V2：新增奖励
                ++D.SchemaVersion;
                break;
            case 2:
                D.Inventory = FHSRInventorySaveData();   // V2→V3：背包格式变了
                D.Rewards = FHSRRewardSaveData();
                ++D.SchemaVersion;
                break;
            case 3:
                D.Quests = FHSRQuestSaveData();          // V3→V4：新增任务
                ++D.SchemaVersion;
                break;
            case 4:
                D.Map = FHSRMapSaveData();                // V4→V5：新增地图
                ++D.SchemaVersion;
                break;
            case 5:
                ++D.SchemaVersion;                       // V5→V6：纯数字增长
                break;
            default:
                return EHSRSaveDecodeResult::MigrationFailed;
        }
    }
    // 迁移后完整性验证
    return EHSRSaveDecodeResult::Success;
}
```

迁移模式的特点：

- **跳过版本升级**：V1 → V3 直接走两次循环（V1→V2→V3）
- **向后兼容**：旧存档加载时自动升级到最新 Schema
- **反向不兼容**：新版本写的存档不能被旧版本读
- **新系统归零**：新增系统时旧存档没有对应数据，重置为空状态——等价于"你更新了游戏后，新增系统从零开始"

---

### 整个读取链的总结

```
磁盘文件字节
  → DecodeEnvelope
       ├─ Magic 验证          → 是 HSR 存档吗？
       ├─ SHA-256 验证        → 文件被改过吗？
       ├─ SlotIdentity        → 是这个槽吗？
       └─ SchemaVersion       → 兼容吗？
  → MigrateToCurrent           → 旧版数据升到最新
  → Validate                   → 数据内部引用完整吗？
  → CanPrepareSnapshot         → 各子系统能接受吗？
  → LoadSnapshot               → 真正写回内存
```

### 恢复流程（Restore）（Step 4）

`HSRSaveSubsystem.cpp` 第 193–205 行的 `LoadSnapshot` 是存档系统最复杂的函数。存档恢复并非简单的"把数值写回去"，而是：**验证 → 转换 → 预检查 → 投影 → 差异计算 → 提交 → 通知**

#### 阶段 1：前置校验（第 195 行）

```cpp
if (Candidate.SchemaVersion < 1 || Candidate.SchemaVersion > 5)
    return EHSRSaveResult::UnsupportedSchema;
if (!Validate(Candidate))
    return EHSRSaveResult::InvalidData;
```
版本范围硬编码检查 + 引用完整性验证（角色 ID 存在、装备 ID 唯一、背包物品有定义）。

#### 阶段 2：格式转换（第 196–199 行）

```cpp
// 存档 DTO → 子系统内部格式
TArray<FHSRCharacterProfileSnapshot> SavedProfiles;
for (const auto& D : Candidate.Profiles) {
    FHSRCharacterProfileSnapshot P;
    P.RuntimeState = D.State;
    P.RuntimeRevision = D.RuntimeRevision;
    SavedProfiles.Add(MoveTemp(P));
}

// 声明各子系统的输出容器
TMap<FName, FHSRCharacterProfileSnapshot> ProfileCandidate;
FHSRPartySnapshot PartyCandidate;
TMap<FGuid, FHSREquipmentRestoreState> EquipmentCandidate;
FHSRInventoryRestoreState InventoryCandidate;
// ...
```
只声明变量 + 转换格式，还没有写回任何子系统。

旧版本兼容处理：
```cpp
Equipment->PrepareRestore(
    Candidate.SchemaVersion == 1 ? EmptyEquipment : Candidate.Equipment,
    EquipmentCandidate
);
```
SchemaVersion 为 1 的存档没有装备，传空数组。

#### 阶段 3：PrepareRestore（第 200 行）

```cpp
if (!Profiles->PrepareRestore(...) || !Party->PrepareRestore(...) || ...)
    return EHSRSaveResult::InvalidData;
```

七个子系统逐一调用 PrepareRestore。每个做三件事：

```
① 验证存档中每个 ID 在子系统的定义（DataAsset）中存在
   → "Character.A" 必须有对应的角色定义
   → "Weapon_Sword_01" 必须有对应的装备定义

② 把存档 DTO 转换为子系统内部的 RestoreState 格式
   → FHSREquipmentSaveDto → TMap<FGuid, FHSREquipmentRestoreState>

③ 验证失败 → 返回 false → 整个 LoadSnapshot 失败
```

任何子系统返回 false 都**直接拒绝，不修改任何子系统状态**。

#### 阶段 4：ProjectRestore（第 201 行）

```cpp
if (!Equipment->ProjectRestore(EquipmentCandidate))
    return EHSRSaveResult::InvalidData;
```

**ProjectRestore 不是"角色名转 GUID"——那一步在 PrepareRestore 已经做完了。**

ProjectRestore 的真正含义是：**装备数据存档恢复后，需要有人把装备的 stat 重新 Apply 到战斗角色身上**。

它的实现是一个 **Delegate**，由 `UHSRBattleCoordinator` 在第 248 行绑定：

```cpp
// HSRBattleCoordinator.cpp
Equipment->SetRestoreProjection(
    FHSREquipmentRestoreProjection::CreateUObject(this, &ThisClass::ProjectEquipmentRestore)
);
```

为什么 Equipment 子系统自己做不到？因为它不依赖 GAS——装备的 stat（ATK+10、DEF+5）需要通过 **GameplayEffect** Apply 到角色的 ASC 上，这件事只有 BattleCoordinator 知道怎么做。

`ProjectEquipmentRestore` 做的事（第 1938–1980 行）：

```
① 遍历 Candidate 中的所有装备实例
② 对每个实例调用 UHSREquipmentStatAggregator::Aggregate → 算出 stat 聚合值
③ 把聚合值以 GE 的形式 Apply 到对应角色的 ASC
④ 对比 EquipmentProjectionStates（旧状态）
   如果旧状态有但新状态没有 → 移除对应的 GE
   如果新旧都一样 → 跳过（你的记忆是对的）
```

注意第 1207 行：
```cpp
EquipmentProjectionStates.Empty();  // 战斗初始化时清空
```
战斗初始化时会清空投影状态，所以下次 `ProjectRestore` 进来时，所有装备都会被视作"新增"并重新 Apply。

**其他系统为什么不需要 ProjectRestore：**

| 子系统 | 需要？ | 原因 |
|---|---|---|
| Party | 不需要 | 队伍数据只是"哪个角色在哪个槽"，不涉及 stat |
| Profile | 不需要 | 等级/经验是独立数值，不依赖跨域 |
| Equipment | **需要** | 装备的 stat 需要通过 GAS Apply 到角色身上 |
| Inventory | 不需要 | 背包物品不直接影响战斗 stat |
| Quest | 不需要 | 任务进度不涉及跨域 |
| Map | 不需要 | 地图状态不涉及跨域 |

**两个不同的装备 stat 生效路径：**

```
进入战斗时（正常流程）：
  BattleCoordinator 初始化 → RefreshCharacterProgression
    → 从 ProfileSubsystem 拉取当前数据（含装备聚合值）
    → Apply GE 到 ASC
  → 不经过 ProjectRestore

存档恢复时（SaveSubsystem 流程）：
  LoadFromSlot → LoadSnapshot → ProjectRestore
    → 把存档中的装备数据重新聚合并 Apply GE
    → 由 SaveSubsystem 编排，BattleCoordinator 执行
```

#### 阶段 5：差异计算（第 202–204 行）

```cpp
// Profile：SaveSubsystem 自己算
TArray<FName> ChangedIds;
for (const auto& It : ProfileCandidate) {
    FHSRCharacterProfileSnapshot Old;
    Profiles->GetProfileSnapshot(It.Key, Old);
    if (旧值 != 新值) ChangedIds.Add(It.Key);
}

// Party：对比 Revision + 槽位
bool PartyChanged = OldParty.Revision != PartyCandidate.Revision;
for (int32 I = 0; !PartyChanged && I < OldParty.Slots.Num(); ++I)
    PartyChanged = OldParty.Slots[I].CharacterId != PartyCandidate.Slots[I].CharacterId;

// 装备：逐个字段对比
TSet<FGuid> EquipmentChanged;
// 对比新旧装备的 InstanceId、DefinitionId、Kind、Slot、EnhancementLevel、Modifiers...

// 其他子系统：用 IsRestoreDifferent
const bool bInventoryChanged = Inventory->IsRestoreDifferent(InventoryCandidate);
const bool bRewardsChanged   = Reward->IsRestoreDifferent(RewardCandidate);
const bool bQuestsChanged    = Quest->IsRestoreDifferent(QuestCandidate);
const bool bMapChanged       = Map->IsRestoreDifferent(MapCandidate);
```

为什么在提交前算差异？因为 Commit 后数据就变了，计算不出"跟原来有什么不同"。

Profile 和 Party 的差异由 SaveSubsystem 自己算，因为需要精确知道"哪个角色变了"。其他子系统用 `IsRestoreDifferent` 接口，内部自己做简单比较。

#### 阶段 6：提交 Commit（第 205 行）

```cpp
// 写入生效——所有子系统同时提交
Profiles->CommitRestoreSilent(MoveTemp(ProfileCandidate));
Party->CommitRestoreSilent(MoveTemp(PartyCandidate));
Equipment->CommitRestore(EquipmentCandidate);
Inventory->CommitRestore(MoveTemp(InventoryCandidate), false);
Reward->CommitRestore(MoveTemp(RewardCandidate), false);
Quest->CommitRestore(MoveTemp(QuestCandidate), false);
Map->CommitRestore(MoveTemp(MapCandidate), false);
```

**两种 Commit 的区别：**

| 提交方式 | 谁算差异 | 谁发通知 | 为什么 |
|---|---|---|---|
| `CommitRestoreSilent` | SaveSubsystem 提前算好 | SaveSubsystem 手动发 | 需要精确告诉下游"哪个角色变了" |
| `CommitRestore` | 子系统内部自己算 | 都发（双通道） | 子系统自己知道怎么通知 |

Profile（角色数据）和 Party（队伍）的差异是结构化的——SaveSubsystem 要精确告诉下游"Character.A 和 Character.B 的等级变了"。所以 SaveSubsystem 自己算差异，用 Silent 提交，然后手动调用 Notify。

装备、背包等其他系统只需知道"变了/没变"，子系统内部自己处理差异计算和通知。

提交后更新 SaveSubsystem 自身的内存快照：
```cpp
Current = Candidate;
// 旧版本没有的数据 → 重置为空
if (Candidate.SchemaVersion < 3) {
    Current.Inventory = FHSRInventorySaveData();
    Current.Rewards = FHSRRewardSaveData();
}
```

#### 阶段 7：广播通知（第 205 行尾部）

```cpp
// 子系统级别的通知
Profiles->NotifyRestored(ChangedIds);
if (PartyChanged) Party->NotifyRestored();
Equipment->NotifyRestored(EquipmentChanged);
if (bInventoryChanged) Inventory->OnInventoryChanged().Broadcast(...);
if (bRewardsChanged)   Reward->OnRewardRestored().Broadcast(...);
if (bQuestsChanged)    Quest->OnQuestRestored().Broadcast(...);
if (bMapChanged)       Map->OnMapStateChanged().Broadcast(...);

// SaveSubsystem 级别的全局通知
FHSRRestoreCommitInfo Info;
Info.ChangedCharacterIds = ChangedIds;
Info.bPartyChanged = PartyChanged;
// ...
Info.TransactionRevision = ++RestoreTransactionRevision;
RestoreCommitted.Broadcast(Info);
```

双通道通知：
- **子系统通知**：各 UI/Gameplay 监听者收到精确的局部变更
- **全局通知**（`RestoreCommitted`）：一次广播包含所有变更摘要，各方按需响应

#### 设计原则

**在 Commit 之前，任何事情都可以失败回滚。** 第 ①–⑤ 步（校验 → 转换 → PrepareRestore → ProjectRestore → 差异计算）没有修改任何子系统状态。只有第 ⑥ 步 Commit 开始才会真正写入数据。

```
LoadSnapshot 整体流程：
  ┌───────────────────────────────────────┐
  │ ① 版本检查 + Validate                 │
  │    （数据本身合法吗？）                 │
  ├───────────────────────────────────────┤
  │ ② 格式转换                            │
  │    （存档 DTO → 子系统内部格式）        │
  ├───────────────────────────────────────┤
  │ ③ PrepareRestore（×7 子系统）          │
  │    （ID 都存在吗？任一失败就拒绝）       │
  ├───────────────────────────────────────┤
  │ ④ ProjectRestore（仅装备）            │
  │    （跨域引用解析）                     │
  ├───────────────────────────────────────┤
  │ ⑤ 差异计算                            │
  │    （改了什么？对比新旧值）              │
  ├───────────────────────────────────────┤
  │ ⑥ Commit（×7 子系统）                 │
  │    （实际写入生效）                     │
  ├───────────────────────────────────────┤
  │ ⑦ 广播通知                            │
  │    （子系统通知 + 全局 RestoreCommitted）│
  └───────────────────────────────────────┘
```

---

### 子系统 Export/Import 模式

每个子系统对 SaveSubsystem 暴露**最小接口**，遵循统一模式：

```
              Export
  子系统内部状态 ────────→ 存档 DTO（纯数据，无引用）

              PrepareRestore          CommitRestore
  存档 DTO ──────────→ RestoreState ──────────→ 子系统内部状态
                         (验证 + 转换)            (写入生效)

              NotifyRestored
  子系统内部状态 ──────────→ 广播通知（事件）
```

#### PartySubsystem（最简单）

**Export — GetSnapshot（第 111 行）：**
```cpp
bool UHSRPartySubsystem::GetSnapshot(FHSRPartySnapshot& Out) const {
    Out.Slots = Slots;              // 直接复制队伍槽位
    Out.Revision = Revision;        // 当前版本号
    return true;
}
```

**PrepareRestore — 验证（第 116 行）：**
```cpp
bool UHSRPartySubsystem::PrepareRestore(const FHSRPartySnapshot& Saved, FHSRPartySnapshot& Out) const {
    if (Saved.Slots.Num() != Capacity || Saved.Revision < 0) return false;
    TSet<FName> Seen;
    for (const auto& Slot : Saved.Slots) {
        if (Slot.IsEmpty()) continue;
        if (Seen.Contains(Slot.CharacterId)) return false;
        if (!IsKnownProfile(Slot.CharacterId)) return false;
        Seen.Add(Slot.CharacterId);
    }
    Out = Saved;    // 通过验证，设为候选
    return true;
}
```

**CommitRestoreSilent — 写入（第 28 行，.h 内联）：**
```cpp
void CommitRestoreSilent(FHSRPartySnapshot&& Candidate) {
    Slots = MoveTemp(Candidate.Slots);
    Revision = Candidate.Revision;
    // 不广播事件
}
```

**NotifyRestored — 通知（第 29 行）：**
```cpp
void NotifyRestored() { PartyChanged.Broadcast(Revision); }
```

#### EquipmentSubsystem（最复杂）

**Export — ExportSaveData：**
```cpp
void UHSREquipmentSubsystem::ExportSaveData(TArray<FHSREquipmentSaveDto>& Out) const {
    for (const auto& Entry : EquipmentMap) {
        FHSREquipmentSaveDto Dto;
        Dto.DefinitionId = Entry.Value.DefinitionId;
        Dto.InstanceId = Entry.Key;
        Dto.CharacterId = /* FName 映射 */;
        Dto.EnhancementLevel = Entry.Value.Level;
        Out.Add(MoveTemp(Dto));
    }
}
```

**PrepareRestore — 验证 + 格式转换：**
```cpp
bool UHSREquipmentSubsystem::PrepareRestore(
    const TArray<FHSREquipmentSaveDto>& In,
    FHSREquipmentRestoreMap& Out
) const {
    for (const auto& Dto : In) {
        if (!HasDefinition(Dto.DefinitionId)) return false;   // 定义存在？
        if (Out.Contains(Dto.InstanceId))     return false;    // 唯一？
        FGuid OwnerGuid = HSRCharacterGuidFromProfileName(Dto.CharacterId);
        if (!OwnerGuid.IsValid())             return false;   // 角色存在？
        FHSREquipmentRestoreState State;
        State.DefinitionId = Dto.DefinitionId;
        State.OwnerGuid = OwnerGuid;           // FName → FGuid
        State.EnhancementLevel = Dto.EnhancementLevel;
        Out.Add(Dto.InstanceId, MoveTemp(State));
    }
    return true;
}
```

**CommitRestore — 写入 + 内部通知：**
```cpp
void UHSREquipmentSubsystem::CommitRestore(const FHSREquipmentRestoreMap& Candidate) {
    EquipmentMap.Empty();
    for (const auto& Entry : Candidate) { /* 安装每个装备 */ }
    // 内部处理差异检测和通知
}
```

#### 三个子系统对比

| 操作 | Party | Profile | Equipment |
|---|---|---|---|
| Export | `GetSnapshot` | `ExportProfiles` | `ExportSaveData` |
| PrepareRestore | 验证角色可识别 | 验证角色定义存在 | 验证定义 + GUID 转换 |
| ProjectRestore | 不需要 | 不需要 | 需要（GAS stat Apply） |
| Commit | **Silent** | **Silent** | 正常（内部通知） |
| Notify | SaveSubsystem 手动调 | SaveSubsystem 手动调 | 双重（内部 + 全局） |

核心设计：**模板方法模式**——每个子系统实现自己的 PrepareRestore/CommitRestore，SaveSubsystem 决定"什么时候调、调几次、以什么顺序调"。

---

### 测试架构（Step 5）

`Source/HSR/Tests/HSRSaveSubsystemTests.cpp` 中的三个测试类覆盖三个维度：

#### 测试 1：`HSR.Save.V1` —— 纯逻辑测试

不涉及磁盘，在内存中测试 LoadSnapshot 的正确性：

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRSaveSubsystemTest, "HSR.Save.V1", ...);
```

测试要点：

| 测试场景 | 验证 |
|---|---|
| SaveSnapshot 捕获 | 返回 Success |
| 无变更加载 | `RestoreEvents == 0`（幂等性） |
| 修改后恢复 | 经验回到 0、队伍回到 A |
| 重复加载 | 事件数不增加 |
| 负数经验 → 拒绝 | `InvalidData` |
| 重复角色 ID → 拒绝 | `InvalidData` |
| 不存在的角色 → 拒绝 | `InvalidData` |
| 未知 SchemaVersion → 拒绝 | `UnsupportedSchema` |
| 坏数据拒绝后数值不变 | Profile/Party 保持修改后的值 |
| 原子性 | 观察者回调中验证所有子系统同时更新 |

第 19 行的原子性测试——在事件回调中验证"所有子系统要么都更新了，要么都没更新"：
```cpp
bool ObserverAtomic = true;
auto Observe = [&]() {
    ObserverAtomic &=
        Profiles->...Experience == 0 &&      // 应还是旧值
        Party->...CharacterId == "A" &&      // 应还是旧值
        Save->...CharacterId == "A";         // 应还是旧值
};
```

#### 测试 2：`HSR.Save.DiskV1` —— 磁盘读写测试

使用 `UGameplayStatics::SaveDataToSlot` 真实写入磁盘再读回：

```cpp
// 新槽不存在
TestEqual("missing", Save->LoadFromSlot(Slot, User), EHSRSaveResult::SlotNotFound);

// 写入 → 读回
Save->SaveToSlot(Slot, User);
Save->LoadFromSlot(Slot, User);
TestEqual("disk profile restored", P.RuntimeState.Experience, 0);

// 旧 USaveGame 格式兼容
UGameplayStatics::SaveGameToSlot(Wrong, Slot, User);
TestEqual("class mismatch", Save->LoadFromSlot(Slot, User), EHSRSaveResult::ClassMismatch);
// → Magic 不对 → 尝试旧格式 → 类型不匹配者
```

#### 测试 3：`HSR.Save.DiskFailures` —— 事务故障注入测试

最精妙的测试——通过 `SetDiskFailureInjection` 模拟各种写入故障：

```cpp
Save->SetDiskFailureInjection(
    bool bCreate,   // 模拟 Capture 阶段失败
    bool bSave,     // 模拟 StagingWrite 阶段失败
    bool bLoad      // 模拟 Load 失败
);
```

对应源码中的条件编译：
```cpp
// HSRSaveSubsystem.cpp 第 215 行
#if WITH_DEV_AUTOMATION_TESTS
if (bInjectCreateFailure) return EHSRSaveResult::CreateFailed;
#endif
```

测试场景：

| 注入点 | SaveToSlot 结果 | 旧文件是否受影响 |
|---|---|---|
| Capture 失败（创建快照） | `CreateFailed` | 不受影响 |
| StagingWrite 失败 | `SaveFailed` | 不受影响（旧文件内容不变） |
| Load 失败 | `LoadFailed` | 内存数据不变 |

通过 `AssertUnchanged` lambda 验证数据完整性：
```cpp
auto AssertUnchanged = [&](const TCHAR* Label) {
    TestEqual(Label, P.RuntimeState.Experience, 100);         // 内存值不变
    TestEqual("current experience unchanged",
        Save->GetSnapshot().Profiles[0].State.Experience, 0); // 快照不变
    TestEqual("profile events unchanged", PE, 0);             // 事件不触发
};
```

#### 测试架构总结

| 测试 | 覆盖内容 |
|---|---|
| **Save.V1** | 快照捕获、幂等性、变更恢复、重复不触发、坏数据拒绝、原子性 |
| **Save.DiskV1** | 磁盘往返、SlotNotFound、旧格式兼容、坏 schema 拒绝 |
| **Save.DiskFailures** | 事务故障注入、旧文件不被覆盖、坏数据拒绝后数据不变 |

核心验证点：
1. **好数据能正常读回**
2. **坏数据被拒绝，且不改变任何现有状态**
3. **写入过程中任何一步失败，旧文件不受影响**
4. **如果数据没变，不触发任何事件通知**

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

## 拓展方向：Cloud Save（差异计算与合并策略）

### 核心问题

```
设备 A → 上传存档 → 云服务器 → 下载 → 设备 B
         ↑                               ↑
    写本地时同步                    读之前先同步

时间线冲突示例：
T1: 设备 A 和 B 存档相同（Level=10, 队伍=[A, B]）
T2: 设备 A 离线升级 → Level=15
T3: 设备 B 离线改队伍 → 队伍=[C, D]
T4: 设备 A 上传 → 服务器：Level=15, 队伍=[A, B]
T5: 设备 B 上传 → 覆盖！队伍=[C, D] 但 Level 回到 10
→ A 的升级被丢了！
```

### 三种主流策略

#### 策略 1：Last-Write-Wins（最后写入胜出）

**做法**：谁最后上传就用谁的，直接覆盖。
**优点**：实现最简单
**缺点**：会丢数据
**适用**：存档内容简单、单设备为主（任天堂 Switch 云存档）

#### 策略 2：版本号 + 冲突检测

利用已有的 `SaveId` + `Generation`：
```
设备 A：{SaveId: "abc-123", Generation: 5}
设备 B：{SaveId: "abc-123", Generation: 4}
         ↑ 同一个 SaveId 说明是同一存档链
```

上传时服务器比对 Generation：
- 客户端 Gen > 服务器 Gen → 直接接受
- 客户端 Gen <= 服务器 Gen → 进入冲突解决

冲突解决的三种做法：

| 做法 | 说明 | 适用 |
|---|---|---|
| 拒绝 + 用户手动选 | 返回两个版本让用户挑一个 | PC 单机（巫师 3） |
| **自动合并（推荐）** | 按字段策略自动合并 diff | 手游、交叉进度（原神） |
| 版本链 | 服务器保留所有历史分支 | 协作型应用（Google Docs） |

#### 策略 3：CRDT（Conflict-Free Replicated Data Type）

**核心思想**：每个操作可交换，不管顺序如何，最终结果一致。
**优点**：不需要服务器做冲突解决，数学保证最终一致
**缺点**：实现复杂、Tombstone（墓碑标记）导致存储膨胀
**适用**：多人实时协作（Figma、Google Docs）

### 推荐方案：版本号 + 字段级合并 + 增量 Diff

非实时同步的单机游戏，CRDT 过于复杂，纯 Last-Write-Wins 丢失进度。

#### 字段级合并策略

按你的 `FHSRSaveData` 结构：

| 字段 | 推荐策略 | 理由 |
|---|---|---|
| `Profiles[].Level` | 取最大值 | 等级不会降 |
| `Profiles[].Experience` | 取最大值 | 经验不会降 |
| `Profiles[].SkillLevels` | 取最大值 | 技能等级不会降 |
| `PartySlots[]` | 以最后主动操作的设备为准 | 队伍编成是主动选择 |
| `Inventory.Stacks` | 并集，相同物品数量相加 | 物品不会凭空消失 |
| `Inventory.UniqueItems` | UUID 去重合并 | 唯一物品不重复 |
| `Equipment` | 以最后主动操作的设备为准 | 装备配置是主动选择 |
| `Quest.States` | 进度取更高者 | 任务不会倒退 |
| `Quest.bRewardClaimed` | 取 true | 领了就是领了 |
| `Map.ExplorationFlags` | 并集 | 探索过就是探索过 |
| `Map.UnlockedRegionIds` | 并集 | 解锁不会回锁 |

#### 增量 Diff

上传时不是传整个存档，而是只传变化部分：

```
{
  baseGeneration: 4,            // 基于哪个版本做的修改
  changes: {
    profiles: ["Character.A"],  // 只传变了的部分
    party: false,
    inventory: true,
  },
  delta: { ... }                // 只包含变化的字段
}
```

#### 完整同步流程

```
设备 A                                  云服务器
  │                                       │
  ├─ SaveToSlot(Local)                    │
  ├─ SyncUpload()                         │
  │   ├─ 算 Diff vs 上次上传的基线         │
  │   └─ POST /sync {diff, Gen} ────────► │
  │                                       ├─ 版本比对
  │                                       │  无冲突 → 合并 Diff
  │                                       │  有冲突 → 返回 CONFLICT
  │◄──────────────── {ok / conflict} ─────┤
  │                                       │
  ├─ 如果是 CONFLICT                      │
  │   ├─ 下载服务器版本                    │
  │   ├─ 本地按字段策略合并                │
  │   └─ 重新上传合并结果 ────────────────► │
```

### 推荐学习顺序（不可改动项目代码的前提下）

1. **Merge 的边界情况**（推荐首选）——纯理论设计：什么时候不该合并、什么是不可逆操作
2. **Diff 的数据结构设计**——增量上传的内容格式，纯思路
3. **项目改哪几层**——架构推演，不动代码
4. **行业案例分析**——原神、Minecraft Realms

### 待拓展方向
- [x] Cloud Save 基础：三种主流策略 + 推荐方案（版本号+字段级合并+增量Diff）
- [x] 方向 3：Merge 边界情况（不可逆操作、双边冲突、依赖顺序、删除、属性刷新）
- [x] 方向 2：Diff 数据结构设计（增量上传内容格式）
- [ ] 方向 4：行业案例分析（原神、Minecraft Realms）
- 增量存档
- 多设备存档同步
