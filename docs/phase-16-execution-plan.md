# Phase 16 详细执行计划：存档总整合、版本迁移与失败恢复

> 基线：2026-07-26；Phase 15=`Ready with inherited follow-ups`。
> Gate 0：四角色已完成只读规划。Reviewer 首轮与窄范围复核均为 `REVISE`；本文件冻结全部阻塞合同，等待最终复核。通过后只允许建立 P16-001 任务卡，不构成实现授权。

## 1. 当前事实与证据边界

- Phase 15：fresh Development Editor Rebuild 24/24；`HSR.Map` 4/4、`HSR.Save` 10/10、`HSR.BattleReturn` 1/1；A↔B、Battle Return、Save v5 冷恢复由用户验证。
- Save v5 已整合 Profile、Party、Equipment、Inventory、Reward ledger、Quest、Map DTO，采用各领域 Export/Prepare/IsDifferent/Commit；Load 不直接旅行。
- 当前仍直接 `SaveGameToSlot` / `LoadGameFromSlot`，没有独立 envelope/header、显式逐版迁移、完整性校验、staging/backup/readback 或恢复来源诊断。
- 现有 v1-v4 测试是在当前 `UHSRSaveGame` 布局上改变版本号，不等于真实历史二进制兼容。必须使用可追溯 golden fixtures，或从稳定自定义 payload 起承诺兼容。
- UE5.6 本地头文件已确认 `SaveGameToMemory`、`LoadGameFromMemory`、`SaveDataToSlot`、`LoadDataFromSlot` 存在；这不证明平台槽位 replace/rename 原子性。
- 故障注入只证明受控阶段故障，不外推 OS 强杀、断电、真实磁盘满/权限/文件锁、云或跨平台。

## 2. 阶段唯一最终结果

玩家在 Map B 形成可区分的角色、队伍、装备、背包、奖励账本、任务和地图状态后保存；关闭并重开 Editor 后，从有效 Primary 恢复完整一致快照。Primary 损坏或不兼容时，只从完整验证通过的 Backup 恢复，并返回可诊断来源与原因。重复 Load 不重复发奖、装备效果或广播。

“完整”只指现有领域 authority。宝箱/持久敌人若由 Map `ExplorationFlags` 或 resolved stable ID 表达，则记录映射；若没有 authority，则列为 follow-up，不为勾选 roadmap 新造 Actor 持久化系统。

## 3. 四层所有权与数据流

`Domain Runtime -> pure-value Save DTO -> versioned Envelope/Header + payload -> Primary/Staging/Backup slots`

恢复顺序固定为：`read/classify -> envelope validate -> deserialize -> step migration -> cross-domain validate -> prepare all -> commit all -> notify actual diffs`。

- Gameplay subsystem 唯一拥有 Runtime，负责 Export、Prepare、IsDifferent、Commit 与投影重建。
- `UHSRSaveSubsystem` 只编排 capture/prepare/commit、重入门禁和槽位事务。
- `HSRSaveVersion.*` 只处理纯 DTO 的 `N -> N+1` 迁移、版本分类与迁移后校验，不访问 Runtime。
- Disk transaction 只处理 Primary/Staging/Backup 写入、读回、轮换、清理和候选选择。
- Settings 属于 `GameUserSettings`/Config；Battle transient、普通 travel pending、battle-return pending 不保存。
- 禁止保存 Actor、Widget、World、Subsystem、UObject 指针、ASC、AttributeSet、GE Handle。Definition 只存稳定 ID。

Schema version 与 envelope/file-format version 是两个维度。Header 至少含 magic/format ID、format/schema/min-compatible version、payload size、save ID/generation、UTC timestamp、slot identity、integrity checksum。Build version 仅诊断；checksum 只证明完整性，不宣称防篡改。

## 4. On-disk format v1 冻结合同

稳定兼容承诺从 `envelope format v1 + payload codec v1 + schema v6` 开始。Envelope 是交给 `SaveDataToSlot` 的最外层自定义字节，不嵌在 `UHSRSaveGame` 对象序列化中。现有 v1-v5 UE SaveGame 文件只进入独立 legacy importer，属于 best-effort；在获得真实历史 fixture 前不承诺其二进制兼容。

Envelope 固定 104 bytes、little-endian、无编译器 struct padding，按下列 offset 顺序读写：

| Offset | 宽度 | 字段 |
|---:|---:|---|
| 0 | 8 | ASCII magic `HSRSAVE\0` |
| 8 | 2 | format version=`1` |
| 10 | 2 | header size=`104` |
| 12 | 4 | schema version |
| 16 | 4 | minimum-compatible schema |
| 20 | 2 | payload codec=`1` |
| 22 | 2 | flags；v1 必须为 0 |
| 24 | 8 | payload size |
| 32 | 16 | save ID；GUID 四个 `uint32` 依次 little-endian |
| 48 | 8 | generation；从 1 开始严格递增 |
| 56 | 8 | signed Unix UTC milliseconds |
| 64 | 8 | slot identity：规范化 UTF-8 `SlotName + ':' + UserIndex` 的 FNV-1a-64 |
| 72 | 32 | SHA-256 integrity digest |

