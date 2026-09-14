# 可玩流程修复与资产清理

## 授权与顺序

修复并验证四人动画、探索、UI、战斗及遗器；审核可纳入 Git 的当前改动后提交至 origin/main。
推送验证完成后提供逐文件清理清单，用户确认具体清单后才删除。被忽略的候选资产先另做本地备份。
不把 Git 推送视为 Content/Assets 等被忽略资源的备份。

## 已定位的问题

- 数字键切换仅按共同 PawnClass 生成角色，未调用角色 ID 投影，导致显示蓝图默认 Kachujin。
- 火花原 ABP 仅使用 Retarget Pose From Mesh，依赖当前 Pawn 不具备的源 Mesh。
- 原迁移脚本创建的 UE 5.6 IKRetargeter 没有 Retarget Ops；文件存在及骨架相同不能证明输出动作。
- 原地图制作脚本仅放置静态挑战物，没有真实巡逻 Pawn 与导航覆盖。
- 小地图仅枚举 2200 cm 内的 SceneInteraction，排除了宝箱和远处目标。
- 装备变更调用了详情刷新，但 RefreshSelected 仅比较角色成长 RuntimeRevision，错误跳过装备刷新。
- 敌人对候选目标排序后固定取第一个；行动解析同步排空敌人队列，没有起手和收尾时间。
- 装备 GE 按目标合并堆叠，强化重应用后可能翻倍；现改为独立效果，并保留当前生命、在最大生命下降时夹紧。
- 导航尚未生成完成就进入 PIE，导致没有可用巡逻路径；现缩小庭院导航范围，完成烘焙后保存，AI 等待导航就绪。
- 本地 Death 动画只有死亡起手，末帧仍站立；动态 Montage 还必须在播放前设置保持末帧，不能在实例初始化后再改。

## 验证记录

本轮所有产物及机器证据位于 Saved/Presentation。该目录不纳入 Git。
构建、Automation、PIE 骨骼采样、可见窗口截图及 Cook 分开记录。
最终结果以本轮最后一次验证为准；中间失败采样保留在执行日志，不作为通过证据。

2026-09-14 清理前验证：

|层级|结果|本地证据|
|---|---|---|
|构建|HSREditor Win64 Development 成功|Saved/Presentation/fixes_build.log|
|Automation|52 项通过，0 失败；42 无警告，10 含边界测试警告|Saved/Presentation/FinalFixTests/index.json|
|四人资源|各自 Mesh / Skeleton / FINAL ABP 匹配，全部战斗动作可加载|Saved/Presentation/character_audit.json|
|PIE 四人动作|逐人切换、移动；16 个攻击/技能/受击/倒地片段通过实际 DefaultSlot 播放并截图；四人倒地末帧已检查|Saved/Presentation/motion_clips_pie.json、Captures/motion_*|
|PIE 战斗|巡逻、巡检机和精英多轮战斗完成；最终巡逻战 20 次我方提交，85 次样本有活动 Montage；四个我方槽位均受到过攻击|Saved/Presentation/paced_battle_*.json|
|PIE 巡逻|实际速度 220 cm/s；巡逻→追踪→返回→恢复巡逻→接触进战；胜利返回城区后巡逻敌人数为 0|Saved/Presentation/patrol_flow_pie.json、patrol_return_pie.json|
|PIE 遗器|当前角色与非当前队员装备/卸下后面板刷新；最大生命与当前生命分离。蕾米埃尔最大生命 115→135，卸下115，读档135，切换后的 Pawn ASC 为135|Saved/Presentation/relic_save_pie.json、relic_stats_pie.json；早期强化过程见编辑器执行日志|
|PIE HUD|720p、1080p、1440p 奖励底部横卡截图；完整地图显示宝箱、距离和跨地图任务出口|Saved/Presentation/Captures/reward_*、full_map_shared_markers.png|
|Cook|三张正式地图 Windows Cook 成功，0 错误、0 警告|Saved/Presentation/fixes_cook.log|

验证边界：16 段动作预览验证实际姿势输出，不能替代每人每种技能及真实战败组合的穷举。战斗结算/一次推进、目标权重、装备强化及生命夹紧另由 Automation 覆盖。此处 Cook 为清理前结果；确认清理后必须重新验证，不能沿用该结果宣称清理通过。

修复后的规则：普通敌人使用独立可复现随机流；精英权重为 `1 + 2 × (1 − HP比例)`，上一目标乘 `0.5`。战斗行动有起手、命中和收尾，结束收尾后才推进或弹结果。奖励按凭证排队，完成领取后由奖励权威状态同步移除宝箱标记。

## 动画来源

使用本地已有 /Game/Assets/Mannequins/Anims 原始移动、近战攻击和受击动画重定向。
四名目标模型来自现有 /Game/Assets/mmd/Character。没有重新下载或替换用户模型。
完整倒地、远程施法和治疗起手来自 Quaternius CC0 标准动作库；来源、固定下载版本和转换步骤见 [动作来源](../ArtSource/Presentation/Quaternius/README.md)。蕾米埃尔和维琳娜使用施法动作，维琳娜治疗使用抬手施法；火花和长夜月保留近战。
生成时的逐动作源路径记录在 Saved/Presentation/combat_animation_sources.json。
原资源的许可证/再分发证据需要在发布审核时核对；本地运行验证不等于公开再分发授权。

## 清理判定

联合检查正式地图递归依赖、代码与配置软路径、动态加载前缀、动画生成依赖、测试专用依赖和 Cook 目录。
清单明确 Git 跟踪、忽略、未跟踪及本地备份保护范围。无直接引用只是线索，不自动触发删除。
打包配置保留动态使用的 Meshes/Materials/UI 目录，动画通过角色定义和 ABP 的真实引用收集，避免把失效的旧 PLAY 迁移资产强制 Cook。旧文件尚未删除。
