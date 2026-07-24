# UI Guidelines

## 职责边界

- C++ 提供稳定的数据接口、事件和行为入口。
- UMG/蓝图负责布局、样式、动画和资源绑定。
- Widget 不直接决定伤害、胜负、奖励或存档真源。

## 更新策略

- HP、当前行动者和战斗状态通过 Delegate/事件更新。
- 默认禁止 Widget Tick 轮询。
- Widget 创建、绑定、解绑和销毁路径必须成对设计。
- UI 被关闭或对象销毁后，不得继续接收失效 UObject 回调。

## 第一阶段 UI

- 玩家 HP 与敌人 HP。
- 当前行动者或当前阶段提示。
- 一个基础攻击按钮。
- 胜利、失败和固定奖励占位提示。

第一阶段不制作正式风格、复杂动画、队伍列表、完整技能菜单或手柄导航。

## Phase 10 closeout boundary

- Battle ViewModel/Widget continue to consume pure-value state and emit stable command or result-confirm intents only; Coordinator/GameMode/Transition retain gameplay and travel authority.
- Result focus may use a dedicated button or the focusable Widget fallback. Keyboard/gamepad handler support is not proof that a physical device was tested.
- P10-005 is archived `PASS WITH FOLLOW-UP`; Phase 10 is `Ready with inherited follow-ups`, without starting Phase 11. Resolution, real-device navigation, and unrun lifecycle matrices retain their actual evidence labels: 1920x1080 is user-observed, while 1280x720 is user-accepted but not verified.

## 可访问性与文本

- 玩家可见文本使用 `FText`。
- 重要状态不只依靠颜色表达。
- 占位 UI 也要保证基本对比度和可读性。
- 在正式展示前补充键鼠/手柄焦点、分辨率和本地化验证。
