# Phase 11 详细执行计划：角色定义、成长、队伍与最小存档

> 当前状态：`PHASE 11 IN PROGRESS / PLAN BACKFILLED / NOT YET VERIFIED`
>
> 前置基线：Phase 10 为 `Ready with inherited follow-ups`。当前工作树已经出现 P11-001 与 P11-003 的 C++ 和用户资产产物，但尚未形成完整的 Phase 11 任务卡、Automation 运行、角色提交与阶段归档，因此不得写成工作包完成。

## 1. 当前事实、证据与 Gate 0

### 1.1 当前事实

- P11-001 已在工作树创建 Character Definition、纯值成长类型、事务库和 Development Automation Test 源码。
- P11-001 已取得真实 `HSREditor Win64 Development` Build 成功证据；Automation Test 仅编译进模块，尚未实际运行。
- P11-003 已提前处理 Battle 路径中的双 ASC、双基础初始化 GE 和 Infinite Progression GE Handle 所有权问题。
- P11-003 最终版本已通过 `git diff --check` 和真实 Development Editor 编译、链接与 metadata 写入。
- 用户创建了 `/Game/GameplayEffects/GE_CharacterProgression_P11`，并在 Battle GameMode 中完成引用配置；二进制资产字段属于用户提供证据。
- 当前仍不存在正式 Profile authority、PartySubsystem、Save DTO/SaveSubsystem 和 Character Details UI。
- 当前 Battle participant 仍是普通 `APawn`，尚未动态证明真实 `AHSRCharacterBase` 的原生 ASC 复用。

### 1.2 2026-07-24 用户 PIE 证据

证据等级：`USER PROVIDED / LOG INSPECTED`。

附件包含两轮完整战斗：

- 第一轮玩家败北，`Outcome=2`；
- 第二轮玩家胜利，`Outcome=1`；
- 两轮均出现 `BuildParticipants - SUCCESS`；
- 两轮结果均被单次消费并返回探索地图；
- 未检出 `P11-003 ... FAILED`、`BuildParticipants ... FAILED` 或 `Blueprint Runtime Error`。

这份日志可以证明当前 Progression GE 配置没有阻断普通 Pawn 战斗、胜负和返回流程。它不能证明：

- 真实 `AHSRCharacterBase` 只有一个 ASC/CoreAttributeSet；
- Character 基础初始化与 Battle 初始化在真实 Character Pawn 上只执行一次；
- 同 revision 重建不叠加、revision 更新替换或异常 remove/rollback；
- Character Runtime 数值已经写入 Progression GE；
- Save/Load、Party 或角色详情 UI 已实现。

### 1.3 Gate 0

进入后续工程包前必须冻结：

1. Phase 10 归档状态与 inherited follow-ups 不被改写。
2. P11-001/P11-003 登记为提前实施、待补任务契约与验收，不倒填历史。
3. Character Definition、Runtime Profile、Party、Save DTO、ASC/GE Handle 与 UI 的唯一所有者。
4. `CharacterId`、累计 EXP、等级上限、突破、技能等级与被动第一版语义。
5. 基础初始化层与成长 Infinite GE 层的职责及来源顺序。
6. 每个工作包只有一张活动任务卡、一个独立验收结果和精确 allowlist。
7. 执行者先复述任务卡，再等待用户对该工作包的单独确认。
8. 不修改或认领 `.claude/settings.local.json`、插件 `__pycache__` 等既有噪声。

## 2. 阶段唯一最终结果

两个原创占位角色由不可变 Character Definition 描述。Runtime Profile 以稳定 `CharacterId` 保存累计经验、等级、突破与技能等级；Party 以稳定 ID 建立无重复编队；成长属性通过唯一、可移除、可重建的 Infinite GE 来源进入每个角色唯一 ASC；最小版本化 Save 只保存纯值 DTO，并能在销毁 Runtime 后由保存 ID 恢复 Profile、Party 与 ASC；角色详情 UI 只读显示 Definition、Runtime 与派生属性。

## 3. 架构与所有权不变量

