# GAS Learning Notes

> 完整学习阶段、实践、测试、Debug、面试和作品集要求见 [HSR GAS 学习路线](gas-learning-roadmap.md)。本文件继续记录实际学习过程中的概念、错误和证据，不复制整份路线图。

## Stage 索引

| Stage | 项目 Phase | 主题 |
|---|---|---|
| 0 | Phase 0–1 | 为什么使用 GAS、适用边界 |
| 1 | Phase 2 | ASC、Owner/Avatar、AttributeSet 和 Delegate |
| 2 | Phase 2/5 | Instant GE、伤害、治疗和 Clamp |
| 3 | Phase 5/6 | GameplayAbility、Cost、Target、Resolution |
| 4 | Phase 6/8 | Gameplay Tags |
| 5 | Phase 7 | MMC、ExecutionCalculation 和伤害公式 |
| 7 | Phase 8 | Weakness、Toughness、Break 和行动延后 |
| 6 | Phase 9 | Buff/Debuff 和持续回合 |
| 8 | Phase 10 | UI 与 GAS |
| 9 | Phase 12 | 装备、遗器和属性来源 |
| 10 | 后期 | ASC 迁移、复制模式、预测和服务器权威 |

实际执行顺序为 `0 → 1 → 2 → 3 → 4 → 5 → 7 → 6 → 8 → 9 → 10`。

## 学会标准

每个 Stage 必须能说明概念本质、项目使用位置、所有权、生命周期、数据边界、失败路径、地图旅行影响、UI 观察方式、测试和 Debug 证据。只复制代码或让蓝图运行不算完成。

## HSR 中的职责

- GAS 管理能力、属性、效果和标签。
- TurnSystem 管理行动者、回合阶段、等待输入、行动解析和结算。
- UMG 通过属性 Delegate 和战斗事件更新，不通过 Tick 轮询。

## 初始学习顺序

1. ASC 与 Ability Actor Info。
2. AttributeSet 与初始化 GameplayEffect。
3. 基础攻击 GameplayAbility。
4. 瞬时伤害 GameplayEffect。
5. Attribute Change Delegate。
6. Gameplay Tags 和激活限制。
7. Cost、Cooldown、GameplayCue。
8. ExecutionCalculation。
9. 最后研究复制与预测。

## 第一阶段决策

- 每个战斗参与角色持有 ASC。
- 玩家 ASC 暂不放入 PlayerState。
- 初始属性为 HP、MaxHP、Attack、Defense、Speed。
- 基础攻击第一版无 Cost、无 Cooldown。
- 第一版伤害先用最小、可验证的 Effect/Modifier；公式变复杂后再使用 ExecutionCalculation。
- 持续 N 回合状态由 TurnSystem 的回合边界事件管理，不把 GE 秒数直接当回合数。
- GameplayCue 只负责表现。

## 每次实现记录模板

### 日期与主题

### Owner/Avatar 与初始化时机

### Ability、Effect、Attribute、Tag 数据流

### 编辑器资产和 C++ 连接点

### 失败现象与根因

### 编译、PIE 和测试证据

### 学习与面试要点

## 2026-07-17｜Phase 2 最小 GAS 属性闭环

### 运行职责与数据流

- ASC：保存 Ability Actor Info、应用 GE、维护属性结果并发布 Attribute Change Delegate。
- AttributeSet：定义 Health、MaxHealth、Energy、MaxEnergy、Speed 及其边界；当前结论只覆盖已验证的 Instant/base GE 路径。
- GE：数据化描述属性变更。初始化 GE 只成功应用一次；测试 GE 可按测试需要重复应用。
- Delegate：通知已绑定观察者，不是任务执行管线。ViewModel 绑定 ASC，Widget 消费 ViewModel。

完整链路：`构造期创建 ASC/AttributeSet → 运行期 InitAbilityActorInfo → ASC 应用 GE → AttributeSet 最终值/收敛 → Delegate → ViewModel → Widget`。

### Snapshot、幂等与生命周期

- Snapshot 是观察者创建时主动读取 ASC 当前状态；它不应用 GE、不重置属性，也不替代后续 Delegate。
- 初始化入口重复不等于初始化 GE 重复。Re-Possess 后 Actor Info 可以刷新，但初始化 GE 成功应用计数保持 1，Delegate 绑定不得叠加。
- GE `Override` 是属性 Modifier Op，与 C++ 虚函数 `override` 无关。Add 重复会叠加；Override 重复可能把战斗后的合法状态覆盖回初值，并产生重复事件，因此两者都需要幂等保护。
- Owner 是 ASC 的逻辑拥有者，Avatar 是当前执行/呈现 GAS 行为的实体。本阶段二者都是 Character，但概念不可合并；对象存在也不等于 Actor Info 完整。

### Debug 顺序与证据边界

当 GE 报告成功但 HUD 没变化时，先读取 AttributeSet/ASC 最终真源。若真源已变，再检查 `Delegate → ViewModel → 当前 Widget`；若真源未变，检查 Modifier、目标属性与 Clamp，而不是先改 UI。

P2-001 保持 `USER ACCEPTED` 和历史 Reviewer `REVISE`；P2-002 为 Reviewer `PASS`，其中 Editor/PIE 与部分计数证据来自用户测试确认。Clamp 不外推到未验证的 Duration/Infinite 聚合路径。

### 学习 Gate

用户已达到 Phase 2 最低学习 Gate：掌握基本数据流、snapshot/Delegate、UI 非真源，以及真源已变后的 `Delegate → ViewModel → Widget` 排查链。初始化幂等细节、Add/Override、Owner/Avatar 为非阻断复习项。

### Reviewer 补答闭环

Reviewer 要求进一步确认后，用户通过最小纠正完成以下闭环：Add 型初始化 GE 在 Health 初值 100 时重复成功应用会得到 200；Re-Possess 后初始化 GE 成功应用次数仍必须为 1；Owner 有效、Avatar 为空不代表依赖 Avatar 的运行行为可靠；HUD 全 0 时先读 AttributeSet/ASC 的 Health 等最终真源，真源也为 0 才向上查 Actor Info 与初始化 GE，真源已变则向下查 `Delegate → ViewModel → Widget`。

用户最初曾将 Add 重复结果答为“不变”，并把 HUD 全 0 的第一检查点答为 Actor Info；最终答案已纠正且明确保留这条学习轨迹。Phase 2 强制学习门禁现已满足，但 Phase 2 `Ready` 仍只能由后续 Reviewer 与 Coordinator 流程判定。
