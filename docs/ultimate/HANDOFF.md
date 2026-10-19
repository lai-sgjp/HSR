# HSR 终结技工程交付

日期：2026-10-19（Asia/Shanghai）。最终状态：**ENGINEERING READY / HUMAN ACCEPTANCE PENDING**。本轮必需工程检查已完成；人工最终试玩尚未执行。

## 实际变更与职责

沿用唯一 `UHSRBattleCoordinator` 和 `UHSRTurnManager`：个人 Energy 型终结技可在非自身回合提交；不可中断的普通/敌方动作完整结算后，按接受序号 FIFO 执行。普通选择与已完成动作的收尾分别恢复，终结技不产生普通回合 AV 重置或 Buff 计数。目标、资源、能力、角色、战斗代次均复验，重复输入只有一个角色请求槽与一次 Cost。

真实技能覆盖既有长夜月/火花单体攻击、蕾米埃尔全敌攻击、维琳娜全队治疗。既有 ASC/Ability/GE/AttributeSet 完成消耗、伤害和治疗；Cost 失败零效果，全部 Effect 失败一次退款，已经部分成功的 AOE 保留已结算效果与一次消耗并记录明确警告。HUD 读取权威快照，目标确认保存完整 ID 集合，死亡不偷偷换目标。实际角色动画、镜头和金/绿光效走现有表现链。

## Baseline / Git

- 起点分支：`codex/character-preview`。
- 起点 HEAD：`cf73c5123cbbb0fbae73dfa8c4e370de93a339a1`。
- 远端：`origin https://github.com/lai-sgjp/HSR.git`。
- 工作分支：`codex/ultimate-arbitration`。
- 开始已有 8 个 VRM4U Editor DLL 修改；二进制 diff、状态与 SHA-256 存于 `Saved/Ultimate/Baseline/`。它们不暂存、不提交，不重置；打包期间复核未变化。
- 现场引擎：`E:/programs/Epic Games/UE_5.6`，实际 5.6.1 / CL44394996；单 HSR Runtime 模块。
- 现场存档 Schema 9；未改存档、养成、装备 Effect Bridge、背包或任务权威。
- 远端 Schannel 只读检查失败，使用单次 `git -c http.sslBackend=openssl` 重试成功；未修改配置或凭据。
- 提交与最终远端 SHA：由提交后生成的 `Saved/Ultimate/git-delivery.json` 保存（local_sha / remote_sha 必须一致），并同步到下方 Notion 发布记录；避免在提交内部写入自引用 SHA。若该报告为 Remote Pending，则不得声称已远端交付。

## 实际子 Agent 与排他所有权

| Owner | 实际工作 | 文件/资产归属 |
|---|---|---|
| Root | 核心调度、状态与生命周期、最终集成、构建、图形输入、Cook、Git | `Battle/HSRBattleCoordinator*`、TurnManager、Participant、Stage/Motion、TargetingPolicy、GameMode 练习入口、AbilityTypes、Energy GE、Config Tags |
| A mechanics_audit，随后 D | 原作资料/源码只读审计，再独立测试与最终只读 Review | REFERENCE-AUDIT、REVIEW、新 UltimateArbitrationTests；明确授权的 Casualty / Patch / DemoCatalog 测试 |
| B gas_energy | GAS Cost/退款、门槛、团队治疗与多敌 prepared GE、一致性修复 | UltimateAbility、GameplayAbilityBase、SkillDefinition、Verina author 脚本、INTERVIEW-DEEP-DIVE |
| C ui_presentation | 能量/就绪/排队卡、角色关联、群体目标、UI 合同与镜头冲突修复 | CommandTypes、Widget、ViewModel、新 UltimateUITests、SOURCE-WALKTHROUGH |

本轮受并发槽位限制，A 完成研究后转 D；未同时让四名子 Agent 争用文件。接口先冻结，所有二进制资产写入、UE 构建/运行由 Root 控制。没有 Cursor 或外部编程 Agent。子 Agent 的静态检查没有被当作集成 PASS。

## 关键文件及必要性

