# TASK-P16-001 — Save Envelope、v6 与显式迁移

Status: `PLANNED / USER AUTHORIZATION REQUIRED`

Phase 16 Gate 0 已最终复核 `PASS`。本任务只建立已冻结的 on-disk format v1、canonical payload codec v1、schema v6、结构化版本分类与 `v1->v2->v3->v4->v5->v6` 相邻迁移，并以 Automation 证明失败时 Runtime 零污染。

允许文件：`Source/HSR/Save/HSRSaveVersion.h/.cpp`、`HSRSaveTypes.h`、`HSRSaveGame.h/.cpp`、`HSRSaveSubsystem.h/.cpp`、`Source/HSR/Tests/HSRSaveVersionTests.cpp`、`HSRSaveSubsystemTests.cpp`、`docs/save-system-design.md` 与任务三件套。

不实现 staging/primary/backup，不修改 Gameplay authority、Content、Config、UI、Battle 或 GAS。没有真实历史 fixture 时必须标记 synthetic/current-layout，不能宣称真实旧二进制兼容。

等待用户确认执行 TASK-P16-001。
