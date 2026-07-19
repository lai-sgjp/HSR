# TASK-P4-003 Independent Reviewer Review

> 审查日期：2026-07-19  
> 审查角色：Independent Reviewer  
> 结论：`REVISE`  
> 审查范围：活动卡、执行报告、P4-002 归档/状态、commits `1ded747` / `2a1e784`、实际源码与资产路径、`Build-Log-P4-003*`、`Saved/Logs`。  
> Reviewer 只新增本文件；未修改 Source/Content/Config，未运行 Build/Editor/PIE，未提交或 push。

## 1. Verdict

P4-003 已取得 fresh UHT/C++/Link 成功证据，Player、Enemy、Neutral 三种 initiative 也均在最新 PIE 中可达。Encounter 第一次/第二次消费、测试返回旅行和连续多轮往返都有日志。

但任务存在一个已被运行日志直接证明的阻断性功能错误：`AHSRExplorationReturnConsumer` 先以引用取得公开 `GetReturnContext()`，随后调用 `ConsumeReturnContext()` 清空 Subsystem 内部 Payload；该引用因此立刻指向被重置的数据。日志中的保存 ReturnLoc 均为非零值，但 Consumer 实际每次都输出并应用 `X=0 Y=0 Z=0`。Return Transform 恢复未成立。

同时，travel failure 恢复、非空不存在地图、重复请求、Return 第二次消费、P4-002 四选三组合回归均未闭合；执行报告还有控制字符和乱码。因此 TASK-P4-003 当前不可归档，也不得进入 P4-004。

## 2. Blocking Findings

### [P1] Return Context 消费后回读导致 Transform 归零

当前数据流：

```text
const FHSRExplorationReturnContext& Ctx = Subsystem->GetReturnContext();
Subsystem->ConsumeReturnContext(); // PendingReturnContext 被默认构造覆盖
PlayerPawn->SetActorTransform(Ctx.ReturnTransform); // Ctx 已是清空后的成员
```

直接运行证据包括：

- 保存 `ReturnLoc=X=578.341 Y=1461.325 Z=107.296`，实际 `Teleported pawn to X=0.000 Y=0.000 Z=0.000`。
- Enemy 返回保存 `X=1094.771 Y=-505.686 Z=107.296`，实际仍为原点。
- Neutral 与后续连续往返同样保存不同非零位置，但每次实际传送到原点。

这与 P4-001 已修复的旧 Pending getter 反模式完全相同。必须删除公开 `GetReturnContext()`，让 `ConsumeReturnContext()` 的 Result 携带被消费的纯值 Context；Consumer 只使用返回值，并在成功返回前立即清空内部 Payload。不得通过复制引用、Peek 后 Consume 或 Blueprint getter 绕过。

### [P1] 旅行失败恢复合同未实现

- 活动卡明确要求用 UE5.6 已确认 delegate/回调在 travel failure 时清理 Traveling；若无法确认必须停止报告。
- 实现确认不了 API 后直接移除 delegate，但仍继续交付；Subsystem 没有 `Deinitialize`、没有 travel failure 绑定/解绑、没有异步失败恢复。
- `OpenLevel` 对非空但不存在地图仍会令 Encounter 或 Return 状态悬挂；P4-003 的核心失败恢复目标未实现。

这不能推迟到 P4-004 或 Phase 5。P4-004 是收尾教学/审查，不是补写 P4-003 核心状态机。若用户明确接受不实现，则必须单独 `USER ACCEPTED` 风险处置；在此之前是阻断。

### [P1] Return Context 校验和单次消费结果不完整

- `RequestTestReturn` 在校验 RequestId/ExplorationMapPath 前先写入 Pending；仅空 path 会随后清理，未拒绝无效 RequestId。
- `ConsumeReturnContext()` 创建局部 `Consumed` 后完全不使用，也不把 Context 放入 Result。
- Return Result 没有携带 Request ID/Context，Consumer 无法原子消费并恢复。
- `EHSREncounterReturnResultType` 没有明确 AlreadyPending；重复 Return 请求错误地映射为 `AlreadyConsumed`。
- 第二次 `ConsumeReturnContext` 没有动态验证；当前只会返回 `NothingPending`，不能证明不返回旧 Context。

## 3. Initiative Review

### 静态来源

- Graybox 暴露 `InteractInitiative`，安全默认 `Player`，显式传给唯一 `RequestEncounter`。
- Enemy AI 固定传 `EHSREncounterInitiative::Enemy`。
- Neutral BP 复用同一 Graybox C++，仅配置 `Neutral`；没有第二套 Request/OpenLevel 逻辑。
- Subsystem 不按 Actor/World 猜测 initiative，只复制调用者输入到 DTO。

