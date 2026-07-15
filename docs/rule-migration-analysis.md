# HSR 规则迁移分析

## 目的

本文件记录 HSR 从 ChatBot SNS 与 Blaster 两份 `agents.md` 中吸收的可迁移经验。HSR 是全新的 UE5.6 C++ GAS 单机 JRPG 学习项目，不继承两个参考项目的业务目标或业务代码。

## ChatBot SNS 规则来源

### 主要内容

- Python 聊天机器人、LLM、记忆、语音、平台适配和插件业务。
- 实时工作日志、任务计划、阶段总结、测试、README、需求变更、实现决策和问题复盘规范。

### 迁移到 HSR

- 持续维护 `worklog.md` 和 `todo_plan.md`。
- 记录需求变更、设计决策、实现依据、踩坑和解决方案。
- 每个阶段结束时统一检查测试、文档、README 和复盘材料。
- 以可验证结果作为任务完成条件。
- 使用模块化、事件驱动和阶段化开发思想。

### 明确排除

- QQ、WeChat、LLM API、ChromaDB、GPT-SoVITS 和 Whisper。
- ChatBot 人格、记忆、语音和机器人插件业务。
- Python、asyncio、pytest、YAML 机器人配置和 Docker 部署流程。

### 改写后迁移

- `pytest` 改为 UBT/UHT 编译、UE 自动化测试、PIE 和编辑器验证。
- Python 配置检查改为模块依赖、项目设置、Gameplay Tags、输入资产和数据资产检查。
- Python 插件思路改为 UE Module、Subsystem、ActorComponent、Interface、Delegate 和 GAS 事件；不为插件化而过早拆分。
- 实时日志服从用户当前回合的 Plan、只读或禁止写入要求。
- Git commit/push 不自动执行，必须先获得用户许可。

### 结论

保留文档协作体系、阶段复盘方法和双向决策记录；完全排除机器人业务和 Python 技术实现。

## Blaster 规则来源

### 主要内容

- UE5.6 C++ 多人射击工程背景、框架和目录结构。
- C++/蓝图边界、反射、GC、Tick、网络复制和框架类职责。
- 功能实现、Debug、Code Review、Skills 和学习复盘结构。

### 迁移到 HSR

- UE5.6 与 C++ 优先；蓝图用于配置、派生、动画、资源绑定和 UI 表现。
- 修改前说明文件与职责，采用小而明确、可编译、可验证的增量。
- 检查 UE 反射、UObject 引用、GC、委托和异步生命周期。
- 默认避免 Tick，优先事件、委托和 Timer。
- 遵守 UE 命名、前向声明、`nullptr`、UE 容器和 `UE_LOG` 规则。
- 明确 GameMode、GameState、PlayerController、PlayerState、Subsystem 与 SaveGame 的职责。
- 保留默认回答、Debug、Code Review、Skills 和学习复盘结构。
- 网络清单仅作为未来联机扩展门禁。

### 明确排除

- 武器拾取、射击、弹药、命中扫描和射击同步。
- SteamSockets、Online Subsystem、MultiplayerSessions。
- Lobby、SeamlessTravel、多人比赛和 Lag Compensation。
- Blaster 的类名、地图名、插件和资源结构。

### 改写后迁移

- 多人射击职责改为单机 JRPG 探索和回合制战斗职责。
- 武器与复制规则改为 GAS 属性、效果、技能和回合状态规则。
- 网络复制从当前必做项降为未来审查项。
- 不因未来联机而过早使用 PlayerState、GameState 或复制框架。
- 学习复盘主题改为 GAS、回合制、RPG 数值和数据驱动设计。

### 结论

保留 UE5.6 工程规范、C++ 边界、反射/GC/Tick 检查、调试结构和学习方法；完全排除多人射击业务。

## 冲突裁决顺序

1. 用户当前回合的明确要求。
2. 文件安全、版权、授权和不可逆操作限制。
3. 当前 HSR 仓库、UE5.6 编译结果和实际 API。
4. HSR 自身 `.agents/agents.md`。
5. 当前阶段设计文档和已记录决策。
6. ChatBot、Blaster 等参考项目的可迁移经验。
7. 课程、示例和历史版本代码。

ChatBot 的业务目标和 Blaster 的多人射击目标不得通过“参考实现”进入 HSR。
