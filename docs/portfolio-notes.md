# HSR 作品集记录

## 当前叙事

HSR 是 UE5.6 C++、GAS 和回合制架构学习项目。它以原创单机 JRPG 为目标，参考系统类别而不复制角色、剧情、UI、数值、模型、音乐或商标。

## 应展示的工程能力

- 从 Blank C++ 建立工程和第三人称探索控制；
- 用独立 Battle Map 和稳定 ID Context 管理战斗切换；
- 用 ASC、AttributeSet、GameplayAbility、GameplayEffect 和 Delegate 建立事件驱动数据流；
- 将 GAS 与 TurnSystem、DataAsset、Runtime、SaveGame、UI 分层；
- 用 Debug Snapshot、Damage Breakdown 和自动化测试呈现可解释证据；
- 说明哪些工作由 Codex 完成、哪些由用户在 Editor 手动完成；
- 记录真实 Bug、设计取舍、性能边界和未完成项。

## 每个阶段结束应归档

- 一张职责或数据流图；
- 一段可复现演示路径；
- 编译/PIE/自动化证据；
- 一个失败路径或 Debug 结论；
- 学习 journal 条目；
- 第三方资产授权和署名记录（如有）。

## 展示禁忌

- 不暗示与商业作品存在官方关联；
- 不公开未授权模型、贴图、音频、提取资源或字体；
- 不用“完成”描述只规划、只编译或未验证的内容；
- 不以大量功能数量替代清晰的职责、数据所有权和验证证据。
- 不把 AI 生成、规划完成或未验证内容包装为个人已经掌握的工程成果。

## AI 协作与学习证据

- 使用 Role Lock 约束低级模型只执行任务卡中的精确范围；规划、审查与教学仅在用户明确授权时由高级模型承担。
- 将真实任务决策、diff、编译/PIE/Debug 结果沉淀到 worklog 与 learning journal，明确区分已验证、待复习和未验证。
- 用 Teacher 将 UE5.6 C++、GAS 与工程实践转化为可复述的机制理解、练习和面试表达，而不是把 AI 代写当成学习完成。

## Phase 1 学习与 Debug 证据

- 从 Blank C++ 工程完成 Character/Camera、Controller/Possession、Enhanced Input、GameMode/HUD 与 AnimBP 的探索闭环。
- 真实定位过“原始按键存在但 Enhanced Action 全部沉默”的故障：沿 Spawn/Possess、LocalPlayer、Context、Binding 到 PlayerController 每帧输入链收集证据，最终恢复被关闭的 Controller Tick。
- 通过 UIOnly 往返和同会话 Re-Possess 验证 Context、Binding、InputComponent 与 HUD 不重复；避免以 Character Tick 轮询或手工 PushInputComponent 掩盖生命周期问题。
- Teacher Gate 中用户能复述 Gameplay 真源、职责边界和幂等序列；反射/GC 核心达到掌握，序列化细节和 AnimBP 内部节点列为后续深化项。