SHA-256 覆盖 `header[0..71] + 32 个零字节 + payload`；验证时先将 digest 区视为零。它只用于完整性，不用于认证或防篡改。

Payload codec v1 是 canonical DTO binary，不使用 UObject/USaveGame 序列化：整数为固定宽度 little-endian；bool 为 `uint8` 0/1；浮点为 UE 当前值转换后的 IEEE-754 `double` 位模式，拒绝 NaN/Infinity 并把 `-0` 规范为 `+0`；字符串/FName 为 `uint32 byte length + UTF-8 bytes`，FName 用稳定 ID 的 display string 且解码后按 FName 相等语义检查重复；数组为 `uint32 count + 固定字段顺序 records`；无隐式 padding、指针或递归对象。所有 domain record 的字段顺序及新增字段默认值必须在 `docs/save-system-design.md` 随 schema 固定。

硬上限：payload 16 MiB、任一数组 65,535 项、任一 UTF-8 字符串 4,096 bytes；任一超限直接拒绝，禁止截断。读取分类顺序固定为：参数/slot read → 最小长度 → magic → format/header size/flags/codec → payload size 与总长度/上限 → slot identity → SHA-256 → schema future/too-old → canonical decode/计数与字符串上限 → 相邻迁移 → Definition/cross-domain validation → prepare。首个失败原因即结构化主原因，后续不得继续解析或写 Runtime。

SlotName 规范化规则为：非空、不得包含 `:`，保持输入 Unicode code points 原样，不做大小写折叠或 Unicode normalization；slot identity 输入是其 UTF-8 bytes、单个 ASCII `:`、UserIndex 的无前导零十进制 ASCII。Save ID 标识一条存档谱系；同一显式保存链沿用 Save ID，新建/覆盖不兼容谱系时生成新 ID。Primary 与其 Backup 必须 Save ID 相同且 Backup generation 小于 Primary；不匹配分类为 `LineageMismatch`。generation=0、同谱系 Backup generation 大于等于 Primary 分类为 `InvalidGeneration`。当 Primary 自身无效而无法可信取得 lineage 时，可独立完整验证 Backup 并恢复，但必须报告 `PrimaryUntrusted`，不得用坏 Primary 的字段否决好 Backup。

P16-001 验收必须包含固定 golden byte vectors：header offset/endianness、canonical payload 确定性、SHA-256 scope、size/count/string 上限、slot/lineage/generation mismatch、分类优先级与 encode→decode→encode 字节完全一致。

## 5. Authority 覆盖审计

| Authority | 持久纯值 | 稳定引用 | 恢复规则 |
|---|---|---|---|
| Character Profile | 等级、经验、revision | CharacterDefinitionId | prepare 后重建成长投影 |
| Party | 成员/编队、revision | CharacterDefinitionId | 缺 Definition 拒绝整档 |
| Equipment/Relic | instance、slot、强化、set source、revision | Equipment/Relic DefinitionId | 不存 GE Handle；重建唯一来源 |
| Inventory | stack/unique、currency、revision | ItemDefinitionId | 重复/非法数量拒绝 |
| Reward | receipt/claim ledger、revision | Reward/transaction ID | 重复 Load 不重复发奖 |
| Quest | objective/completed/claimed、revision | QuestDefinitionId | 缺 Definition 不丢行，拒绝整档 |
| Map | Map/Arrival、Region/Teleport、Flags、revision | Map/Teleport/Region ID | Load 不旅行 |
| Chest/Enemy | 当前无通用独立 authority | 仅已有 flag/resolved ID | 不保存 Actor；缺口 follow-up |

任一领域 prepare、Definition 校验或投影准备失败，整次 restore 拒绝，旧 Runtime 零污染；不得把 Primary 与 Backup 按字段拼接。

## 6. 串行工作包

### P16-001：Envelope、版本分类与显式迁移

建立 v6、Header、结构化分类与 `v1->v2->v3->v4->v5->v6` 相邻纯 DTO 迁移；future/too-old/magic/class/size/checksum/迁移后校验失败均零 Runtime 副作用。

Allowlist：`Source/HSR/Save/HSRSaveVersion.h/.cpp`、`HSRSaveTypes.h`、`HSRSaveGame.h/.cpp`、`HSRSaveSubsystem.h/.cpp`、`Source/HSR/Tests/HSRSaveVersionTests.cpp`、`HSRSaveSubsystemTests.cpp`、`docs/save-system-design.md` 与任务三件套。

保守默认：v1 Equipment 空；v1-v2 Inventory/Reward 空；v1-v3 Quest 空；v1-v4 Map 空；不得猜测进度、领奖、解锁或位置。没有真实 fixture 时标记 synthetic/current-layout，不声称真实旧二进制兼容。

### P16-002：Golden fixtures 与跨领域 validation

