# TASK-P1-006：Phase 1 教学与阶段收尾

## 1. 唯一目标

在不修改 Gameplay C++、Blueprint 或资产的前提下，完成 Phase 1 工程 Gate 核对、Teacher 技术教学与源码复盘、用户练习、Teacher 独立教学提交，以及 Coordinator 最终阶段判定与收尾提交/push。

本文件是 P1-006 唯一任务卡。内部按 G0～G5 串行推进，只更新本卡和累计执行结果，不为步骤另建任务卡，避免多余 CC-SWITCH。

## 2. 当前状态与交接

- 当前阶段：Phase 1 进行中，P1-001～P1-005 已归档。
- P1-005 最终 Reviewer：`af6b14898f589cd44fbd176488dcd5e82c309d4b`，结论 `PASS WITH FOLLOW-UP`。
- 当前步骤：`G1 Teacher 接收`。
- 当前交接：`Coordinator → Teacher`。
- Teacher 无需 Implementation Agent 式首次复述，也不需要重新创建活动卡；收到本卡后直接核对接收条件并开展教学。
- 当前不进入 Phase 2，不 push。

## 3. 全流程状态表

| Gate | 角色 | 状态 | 退出条件 |
|---|---|---|---|
| G0 工程 Gate 汇总 | Coordinator | `完成（依据已归档证据）` | P1-001～P1-005、Build/Editor/PIE/失败路径和风险边界可追溯 |
| G1 Teacher 接收 | Teacher | `READY` | 读取本卡、Phase 1 源码/diff/commits、归档证据和学习文档 |
| G2 教学与源码复盘 | Teacher | `LOCKED` | 覆盖本卡全部技术主题，不修改工程实现 |
| G3 用户练习 | Teacher + 用户 | `LOCKED` | 完成复述、预测题、源码阅读题、Debug 练习并记录真实结果 |
| G4 Teacher 教学提交 | Teacher | `LOCKED` | 更新授权教学文档并创建独立 Teacher commit，回传完整 hash |
| G5 Phase 1 最终收尾 | Coordinator | `LOCKED` | 接收 Teacher hash，复核工程/学习/Git Gate，独立收尾 commit；满足授权后 push |

## 4. G0：Coordinator 工程 Gate 交付

### 已完成工程范围

- P1-001：CharacterBase、ExplorationCharacter、CharacterMovement、CameraBoom、FollowCamera。
- P1-002：PlayerController、Possession/UnPossession、幂等 ControlMode。
- P1-003：Enhanced Input C++、IA_Move/Look/Jump/Interact、IMC_Exploration。
- P1-004：GameMode、派生 Blueprint、灰盒探索地图、HUD/UserWidget、可玩闭环和输入生命周期修复。
- P1-005：角色/骨骼/动画、AnimBP、资产引用、最终回归和失败路径。

### 必须保留的证据边界

- Build、Editor、PIE 和二进制资产内部配置中，部分结果来自用户回传；不得改写为 Teacher 或 Reviewer 独立运行。
- P1-005 许可证台账为 `docs/asset-licenses/mixamo-kachujin.md`；状态 `OWNER ACCEPTED`。未来正式发布、迁移资产或改变分发方式时复核官方条款，只是非阻断 follow-up。
- `Config/DefaultEditor.ini` 是本地预览配置，已精确忽略，不提交。
- MSVC 非 preferred 版本是已知非阻断风险；已有成功构建证据。

## 5. Teacher 正式接收材料

Teacher 必须读取并以真实仓库内容为准：

1. `tasks/active-task.md` 与 `tasks/execution-result.md`。
2. `Source/HSR/Character/`、`Source/HSR/Player/`、`Source/HSR/Framework/`、`Source/HSR/UI/`、`Source/HSR/HSR.Build.cs`。
3. `tasks/archive/TASK-P1-001-*` 至 `TASK-P1-005-*` 的活动卡、执行结果和已有审查报告。
4. `docs/phase-1-execution-plan.md`、`PROJECT_STATE.md`、`worklog.md`、`learning-journal.md`。
5. 与 Phase 1 相关的真实 commits/diffs，至少包括：
   - P1-001 Implementation `14b3cc96592f65dd5a0db8c1ef2ffd3987345c32`
   - P1-002 Implementation `7fef22f1afc400c9488ad36b1ae39a8c03435ec5`
   - P1-003A Implementation `490440f280f82dbffd84976bad1dd2a92ee1f39d`
   - P1-003B 用户资产 `7c71ae825fb840ace6d76fc6232883b807d395d1`
   - P1-004 C++ `ac4db1a`、用户资产 `a9446a8`、受控修复 `074e5fc`、A2 `cec07d661598c6f21587a913232403b1e6ce6a80`、最终 Reviewer `6b19d179562f03c8cc50b94456d3a943478855c0`
   - P1-005 Implementation `649e125` / `0c85794` / `3d94b74`、用户资产 `a539b6d994d638529754c0ce8da6b3b3432b4794` / `abca67921f31a6ddfc5dee468bdd7fb0cdb598d6`、Coordinator `1e8e155db6d18339496f46d67662f88a5de3e009`、Reviewer `af6b14898f589cd44fbd176488dcd5e82c309d4b`。

缺少某个短 hash 的完整值时，Teacher 可用只读 Git 查询取得；不得据提交说明臆测实现。

## 6. G2：Teacher 教学与源码复盘范围

Teacher 必须以项目真实代码和 bug 为例，覆盖：

