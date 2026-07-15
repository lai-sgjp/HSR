# HSR Codex 与 UE Editor 协作边界

## 目标

HSR 采用“Codex 负责可审查的 C++/文档，用户负责 UE Editor 中需要视觉确认和资产绑定的操作”的学习协作方式。Codex 必须解释手动操作的原因和验证方法，用户必须反馈截图、日志、编译结果或 PIE 现象。

## Codex 适合直接完成

- `.h/.cpp`、Build.cs 和受授权的配置入口修改；
- UE 反射、Component、Subsystem、Interface、GAS 基础类；
- 数据结构、Runtime 事务和 Save DTO；
- 文档、日志、学习复盘和测试计划；
- 日志分析、错误定位和最小修复建议。

## 用户在 Editor 中手动完成

- 创建 Blueprint 派生类、DataAsset、GameplayEffect、InputAction、InputMappingContext 和 UMG Widget；
- 设置 GameMode、默认 Pawn、PlayerController、输入模式和地图；
- 配置 Gameplay Tags、资产引用、动画绑定、材质、音频、VFX 和 Niagara；
- 摆放灰盒地图、PlayerStart、交互物、敌人和 NavMesh；
- 在 PIE 中观察输入、UI、动画、战斗表现和失败路径；
- 确认第三方资产授权、署名和公开展示范围。

## 每轮 Editor 清单格式

Codex 输出：

1. 要打开的资产或 Project Settings 页面；
2. 要创建、拖拽或绑定的对象；
3. 每个配置的目的和依赖；
4. 配错时的典型症状；
5. 保存后如何编译/PIE 验证；
6. 用户需要反馈的截图、日志或错误信息。

## 边界规则

- Blueprint 只负责配置、派生、动画、UI 表现和资源绑定，不承担核心规则真源。
- UI 只读取数据并提交命令，不直接修改 Attribute、Turn、Reward、Quest、Inventory 或 SaveGame。
- GameplayCue、Montage、音频和 VFX 不能决定伤害、回合完成或奖励结算。
- DataAsset 是静态定义；Runtime 是当前会话；SaveGame 是跨会话稳定 DTO。
- 地图切换后不得继续使用旧 World 的 Actor、ASC、Widget 或 Active GE Handle。
- 无法在 Editor 中确认的配置不得被 Codex 声称为已完成。