为每个受支持历史版本建立可追溯 fixture，证明迁移确定性；完成 authority→DTO→Definition→prepare/commit 矩阵。若无法取得历史构建 fixture，则从稳定 envelope/payload 起承诺兼容，旧 UE SaveGame binary 仅为 legacy best-effort。

Allowlist：P16-001 Save 文件、相关现有 subsystem 的最小 validation seam、`Source/HSR/Tests/Fixtures/Save/`、版本/Subsystem 测试与任务文档。禁止 Battle/GAS/UI/Config/Content。

### P16-003：Staging/Primary/Backup 写入事务

顺序固定：`capture+validate -> write staging -> readback full validate -> preserve verified old primary as backup -> promote/write primary -> readback validate -> cleanup staging`。每阶段可注入失败，并保留至少一个已知有效副本。只使用已验证 UE API；不得硬编码 `Saved/SaveGames`，不得宣称 atomic rename。Backup 只能来自已验证旧 Primary，坏 Primary 不得覆盖好 Backup。

Allowlist：Save 文件、`Source/HSR/Tests/HSRSaveRecoveryTests.cpp` 与任务/设计文档。禁止新模块、外部文件库、Content/Config。

### P16-004：Load 分类、Backup recovery 与原子恢复

有效 Primary 优先；Primary 无效时独立验证 Backup；双坏结构化失败且 Runtime 零污染。Backup 成功返回恢复来源/原因，Load 不静默覆盖 Primary，后续成功 Save 才修复。future Primary 仍可尝试兼容 Backup；stale Backup 必须报告 generation。Save/Load 与 travel/battle-return pending、并发/回调重入、slot collision 均拒绝。

Allowlist：Save 文件、各领域现有 prepare/commit 的最小编排 seam、Recovery/领域 Save 测试与文档。禁止 UI、Content、Config、Gameplay 扩张。

### P16-005：现有 authority 全量冷恢复切片

验证 Profile、Party、Equipment/Relic、Inventory、Reward、Quest、Map 的 S1/S2 正常冷恢复、受控 Primary 损坏后恢复 S1 Backup、重复 Load no-op。Chest/enemy 只验收已有 stable flag/resolved 映射。

用户只操作命名为 Phase16 的临时 slot：两轮完全关闭/重开；专用受控 corruption seam；核对 slot role、save ID/generation、schema 迁移、reason、领域 revision/count、RestoreTx/通知/GE 来源不增长；最后专用 cleanup。

### P16-006：阶段收尾

fresh Development Editor Build、全 Save/领域专项回归、Editor cold recovery、Teacher、Independent Review、provenance、归档与 push。Packaged persistence 未实测则保持 `NOT VERIFIED`。

## 7. 冻结失败矩阵

- 读取：空 slot、坏参数、错误 class/magic、截断、size/checksum mismatch、反序列化失败、future/too-old。
- 迁移：step 缺失/失败、非确定、post-validation、Definition 缺失/重命名。
- 保存：capture、serialize、staging write/readback、backup rotate、primary write/readback、cleanup、受控 storage/permission failure。
- 恢复：primary 坏+backup 好、双坏、stale backup、future primary+compatible backup、任一领域 prepare/projection failure。
- 事务：全局零污染、真实 diff 单次通知、重复 Load no-op、overlap/reentry/slot collision、travel/battle-return pending。
- 一致性：全领域 roundtrip、ID 完整性、reward/equipment exactly-once、Map Load 不旅行。

## 8. 教学与证据门禁

问答覆盖 schema/generation/build version、相邻迁移与保守默认、staging/backup/readback、恢复顺序、持久与重建边界，以及三类证据差异。

- `AGENT VERIFIED`：fresh Build、分类/迁移、失败矩阵、candidate-first、failure injection、Primary/Backup 选择、重复恢复与领域回归。
- `USER PROVIDED`：Primary 冷恢复、受控损坏临时 Primary 后 Backup 冷恢复、重复 Load no-op。
- `STATIC ONLY / NOT VERIFIED`：真实强杀/断电/磁盘满/权限/锁、Packaged/Shipping、跨平台、云、多人与任意内容漂移恢复，除非实际运行。

## 9. 停止条件与非目标

删除/移动资产、Config、新模块/依赖、原始文件路径方案、扩大 Gameplay authority、无法证明的 replace 原子性、破坏真实用户存档时停止申请授权。

非目标：云存档、多 profile UI、自动保存、加密/反作弊、跨平台同步、多人、Phase 17 UI、保存 Battle transient、为宝箱/敌人新造 authority、宣称真实断电保证。

## 10. Gate 0 与当前下一步

Reviewer 的 on-disk contract、authority audit、恢复政策、失败矩阵、历史 fixture 与证据阻塞均已冻结；Teacher=`PASS`；Implementation=`PASS for detailed planning`；Independent Reviewer 最终=`PASS`；Coordinator 判定 Gate 0=`PASS`。

唯一下一步为 `TASK-P16-001`，状态 `PLANNED / USER AUTHORIZATION REQUIRED`；不得提前进行备份文件操作或 Gameplay 实现。
