# Phase 1：工程基线与灰盒战斗闭环

## 阶段目标

先验证 UE5.6 C++ 工程和工具链，再逐步完成：

`探索 → 遭遇 → 1v1 回合战斗 → 胜负结算 → 固定奖励 → 返回探索`

## Phase 1A：工程基线

### 范围

- 在已通过 Phase 0 验收的 UE5.6 Blank C++ 项目中，从零建立第三人称探索角色。
- 使用 Visual Studio 2026 生成并构建 Development Editor。
- 记录 UE5.6、UBT 和实际 C++ 标准配置。
- 在 PIE 中验证 Enhanced Input 移动与镜头。

### 不在范围内

- GAS、战斗、存档和第三方资产。
- 无关插件、网络复制和正式 UI。

### 验收

- `.uproject` 可由 UE5.6 打开。
- HSR C++ Runtime Module 成功加载。
- Development Editor 构建无未解释错误。
- PIE 中角色可移动和控制镜头。
- 工具链版本和验证证据已写入 README/worklog。

## Phase 1B：最小 GAS 基线

- 配置必要模块依赖。
- 每个战斗参与者持有 ASC。
- 初始化 Actor Info，创建最小 AttributeSet。
- 使用初始化 GE 设置 HP、Attack、Defense、Speed。
- 使用属性 Delegate 输出或显示 HP 变化。

验收：两个测试参与者能初始化不同属性，Effect 能改变 HP，重复初始化和空目标有明确处理。

## Phase 1C：1v1 回合战斗

- 独立 TurnSystem 管理状态和当前行动者。
- 按 Speed 排序，相同 Speed 使用稳定规则打破平局。
- 玩家通过基础攻击 Ability 选择敌人。
- 敌人使用确定性的基础攻击。
- HP 归零触发胜负，清理等待状态和临时效果。

验收：可连续完成至少三次不同初始属性的战斗，胜负结果可复现。

## Phase 1D：探索到奖励

- 探索地图触发 Encounter，通过稳定 ID Context 进入独立 Battle Map。
- Battle Map 重建参与者和 ASC；结束后返回探索地图与保存的 Transform。
- 胜利发放固定占位奖励；失败不发放。
- 避免同一敌人重复触发并发战斗。

## 测试场景

1. 正常进入战斗并获胜。
2. 玩家失败。
3. 玩家与敌人速度相同。
4. 无效或已死亡目标被拒绝。
5. 战斗中重复触发遭遇被拒绝。
6. Ability 激活失败后 TurnSystem 不被永久锁死。
7. UI 在 Delegate 解绑后不访问已销毁对象。
8. 返回探索后输入恢复、位置正确且不会重复发奖。
9. 跨地图 Context 不包含 Actor、Widget、ASC 或 GE Handle。

## 阶段结束材料

- 构建、PIE 和测试证据。
- 战斗数据流图和类职责说明。
- GAS 学习复盘与至少三道面试题。
- 原创性与第三方资源检查。
- 下一阶段范围、遗留问题和技术债。
