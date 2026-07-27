# TASK-P17-PATCH-01D — Patch 01 Regression and Closeout

Status: `PLANNED / TASK GATE REVIEW REQUIRED`

## Single outcome

对 PATCH-01A～01C 做一次不新增 Gameplay 的最终回归与证据审计，确认 ActiveStatus、RepeatableBreak、ActionDistance 及既有 Battle/Progression 边界可共同通过，并把 Patch 01 结论真实同步到计划、状态、日志和归档；PIE 未运行项必须明确保留为 `NOT VERIFIED`。

## Frozen scope and ownership

- 本任务只允许测试、证据汇总、文档状态同步和三件套归档；禁止修改 Status、Break、TurnManager、Coordinator 或其他生产 Gameplay 逻辑。
- 已通过的 PATCH-01A/B/C Reviewer 结论与历史 REVISE/Build 首错不得覆盖或改写。
- Automation、Build、PIE、用户提供证据必须分级记录；Automation 不冒充 PIE。
- 若最终回归发现生产缺陷，本任务停止并创建独立 revision task card；不得在 closeout 卡内顺手修复。

## Exact allowlist

- `Source/HSR/Tests/HSRCombatPatchTests.cpp`（仅当 Task Gate 证明现有测试入口无法组合运行时，才允许最小测试编排修订；禁止生产行为变化）
- `docs/phase-17-patch-01-execution-plan.md`
- `PROJECT_STATE.md`
- `worklog.md`
- `todo_plan.md`
- `learning-journal.md`
- `tasks/execution-result.md`

Implementation 不得修改生产 Battle/Status/Turn 代码、Config、Content、Build.cs、Save、UI、网络、Behavior Tree 或 P17-005 文件。需要任何白名单外修订时立即停止申请最小授权。

## Required evidence matrix

- Fresh `HSREditor Win64 Development`：记录 Target、UHT、Compile、Link、WriteMetadata、exit code 与首个真实错误/警告。
- 运行 `Automation RunTests HSR.Battle`，至少确认 ActionDistance 六组、RepeatableBreak、StatusGeneric、BattleReturn MapContract 全部 Success。
- 运行可用的 `HSR.Progression` 定向 Automation；不存在或环境不可用时记录准确命令与 `NOT RUN`，不得写成通过。
- 审计 PATCH-01A 用户 P9-001/002/003 证据、PATCH-01B 用户 P9-003 19-case PIE 与 PATCH-01C `PIE NOT RUN` 边界，保持来源等级。
- `git diff --check`；审计 PATCH-01A～C commit/provenance、三件套归档、allowlist 和用户本地 `learn/SaveSystem.md`、`.claude/**` 隔离。
- 输出 Patch 01 总结：方向 1～3 是否全部关闭、保留 follow-up、为何 Behavior Tree 属于独立 Patch 02、为何 P17-005 尚未开始。

## Acceptance

- 所有实际运行的 Build/Automation 通过且日志可复核；任何失败保留首错并返回 `REVISE/BLOCKED`，不能用旧成功替代。
- 文档只陈述真实证据；PATCH-01A～C 三件套已归档，01D 完成后再由 Coordinator 归档本卡。
- Independent Reviewer 确认范围、证据等级、用户文件隔离与 Patch 01 完成结论为 `PASS` 或 `PASS WITH FOLLOW-UP`。

## Non-goals and stop conditions

不实现 Behavior Tree Patch 02、P17-005、正式 Action Bar、资产、Config、网络、Save 或任何生产 Gameplay。发现回归失败、缺少必要测试入口、需修改生产代码或需用户 Editor 操作时停止并报告精确边界；未经授权不得扩权。
