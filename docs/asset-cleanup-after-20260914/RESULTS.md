# 资产清理结果与复验

2026-09-14：已执行用户确认的原清单中全部 2,093 个“建议删除”文件，项目内减少 7,758,630,565 字节（7.23 GiB）。逐文件比对原始清单，未发现未授权文件缺失，未追加删除新候选。

- [可搜索、按状态和目录筛选的报告](index.html)
- [已删除的 2,093 个文件](deleted.csv)
- [清理后剩余的 1,264 个文件](files.csv)：708 个仍被使用，556 个待核实。
- [原始确认清单](../asset-cleanup-review/files.csv)保留不变。

## 备份与删除方式

清理前已推送的备份提交为 `0691fe397cf3d5506c3f16313e6936d43a4e7640`，远端 `origin` 为 `https://github.com/lai-sgjp/HSR.git`，分支 `main`。Git 仅覆盖其中 18 个受跟踪文件，另外 2,075 个被忽略文件不在 Git 备份内。

全部 2,093 个文件另有项目外完整副本：`E:\work\unreal_projects\HSR_asset_backups\20260914_172410_approved_cleanup`。目录内的 `manifest.json` 记录逐文件路径、大小和 SHA-256，复制完成后已逐文件验证。恢复时关闭编辑器，按备份内的 `Content` 相对路径复制回项目，再重开编辑器扫描。备份仍占用同一磁盘空间，因此项目体积下降不等于磁盘净释放 7.23 GiB。

编辑器原生删除了 1,925 个 UE 资源包。原生删除过程中遇到引擎断言和内存提交不足；剩余 89 个旧包在新编辑器会话中重新检查硬引用、软引用和管理引用，确认没有保留资源引用后，关闭编辑器，核对外部备份及哈希，再按精确路径清理。另清理 79 个已确认的 FBX/PNG 导入源文件和 166 个已空的资产目录。没有扩大到缓存、插件、代码或私人文件。

删除后重新打开编辑器并完整扫描 AssetRegistry，剩余 1,261 个 UE 资源包。Git 的 Content 差异仅为批准清单中的 18 个删除，没有修改保留资产。本轮清理及报告保留在工作区，未另行提交或推送。

## 清理后验证

|证据层级|结果|本地记录|
|---|---|---|
|静态与资产扫描|四人资源审计完成；全部批准文件已删除，未授权缺失为 0|`Saved/Presentation/CleanupValidation/character_audit.json`、`cleanup-result.json`|
|Automation|52 项通过、0 失败；其中 10 项带预期警告|`Saved/Presentation/CleanupValidation/Automation/index.json`|
|PIE 探索|观景厅调查、实际前往城区、四人切换移动、巡逻追踪与接触进战通过|`Saved/Presentation/CleanupValidation/` 下的探索和角色 JSON|
|PIE 战斗|完成 20 次行动并结算，返回城区后已击败巡逻敌人不再出现|`paced_battle_validation.json`、`patrol_return_pie.json`|
|PIE 遗器与存档|装备最大生命 135，卸下 115，读档恢复 135；切换后实际角色属性为 135|`Saved/Presentation/CleanupValidation/relic_save_pie.json`|
|Cook|三张正式地图 Windows Cook 成功，退出码 0，0 错误、0 警告|`Saved/Presentation/cleanup_cook.log`|

本轮没有修改 C++，沿用修复阶段的构建证据。上述 PIE 和 Cook 均在删除后重新运行；本轮未重复全部分辨率 HUD 截图或全部技能组合验收。存档验证只使用任务专用 `PlayableFixQA_*` 槽位。

## 剩余资产

556 个待核实文件包含制作脚本、测试、重定向中间资源及资源包内部依赖，不能仅凭未被正式地图直接引用判定可删。本轮保守审计没有新增明确“建议删除”文件；这些资源全部保留。任何后续删除都需要新的逐文件清单确认。
