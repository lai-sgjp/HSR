# TASK-P3-002 执行结果（Final - 含 A1/A2/A3/A4）

> 状态：A4 仅完成交付卫生清理；功能与运行 Gate 的证据边界见下文。
> 日期：2026-07-18

## 修订历史

| 轮次 | 范围 | 已闭合 |
|---|---|---|
| 初版 | ViewModel、Widget、HUD、PlayerController | OnUnPossess、实例计数、VM-ID |
| A1 | ViewModel、Widget、HUD | 缓存去重、ForceCurrentSnapshot、合法按钮专项 |
| A2 | ViewModel、Widget、HUD.cpp | 纯幂等 Observe、同 VM no-op、NativeDestruct teardown、TeardownCount |
| A3 | UserWidget.cpp、报告 | 无条件 Destruct 日志；两轮最终目标地图 PIE 证据 |
| A4 | 本报告、临时 helper | UTF-8/控制字符清理、证据边界补齐、删除未跟踪 helper |

## 实际修改文件总表

### 新增

- `Source/HSR/UI/HSRInteractionViewModel.h/.cpp`

### 修改

- `Source/HSR/UI/HSRUserWidget.h/.cpp` — InstanceId、PromptReceiveCount、同 VM no-op、NativeDestruct teardown 与无条件 Destruct 日志。
- `Source/HSR/UI/HSRHUD.h/.cpp` — InteractionViewModel、RefreshInteractionObserver（Observe 到 SetVM 的顺序）。
- `Source/HSR/Player/HSRPlayerController.cpp` — OnPossess 刷新、OnUnPossess Clear。
- `Source/HSR/Interaction/HSRInteractionComponent.h/.cpp` — `GetCurrentPrompt()`。

### 用户资产

- `Content/UI/WBP_ExplorationHUD.uasset` — `OnInteractionPromptChanged` 表现事件。
- `Content/Blueprints/UI/BP_HSRHUD.uasset` — ExplorationWidgetClass 引用修复。
- `Content/UI/WBP_AttributeDebug.uasset` — 测试按钮隐藏。

`f9fc069` 与 `04ddd82` 均由用户本人作为资产作者创作，用户已事后接受这两笔资产修改；该事后授权的流程偏差保留记录。专项测试期间曾临时显示测试按钮，最终已恢复隐藏，最终 Content 无临时按钮 diff。

## 总提交链

| Hash | 来源 | 内容 |
|---|---|---|
| `6d86e1a` | 用户 | WBP_ExplorationHUD 交互提示 |
| `f9fc069` | 用户 | BP_HSRHUD 引用修复 |
| `9fb5deb` | Implementation Agent | 初版 C++ |
| `04ddd82` | 用户 | WBP_AttributeDebug 视觉调整 |

## 专项验证路径与边界

早期 HUD Rebuild 与 Re-Possess 的 Console Execute（CE）命令两次未被处理并产生 Error；该路径随后弃用。专项验证改为用户资产作者临时显示既有合法测试按钮，完成 HUD Rebuild 与 Re-Possess 验证后恢复隐藏；没有新增 Level Blueprint、控制台伪入口或临时业务 API。

Build 证据的真实边界：A3 源码时间为 16:52:14，编译中间产物/模块 DLL 时间为 16:53:13，之后的 Editor 日志加载并运行最终代码，时间链成立。完整 fresh Build 的控制台输出与 UHT 明细未保存；本报告不声称已保存这些明细。

## 最终两轮 PIE 证据

证据来源为最终代码之后的 `HSR.log`，目标地图均为 `Map_Phase1_Exploration`。

**Round 1（16:54）：**

- Widget[10]：首次 `ForceCurrentSnapshot` 后 `totalReceived=1`，随后由 Register/Unregister 事件递增至 8。
- HUD rebuild 后，Widget[10]/VM2 不再出现回调；Widget[10] `NativeDestruct (VM=-1, total=8)`。
- Widget[11] 共接收四次，准确顺序为：VM3 snapshot、第一次 Re-Possess 后 VM4 snapshot、候选离开隐藏、第二次 Re-Possess 后 VM5 snapshot；Widget[11] `NativeDestruct (VM=-1, total=4)`。
- HUD rebuild 与两次 Re-Possess 均由上述合法按钮触发。

**Round 2（16:55）：**

- Widget[12]：无候选初始 snapshot 后 `totalReceived=1`，进入/离开事件递增至 5。
- Widget[12] `NativeDestruct (VM=-1, total=5)`；无跨 PIE 的旧 Widget/ViewModel 回调增长。

两轮最终日志均未见 Error、Ensure 或 GC warning。

## A3 后验证 Gate

- [x] 首次 Show：Widget 恰好一次 `ForceCurrentSnapshot`。
- [x] 同 Widget/同 Component 重复 Refresh：Observe 为纯幂等（无新增接收）。
- [x] Re-Possess：旧 VM teardown，新 VM/Widget 链唯一；GAS Health/Energy/Speed 与 `InitApplyCount=1` 在 Re-Possess 前后保持。
- [x] HUD rebuild：旧 Widget/ViewModel teardown 后零回调增长，新实例恰好一次初始 snapshot。
- [x] NativeDestruct：Widget[10]、Widget[11]、Widget[12] 均有无条件 Destruct 总计日志。
- [x] 两轮 `Map_Phase1_Exploration` PIE：无跨会话残留。
- [x] 用户人工确认：Move、Look、Jump、Interact、UIOnly、视觉 Prompt 与 GAS HUD 正常。
- [x] 最终 Content/Config 无 diff；测试按钮已恢复隐藏。

## 历史与非目标边界

- P3-001 的 follow-up（包括动态命中 `OutOfRange`、完整 Build/UHT 控制台未保存、历史 Git Gate 偏差与同一 Git 身份证据等级）不在本任务改写。
- 本任务不包含多候选排序、正式交互菜单、NPC/宝箱/传送、GAS Tags、SaveGame、Battle、Phase 4、新 Config、自动化测试或 Editor 扩展。
- A4 未修改 Source、Content 或 Config，且未重新运行构建、Editor 或 PIE。
