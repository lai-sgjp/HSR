# TASK-P12-004C: Development PIE Harness

Status: `ARCHIVED / PASS`

## 唯一验收结果

在不实现正式 Inventory/UI 的前提下，用户可在 PIE 控制台建立固定 Character.A 装备/两件套、执行 2->1->2、清空、Save/Load，并显示/重建只读 Equipment Detail Widget。

## 精确范围

- 允许新增 `Source/HSR/Equipment/HSREquipmentDevelopmentHarness.h/.cpp`。
- 允许新增 `Source/HSR/Tests/HSREquipmentDevelopmentHarnessTests.cpp`。
- 允许对 `Source/HSR/Battle/HSRBattleCoordinator.cpp` 和 `Source/HSR/Equipment/HSREquipmentEffectBridge.cpp` 增加 P12-004C PIE 失败诊断日志；不得改变正式 Gameplay 语义，除非后续独立复审确认需要修正。
- 允许修改本任务三件套。
- 不修改 Config 或用户 Content 资产。
- Harness revision 单调性只承诺同一 PIE/GameInstance 会话；空 loadout revision 不扩展正式 Save DTO，跨 PIE 空状态 revision 作为非阻断 follow-up。
- 必须保留用户资产作者身份和 `USER PROVIDED` 证据等级。
- `.claude/settings.local.json` 不得修改、暂存或提交。

## 已有工程证据

- Development Editor Build：成功，最新模块 DLL 2026-07-25 20:19:20，UBT `Result: Succeeded`。
- `HSR.Save`：6/6 Success，exit code 0，20:20:34。
- `HSR.Equipment`：4/4 Success，exit code 0，20:15:19。
- `HSR.UI.EquipmentDetail.ViewModel`：1/1 Success，exit code 0，20:20:05。
- `git diff --check`：通过，仅换行符警告。
- P12-004 Independent Reviewer：`PASS WITH FOLLOW-UP`。
- 用户 Editor 资产均在约定路径且重开后无错误。

## Harness 命令合同

- `HSR.Equipment.Setup`
- `HSR.Equipment.RemoveSecondRelic`
- `HSR.Equipment.RestoreSecondRelic`
- `HSR.Equipment.Clear`
- `HSR.Equipment.ShowDetail`
- `HSR.Equipment.HideDetail`
- `HSR.Equipment.Save`
- `HSR.Equipment.Load`
- `HSR.Equipment.Cleanup`

## 后续 Gate

1. Build 和 Harness Automation。
2. Independent Review。
3. 用户 PIE Gate。
4. 归档 P12-004C 并恢复 P12-005 收尾。

## 非目标

不进入 Phase 13，不实现正式 Inventory/Reward/Drop 或可操作装备 UI，不新增资产、模块、依赖、网络或正式美术。