- `Source/HSR/Battle/HSRBattleCoordinatorUltimate.cpp`、`HSRUltimateTypes.h`：值请求、确定序号、安全点、复验/取消、普通流程恢复与代次隔离。
- `HSRBattleCoordinator.cpp`、`Presentation.cpp`：继续复用现有正式伤害、目标、GAS、SP、状态/击破流程；仅把终结技从普通行动收尾分离。多目标独立 Context，逐目标死亡登记后只发布一个战斗结果。
- `HSRTurnManager.*`、`HSRBattleParticipant.cpp`：失效当前角色的无行动跳过、销毁中 Actor 统一存活判定；原 10000/Speed 与速度/推进延迟入口不变。
- `GAS/Ability/HSRUltimateAbility.*`、`HSRGameplayAbilityBase.*`：同步原子行为、一次 Commit、Instant GE 成功标记、逐目标一次 prepared Effect、Health 真实前后差值展示。
- `GAS/HSRCombatEnergyEffect.*`：动作/受击/击杀统一 Instant GE 回能，上限由原 AttributeSet 约束。
- `Data/HSRSkillDefinition.h`、`HSRAbilityTypes.h`：独立释放门槛、治疗能力谓词、AllAllies / AllEnemies。
- `UI/HSRBattleCommand*`：每队员独立终结技意图与权威状态；普通输入继续 Reject，终结技不被全局普通锁吞掉。
- `Battle/HSRBattleStage.*`、`HSRCombatMotionComponent.*`：真实角色演出、镜头和 Engine 资源光效；只有 Coordinator 开始执行时切动作镜头，排队不抢镜头。
- 原地资产：`Content/Data/VerticalSlice/Skills/DA_Skill_Verina_BreathingGarden.uasset`、`DA_Skill_Remiel_SkyJudgment.uasset`。没有增加第二套角色或复制平行技能页，没有导入外部商业资源。
- `Tests/HSRUltimateArbitrationTests.cpp`、`HSRUltimateUITests.cpp`：真实 UWorld / ASC / GE 专项与 UI 意图测试；受影响旧测试仅修正已核实的前置条件/目录漂移。
- `tools/Ultimate/`：原地资产脚本、PID 校验的真实 Windows 输入/截图工具、开发试玩启动入口。

## 构建、自动化与失败记录

- Editor：`editor-build-10.log`、`editor-build-11.log` ExitCode 0；UHT 通过。11 轮编号包含首轮工具边界失败与一次 C++ 修复；11 仅重新编译内容目录测试，详见 TEST-MATRIX；不把旧日志冒充最终成功。
- 战斗：`Automation03/index.json`，45/45，41 Success + 4 SuccessWithWarnings，0 Failed / 0 NotRun；包含 20 个新专项。
- 压力：`automation-03.log` 的 UltimateStress Summary，ActualRounds=20、Successes=20、Failures=0；每场两名角色各一次 Cost，多次重复输入，交错敌人动作、受击/击杀能量和重建。
- UI：`UIAutomation01/index.json`，81/81，71 Success + 10 SuccessWithWarnings，0 Failed / 0 NotRun；包含四项新终结技 UI 合同测试。
- DemoCatalog：`DemoAutomation02/index.json` 4/4 Success，Exit0。首轮失败保留 `demo-automation-01.log`；只修测试中与现场资源不符的 Remiel 回能、旧地图路径和宝箱内容预期。以上三个最终报告共 130/130，零 Failed / NotRun。
- Windows Game / Cook / Stage / Archive：最终 `package-02.log` UAT Exit0。首包 `package-01.log` UAT 也为0，但实际启动发现字符串加载的原有 Items/Drops/Rewards 资源未 Cook；补三处目录后重包，最终启动 ProductionDefinitionBootstrap=READY，零 Error 日志。没有修改存档/奖励/装备实现。
- 已知工具链警告：MSVC14.51 非 UE5.6 首选版本；部分 Engine AIModule 使用废弃 CleanupWorld API。负例测试警告保留，未关闭断言或吞掉错误。

## 真实图形证据

本轮通过 UE 图形 Game 模式、1280×720 真实 Windows 鼠标/键盘运行了两场生产角色战斗，非 Mock Manager / 纯日志假演出。`Saved/Ultimate/Runtime/runtime-verification.json` 对照 `practice-02.log` 断言 6 个唯一 Accepted 请求、6 个 Completed、各一次 Cost 和成功 Effect，敌方命中先于排队终结技，普通流程恢复正确。