### 动态证据

- Player 路径多次消费 `Initiative=0`。
- Enemy 路径多次消费 `Initiative=1`。
- 最新 01:36 会话中 Neutral 两轮连续消费 `Initiative=2`。

早先 01:33 的 `BP_HSRNeutralEncounterTest` 仍消费 `Initiative=0`，证明资产最初配置错误；最新会话已更正为 2。该历史不得删除，但三种来源在最终日志中确实可达且确定。

## 4. Encounter / Return Consumption

### Encounter 已成立

- 每轮 Encounter 第一次 Consume 成功，日志记录 Request ID、initiative、地图、Transform。
- 第二次 Consume 为 `AlreadyConsumed`，失败 DTO 的 EncounterId 为 None。
- 内部 Encounter Payload 在成功消费时清空；没有公开 Encounter getter。

### Return 未成立

- RequestTestReturn 和 ConsumeReturnContext 日志表明跨图 Presence 存活。
- 但返回 Payload 通过公开 getter 暴露，Consumer 消费后继续使用引用，造成归零。
- 日志没有第二次 Return Consume 的结构化失败，也没有证明旧 Return Context 不可再次读取。
- `GetReturnContext()` 公开 BlueprintPure API 本身破坏“消费是唯一出站口”的不变量。

## 5. Failure / Repeat Matrix

### 已有材料

- Battle Map 直接启动时旧日志有 NothingPending 安全失败路径。
- 多轮连续 Player/Enemy/Neutral Encounter→Battle→Return 没有 crash、Ensure、GC warning 或 Blueprint Runtime Error。
- P4-002 日志包含 lost sight 后恢复，再完成 Enemy Encounter。

### 未完成

- 同帧/连续两个 Encounter 请求的 `AlreadyPending`、首个 ID/initiative 不被覆盖、只旅行一次：未保存专项证据。
- Definition/ID/空地图/不存在软地图的完整失败与修正后重试：P4-003 最新证据未覆盖。
- 非空不存在地图的 preflight 校验：源码未实现。
- 可控 travel failure 后从 Traveling 恢复并再次成功：源码和动态证据均无。
- 无效 Return RequestId/Map/Transform 的拒绝与清理：源码/动态证据不足。
- Return 第二次 Consume：未验证。
- Transform 恢复容差：失败，实际全部归原点。

## 6. P4-002 Combination Regression

活动卡要求四项中至少覆盖三项并保存组合证据：目标销毁、重复 Perception、MoveTo Failed/Aborted、UnPossess/Re-Possess。

当前最新日志只明确支持：

- lost sight 后重新感知并完成 Enemy Encounter；
- 多次 World teardown 无未解释 crash/ensure。

没有目标销毁专项、重复 perception 计数、MoveTo Failed/Aborted 有界恢复、独立 UnPossess→Re-Possess 零增长证据。因此未达到三项组合门槛。

## 7. Reflection / GC / Timer / Tick / GAS / Network

### 通过项

- 新 enum/struct/class 反射宏、generated include、安全默认值基本正确。
- Encounter/Return DTO 都是 `FGuid/FName/enum/FTransform` 纯值，没有 Actor/World/ASC/Widget/Handle。
- Battle Test Consumer、Return Consumer、Graybox、AIController 无自定义 Gameplay Tick。
- Battle Return Timer 和 Exploration retry Timer 为成员 Handle，EndPlay 清理；retry 上限为 10。
- AI Perception delegate、Init/Patrol Timer 保持 P4-002 A1 生命周期边界。
- 未修改 GAS、ASC、Attribute、GE、Ability；无 RPC、Replication、Prediction。
- 没有 Battle Actor、TurnManager、攻击、胜负、奖励、BattleResult 或正式战后流程。

### 需要修订

- Return Consumer `EndPlay` 直接使用 `GetWorld()`，应在 teardown 边界安全检查。
- 无 Pawn 重试耗尽时调用 `ClearPending()`，它清的是 Encounter 状态而不是 Return Context；Return Pending 会残留。必须提供针对 Return 的清理/失败事务，不能清错状态。
- `RetryCount`/RetryInterval 默认值虽安全，但失败后需要明确清 Return Payload 的 API 和日志。

## 8. Build Evidence

`Build-Log-P4-003.txt` 直接证明：