- `UHSRCharacterDefinition`：不可变 authored 输入；保存稳定 ID、显示数据、软资源引用和成长规则，不被 Runtime 回写。
- Runtime Profile：GameInstance 生命周期纯值状态；不得持有 Actor、ASC、AttributeSet、Widget 或 GE Handle。
- Party：只保存稳定 CharacterId；拒绝空 ID、未知 ID、重复角色和越界槽位。
- Save DTO：只保存 schema version、CharacterId、Level、累计 EXP、Ascension、SkillLevels 和 Party slots。
- Save 不保存 UObject、Actor、ASC、AttributeSet、Widget、Active GE Handle 或最终派生 Attribute。
- Load 必须先构建并完整验证 Candidate，全部成功后才能一次性替换有效 Runtime。
- 每个场景 Actor 恰好一个 ASC 与一个 CoreAttributeSet。
- 基础初始化 GE 是一次性基础层；Progression GE 是独立、可追踪、可移除的 Infinite 成长层。
- Progression Handle 必须同时绑定所属 ASC、Effect Class、Revision 与 Epoch；失败清理不得静默丢 token。
- UI 只读 Snapshot 并提交命令，不直接写 Profile、ASC 或 Save。
- 全阶段禁止 Gameplay/Widget Tick 轮询。
- 本阶段保留未来服务器权威入口，但不实现复制、RPC 或 Prediction。

主数据流：

`CharacterId → CharacterDefinition + Runtime Profile → Validate/Aggregate → Infinite Progression GE → ASC → Delegate/Snapshot → Character UI`

存档流：

`Runtime Profile + Party IDs → Save DTO → Disk → Validate Candidate → Commit Runtime → Rebuild ASC`

## 4. 工作包与严格顺序

严格顺序：

`P11-000 → P11-001 → P11-002 → P11-003 → P11-004 → P11-005 → P11-006`

P11-001 与 P11-003 已有提前产物，但仍必须补齐各自任务卡、证据、独立审查和归档。

### P11-000：状态校准与阶段契约冻结

唯一验收结果：Phase 11 的字段语义、所有权、工作包、失败语义、资产作者与证据等级均可独立复核。

允许范围：

- `docs/phase-11-execution-plan.md`
- `tasks/active-task.md`
- `tasks/execution-result.md`
- `tasks/final-review.md`
- 对应 P11-000 archive 文件

必须完成：

- 对账 dirty tree 与 staged/unstaged provenance；
- 记录 P11-001/P11-003 的真实提前实施历史；
- 冻结累计 EXP、等级、突破、技能等级、Party 容量和 Save v1 schema；
- 不修改 Source、Content 或 Config。

### P11-001：Character Definition 与纯值成长事务

当前状态：`IMPLEMENTED IN WORKTREE / BUILD PASS / AUTOMATION NOT RUN / NOT ACCEPTED`。

唯一验收结果：一次 EXP 发放可跨越多级；非法 Definition、曲线、EXP、状态或技能等级被结构化拒绝，且失败时旧状态完全不变。

精确源码范围：

- `Source/HSR/Data/Definitions/HSRCharacterDefinition.h`
- `Source/HSR/Progression/HSRCharacterProgressionTypes.h`
- `Source/HSR/Progression/HSRCharacterProgressionLibrary.h`
- `Source/HSR/Progression/HSRCharacterProgressionLibrary.cpp`
- `Source/HSR/Tests/HSRCharacterProgressionTests.cpp`

用户资产范围：

- `Content/Data/Characters/DA_Character_P11_A.uasset`
- `Content/Data/Characters/DA_Character_P11_B.uasset`
- `Content/Data/Progression/Curve_CharacterEXP_P11.uasset`

必须补齐：

- 两个唯一且非空的 CharacterId；
- 累计 EXP 曲线严格递增、缺失/NaN/溢出/上限策略；
- ID mismatch、MaxLevel、负 EXP、overflow、非法 SkillId/SkillLevel 失败矩阵；
- 实际运行 `HSR.Progression.Character.Transaction` Automation；
- Editor 保存、关闭并重开后的 DataAsset/Curve 引用；
- Independent Reviewer 结论与归档。

### P11-002：Profile、Definition Resolver 与成长写权限

唯一验收结果：两个 CharacterId 可以被解析并建立唯一 Runtime Profile；所有成长写操作只经过 Progression service，失败不污染现有 Profile。

建议精确范围：

- `Source/HSR/Progression/HSRCharacterProfileTypes.h`
- `Source/HSR/Progression/HSRProgressionSubsystem.h`
- `Source/HSR/Progression/HSRProgressionSubsystem.cpp`
- `Source/HSR/Tests/HSRProgressionSubsystemTests.cpp`

本包禁止修改 Battle、CharacterBase、GameMode、Party、Save、UI、Config 和 Content。

必须验证：

- 空/重复/未知 CharacterId 拒绝；
- Definition 解析失败零副作用；
- AddExperience、Ascend、SetSkillLevel 由单一服务写入；
- 跨 OpenLevel 后 GameInstance Runtime 仍存在；
- Snapshot/Delegate 不泄漏 UObject 或 Actor 引用。

### P11-003：单 ASC 与幂等 Progression GE 所有权

当前状态：`IMPLEMENTED FOUNDATION / BUILD PASS / USER PIE PARTIAL PASS / NOT ACCEPTED`。

