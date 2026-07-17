# TASK-P2-003：Phase 2 教学、最终回归与阶段收尾

> 最终处置（2026-07-17）：`USER ACCEPTED`。Reviewer commits `8c34a33`、`0e1c7c8` 的唯一 final-review 结论仍为 `REVISE`；用户明确接受尚未完全掌握的学习项作为非阻断复习项，并授权 Coordinator 完成 Phase 2 收尾。本注记不构成 Reviewer `PASS`。

## 角色锁定与当前状态

- Phase 2：工程闭环已完成，阶段收尾进行中；尚未判定 `Ready`。
- P2-001：已归档为 `USER ACCEPTED`，保留历史 Reviewer `REVISE` 与用户接受的证据边界。
- P2-002：最终 Reviewer `PASS`，已归档；A1/A2/A3 和历史 `REVISE` 链完整保留。
- P2-003：Gate 0 用户资产提交与 Gate 1 Coordinator 工程核对/交接已完成；当前等待 Teacher 首次接收，尚未开始教学。
- Phase 3：禁止开始。

本卡的强制角色链为：用户资产作者 → Coordinator 核对/交接 → Teacher 教学与出题 → 用户学习者原始作答 → Teacher 纠正/评估/文档与独立提交 → Reviewer 独立只读审查与独立提交 → Coordinator 最终收尾。任何角色不得冒充其他作者、Reviewer 或提交者。

## 唯一可验收结果

Phase 2 的真实资产作者提交、工程证据复核、GAS 概念与源码教学、用户复述/预测/Debug 练习、长期文档、角色提交与最终 Git 交付全部闭合；只有全部门禁满足时，Coordinator 才能把 Phase 2 标记为 `Ready`。

## 强制顺序

### Gate 0：用户资产作者提交（已完成）

用户先提交自己当前工作树中的以下资产：

- `Content/GameplayEffects/BP_GE_InitializeCoreAttributes.uasset`
- `Content/GameplayEffects/BP_GE_Test_HealthBelowZero.uasset`
- `Content/GameplayEffects/BP_GE_Test_HealthAboveMax.uasset`
- `Content/GameplayEffects/BP_GE_Test_LowerMaxHealth.uasset`
- `Content/GameplayEffects/BP_GE_Test_EnergyBounds.uasset`
- `Content/GameplayEffects/BP_GE_Test_SpeedBelowZero.uasset`
- `Content/UI/WBP_AttributeDebug.uasset`

提交必须由用户本人完成并回传 commit hash。Coordinator、Teacher 或其他 agent 不得冒充资产作者，也不得代办 stage/commit；若用户希望代理提交，必须在新消息中另行明确授权，并仍如实记录真实作者与代理操作边界。此门禁完成前，不进入后续核对或教学。

完成证据：`44808d9d30380efd66bd25a68a24eb22cb97e36c`，Git author `lai_sgjp`，精确包含上述七个资产，无 Source、Config、地图或其他资产夹带；提交后工作树干净。

### Gate 1：Coordinator 工程交接核对

Coordinator 只读核对并记录：

- P2-001/P2-002 的最终源码、任务归档和实际 commits。
- 用户资产 commit 的文件范围与作者归属。
- 最终 `HSREditor Win64 Development` 构建证据。
- Editor 重开、初始化主路径与缺失 GE 失败路径。
- 五个测试 GE、Clamp、Re-Possess、HUD 单 snapshot/teardown 与两轮连续 PIE。
- 仍存在的非阻断风险，不把用户证据冒充为 Coordinator 或 Reviewer 亲自运行。

核对完成后，Coordinator 将真实源码路径、diff/commits、证据、历史错误与未验证风险交接给 Teacher。

状态：已完成。累计交接摘要位于 `tasks/execution-result.md`；当前进入 Gate 2 Teacher 首次接收门禁。

### Gate 2：Teacher 教学与出题

Teacher 必须先完成概念教学、源码导读并向用户出题：

- ASC、AttributeSet、GameplayEffect、Attribute Delegate 的职责。
- Owner Actor、Avatar Actor 与 Ability Actor Info。
- 构造期默认子对象与运行期 Actor Info 初始化的区别。
- 初始化 GE 幂等与普通测试 GE 可重复应用的区别。
- Health/MaxHealth、Energy/MaxEnergy、Speed 的 AttributeSet Clamp 边界及实际覆盖范围。
- 初始 snapshot、Delegate 重复绑定/解绑、ViewModel/Widget/GC 生命周期。
- UI 只是观察者，ASC/AttributeSet 是 Gameplay 真源。
- 按 `ASC/Actor Info → GE → AttributeSet → Delegate → ViewModel/Widget` 分层 Debug。

此时 Teacher 不得预先评估用户掌握度、替用户作答、写成练习已完成或创建 Teacher commit；必须先等待用户提交原始答案。

### Gate 3：用户原始作答门禁

用户须完成：

1. 用自己的话复述上述核心职责与数据流。
2. 回答预测题：Health 初值为 100，Re-Possess 后初始化入口再次运行；Add 与 Override 初始化 GE 在无幂等保护时分别可能发生什么？为什么 HUD 去重无法修复？Owner 有效但 Avatar 为空时，哪些对象可能存在，哪些后续行为仍不可靠？
3. 完成真实 Debug 练习：面对“HUD 为 0、初始化入口两次、测试 GE 后 UI 不刷新”，按固定层级定位并说明最小修复与复验路径。

