# Debug Tools

## 第一阶段目标

先提供可验证的日志和状态快照，不立即开发完整 GM 控制台。

## 日志类别规划

- Framework
- Exploration
- Encounter
- Battle
- TurnSystem
- AbilitySystem
- UI
- Data
- Save

创建代码时应使用项目自定义 Log Category；日志至少包含战斗实例、行动者、目标、Ability/Tag、状态迁移和失败原因。

## 最小调试输出

- 当前战斗状态。
- 当前行动者和有效参与者列表。
- 行动顺序及 Speed。
- Ability 激活成功或失败原因。
- Effect 来源、目标和 HP 前后值。
- 胜负结果与奖励是否已经发放。

## Debug 排查顺序

1. 记录现象和预期。
2. 找到第一处有效错误或第一次错误状态迁移。
3. 检查对象生命周期和空引用。
4. 检查 ASC Actor Info、Ability 授予、Tag 和 Effect。
5. 检查 TurnSystem 是否仍处于允许行动的状态。
6. 检查 UI 是否只是显示问题。
7. 采用最小修复并重跑同一复现步骤。
8. 将根因和验证证据写入 worklog/learning journal。

删除缓存、重建工程文件或批量清理前必须说明影响并获得用户许可。