- Target 为 `HSREditor Win64 Development`。
- UHT 因反射 header 变化实际运行，`Total of 3 written`。
- 编译 Transition Subsystem、Battle Test Consumer、Exploration Return Consumer、Graybox、Enemy AIController、Module.HSR。
- 链接 `UnrealEditor-HSR.lib` 与 `.dll`。
- `Result: Succeeded`，总执行时间 8.58 秒。
- 可见 warning 包含既有 non-preferred VS compiler，以及 UE5.6 `UAISystemBase::CleanupWorld` deprecation warning；执行报告只写“仅 VS warning”并不准确。

Build 发生在 Implementation commit 前，覆盖当前 C++ 变化；构建门槛通过，但运行语义失败。

## 9. Commit / Asset Scope

- `1ded747` 精确包含 10 个允许 C++ 文件与执行报告；未修改 Build.cs、Config 或白名单外 Source。
- `2a1e784` 包含 Neutral BP、Battle Map、Exploration Map，均在用户资产白名单内。
- 活动卡还允许修改现有 Graybox BP，但本次资产 commit 未包含它；Player 默认由 C++ 默认值/既有资产承担，日志证明 Player=0，非阻断。
- 资产 commit 晚于最新 PIE 日志约两分钟；日志证明最终内存/保存前配置可运行，但没有 commit 后完整 Editor 重开证据。
- 两个 commits author/committer 都是同一 Git 身份，角色作者只能由 message/用户声明支持。
- Implementation commit 由 Implementation 名义执行 Git，违反活动卡“不授权 Implementation Git”；历史偏差保留。

## 10. Execution Report Hygiene

当前 `tasks/execution-result.md` 不可作为最终审查证据：

- 含 3 个 `0x08` 控制字符，分别污染 `bEnableTestReturn` / `bAvailable` 等字段。
- 含 4 处 `???` 乱码，包括 commit message 示例。
- 状态仍为 `Pending user Editor operations`，没有记录已存在的 Player/Enemy/Neutral 动态结果、连续往返和 Transform 归零失败。
- 没有记录 Neutral 初次配置为 Player、后来修正的历史。
- 没有记录 travel failure delegate 被删除后核心 Gate 未实现。
- Build warning 记录不完整。
- 报告内包含 Git stage/commit 命令，与 Implementation 禁止 Git 的角色规则冲突，且实际 commits 已存在。

必须清理控制字符/乱码并按 S/B/D 分层写入真实证据、失败和 Git 偏差。

## 11. Required Revision

Coordinator 应锁定最小修订，至少包括：

1. 删除/私有化 `GetReturnContext()`；让 `FHSRExplorationReturnResult` 返回被消费 Context（或安全 output DTO），成功时先复制、再清内部 Payload，Consumer 只用返回值。
2. 修复无 Pawn 重试耗尽清错状态；增加 Return 专用 Clear/Fail 事务，并做 World-safe Timer teardown。
3. 明确处置 travel failure：实现经 UE5.6 头文件确认的对称回调与状态恢复；若用户选择不实现，必须明确 `USER ACCEPTED`，不得称 P4-003 失败恢复完成。
4. 增加非空不存在地图的可验证 preflight，或明确无法安全验证并由用户处置；空 path/无效 RequestId 必须在写 Pending 前拒绝。
5. fresh Build 保存 UHT/C++/Link/Result。
6. Editor/PIE 重测三种 initiative，确认 Return 实际位置与保存 Transform 在容差内；每种至少一次，另做两轮连续往返。
7. 动态验证 Return 第二次 Consume、重复 Encounter 请求、无效/不存在地图、失败后可重试。
8. 保存 P4-002 四选三组合生命周期证据，或逐项取得用户 `USER ACCEPTED` 风险处置。
9. 清理 execution report 控制字符/乱码，记录全部真实证据与失败。

## 12. Final Decision

**`REVISE`**

- 三种 initiative 来源与可达性：通过。
- Encounter 单次消费：通过。
- Return 单次消费与 Transform 恢复：未通过，存在运行证明的归零错误。
- 重复请求/无效地图/travel failure 恢复：未通过。
- 连续往返：旅行链存在，但位置恢复错误，不能通过。
- P4-002 三项组合回归：未通过。
- Reflection/GC/Timer/无 Tick/GAS/网络/Phase 5 边界：大部分通过，Return 失败清理需修。
- Build：通过，报告 warning 不准确。
- 白名单：通过，Git 角色/同身份偏差保留。
- 执行报告：未通过。
- TASK-P4-003：不可归档。
- P4-004：不得进入。