用户答案必须先以原始形式保留，不能由 Teacher 代答或先润色成正确答案。

### Gate 4：Teacher 纠正、评估、文档与独立提交

Teacher 在收到用户原始答案后，逐项纠正并记录掌握项、未掌握项和非阻断复习建议；随后更新允许的教学文档与执行记录，创建独立 Teacher commit，并向 Coordinator 回传教学产物、掌握度与 commit hash。Teacher 不得判定 Phase 2 `Ready`。

### Gate 5：Reviewer 独立只读审查与独立提交

Reviewer 独立只读核对工程 Gate、用户资产提交、Coordinator 交接、Teacher 教学、用户原始作答、Teacher 纠正/掌握度、角色 commits 与文件范围。Reviewer 的唯一可写产物为 `tasks/archive/TASK-P2-003-final-review.md`；Reviewer 独立创建该文件并提交自己的 commit，结论只能是 `PASS`、`REVISE` 或 `BLOCKED`。Coordinator 不得创建、代写或修改 Reviewer 结论，只可在后续归档中原样保留。

### Gate 6：Coordinator 最终 Ready Gate 与交付

只有 Reviewer `PASS` 后，Coordinator 才核对 docs 计划 §8.4 的工程 Gate、学习 Gate、角色 commits 与工作树，更新长期文档、归档任务并创建 Phase 2 收尾 commit。工程/学习 Gate 的 `Ready` 判定与远端 push 交付状态必须分开记录：按项目规则尝试 push 且禁止 force push；push 成功则记录远端与结果，push 失败则保留真实错误和未完成交付状态，不得伪造已推送。Reviewer 未 `PASS` 时不得判定 `Ready` 或进入 Phase 3。

## 精确允许范围

### 用户资产作者

仅 Gate 0 列出的七个 `.uasset`，以及 Git 提交操作；不得夹带 Source、Config、地图或其他资产。

### Coordinator 可修改 Markdown

- `tasks/active-task.md`
- `tasks/execution-result.md`（按角色交接需要创建）
- `tasks/archive/TASK-P2-003-active-task.md`
- `tasks/archive/TASK-P2-003-execution-result.md`
- `PROJECT_STATE.md`
- `todo_plan.md`
- `worklog.md`
- `README.md`
- `docs/phase-2-execution-plan.md`
- `docs/gas-notes.md`
- `learning-journal.md`

### Teacher 可修改 Markdown

- `learning-journal.md`
- `docs/gas-notes.md`
- `tasks/execution-result.md`

Teacher 若需记录到其他 Markdown，必须先由 Coordinator 扩权。所有 Source、Content 与 Config 对 Teacher 只读。

### Reviewer 可修改 Markdown

- `tasks/archive/TASK-P2-003-final-review.md`

Reviewer 对其余文件只读；Reviewer 必须独立提交自己的审查产物。

## 明确禁止

- 不修改任何 `Source/**` Gameplay 实现、`Content/**` 资产内容、`Config/**`、`.uproject` 或模块依赖。
- 不新增 Ability、伤害、治疗、回合、交互、网络、存档或 Phase 3 内容。
- 不重新设计 ASC 所有权，不把调试按钮变成正式 Gameplay。
- 不删除或改写 P2-001/P2-002 的历史 `REVISE`、作者归属或证据边界。
- 不 force push，不提交其他人的未提交产物，不伪造 Build/Editor/PIE/教学证据。

## 证据与交接要求

- 用户资产 commit hash 与精确文件列表。
- P2-001/P2-002 Implementation、Reviewer/用户验收、Coordinator 与资产作者的真实 commits/归档。
- `HSREditor Win64 Development` 成功日志摘要及用户 Editor/PIE 证据来源说明。
- Teacher 教学/出题记录、用户原始复述/预测/Debug 作答、作答后的 Teacher 纠正与掌握度。
- Teacher 独立 commit hash。
- Reviewer 独立 final-review、明确结论与独立 commit hash。
- Reviewer `PASS` 后的 Coordinator 最终 Gate 清单、收尾 commit、push 真实结果与干净/已解释工作树；push 失败与 Ready 状态分开记录。

## 首次角色门禁

当前唯一下一步是 Teacher 首次接收。Teacher 首次行动只能读取 `tasks/active-task.md` 与 `tasks/execution-result.md`，先复述教学范围、出题与等待原始作答门禁、作答后才能纠正/评估/提交、允许文档与禁止项，并以以下精确文本结束：

`等待用户确认开始 TASK-P2-003 教学。`

用户再次明确确认前，Teacher 不得开始教学或修改文档。

## 当前执行结果

- 已完成：P2-002 最终 `PASS` 归档；P2-003 契约；Gate 0 用户资产提交；Gate 1 Coordinator 工程核对与 Teacher 交接摘要。
- 当前门禁：Teacher 首次只读接收并复述，等待用户确认开始教学。
- 未完成：Teacher 教学/出题、用户原始作答、Teacher 纠正/评估/提交、Reviewer 独立审查/提交、Phase 2 最终 Ready Gate、收尾 commit 与 push 交付。
- Phase 3：不得开始。
- Git：Coordinator 本轮不提交、不 push。