唯一验收结果：相同角色与相同 revision 连续重建不会叠加；revision 更新只替换该角色自己的成长来源；Reset、Actor/ASC 更换和失败回滚不遗留或丢失 Handle。

精确 C++ 范围：

- `Source/HSR/Character/HSRCharacterBase.h`
- `Source/HSR/Battle/HSRBattleCoordinator.h`
- `Source/HSR/Battle/HSRBattleCoordinator.cpp`
- `Source/HSR/Battle/HSRBattleGameMode.h`
- `Source/HSR/Battle/HSRBattleGameMode.cpp`

用户资产范围：

- `Content/GameplayEffects/GE_CharacterProgression_P11.uasset`
- `Content/Blueprints/Framework/BP_HSRBattleGameMode.uasset`

现有实现只证明所有权骨架；当前 Progression GE 没有成长 Modifier，尚未证明 `CharacterId + Runtime Level → Attribute`。

必须补齐：

- Progression service 提供明确 revision 与派生值；
- Infinite GE 的 SetByCaller/Modifier 合同及属性对账；
- 同 revision 重入、新 revision 替换、Instant 拒绝；
- old remove/new rollback/secondary retry/Clear failure；
- 同 CharacterId 跨不同 ASC；
- 真实 `AHSRCharacterBase` battle Pawn 的 ASC count=1、基础初始化一次、成长层一次；
- ResetAndRebuild 不复用旧 Actor/Handle。

### P11-004：PartySubsystem 与双角色编队

唯一验收结果：两个已拥有的不同 CharacterId 可以组成固定容量队伍；重复、未知、空 ID 和非法槽位被拒绝，原队伍不变。

建议精确范围：

- `Source/HSR/Party/HSRPartyTypes.h`
- `Source/HSR/Party/HSRPartySubsystem.h`
- `Source/HSR/Party/HSRPartySubsystem.cpp`
- `Source/HSR/Tests/HSRPartySubsystemTests.cpp`

必须验证：

- Add、Remove、Replace、Swap 全部 Candidate-first；
- 固定容量、稳定顺序、无重复；
- 只允许 Profile authority 中已拥有角色；
- 跨地图保持，Actor 销毁不影响 Party；
- Party Snapshot 只含稳定纯值 ID。

### P11-005：最小 Save、ASC 重建与角色详情闭环

唯一可见结果：升级并编队两个角色后保存，清空 Runtime/重新加载，再由保存 ID 恢复 Profile、Party、ASC 成长层和只读详情显示；重复 Load/Rebuild 不叠加。

建议 C++ 范围：

- `Source/HSR/Save/HSRSaveTypes.h`
- `Source/HSR/Save/HSRSaveGame.h/.cpp`
- `Source/HSR/Save/HSRSaveSubsystem.h/.cpp`
- `Source/HSR/Tests/HSRSaveSubsystemTests.cpp`
- `Source/HSR/UI/HSRCharacterDetailTypes.h`
- `Source/HSR/UI/HSRCharacterDetailViewModel.h/.cpp`
- 经任务卡冻结的最小 UI 接线文件

用户资产范围：

- `Content/UI/WBP_CharacterDetail_P11.uasset`
- 两个原创占位 Portrait，可为空并使用安全占位

Save v1 只包含：

- `SchemaVersion`
- Character Runtime DTO 数组
- Active Party CharacterId 数组

必须验证：

- Save→改变状态→Load→恢复；
- 关闭 Editor 后重新打开再 Load；
- 重复 Load 不叠加；
- 空/重复/未知 ID、坏 Level/EXP、缺 Definition、schema mismatch；
- Load 失败时有效 Runtime/Party 不变；
- UI 初始 Snapshot、后续 Delegate、Widget destruct/rebuild 与无 Tick；
- UI 缺失不阻断 Progression、Party、Save 或 Battle。

Phase 16 的备份、迁移链、损坏恢复和全系统 Save 明确延后。

### P11-006：全阶段回归、教学、独立审查与归档

唯一验收结果：P11-000～P11-005 的工程、资产、运行、学习和 Git 证据可独立复核，Phase 11 得到正式 Reviewer 结论并完成归档。

本包不新增 Gameplay，只允许：

- fresh Build、Automation、Editor 重开、PIE 和失败路径；
- Teacher 教学与用户真实回答；
- Independent Reviewer 审查；
- `PROJECT_STATE.md`、`todo_plan.md`、`worklog.md`、`learning-journal.md`、`README.md`；
- `docs/phase-11-execution-plan.md`、`docs/gas-notes.md`、`docs/data-asset-guidelines.md`；
- `tasks/*` 与归档文件。

## 5. 阶段验证矩阵

