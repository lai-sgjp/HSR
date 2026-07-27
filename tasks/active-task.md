# TASK-P17-PATCH-01C — Complete Action-Distance Turn Model

Status: `PLANNED / TASK GATE REVIEW REQUIRED`

## Single outcome

TurnManager 使用统一行动距离决定下一行动者：运行中 Speed/Slow、Advance、Delay 与 Break Delay 均可重排未来行动，当前已开始的行动不被取消；高速角色可更频繁行动，所有请求 exactly-once，Tie-break、BattleEpoch、TurnSequence 与回合预算保持一致。

## Frozen model and ownership

- TurnManager 是 battle-local 行动距离和顺序唯一 authority；ASC Speed 是有效速度真源；Status/GAS/Coordinator 只能提交变化或纯值请求，UI 只读快照。
- 原创公式：`BaseActionDistance = 10000.0 / max(EffectiveSpeed, 1.0)`；距离越小越早行动。
- 每个参与者保存纯值 `EffectiveSpeed`、`BaseActionDistance`、`RemainingActionDistance` 与稳定 ParticipantId；不持久化、不跨地图、不保存到 SaveGame。
- 初始化时 Remaining=Base。选人前从所有合法候选 Remaining 减去最小值；零距离候选按 ParticipantId 字典序稳定 Tie-break。
- 当前行动一旦 TurnStarted 即锁定。Speed/Advance/Delay 只影响未来选择，不取消、不重播当前行动。
- ResolveAction 后，行动者 Remaining 增加当前 Base；不得简单重置并丢失超前/延后余额。
- Speed/Slow 通过 ASC Speed delegate 事件驱动，无 Tick。非当前参与者按 `NewRemaining = OldRemaining * NewBase / OldBase` 保留进度；当前参与者的新 Base 在本次行动结束补距时生效。
- 通用 Advance/Delay 请求为纯值：BattleEpoch、OperationId、TargetParticipantId、Ratio、Kind。Ratio 必须 finite 且 `[0,1]`；Advance 减 `Base*Ratio`（下限 0），Delay 加 `Base*Ratio`。
- Break Delay 适配为同一 Delay 请求、Ratio=`1.0`，保持延后一整个自身基础行动距离；PATCH-01B 的 admitted-alive pending-deferred-defeat 资格仍只限当前同步事务。
- OperationId battle-local exactly-once；无效、重复、旧 Epoch、未知/死亡目标、Finished 请求零变化并返回结构化结果。

## Exact candidate allowlist

- `Source/HSR/Data/HSRBreakTypes.h`
- `Source/HSR/Battle/HSRBattleParticipant.h`
- `Source/HSR/Battle/HSRTurnManager.h`
- `Source/HSR/Battle/HSRTurnManager.cpp`
- `Source/HSR/Battle/HSRBattleCoordinator.h`
- `Source/HSR/Battle/HSRBattleCoordinator.cpp`
- `Source/HSR/Battle/HSRBattleGameMode.h`（仅复用/最小扩展 `WITH_DEV_AUTOMATION_TESTS` fixture；禁止新属性/开关）
- `Source/HSR/Battle/HSRBattleGameMode.cpp`（仅 fixture 与现有 P8/P9/Turn harness）
- `Source/HSR/Tests/HSRCombatPatchTests.cpp`
- `tasks/execution-result.md`

Implementation 不得修改 active-task、计划、PROJECT_STATE、worklog/todo/learn、Config、Content、Save、UI 或网络代码。新增生产文件、Build.cs、Gameplay Tag/GE/DataAsset 或 Turn UI 快照文件需停止申请授权。

## Required matrix

- A/B/C 初始速度产生可解释的行动距离与稳定顺序；高速角色在足够多次 resolve 中可比低速角色多行动。
- 当前 A 行动期间 B Speed Up、C Slow：A 不变，后续 Remaining 按进度比例换算，下一行动者正确。
- Advance 0/0.25/1、Delay 0/0.3/1、连续组合及边界值；重复 OperationId 全部零变化。
- Break Delay 只接受一次并表现为 `+1.0 Base`；不得再保留独立 skip-once 容器或双重延后。
- 相同 Remaining 使用 ParticipantId Tie-break；浮点比较使用明确容差，不依赖 UObject 地址或数组偶然顺序。
- 当前行动者 Speed 变化不取消当前行动；Resolve 后使用新 Base 补距。
- 无效 Ratio、NaN/Inf、旧 Epoch、未知/死亡目标、Finished、全部失效、唯一存活角色均结构化且无重复/丢失行动。
- Reset/新 Battle 清理 delegate、OperationId 与行动距离；旧 ASC Speed callback 零副作用；不得泄漏绑定。
- same-frame deferred defeat 仍通过 PATCH-01B 回归；Status/Break 不因行动值迁移回归。

## Evidence and user gate

- Development Editor Build。
- `HSR.Battle.Patch.ActionDistance` 真实/受控 runtime Automation；回归 `HSR.Battle.Patch`、适用 Turn/P8/P9/Battle tests。
- 日志记录每次 OperationId、old/new Speed、Base/Remaining、current/next、TurnSequence、接受/拒绝结果。
- PIE 复用既有 Battle development harness；若无合法 Speed/Advance/Delay 资产入口，结构化 harness 证明 runtime，用户只验证现有 Battle 行动不中断与 Break 回归，不得临时创建 Content/Blueprint 规则。
- `git diff --check` 与 allowlist/provenance 审计。

## Explicit non-goals and stop conditions

不实现正式行动条 UI、动画、VFX、网络预测/复制、SaveGame、AI、Behavior Tree、P17-005 或新 Content/Config。不得复制商业游戏常量/公式。需要白名单外生产消费者、反射接口、资产或扩大 Ratio/公式契约时停止请求最小授权。
