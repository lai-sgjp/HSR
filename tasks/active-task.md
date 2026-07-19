# TASK-P4-004：Phase 4 最终回归、教学、独立审查与阶段收尾

> 创建日期：2026-07-19
> 状态：`WAITING FOR FIRST READ-BACK`  
> 本卡是当前唯一活动任务契约。P4-001～003 已分别以 `PASS WITH FOLLOW-UP` 归档；本卡只做文档、证据核对、教学与阶段判定，不新增 Gameplay、Content 或 Config。

## Role Lock / 首次只读复述与二次确认

低级执行模型当前且仅能担任 `Implementation/Verification Agent`，不得切换为 Coordinator、Reviewer、Teacher 或资产作者。

首次收到本卡后只能读取本文件，不得调用工具。必须复述：任务编号、唯一收尾结果、精确可修改 Markdown、只读证据范围、Build/Editor/PIE/失败矩阵、P4-001～003 follow-up、Teacher/Reviewer 顺序、明确非目标与停止条件。末尾必须逐字写：

`等待用户确认执行 TASK-P4-004。`

随后停止。只有用户在复述后另发明确确认，才可调用工具；此确认不授权新增 Gameplay 或资产实施。

## 唯一可验收结果

Phase 4 的 P4-001～003 真实证据、用户处置、未验证边界、源码/资产作者、Build/Editor/PIE 证据、Teacher 原始作答与 Reviewer 独立结论全部被一致归档；Coordinator 依据证据将 Phase 4 判定为 `Ready`、`Not verified` 或 `Blocked`。不得把规划、静态 guards、用户自述或 `USER ACCEPTED` 写成 Reviewer 动态通过。

## 精确可修改文件

仅允许修改以下 Markdown：

- `tasks/execution-result.md`
- `tasks/final-review.md`
- `PROJECT_STATE.md`
- `README.md`
- `todo_plan.md`
- `worklog.md`
- `learning-journal.md`
- `docs/phase-4-execution-plan.md`
- `docs/teacher-role-guide.md`（仅必要教学记录；若不存在则停止）

任务归档文件只能由 Coordinator 在审查顺序完成后移动到 `tasks/archive/TASK-P4-004-*`；不得删除历史链。`tasks/active-task.md` 只由 Coordinator 维护，低级执行者不得修改。

## 精确只读证据范围

- `tasks/archive/TASK-P4-001-*`
- `tasks/archive/TASK-P4-002-*`
- `tasks/archive/TASK-P4-003-*`
- 当前所有 `Build-Log-P4-*`、`Saved/Logs/HSR.log`（只读，不删除）
- P4-001～003 的实际 commits、`git show/status/diff` 输出（只读，不提交）
- P4-001～003 实际 Source/Content/Config（只读静态核对）
- 用户提供的 Editor/PIE 日志、截图、结构化答复（只记录来源，不冒充 Coordinator/Reviewer 亲验）

不得修改上述源码或二进制资产；不得因为证据缺失补写日志、截图、用户原话或运行结果。

## 强制收尾顺序

1. Coordinator 核对 P4-001～003 归档完整性、白名单、作者/提交顺序与所有历史 `REVISE`/`USER ACCEPTED` 边界。
2. Verification 只读核对当前可定位 Build 日志、PIE 来源、源码静态边界和文档一致性；不运行新 Build/Editor/PIE，除非用户另行授权且新增任务卡。
3. Teacher 读取真实 P4 源码与证据，讲解 DataAsset/Runtime/Return、AI 事件链、initiative、OpenLevel 生命周期和失败恢复；先保存用户原始答案，再纠正并记录掌握项/复习项。不得代答或把摘要冒充原话。
4. Independent Reviewer 只读审查实际 diff、反射/GC/Tick、GAS/网络边界、Build/Editor/PIE 证据等级、失败路径与文档一致性，给唯一结论。
5. Coordinator 只有在 Reviewer 放行或用户明确接受仍存在的风险后，才同步 Phase 4 状态并归档；两种处置不得混写。

## 必须核对的 Phase 4 Gate

- P4-001：A2 后 Editor/PIE 为用户 `USER ACCEPTED`；Build/UHT/C++/Link、DTO 无运行指针、单次 Consume 与历史 Git/同身份偏差保持真实等级。
- P4-002：A1 Build 日志已删除、仅报告级；02:07 PIE 是用户证据；BP 路径事后接受、Map 误保存撤回、mixed commit/同身份保留；目标销毁、重复 perception、MoveTo Failed/Aborted、独立 UnPossess follow-up 保留。
- P4-003：A4c 仅一份可定位 Build 日志；执行报告不得写“4/4 resolved”，只能写静态 guards/1 of 4 dynamic；HandleTravelFailure、AlreadyPending、invalid RequestId、World=null、失败重试及三项 P4-002 场景的用户接受/延期边界保留。
- 三种 initiative、Encounter/Return 纯值 Context、空图单次消费/返回、至少两轮往返与 P4-002 组合证据来源清楚区分 S/B/D/User。
- 无新增 Gameplay/UI Tick、Actor/ASC/Widget 跨图引用、GAS 规则、RPC/Replication/Prediction、Phase 5 BattleResult/正式返回。

## 证据矩阵（不得补造）

- [ ] 每个 P4 包有实际允许文件、作者/commit、审查结论和归档路径。
- [ ] 每个 P4 包的 Build/UHT/C++/Link 证据标明可定位日志、报告级或缺失。
- [ ] Editor 重开、PIE 主路径、失败路径、回归路径标明用户证据/Reviewer 证据/未验证。
- [ ] `Saved/Logs` 中的旧错误、修复前 crash、删除的 Build log 和用户接受项均保留历史，不改写。
- [ ] 文档状态与实际归档一致；Phase 4 不因三个工作包 `PASS WITH FOLLOW-UP` 自动写成无风险完成。
- [ ] Teacher 保存用户原始答案与纠正链；Reviewer 结论独立且不由 Coordinator 预填。

## 明确非目标与停止条件

- 不修改任何 `Source/`、`Content/`、`Config/`、`.uproject`、Build.cs 或插件；不创建新 Gameplay/资产/输入/UI。
- 不运行 Build、Editor、PIE、Automation 或外部网络；若收尾需要新运行证据，停止请求用户授权和新任务卡。
- 不执行 Git stage/commit/push/reset/clean，不删除 Build logs、Saved logs 或历史归档。
- 不进入 Phase 5 Battle Actor/ASC、TurnManager、Ability、伤害、胜负、奖励、正式 BattleResult/返回。
- 发现归档缺失、证据互相矛盾、用户处置无原话或 Reviewer 结论缺失时，停止并判 `Not verified/Blocked`，不得用文档措辞掩盖。

## 当前唯一下一步

当前 Reviewer 结论为 `REVISE`。先由 Verification、Teacher、Reviewer 分别提交自己的真实产物；Coordinator 不代交。随后 Coordinator 只提交协调/归档 Markdown，再交 Independent Reviewer 复审。最终放行前不得归档 P4-004、push 或进入 Phase 5。
