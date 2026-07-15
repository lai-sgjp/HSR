# HSR 面试复盘记录

## 通用回答框架

1. 先说需求和成功标准；
2. 说明 UE 类职责和数据所有权；
3. 解释 C++、Blueprint、GAS、DataAsset、Runtime、SaveGame 边界；
4. 给出事件流、失败路径和生命周期；
5. 说明如何编译、PIE、自动化或 Debug 验证；
6. 说明当前单机限制和未来联机风险。

## 计划中的问题

- 为什么第一阶段单模块起步，何时拆 Runtime/UI/Developer/Editor？
- 为什么当前 ASC 放 Character/BattleCharacter，而不是为了未来联机提前放 PlayerState？
- Owner Actor 和 Avatar Actor 有什么区别？
- 为什么回合持续 N 回合不能直接用 GameplayEffect 的秒数 Duration？
- 为什么 TurnSystem 不应塞进一个 GameplayAbility？
- 为什么 UI 只能提交 Command，不能直接 Apply GameplayEffect？
- 为什么跨 Battle Map 只能保存稳定 ID 和 Context，不能保存 Actor 指针？
- 如何保证奖励事务幂等，避免任务、战斗和宝箱重复发奖？
- 如何在低级模型和 Loop Engineering 中控制范围漂移？

## 复盘要求

每个答案都要补充一个真实项目证据、一个踩坑或失败路径，以及一个可以继续改进的限制。没有实际验证的内容必须标记为设计假设。
