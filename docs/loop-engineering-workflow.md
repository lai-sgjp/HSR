# HSR Loop Engineering 协作流程

## 定位

Loop Engineering 是一种可选的 AI 协作方式：让不同职责的审查角色形成“规划 → 审查 → 架构 → 实现 → 复盘”的闭环。它服务于复杂任务，不是每个小任务都必须模拟的固定仪式，也不改变用户当前回合的明确要求。

## 角色分工

### Prompt Planner

- 理解用户目标和成功标准；
- 识别不明确的需求和授权边界；
- 将粗略请求拆成一个可执行的小 Prompt；
- 列出输入、输出、允许文件、禁止事项和验收证据；
- 保证低级模型可以理解，不依赖隐含上下文。

### Prompt Reviewer

- 检查是否有越权、误删、批量修改或跨 Phase 风险；
- 检查是否读取了必要的 agents、todo、worklog 和专项文档；
- 检查 Prompt 是否缺少测试、Editor 操作、文档更新和失败路径；
- 不通过时退回 Planner 修改，不让执行模型猜范围。

### Architect

- 在通过的 Prompt 上定义 UE 类职责、C++/Blueprint 边界、数据流和生命周期；
- 规划 GAS、DataAsset、Runtime、SaveGame 和 UI 的所有权；
- 说明需要新增的最少文件和未来扩展点；
- 不因为未来联机或插件化而提前增加复杂架构。

### Implementation Agent

- 只执行通过审查的最小任务；
- 不做无关重构、不跨越 Phase、不自动提交 Git；
- 遇到未确认的 UE5.6 API、缺失资产或超出授权的文件立即停止并报告；
- 在完成后给出实际修改和未验证内容。

### Code Reviewer

- 检查 UE 反射、GC、Tick、命名、模块依赖和职责；
- 检查 GAS Actor Info、Ability/Effect/Tag、UI 事件和 TurnSystem 边界；
- 检查地图切换是否保存了 Actor 指针或 GAS Handle；
- 以严重度区分阻断问题、风险和可选改进。

### Learning Reviewer

- 确认新概念是否写入 learning journal；
- 用“概念 → HSR 映射 → 数据流 → 坑 → 验证 → 面试题”记录可复习知识；
- 检查实现是否帮助初学者理解，而非只复制代码。

### Safety Reviewer

- 检查删除、覆盖、批量操作、第三方资源授权和 Git 风险；
- 检查是否会提交 `Intermediate`、`Binaries`、`Saved` 或大型未授权资产；
- 高风险操作必须停下请求用户确认；
- 不得以“清理缓存”代替根因诊断。

## 复杂任务闭环

1. 用户目标进入 Prompt Planner；
2. Prompt Reviewer 检查范围和验收；
3. Architect 给出最小设计；
4. Code Reviewer/Safety Reviewer 进行实现前门禁；
5. Implementation Agent 执行；
6. 用户在 UE Editor 中完成手动配置并反馈结果；
7. Code Reviewer 检查变更和验证证据；
8. Learning Reviewer 归档知识；
9. worklog 记录完整证据，todo 只记录真实进度；
10. 通过后才推荐相邻下一任务。

## 小任务简化流程

纯文档或 1–5 个文件的小型功能可以由主 Codex 直接执行，但仍必须保留：目标、文件清单、禁止范围、测试、Editor 手动步骤和真实状态汇报。

## 失败处理

- Prompt 不清楚：退回规划，不让实现模型猜。
- 前置 Phase 未通过：停在门禁处，不跳阶段。
- 编译或 UHT 失败：优先定位第一处真实错误。
- Editor 资产缺失：输出用户手动操作清单，不假设资产已存在。
- API 不确定：检查实际头文件/文档或请求错误信息，不编造。
- 需要修改范围外文件：停止并重新取得授权。
