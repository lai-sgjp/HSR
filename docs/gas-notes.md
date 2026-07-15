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
