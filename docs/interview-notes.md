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

## AI-assisted UE Project Learning Interview Notes

### 角色分工与越权控制

项目通过活动任务卡把规划、实现、审查和教学分开：低级模型只作为 Implementation Agent 执行精确范围；高级模型按用户授权承担规划、架构、审查或 Teacher；审查基于实际改动和证据而非执行模型自述。未列入允许清单的文件默认禁止修改，出现歧义、风险或高阶需求时必须停止并请求授权。

### 用学习记录证明理解过程

`learning-journal.md` 记录概念、HSR 映射、常见误区、验证思路、练习与面试题；`worklog.md` 保留真实决策、diff 和编译/PIE/Debug 证据。面试表达应区分已掌握、需要复习、尚未验证和后续深入，避免将 AI 生成内容伪装为亲自验证的结论。

### 将工程实践转化为面试表达

采用“需求与成功标准 → UE/GAS 职责与数据所有权 → 数据流/生命周期 → 真实验证与失败路径 → 当前限制”的顺序。例如解释 GAS 时，不只说会调用 Ability，而是说明 ASC Owner/Avatar、AttributeSet、GameplayEffect、GameplayTag 与 TurnSystem 的边界及实际证据。

### 示例问题

- 你如何保证 AI 生成 UE C++ 代码的可靠性？
- 你如何审查 UE C++ 代码？
- 你如何通过文档约束低级模型不越权？
- 你如何学习 GAS 的底层机制？
- 你如何避免自己只会调用 AI 而不理解代码？

回答这些问题时，必须引用实际任务卡、真实验证或明确标记的未验证项；不能用泛泛的“AI 已检查”代替证据。

## Phase 1 可复述案例

### 为什么 Enhanced Input 资产和绑定都正确，Action 回调仍可能完全沉默？

HSR 的真实故障中，原始 `InputKey` 能收到 W，Pawn、Possession、LocalPlayer、IMC、IA 和 Binding 均正确，但 `PostProcessInput` 不运行。根因是关闭了 PlayerController Tick，切断了引擎每帧 Player Input 处理链。最小修复是恢复 Controller Tick；不应给 Character 增加轮询 Tick，也不应手工 `PushInputComponent`，后者可能在 Re-Possess 后造成重复入栈。

### `Transient` 与资产默认值如何区分？

设计期稳定配置和资源引用适合进入资产/Blueprint CDO；当前控制模式、Context 是否已添加、临时 Widget 实例等运行态适合 `Transient`。保存资产会影响下次加载和新实例默认，而玩家跨会话进度应走独立 SaveGame 数据流。该区分避免 PIE 临时状态污染设计默认值。