| Gate | 必须证据 |
|---|---|
| Static | `git diff --check`；反射、GC、软引用、无 Tick、无跨图 Actor 引用审查 |
| Build | Fresh `HSREditor Win64 Development`；记录 UHT、C++、Link、metadata、exit code 和第一处真实失败 |
| Automation | Progression transaction、Profile authority、Party transaction、GE rebuild、Save round-trip/repeated load |
| Editor | 两个 Character Definition、EXP Curve、Infinite Progression GE、Battle GameMode 引用、Details WBP 保存重开 |
| 主 PIE | 两角色建立→编队→跨级→属性重建→Save→清空/重开→Load→UI/ASC 恢复 |
| 失败 PIE | 重复角色、非法曲线/EXP、缺 Definition、错误 GE 类型、坏 Save、Apply/Remove 失败 |
| 生命周期 | Victory、Defeat、两轮返回、Reset、Actor/ASC replacement、Widget 重建 |
| 回归 | Phase 2 属性初始化与 Phase 5～10 战斗、状态、UI、结果返回不回归 |
| 证据等级 | `AGENT VERIFIED`、`USER PROVIDED`、`STATIC REVIEW`、`NOT VERIFIED` 分开记录 |

## 6. 角色交接与 Git 规则

每个工作包固定顺序：

`Coordinator → Terra Implementation → User Editor → Teacher → Independent Reviewer → Coordinator Archive`

- Coordinator：任务卡、allowlist、状态同步和归档。
- Terra Implementation：只修改活动卡授权的 C++/测试文件。
- User：Editor 资产作者，资产独立记录，不由执行者冒认。
- Teacher：保存真实教学、用户原始回答、纠正和掌握度。
- Independent Reviewer：写入明确的 `PASS / PASS WITH FOLLOW-UP / REVISE / BLOCKED`。
- 每个角色只提交自己的实际产物并记录 hash。
- Phase 中途不 push；阶段全部工作包审查和归档后由 Coordinator 收尾并 push。
- 禁止提交 `.claude/settings.local.json`、插件 `__pycache__`、Binaries、Intermediate、Saved 或未知资产。

## 7. 风险与停止条件

主要风险：

- P11-003 提前于 Profile/Party/Save 实施，不能把 Handle 骨架误写成完整成长重建。
- 当前 Progression GE 无成长 Modifier，不能证明派生属性。
- 当前战斗普通 Pawn 不能证明 Character-owned ASC 复用。
- SkillLevels 尚未验证 Definition 中稳定 SkillId 与上限。
- Ascension、被动和成长属性来源顺序尚未冻结。
- Save 若边验证边写会产生半加载状态。
- CharacterId 重命名会破坏 Save，禁止 DisplayName 兜底。
- Phase 12 会引入更多 GE 来源，Phase 11 必须先形成稳定、可逆的成长来源合同。

停止并请求扩权的情况：

- 需要新 Runtime Module、危险 Config、第三方依赖或批量资产迁移；
- 需要进入 Equipment、Inventory 或完整 Phase 16 Save；
- 需要保存 UObject、Actor、ASC、Widget 或 GE Handle；
- 需要修改活动卡 allowlist 外文件；
- GE 或 Save Candidate 无法安全回滚；
- Editor 资产字段与代码合同不一致。

## 8. Phase 11 完成定义

只有以下全部成立，Phase 11 才能标记为 `Ready` 或 `Ready with inherited follow-ups`：

1. P11-000～P11-006 均有活动卡、执行结果、独立 Reviewer 结论和归档。
2. 两个 Character Definition、累计 EXP Curve 和双角色 Party 经 Editor 重开仍有效。
3. 跨多级、cap、非法曲线、突破、技能等级和被动第一版具有明确实现或明确范围裁决。
4. 一个 Actor 恰好一个 ASC/CoreAttributeSet，基础层与成长层 authority 唯一。
5. Progression GE 重复 rebuild 不叠加，失败可回滚，Reset/ASC 更换不泄漏。
6. Save v1 往返和重复 Load 通过，不保存派生对象、Handle 或最终 Attribute。
7. 角色详情 UI 事件驱动、无 Tick、只读。
8. Fresh Build、Automation、主/失败 PIE、两轮生命周期和 Phase 2/5～10 回归均有证据。
9. Teacher、Reviewer、provenance、角色 commits、阶段收尾 commit 和 remote push 闭合。

## 9. 当前唯一下一步

创建并执行 `P11-000` 状态校准任务：只整理当前 P11-001/P11-003 的 provenance、证据和字段契约，不继续扩大 Gameplay。P11-000 通过后，首先补齐 P11-001 Automation 与两个 Character Definition/EXP Curve 资产门禁，再进入 P11-002 Profile authority。
