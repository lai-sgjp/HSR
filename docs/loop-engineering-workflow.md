# HSR Loop Engineering 协作流程

## 定位

Loop Engineering 是一种可选的 AI 协作方式：让不同职责的审查角色形成“规划 → 审查 → 架构 → 实现 → 复盘”的闭环。它服务于复杂任务，不是每个小任务都必须模拟的固定仪式，也不改变用户当前回合的明确要求。

## 文件流是闭环载体

CC-SWITCH 会让不同 API 源和模型之间的聊天上下文断开，因此 Loop Engineering 不把对话历史当作任务记忆。每个角色通过仓库文件交接：

`PROJECT_STATE.md → tasks/active-task.md → 实际改动与验证证据 → tasks/review-template.md → tasks/archive/ → PROJECT_STATE.md`

- 高级模型从 `PROJECT_STATE.md`、agents、todo、worklog 和 Phase 文档恢复全局状态，并生成自包含活动任务卡。
- 低级模型只把 `tasks/active-task.md` 当作上下文入口，不依赖聊天中的隐含决定。
- 用户通过日志、截图、PIE 现象和 Editor 操作结果补充机器无法取得的证据。
- 审查模型对照任务卡和实际证据审查，不依赖执行模型的口头总结。
- 高级模型在审查后更新快照、日志与进度，并归档任务卡；下一模型可从文件恢复，而无需重放完整对话。

聊天可以用来讨论，但所有会影响范围、授权、设计、验收或下一步的决定都必须写入对应文件后才算可传递上下文。

## 角色分工

### Prompt Planner

- 理解用户目标和成功标准；
- 识别不明确的需求和授权边界；
- 将粗略请求拆成一个可执行的小 Prompt；
- 列出输入、输出、允许文件、禁止事项和验收证据；
- 保证低级模型可以理解，不依赖隐含上下文。

### Prompt Reviewer

- 检查是否有越权、误删、批量修改或跨 Phase 风险；
- 检查高级模型是否读取了必要的 agents、todo、worklog 和专项文档，并已把必要上下文写入活动任务卡；
- 检查 Prompt 是否缺少测试、Editor 操作、文档更新和失败路径；
- 不通过时退回 Planner 修改，不让执行模型猜范围。

### Architect

- 在通过的 Prompt 上定义 UE 类职责、C++/Blueprint 边界、数据流和生命周期；
- 规划 GAS、DataAsset、Runtime、SaveGame 和 UI 的所有权；
- 说明需要新增的最少文件和未来扩展点；
- 不因为未来联机或插件化而提前增加复杂架构。

### Implementation Agent

- 只执行通过审查的最小任务；
- 不做无关重构、不跨越 Phase；任务完成后按角色交付规则提交自己的执行产物；
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

### Teacher / 教师

- 仅由高级模型在用户明确指定时承担，负责教学、工程复盘、源码机制理解、练习题和面试沉淀；
- 依据学习记录、工作日志、真实 diff 与验证证据教学，不伪造已掌握或已验证的事实；
- 不执行低级实现任务，不在教学过程中顺手改 C++；需要修改文档时先列出文件和目的；
- 与 Learning Reviewer 的边界：Teacher 负责教学，Learning Reviewer 检查学习记录的完整性。

### Safety Reviewer

- 检查删除、覆盖、批量操作、第三方资源授权和 Git 风险；
- 检查是否会提交 `Intermediate`、`Binaries`、`Saved` 或大型未授权资产；
- 高风险操作必须停下请求用户确认；
- 不得以“清理缓存”代替根因诊断。

## No Unauthorized Role Switching / 禁止未经授权的角色切换

角色是任务边界的一部分。所有模型只能承担当前 Prompt 明确指定的角色；低级模型只能担任 Implementation Agent，不能自行转为 Planner、Reviewer、Architect、Teacher 或高级模型。模型不得以安全、自我反思或提高完成质量为由扩权、重写任务、扩大文件范围、改变 Phase 或验收标准。

如需角色切换，必须由用户明确授权，或由 `tasks/active-task.md` 明确授权；切换时必须显式说明触发依据和授权来源。发现风险、歧义或需要其他角色时，正确行为是停止并请求授权，未经授权的切换视为任务失败。

## 复杂任务闭环

1. 用户目标进入 Prompt Planner，高级模型先从项目文件恢复状态；
2. Planner 将目标写入 `tasks/active-task.md`，而不是只留在聊天中；
3. Prompt Reviewer 检查范围、授权和验收；
4. Architect 给出最小设计并写回任务卡；
5. Code Reviewer/Safety Reviewer 进行实现前门禁；
6. Implementation Agent 只按活动任务卡执行；
7. 用户在 UE Editor 中完成手动配置并回传证据；
8. Code Reviewer 使用审查模板检查变更和验证证据；
9. Learning Reviewer 归档知识，worklog 记录证据，todo 只记录真实进度；
10. 高级模型更新 `PROJECT_STATE.md`，归档任务卡，通过后才推荐一个相邻下一任务。

## 小任务简化流程

纯文档或 1–5 个文件的小型功能可以由主 Codex 直接执行，但仍必须保留：目标、文件清单、禁止范围、测试、Editor 手动步骤和真实状态汇报。

## 失败处理

- Prompt 不清楚：退回规划，不让实现模型猜。
- 前置 Phase 未通过：停在门禁处，不跳阶段。
- 编译或 UHT 失败：优先定位第一处真实错误。
- Editor 资产缺失：输出用户手动操作清单，不假设资产已存在。
- API 不确定：检查实际头文件/文档或请求错误信息，不编造。
- 需要修改范围外文件：停止并重新取得授权。

## 角色提交与 Phase 推送

每个角色完成本轮职责后，必须先检查工作树和任务允许范围，再提交自己的产物。commit message 固定为：`角色+人格+当前任务与阶段+简短告诉自己做了什么`。角色没有实际文件变化时，先写入可审查的结果文件，禁止创建无内容的空提交。

一个 Phase 的全部子任务必须完成、审查、归档且角色提交齐全后，由高级协调者创建 Phase 收尾 commit 并推送远端。中途不推送，不强制推送，不提交派生产物；失败时记录第一处真实错误并停止。