- `Runtime/ready-four-party.png`：四名实际角色、能量和就绪卡。
- `Runtime/own-turn-ultimate.png`：真实火花动作、光效和执行卡。
- `Runtime/two-ultimates-queued.png`：等待安全点序号；`team-heal-motion.png`：实际治疗演出。
- `Runtime/ordinary-selection-restored.png`：全队 HP 更新、原普通行动者仍有行动权。
- `Runtime/enemy-action-ultimate-request.png`、`aoe-ultimate-motion.png`：敌方期间提交与后续释放。
- `Runtime/normal-attack-two-ultimates.gif`：普通攻击后两人 FIFO、最后正常收尾，含实际每帧时间。
- `Runtime/battle-finish-and-reentry.gif`：结束、确认、实际新 World 重进；没有继承旧队列。
- `Runtime/battle-input-and-ultimate-chain.gif`：完整第一条链。此首条 GIF 使用固定帧时长，实际捕获约16秒，GIF播放更快；它仍是真实连续采集帧。随后两条记录器已改为按采集时刻保存帧时长。
- 同名 `*-inputs.json` 保存 PID、窗口、鼠标/键盘事件时刻；没有把 Blueprint 方法调用冒充物理输入。

最终用户验收尚未执行；没有声称原作电影级美术、所有角色例外、多分辨率或 Shipping 公开发布验收。最终独立包另完成两次终结技和恢复普攻，见文末。

## 启动与验收

双击 `tools/Ultimate/Play-Ultimate-Practice.cmd`，按 HUMAN-ACCEPTANCE.md 的七步试玩。普通玩家路线仍使用现有探索/挑战入口；开发练习只在显式参数下初始充能。结算确认开启新一场练习，不发持久奖励。

## 已知边界

未实现特殊非 Energy 资源、特殊角色增强模式、通用追加攻击/反击/敌方阶段行动引擎。当前工程原先没有这些机制；本轮保留扩展点，不将社区描述冒充官方架构。全目标在接受时固定；失效取消，用户不能撤回已接受请求。效果已经部分结算时保留一次 Cost，日志显示 PartialSettlement；不伪造全局回滚。普攻/战技执行中不自动排队。

Notion：用户原技术契约链接无效，已通过搜索读取正确的“四项目简历追问题库” HSR 部分。可对应同步位置为该页下的 HSR 专属交付子页，未修改 MyZZZ。已同步并 fetch 读回状态、130/130 和试玩入口：[HSR｜终结技工程交付与待验收](https://app.notion.com/p/3fea67dbf15281069199d0d946def6c7)。Git SHA 在实际 Push 读回后再更新发布记录。

## 最终证据补充

- 最终独立包：`Saved/Ultimate/Package/Windows/HSR.exe`，Game / Cook / Archive Exit0。
- `packaged-verification.json` 对照 `packaged-practice-final.log`：两名角色 Accepted=2 / Completed=2，每次 CostCommitted=1、EffectResolved=1；重复点击没有第二次消耗/伤害；FIFO 后恢复原火花普攻（10 HP）并正常进入维琳娜普通回合。最终包正常关闭 Exit0。
- `Runtime/packaged-ultimate-and-normal-mouse.gif`：8.08 秒真实连续鼠标操作、实际逐帧时长；同名 inputs.json 记录物理事件。`packaged-final-ultimate-motion.png`、`packaged-final-fifo.png`、`packaged-final-normal-input-restored.png` 可直接查看。总图形证据为 Editor Game 两场6次 + 独立包1场2次，合计8次终结技。
- 首次进入时先点击技能卡使键盘焦点进入战斗 HUD；未聚焦直接按数字会被已有探索 Controller 拒绝。完整全鼠标路线已实测，后续键盘操作见 HUMAN-ACCEPTANCE。首个仅键盘采集不作为终结技成功证据，保留在 Runtime 中。
- 新资源仅是已有 Engine Cylinder / Native Light，已在最终包显示；没有新增受保护外部资产。角色模型沿用项目既有资源，不代表获得新的公开分发授权。
- 最终 8 个继承 DLL SHA256 全部匹配 Baseline，不纳入暂存。Git 交付机器报告和 Notion 发布 SHA 在提交/推送后生成，工程源码不再更改。
- 运行警告边界：现有隐藏 CharacterDetail 初始化 SelectionResult=6 警告保留，不影响本轮战斗 HUD；初次 D3D12 PSO 生成可短暂停顿。未宣称所有 UI/性能或 Shipping 验收通过。
