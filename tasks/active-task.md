# TASK-P9-006：Phase 9 独立验收、教学与阶段归档

状态：`CLOSEOUT CONTRACT ACTIVE / IMPLEMENTATION COMPLETE / PHASE NOT READY`

## 唯一目标

不新增 Gameplay。汇总 P9-000～005 的 Build、用户资产、两轮 PIE、失败/REVISE 历史、Teacher 学习与 Independent Reviewer Gate；完成作者归属、三件套、状态文档和阶段交付准备。只有最终 Gate、角色提交和远端交付均闭合后，Phase 9 才可标记 Ready with inherited follow-ups。

## 允许范围与禁止

只允许 Coordinator/Teacher/Reviewer/Implementation 各自的 Markdown 证据文件、必要 fresh Build/只读 Git 核对和用户回传证据。禁止新增/修改 Gameplay、Config、Content、资产、Tags、UI、P9-005 逻辑、Phase 10 工作；禁止 reset/clean、混合作者认领或提前 push。

## Gate

- Fresh HSREditor Build：UHT/C++/Link/metadata/exit0，首错与 warnings 保留。
- 最终矩阵基线：P9-005/004/003/002/001=`28/38/36/16/28 PASS`、各2 COMPLETE、零失败；P6-004A 空VM warnings 为 HARNESS EXPECTED。
- Asset/Config/Content 路径和 User 作者归属逐项对账；Config EOF 在提交前处理并重跑全局 diff-check。
- Teacher 记录真实回答、掌握/复习/未评估；Reviewer 独立给出 PASS/PASS WITH FOLLOW-UP/REVISE。
- P9-000～005 三件套、历史 BLOCKED/REVISE/失败、follow-ups、dirty tree/provenance 精确闭合。

## 首次响应门禁

各角色首次复述自己的范围、证据与禁止项，等待用户确认。P9-006 不授权 Phase 10 或 Gameplay/Git 写操作；角色提交/Phase push 仅按项目长期规则和最终 Gate 单独执行。