1. `Actor → Pawn → Character`，Capsule/CharacterMovement、SpringArm/Camera 的职责与组件附件关系。
2. PlayerController、Possession/UnPossession、Pawn 生命周期与 `EHSRPlayerControlMode` 的幂等切换。
3. Enhanced Input 数据流：设备 → IMC → IA → EnhancedInputComponent → Character/Controller；Action 值类型、Binding 和 Context 对称生命周期。
4. GameMode 的默认 Pawn/Controller/HUD 职责、World/PlayerStart/Spawn/Possess 关系。
5. HUD 与 UserWidget 的创建、Owning Player、单实例、空配置安全和“显示层不是真源”。
6. AnimBP 如何消费 Speed/Direction/IsFalling，为什么动画、Root Motion 或 Montage 回调不能成为本阶段 Gameplay 真源。
7. UE 反射与 GC：`UCLASS/USTRUCT/UENUM/UPROPERTY/UFUNCTION`、`generated.h`、CDO、`TObjectPtr`、Transient、UObject 引用生命周期。
8. Tick 边界：Character/Widget 不轮询；PlayerController 的引擎输入处理依赖 Tick，不能机械关闭基类 Tick。
9. 真实 bugs 与修复：PlayerController Tick 导致 Enhanced Input 静默；Context 生命周期与幂等；手工 `PushInputComponent` 的重复输入栈风险；高频 Move/Look 日志清理。
10. Debug 流程：从 Spawn/Possess、LocalPlayer/EnhancedPlayerInput、IMC/Action、InputComponent/Bindings 到 `ProcessPlayerInput/PostProcessInput` 的逐层定位，以及第一处真实错误证据原则。

Teacher 要结合具体文件、类、函数和 commit diff 讲解，不得只输出通用 UE 教程。

## 7. G3：用户学习 Gate

Teacher 必须引导并记录用户真实回答，至少包括：

- 1 分钟复述：第三人称探索输入从按键到移动/镜头的完整链路。
- 预测题：若 Possess、IMC、Action、Binding 都正确，但所有 Enhanced Action 回调沉默，应优先检查什么，为什么？
- 源码阅读题：指出 Context 的添加/移除入口、Action 的绑定入口、HUD 幂等创建入口和 AnimBP 的状态来源。
- Debug 练习：给出“UIOnly 恢复后输入重复”或“Re-Possess 后触发两次”的症状，要求用户按证据顺序定位并提出最小修复。
- 反射/GC 题：解释运行时状态为何可用 `Transient`，UObject 资产引用为何需要受 GC 追踪。

Teacher 必须如实记录“掌握 / 部分掌握 / 待复习”和错误修正，不得代替用户回答或把未回答写成通过。若用户尚未完成练习，P1-006 保持活动，不标记 Blocked，也不进入 Phase 2。

## 8. Teacher 允许与禁止修改

### 允许修改

- `learning-journal.md`
- `docs/interview-notes.md`
- `docs/portfolio-notes.md`
- 如确有必要，新建或更新 `docs/phase-1-learning-review.md`
- `tasks/active-task.md` 仅更新 G1～G4 状态和 Teacher 交接记录
- `tasks/execution-result.md` 仅追加教学过程、用户回答、掌握度、Teacher commit 计划与未掌握项

### 严格禁止

- `Source/**`、`Content/**`、`Config/**`、`.uproject`、Build.cs、Blueprint、动画或地图资产。
- 构建、Editor/PIE 修改、顺手修 bug、进入 Phase 2、修改许可证结论或删除历史审查证据。
- 代替用户编造回答、代替 Coordinator 宣布 Phase 1 Ready、替 Coordinator push。

## 9. G4：Teacher 提交与交接

Teacher 完成教学且用户练习有真实记录后：

1. 检查 diff 只包含第 8 节允许文件。
2. 创建独立提交：`高级教师+Teacher+TASK-P1-006/Phase 1+完成技术教学与源码复盘`。
3. 回传完整 commit hash、实际修改文件、用户掌握度、未掌握项和任何非阻断 follow-up。
4. 正式交接：`Teacher → Coordinator`。

Teacher 不 push。

## 10. G5：Coordinator 最终收尾

收到 Teacher 完整 hash 后，Coordinator 必须：

1. 核对 Teacher commit 作者范围与教学/练习证据，不代写 Teacher 产物。
2. 复核 Phase 1 工程 Gate、学习 Gate、资产许可台账、工作树与全部人格提交。
3. 更新并归档本卡和累计执行结果，同步 `PROJECT_STATE.md`、`todo_plan.md`、`worklog.md`、`README.md`、`docs/phase-1-execution-plan.md`。
4. 仅当所有 Gate 真实满足时，将 Phase 1 标记 `Ready`；否则保持 `Not verified` 并列出证据缺口。
5. 创建独立 Coordinator Phase 1 收尾 commit。
6. 按项目长期授权和远端状态执行 push；push 前再次检查分支、远端和工作树，记录结果。若远端/权限失败，如实报告，不影响本地 Gate 判定。

## 11. P1-006 完成条件

- G0～G5 全部有真实产物与角色独立提交。
- Teacher 覆盖全部指定技术点和真实 bug，并完成用户复述/预测/阅读/Debug/GC 练习记录。
- Teacher 不修改 C++ 或资产，Coordinator 不冒充 Teacher。
- Phase 1 状态与 Build、Editor、PIE、失败路径及学习证据一致。
- 没有未解释 Error/Ensure/Accessed None、重复输入绑定或生命周期问题被文档掩盖。
- 第三方资产 follow-up 仍为未来分发变化时复核，不阻断当前阶段。

## 12. 非目标

任何 Phase 2 GAS 实现、Gameplay 修复、正式发布、资产替换、历史重写、额外 Reviewer 轮次、自动化测试框架或新任务卡。
