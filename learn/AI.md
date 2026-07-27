# UE5 AI 系统

> UE5 AI 基础与实践
> 对应项目：AHSREnemyAIController、HSREnemyCharacter

---

## Behavior Tree 三大件

### 整体结构

```
节点树 = 决策逻辑
黑板   = 共享数据
AIController = 执行器
```

Behavior Tree 是一棵决策树。**每帧从根节点开始评估**，根据 Decorator 的条件走相应分支，执行 Task。

### Task（任务）

**"做什么"**——叶子节点，树执行到它时做一个具体的动作。

```cpp
UCLASS()
class UBTTask_FindPatrolPoint : public UBTTaskNode
{
    GENERATED_BODY()

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override
    {
        // 读黑板 → 找随机点 → 写回黑板
        return EBTNodeResult::Succeeded;
    }
};
```

常见 Task：
- `BTTask_MoveTo`（移动到指定位置）
- `BTTask_Wait`（等待）
- `BTTask_PlayAnimation`（播放动画）

### Decorator（装饰器）

**"能不能做"**——附加在节点前的条件判断。

```
Decorator 返回 false → 跳过整个分支
Decorator 返回 true  → 进入分支执行
```

常见 Decorator：
- `Decorator_HasTarget` → 黑板上的目标是否有效
- `Decorator_CompareHealth` → 血量是否大于门限
- `Decorator_Blackboard` → 检测黑板值

Decorator 默认每帧重新评估。当条件从 false 变 true 时，BT 自动中断当前分支，切换到新分支。不需要手写状态转换。

### Service（服务）

**"执行 Task 期间持续做什么"**——附加在节点上，执行期间定期运行。

```cpp
UCLASS()
class UBTS_UpdatePlayerLocation : public UBTTaskNode
{
    GENERATED_BODY()

    virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override
    {
        // 把目标位置持续写入黑板
        OwnerComp.GetBlackboardComponent()->SetValueAsVector(TEXT("TargetLocation"), Target->GetActorLocation());
    }
};
```

常见 Service：
- `Service_UpdatePlayerLocation`（持续更新目标位置）
- `Service_CheckHealth`（持续监控血量）
- `Service_FindNearestEnemy`（持续寻找最近敌人）

### 三者关系

```
Selector（选择器）
  ├── Decorator: HasTarget?（条件）
  │      │
  │      └── Task: Chase（动作）
  │             └── Service: UpdatePlayerLocation（Task 期间持续运行）
  │
  └── Sequence（顺序执行）
         ├── Task: FindPatrolPoint
         ├── Task: MoveTo
         └── Task: Wait
```

Selector：从左到右试，**第一个能执行的**去执行。相当于"或"。
Sequence：按顺序执行，**全部成功才算成功**。相当于"与"。

### Decorator 的执行时机（重要区分）

| 机制 | 什么时候运行 | 干什么 |
|---|---|---|
| **Decorator（None）** | 进入节点时执行一次 | 门卫，过了就不管了 |
| **Decorator（有 Observer Aborts）** | 入口执行一次 + 黑板变化时被动叫醒 | 条件检查 + 黑板监听 |
| **Service** | 节点执行期间按间隔运行 | 执行背景工作（更新黑板数据） |
| **Task** | 到达叶子节点时执行 | 执行具体动作 |

**关键理解：** Decorator 不是一直在跑的。Observer Aborts 不是让 Decorator "一直轮询"——是 Decorator 在黑板注册了一个监听器，Key 变化时黑板通知 Decorator 重新检查。

```
Service_UpdatePlayerLocation（每 0.25 秒运行一次）：
  写操作——把玩家位置写到黑板

Decorator_HasTarget（黑板变化时被叫醒）：
  读操作——检查 TargetActor 是否有值
  有 → 触发中断 → 切换分支
```

### Observer Aborts 详解

**解决的问题：** BT 默认只在进入节点时检查一次。如果进入巡逻后黑板才出现目标，BT 不会知道。Observer Aborts 改变这个行为。

**四个值：**

| 值 | 含义 | 场景 |
|---|---|---|
| `None` | 不观望。只在进入时检查一次 | 一次性检查 |
| `Self` | 条件从 true → false 时中断自己 | 追击中目标丢失 → 中断追击 |
| `LowerPriority` | 条件从 false → true 时中断右侧 | 无目标时出现目标 → 中断巡逻去追击 |
| `Both` | 以上两者都要 | 最常用 |

**执行过程：**

```
巡逻执行中 → 玩家出现 → 黑板 TargetActor 被写入
  → Decorator_HasTarget 的黑板监听器触发
  → 重新检查条件 → 从 false 变 true
  → Observer Aborts = LowerPriority/Both
  → 中断巡逻 → 切换到追击

追击执行中 → 目标丢失 → 黑板 TargetActor 被清空
  → Decorator_HasTarget 的黑板监听器触发
  → 重新检查条件 → 从 true 变 false
  → Observer Aborts = Self/Both
  → 中断追击（自己） → Selector 回退到右侧默认分支
```

**源码（BehaviorTreeComponent.cpp 第 964 行）：**

```cpp
if (AbortMode == EBTFlowAbortMode::Both)
{
    const bool bIsExecutingChildNodes = IsExecutingBranch(&RequestedBy, RequestedBy.GetChildIndex());
    AbortMode = bIsExecutingChildNodes ? EBTFlowAbortMode::Self : EBTFlowAbortMode::LowerPriority;
}

EBTNodeResult::Type ContinueResult = (AbortMode == EBTFlowAbortMode::Self)
    ? EBTNodeResult::Failed  // 中断自己
    : EBTNodeResult::Aborted; // 中断右侧
```

`Both` = 问"现在执行的是我管的分支吗？是 → Self，不是 → LowerPriority"。

---

## 黑板（Blackboard）

### 为什么需要黑板

BT 各节点之间不能直接调用对方。所有共享数据通过 Blackboard 读写。

```
Blackboard = 命名数据存储
    Key: "TargetActor"      → AActor*
    Key: "TargetLocation"   → FVector
    Key: "PatrolOrigin"     → FVector
    Key: "SelfHealthPercent" → float
```

Task 通过 Blackboard 读写数据：

```cpp
// 读
AActor* Target = Cast<AActor*>(Blackboard->GetValueAsObject("TargetActor"));

// 写
Blackboard->SetValueAsVector("TargetLocation", Location);
```

Decorator 绑定了 Key 后自动读取。

### BT 执行模型（与手写状态机的区别）

```
手写状态机：
  Timer 触发 → if 检查条件 → switch 执行状态

Behavior Tree：
  每帧从 Root 开始遍历整棵树
  Decorator 实时评估条件
  条件变化 → 立即中断当前 Task → 切换到新分支
  你不需要写任何状态切换代码
```

### 对照手写的感知回调

当前代码中 `OnTargetPerceptionUpdated` 做了三件事：
① 感知事件响应（"看到玩家了"）
② 决策（"现在该追击"）
③ 执行（"开始移动"）

BT 架构下只需做第 ① 步——写黑板：

```cpp
// 感知到目标 → 写黑板
Blackboard->SetValueAsObject("TargetActor", Actor);

// 目标丢失 → 清黑板
Blackboard->ClearValue("TargetActor");
```

BT 每帧自动评估，看到黑板上有 TargetActor 就进入追击分支。不需要手写任何状态判断。

---

## 对照项目：手写状态机 → Behavior Tree

### 现有 C++ 状态机

```cpp
switch (CurrentState)
{
    case Idle:            break;
    case MovingToPatrol:  MoveToLocation(...); break;
    case PatrolWaiting:   WaitAndRetry(); break;
    case Alert:           StopMovement(); break;
    case Chasing:         MoveToActor(Target); break;
    case LostTarget:      WaitThenPatrol(); break;
    case EncounterPending: StopAndWait(); break;
}
```

### Behavior Tree 等效

```
Root（Selector）
  │
  ├── Decorator: HasTarget?
  │   └── Task_MoveTo (Chase)
  │        └── Service: UpdatePlayerLocation（每帧更新目标位置）
  │
  └── Sequence
        ├── Task_FindPatrolPoint
        ├── Task_MoveTo
        └── Task_Wait
```

Behavior Tree 自动管理状态切换——Decorator 每帧重新评估，条件变化时自动中断当前任务。

---

## 你项目现有的 AI 资产

- `AHSREnemyAIController` — 感知、导航、遭遇触发
- `AHSREnemyCharacter` — 碰撞触发遭遇
- `UHSREnemyDefinition` — 巡逻半径、追击距离等配置
- 感知系统（UAIPerceptionComponent + UAISenseConfig_Sight）— 视野检测

目前缺少的：
- Behavior Tree 资产
- 黑板资产
- 自定义 Task 和 Service

---

## 练习

> 如果你要把现在的追击逻辑改成一个"有概率逃跑"的 AI：血量低于 30% 时逃跑，高于 50% 时追击，中间随机。
>
> 用 Behavior Tree 的概念来描述：
>
> ① Decorator 要检查什么条件？
> ② 有几个分支？每个分支对应什么 Task？
> ③ 哪里需要 Service？

---

## Behavior Tree + Blackboard 实战设计

### 项目迁移思路

当前 C++ 状态机（不可修改代码，概念映射）：

```cpp
// 现有的 C++ 状态（概念上对照 BT）：
Idle             →
MovingToPatrol   → Task_MoveTo + 巡逻路径
PatrolWaiting    → Task_Wait
Alert            → （被 Decorator 替代，不再需要独立状态）
Chasing          → Task_MoveTo + Service_UpdateLocation
LostTarget       → Selector 自动回退到巡逻分支
EncounterPending → Decorator 阻挡所有移动（战斗已触发）
```

### 黑板设计

BT 不直接持数据。所有数据通过 Blackboard（黑板）共享：

```
Blackboard 键（Key）:
  TargetActor       → AActor*    （当前目标玩家）
  TargetLocation     → FVector    （目标最新位置）
  PatrolOrigin       → FVector    （出生/巡逻原点）
  bEncounterPending  → bool       （是否已触发战斗）
  SelfHealthPercent  → float      （当前血量百分比）
  bIsAlive           → bool       （是否存活）
```

### BT 流程（概念）

```
                    Root（Selector）
                         │
       ┌─────────────────┼──────────────────────┐
       ▼                 ▼                      ▼
   [Decorator]      [Decorator]             [Sequence]
   Encounter?       HasTarget?               Patrol
       │                 │                      │
       ▼                 ├── [Decorator]     Task_FindPatrolPoint
   Task_Wait             │    Health<30%?    Task_MoveTo
   （战斗已触发，        │      │            Task_Wait（PatrolWaitTime）
    不移动）              │      ▼
                         │   Task_Flee
                         │   [Service: UpdateLocation]
                         │
                         ├── [Decorator]
                         │    Health>50%?
                         │      │
                         │      ▼
                         │   Task_Chase
                         │   [Service: UpdateLocation]
                         │
                         └── [Decorator]
                              RandomChance(50%)
                                │
                                ├── 50% → Task_Chase [Service]
                                └── 50% → Task_Flee [Service]
```

### Task 设计（概念）

```
Task_FindPatrolPoint：
  从 PatrolOrigin 半径内找一个随机可达的 NavMesh 点
  → 写入 Blackboard.TargetLocation
  → 返回 Succeeded

Task_MoveTo：
  读取 Blackboard.TargetLocation
  → 调用 MoveToLocation
  → 到达后返回 Succeeded

Flee / Chase：
  都是一个 MoveTo ——
  Chase：目标是玩家位置
  Flee：目标是 PatrolOrigin 反方向的一个点
```

### Service 设计

```
Service_UpdateLocation：
  每 0.25 秒执行一次
  → 读取 Blackboard.TargetActor 的位置
  → 写入 Blackboard.TargetLocation
  → 让 MoveTo 任务拿到最新位置

Service_MonitorHealth：
  每 0.5 秒执行一次
  → 从 Owner 的 AttributeSet 读取 HP%
  → 写入 Blackboard.SelfHealthPercent
  → Decorator 根据这个值判断走哪个分支
```

### 现有的可能已有资产

```
Content/Enemy/AI/  ← 建议位置
  ├── BB_EnemyBehavior （Blackboard 资产）
  ├── BT_EnemyBehavior  （BehaviorTree 资产）
  └── Task/
       ├── BTT_FindPatrolPoint
       ├── BTT_Flee
       └── Service/
            └── BTS_UpdateLocation
```

### 迁移步骤（不碰现有代码时）

```
① 在编辑器中创建 Behavior Tree 和 Blackboard 资产
② 创建自定义 Task（C++ 类，继承 BTTaskNode）
③ 创建自定义 Service（C++ 类，继承 BTService）
④ 在 AIController 的 BeginPlay 中：
     → 加载 BT 资产
     → 设置黑板值（PatrolOrigin）
     → RunBehaviorTree(BTAsset)
⑤ 测试：BT 应该在巡逻
⑥ 启用 Decorator 条件 → 开始追击
⑦ 将现有状态机代码保留不动，新 BT 独立运行
```

### 注意事项

- **不用删除现有状态机**——BT 和状态机可以共存
- **RunBehaviorTree** 会覆盖 AIController 的 BrainComponent，接管决策
- `MoveToLocation`/`MoveToActor` 可以在 Task 中直接调用（AIController 的方法）
- 每帧在 BT 中不要做复杂运算——Decorator 默认每帧评估，用 Service 的 Interval（0.1~0.5s）控制频率

---

## EQS（环境查询系统）

### EQS 要解决的问题

你现有代码中巡逻找点的方式：

```cpp
NavSys->GetRandomReachablePointInRadius(Origin, Radius, NavLoc);
// 在半径内找一个随机可达点
```

问题是"随机"——可能选到墙角、悬崖边、无意义的位置。

EQS 让你指定**"什么算好的点"**，然后从候选点中选得分最高的。

### 三个概念

```
Generator（生成器） —— 候选点从哪里来
  → UEnvQueryGenerator_ProjectedPoints（圆/网格/扇形分布）
  → UEnvQueryGenerator_ActorsOfClass（场景中的某类 Actor）

Test（测试） —— 对候选点打分
  → 距离测试：越近/越远分别加分
  → 可见性测试：是否能看见目标
  → NavMesh 测试：是否在可走区域
  → 路径测试：是否能走到

Context（上下文） —— 参考系
  → UEnvQueryContext_Querier（查询者自己）
  → UEnvQueryContext_Item（候选点本身）
```

### 公式

```
最终得分 = Σ（每个 Test 的原始分数 × Weight）
```

Generator 生成候选点 → 跑所有 Test → 加权求和 → 排序 → 返回最高分点。

### 巡逻点的 EQS（替代 GetRandomReachablePointInRadius）

```
Generator:
  以 PatrolOrigin 为中心，PatrolRadius 为半径的圆形分布（16 个候选点）

Test 1: 是否在 NavMesh 上（强权重）
  → 在 = +1000 / 不在 = -100000（筛掉无效点）

Test 2: 与 PatrolOrigin 的距离
  → 离原点近 → 加分（希望靠近巡逻原点）
```

### 追击位置的 EQS

```
Generator:
  以玩家位置为中心的环形点

Test 1: NavMesh（强权重）
Test 2: 离玩家距离（近 → 高分）
Test 3: 对该点是否能看到玩家（可见 → 高分）
```

### 射击位置的 EQS（练习答案）

```
Generator:
  以敌人自身为中心的圆形点

Tests:
  ① NavMesh 必须（强权重）
  ② 与玩家的距离 → 远 → 高分（射击位要远程）
  ③ 可见性 → 能看见玩家 → 高分（否则无法攻击）

与追击位置的区别：
  追击 → 近 + 可见
  射击 → 远 + 可见 + 安全距离
```

### EQS 在项目中的潜在用途

| 查询 | Generator | 主要 Test | 替代的现有代码 |
|---|---|---|---|
| 找巡逻点 | 圆形分布（PatrolOrigin） | NavMesh + 距离原点 | GetRandomReachablePointInRadius |
| 追击路线 | 玩家周围点 | NavMesh + 距离 + 可见 | MoveToActor |
| 逃跑位置 | 自己周围 | NavMesh + 远离玩家 + 靠近 PatrolOrigin | 无（新行为） |

### EQS + BT 集成（无需 C++）

BT 编辑器中直接拖入 `BTTask_RunEQSQuery` 即可：

```
Sequence（巡逻）
  ├── BTTask_RunEQSQuery
  │     QueryTemplate = EQS_Patrol（EQS 资产）
  │     RunMode = SingleResult
  │     QueryConfig:
  │       PatrolOrigin ← Blackboard.PatrolOrigin
  │       PatrolRadius = 500.0
  │     BlackboardKey = TargetLocation（结果写入）
  └── BTTask_MoveTo
        BlackboardKey = TargetLocation（读这个 Key 走过去）

逃跑、追击共用同一个 TargetLocation Key + 同一个 BTTask_MoveTo。
区别只在于谁往 TargetLocation 里写数据。

## 完整代码示例：用 EQS 的巡逻+追击 BT

### 整体 BT 结构（编辑器中配置）

```
Root（Selector）
  │
  ├── [Decorator: HasTarget?] → Observer Aborts = Both
  │   └── Sequence（追击）
  │        ├── [Service: BTS_RunEQS_Chase]
  │        │      每 0.5 秒运行一次 EQS 查询
  │        │      Generator: 玩家周围 800 半径内的点
  │        │      Tests: NavMesh + 距离玩家近 + 可见
  │        │      结果写入 → TargetLocation
  │        │
  │        └── BTTask_MoveTo
  │               BlackboardKey = TargetLocation
  │
  └── Sequence（巡逻）
       ├── [Service: BTS_RunEQS_Patrol]
       │      每 2 秒运行一次 EQS 查询
       │      Generator: PatrolOrigin 周围 500 半径内的点
       │      Tests: NavMesh + 距离 PatrolOrigin 近
       │      结果写入 → TargetLocation
       │
       ├── BTTask_MoveTo
       │      BlackboardKey = TargetLocation
       │
       └── BTTask_Wait
              WaitTime = 2.0
```

巡逻和追击都用了 **Service + BTTask_RunEQSQuery + BTTask_MoveTo**。
区别只在于 EQS 查询的 Generator 和 Test 不同。

### 真正需要 C++ 写的：BTS_UpdateTargetLocation

`BTS_UpdateTargetLocation.h`：

```cpp
#pragma once

// BTService 是"在 Task 执行期间持续运行的逻辑"
// 这个 Service 的作用：每隔一小段时间，把目标的位置刷新到黑板上
#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTS_UpdateTargetLocation.generated.h"

UCLASS()
class HSR_API UBTS_UpdateTargetLocation : public UBTService
{
    GENERATED_BODY()

public:
    UBTS_UpdateTargetLocation();

protected:
    // TickNode 是 Service 的核心函数——每 Interval 秒执行一次
    virtual void TickNode(UBehaviorTreeComponent& OwnerComp,
        uint8* NodeMemory, float DeltaSeconds) override;

    // 以下两个是"黑板 Key 选择器"
    // 在 BT 编辑器中拖入这个 Service 后，你会在细节面板中看到下拉框
    // 你选择黑板上已有的 Key 名字，运行时自动绑定

    // 读：目标是哪个 Actor（由 Perception 系统写入）
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector TargetActorKey;

    // 写：目标的位置写到这里（MoveTo 任务会读这个 Key）
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector TargetLocationKey;
};
```

`BTS_UpdateTargetLocation.cpp`：

```cpp
#include "BTS_UpdateTargetLocation.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"

UBTS_UpdateTargetLocation::UBTS_UpdateTargetLocation()
{
    // Interval 是父类 UBTService 的成员变量
    // 控制 TickNode 每隔多少秒执行一次
    // 这里设为 0.25 秒 = 每秒刷新 4 次玩家位置
    Interval = 0.25f;

    // 进入/退出分支时不需要额外逻辑，所以关掉
    bNotifyBecomeRelevant = false;
    bNotifyCeaseRelevant  = false;

    // 在 BT 编辑器中这个节点显示的名字
    NodeName = TEXT("Update Target Location");
}

void UBTS_UpdateTargetLocation::TickNode(
    UBehaviorTreeComponent& OwnerComp,
    uint8* NodeMemory,
    float DeltaSeconds)
{
    // 必须调用父类版本（UE 标准做法）
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

    // OwnerComp 是"运行这个 BT 的组件"，通过它拿到黑板和 AIController
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    AAIController* AIController = OwnerComp.GetAIOwner();

    // 黑板或控制器没了 → 什么都不做
    if (!BB || !AIController) return;

    // 从黑板读 "TargetActor"——这个 Key 存的是玩家 Actor 的引用
    // GetValueAsObject 返回 UObject*，需要 Cast 到 AActor*
    AActor* Target = Cast<AActor>(
        BB->GetValueAsObject(TargetActorKey.SelectedKeyName));

    // 如果黑板里没有目标（比如已经丢失了），啥也不做
    // 上一次写入的 TargetLocation 值会保留，MoveTo 继续走向最后已知位置
    if (!Target) return;

    // 把目标当前的位置写到黑板的 "TargetLocation" Key
    // MoveTo 任务正在读这个 Key，所以它会自动走向新位置
    BB->SetValueAsVector(
        TargetLocationKey.SelectedKeyName,
        Target->GetActorLocation());

    // 至此：
    // TargetLocation 已经被更新为玩家的最新位置
    // BTTask_MoveTo 正在持续往这个位置走
    // 下次 TickNode 会在 0.25 秒后再执行一次
}
```

### Perception 回调精简版

```cpp
// 这个函数是 UAIPerceptionComponent 发现/丢失目标时自动触发的回调
// 参数：
//   Actor —— 被感知到的那个 Actor（可能是玩家，也可能是其他东西）
//   Stimulus —— 感知结果（成功感知到 / 丢失目标）

void AHSREnemyAIController::OnTargetPerceptionUpdated(
    AActor* Actor, FAIStimulus Stimulus)
{
    // 只对玩家角色感兴趣，其他角色（NPC、小动物等）忽略
    AHSRExplorationCharacter* PlayerChar =
        Cast<AHSRExplorationCharacter>(Actor);
    if (!PlayerChar) return;

    // 获取 BT 的黑板组件
    // 没有黑板说明 BT 还没启动，跳过
    UBlackboardComponent* BB = GetBlackboardComponent();
    if (!BB) return;

    if (Stimulus.WasSuccessfullySensed())
    {
        // === 看到玩家了 ===
        // 只做一件事：把玩家的引用写到黑板的 "TargetActor" Key
        // 剩下的——"现在该不该追？要不要中断巡逻？"——全由 BT 的
        // Decorator: HasTarget? 通过 Observer Aborts 自动处理
        BB->SetValueAsObject("TargetActor", Actor);

        // 注意：这里没有 SetState(Chasing)，没有 MoveToActor
        // 行为控制完全交给了 Behavior Tree
    }
    else
    {
        // === 丢失目标了（玩家跑远了/被挡住了） ===
        // 只做一件事：清空黑板的 "TargetActor" Key
        // BT 的 Decorator 检测到 Key 被清空 → 条件从 true 变 false
        // → Observer Aborts = Self → 中断追击分支
        // → Selector 回退到巡逻分支
        BB->ClearValue("TargetActor");
    }

    // 整个函数的核心思想：
    // Perception 回调只负责"输入"——把看到的东西写进黑板
    // BT 负责"决策"——判断该做什么
    // Task 负责"执行"——走到点、等待、播放动画
    // 三层分离，互不干扰
}
```

### 完整数据流

```
玩家进入视野：
  Perception 回调 → 写黑板 TargetActor = 玩家
  → Decorator: HasTarget? 的黑板监听器触发
  → Observer Aborts = Both
  → 条件从 false → true → LowerPriority
  → 中断巡逻 → 进入追击

追击期间：
  BTS_UpdateTargetLocation 每 0.25 秒
    → 读 TargetActor → 取位置 → 写 TargetLocation
  BTTask_MoveTo 持续往 TargetLocation 走

目标丢失：
  Perception 回调 → 清空黑板 TargetActor
  → Decorator: HasTarget? 条件从 true → false
  → Observer Aborts = Self
  → 中断追击 → Selector 回退到巡逻
```

## UE 内置 Task 源码：BTTask_MoveTo

实际路径：`Engine/Source/Runtime/AIModule/Private/BehaviorTree/Tasks/BTTask_MoveTo.cpp`

核心流程（第 39-72 行）——ExecuteTask：

```
① 拿 AIController（OwnerComp.GetAIOwner）
② 交给 PerformMoveTask

PerformMoveTask（第 75-163 行）：
  → 从黑板读目标位置：
     Key 是 Actor 类型 → MoveReq.SetGoalActor(TargetActor)
     Key 是 Vector 类型 → MoveReq.SetGoalLocation(TargetLocation)
  → 创建 UAITask_MoveTo（异步移动任务）
  → ReadyForActivation()（激活移动）
  → 返回 EBTNodeResult::InProgress（告诉 BT：还在走，没完成）

移动完成后（第 285 行 OnGameplayTaskDeactivated）：
  → 调 FinishLatentTask，传 Succeeded 或 Failed
  → BT 引擎继续执行下一个节点
```

关键：BTTask_MoveTo 内部调的是 UAITask_MoveTo，最终调的是 AAIController::MoveToLocation()，和你现有 C++ 代码手动调的是同一个函数。UE 把它包装成了 BT 节点。

## AI 体系全景回顾

### 一句话总结每个概念

```
Behavior Tree: 决策树，每帧从根评估，选分支执行

Task:     "做什么"——叶子节点，执行一个具体动作（移动、等待、攻击）
Decorator:"能不能做"——挂在节点前的条件判断
Service:  "持续做什么"——执行期间定期运行的背景逻辑

Blackboard: 共享数据存储，所有节点通过它交换数据
  Key: "TargetActor" → AActor*
  Key: "TargetLocation" → FVector

Observer Aborts: Decorator 注册黑板监听器，条件变化时自动切换分支
  None          → 只在进入时检查一次，不监听
  Self          → 条件变 false 时中断自己
  LowerPriority → 条件变 true 时中断右侧
  Both          → 两者都要

EQS: 环境查询，从候选点中选最佳
  Generator: 生成候选点
  Test: 打分
  Context: 参考系

BTTask_RunEQSQuery: 内置 Task，在 BT 中直接执行 EQS 查询
```

### 完整数据流——巡逻+追击

```
玩家进入视野：
  AIController::OnTargetPerceptionUpdated
    → 写黑板 TargetActor = 玩家角色
    →（只写黑板，不做其他事）

  BT 下一帧评估：
    → Decorator: HasTarget? 的黑板监听器触发
    → Observer Aborts = Both → 条件从 false → true
    → LowerPriority → 中断巡逻
    → 进入追击分支

追击期间：
  BTS_UpdateTargetLocation 每 0.25 秒
    → 读 TargetActor → 取玩家位置 → 写 TargetLocation

  BTTask_MoveTo
    → 读 TargetLocation
    → MoveToLocation(位置)
    → 返回 InProgress（移动中）

目标丢失：
  Perception 回调 → 清空 TargetActor

  BT 下一帧评估：
    → Decorator: HasTarget? 条件从 true → false
    → Observer Aborts = Both → Self
    → 中断追击
    → Selector 回退到巡逻分支
```

### 自定义 Task 的规律

```
同步 Task（一次执行完）：
  ExecuteTask 内做完所有事，返回 Succeeded/Failed
  例如：BTTask_FindPatrolPoint（找点 → 写黑板 → 返回 Succeeded）

异步 Task（需要等待）：
  ExecuteTask 返回 InProgress
  TickTask 或回调中调 FinishLatentTask 来结束
  例如：BTTask_MoveTo（移动中 → InProgress → 走到后 FinishLatentTask）
```

### Perception vs Service 对比

| | Perception | Service |
|---|---|---|
| **写在** | AIController 上 | Behavior Tree 中 |
| **触发方式** | 感知系统回调触发 | 按 Interval 定时执行 |
| **职责** | 发现/丢失"新"目标 | 刷新"已知"目标的位置 |
| **执行次数** | 触发一次 | 每 0.25~0.5 秒一次 |

分工：Perception 负责"有没有目标"，Service 负责"目标当前在哪"。

### 最小 Task 模板

头文件：

```cpp
#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_YourName.generated.h"

UCLASS()
class HSR_API UBTTask_YourName : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_YourName();
    virtual EBTNodeResult::Type ExecuteTask(
        UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
```

实现文件：

```cpp
#include "BTTask_YourName.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"

UBTTask_YourName::UBTTask_YourName()
{
    INIT_TASK_NODE_NOTIFY_FLAGS();
    NodeName = TEXT("YourName");
}

EBTNodeResult::Type UBTTask_YourName::ExecuteTask(
    UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AI = OwnerComp.GetAIOwner();
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!AI || !BB) return EBTNodeResult::Failed;

    // 读黑板 → 做事 → 写黑板

    return EBTNodeResult::Succeeded; // 或 InProgress（异步）
}
```

### 从头搭建 AI 的 5 步

```
第 1 步：编辑器建资产
  内容浏览器 → 右键 → AI
    Behavior Tree（如 BT_EnemyBehavior）
    Blackboard（如 BB_EnemyBehavior）
  在 Blackboard 中加 Key：
    TargetActor (Object), TargetLocation (Vector), PatrolOrigin (Vector)

第 2 步：写 Service（C++，约 40 行）
  BTS_UpdateTargetLocation：每 0.25 秒刷新 TargetActor 位置到 TargetLocation

第 3 步：写 Task（C++，约 30 行）
  BTTask_FindPatrolPoint：找一个随机巡逻点，写入 TargetLocation

第 4 步：打开 BT 资产拖节点
  Root（Selector）
    ├── [Decorator: TargetActor 有值]
    │   └── Sequence
    │        ├── BTS_UpdateTargetLocation（Service）
    │        └── BTTask_MoveTo（Key = TargetLocation）
    │
    └── Sequence
         ├── BTTask_FindPatrolPoint
         └── BTTask_MoveTo（Key = TargetLocation）

第 5 步：AIController 启动 BT + 写初始黑板值
  RunBehaviorTree(BTAsset)
  Blackboard->SetValueAsVector("PatrolOrigin", GetPawn()->GetActorLocation())
  Perception 回调只写 TargetActor
```

## 综合回顾：模拟面试 QA

### GAS

**Q: InstancedPerActor 和 InstancedPerExecution 有什么区别？**

InstancedPerActor：每个 ASC 创建一份能力实例，每次激活用同一个，需手动清理状态（如 ClearPreparedFormalDamage）。性能好，适合回合制技能。

InstancedPerExecution：每次激活创建新实例，用完销毁，不需要手动清理。适合"每次激活状态完全不同"的能力（如召唤随从），但性能开销更大。

**你项目中的 4 个能力全部是 InstancedPerActor。**

### C++ 引擎

**Q: UObject 的 GC 基于什么算法？UPROPERTY 起什么作用？**

算法：Mark-Sweep（标记-清扫）。
① 从根集（Root Set：全局 UObject、当前 World 下的 Actor、UPROPERTY 引用）开始遍历
② 标记所有可达对象
③ 清扫所有未被标记的对象

UPROPERTY：告诉 GC"这个引用存在，不要回收"。没有 UPROPERTY 的裸指针 GC 不知道，对象可能被回收导致野指针崩溃。

### 战斗系统

**Q: FHSRBattleParticipant 为什么用 TWeakObjectPtr 不用 TObjectPtr？**

TWeakObjectPtr 不阻止 GC，对象销毁后 IsValid() 返回 false，不会崩溃。
TObjectPtr 阻止 GC，如果参与者被 Destroy 但 FHSRBattleParticipant 仍然持有引用 → 内存泄漏。

战斗系统不拥有角色的生命周期，只是观察者。

### AI

**Q: Decorator 的 Observer Aborts = None 和 Both 的区别？**

None：只在进入节点时检查一次。之后黑板变化不会重新评估。
Both：进入时检查 + 注册黑板监听器。条件变化时自动中断当前分支（条件变 false 中断自己，条件变 true 中断右侧低优先级分支）。

### 感知回调 BT 改造

```
改造前（当前代码）：
  OnTargetPerceptionUpdated 做了三件事：
    ① 状态判断（是否已 EncounterPending？是否已在追击？）
    ② 状态切换（SetState(Chasing)）
    ③ 执行（MoveToActor、StopMovement）

改造后（BT 模式）：
  回调只写黑板：
    if (看到玩家) BB->SetValueAsObject("TargetActor", Actor)
    if (玩家丢失) BB->ClearValue("TargetActor")

  BT 接管：
    判断 → Decorator: HasTarget? + Observer Aborts
    移动 → BTTask_MoveTo
    持续刷新 → BTS_UpdateTargetLocation
```

### Flee Task 示例

```cpp
EBTNodeResult::Type UBTTask_Flee::ExecuteTask(
    UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AI = OwnerComp.GetAIOwner();
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!AI || !BB) return EBTNodeResult::Failed;

    AActor* Target = Cast<AActor>(BB->GetValueAsObject("TargetActor"));
    FVector PatrolOrigin = BB->GetValueAsVector("PatrolOrigin");
    if (!Target) return EBTNodeResult::Failed;

    FVector AILocation = AI->GetPawn()->GetActorLocation();
    FVector FleeDirection = (AILocation - Target->GetActorLocation()).GetSafeNormal();
    FVector ToOrigin = (PatrolOrigin - AILocation).GetSafeNormal();

    BB->SetValueAsVector("TargetLocation",
        AILocation + FleeDirection * 500.0f + ToOrigin * 300.0f);

    return EBTNodeResult::Succeeded;
}
```

## 第 6 课：EQS 自定义 Test

### 为什么需要自定义 Test

内置的 Test（距离、可见性、NavMesh）只能做"单一维度"评分。

当你需要"距离玩家远 + 距离巡逻原点近 + 能看到玩家"这三个条件**按特定公式组合**时，内置 Test 做不了——要么相乘/相加（不够灵活），要么拖多个 Test（权重不好调）。

自定义 Test 允许你在 `RunTest` 中写任意评分公式。

### 核心：RunTest + SetScore

```cpp
// 唯一需要重写的函数
virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

// RunTest 内部的核心模式：
for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
{
    float ItemScore = 你的计算公式(候选点);          // 算分
    It.SetScore(TestPurpose, FilterType, ItemScore, MinThreshold, MaxThreshold);  // 写入 EQS
}
```

### SetScore 的职责

`SetScore` 把分数写入当前候选点的条目中，EQS 引擎自动处理后续的**排序和选最优**。你不需要维护"当前最高分"。

```
Generator 生成 16 个候选点
  → RunTest 遍历 16 个点
    → 每个点调用 It.SetScore(分数)
    → EQS 内部将分数挂到该点的数据结构上
  → 遍历结束
  → EQS 管理器将 16 个点按分数排序
  → 取最高分的点（或根据 RunMode 取随机）
  → 写入结果到黑板
```

### 最小自定义 Test

`EnvQueryTest_SafeShotRange.h`：

```cpp
#pragma once
#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvQueryTest_SafeShotRange.generated.h"

UCLASS()
class HSR_API UEnvQueryTest_SafeShotRange : public UEnvQueryTest
{
    GENERATED_BODY()

public:
    UEnvQueryTest_SafeShotRange();
    virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

    UPROPERTY(EditAnywhere, Category = Distance)
    TSubclassOf<UEnvQueryContext> DistanceTo;
};
```

`EnvQueryTest_SafeShotRange.cpp`：

```cpp
#include "EnvQueryTest_SafeShotRange.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"

UEnvQueryTest_SafeShotRange::UEnvQueryTest_SafeShotRange()
{
    DistanceTo = UEnvQueryContext_Querier::StaticClass();
    ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
    Cost = EEnvTestCost::Low;
}

void UEnvQueryTest_SafeShotRange::RunTest(FEnvQueryInstance& QueryInstance) const
{
    // 1. 获取参考位置
    TArray<FVector> ContextLocations;
    if (!QueryInstance.PrepareContext(DistanceTo, ContextLocations))
        return;

    // 2. 遍历每个候选点
    for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
    {
        FVector ItemLocation = GetItemLocation(QueryInstance, It.GetIndex());

        float BestScore = 0.0f;
        for (int32 ContextIndex = 0; ContextIndex < ContextLocations.Num(); ContextIndex++)
        {
            float Dist = FVector::Dist(ItemLocation, ContextLocations[ContextIndex]);
            float Score = Dist;  // 你的评分逻辑
            BestScore = FMath::Max(BestScore, Score);
        }

        // 3. 打分（EQS 引擎自动记录并排序）
        It.SetScore(
            TestPurpose,     // 编辑器配置
            FilterType,      // 编辑器配置
            BestScore,       // 自定分数
            FloatValueMin,   // 编辑器配置
            FloatValueMax    // 编辑器配置
        );
    }
}
```

### 什么时候用自定义 Test

| | 内置 Test | 自定义 Test |
|---|---|---|
| 场景 | 单独的距离/可见性检查 | 距离+可见性+方向按公式组合 |
| 配置方式 | 编辑器拖节点 | 写 C++ RunTest |
| 灵活性 | 低（只能调权重） | 高（任意公式） |

最简单的原则：**能用编辑器配出来的，就不要写自定义 Test。** 只有当"距离测试 × 0.7 + 可见性测试 × 0.3"的组合不符合你的需求时，才需要写自定义 Test。

## 第 7 课：StateTree——UE5 的新 AI 框架

### BT → StateTree 对应表

| BT | StateTree | 源码 | 职责 |
|---|---|---|---|
| BTTaskNode | FStateTreeTaskBase | `StateTreeTaskBase.h` | 进入状态时执行的动作 |
| BTDecorator | FStateTreeConditionBase | `StateTreeConditionBase.h` | Transition 条件判断 |
| BTService | FStateTreeEvaluatorBase | `StateTreeEvaluatorBase.h` | 状态持续期的背景逻辑 |
| Blackboard | 属性绑定 | 内置 | 共享数据 |

### 核心函数对比

```cpp
// ===== BT Task =====
ExecuteTask(OwnerComp, NodeMemory)     → Succeeded/Failed/InProgress
TickTask(OwnerComp, NodeMemory, DT)    → 每帧（需返回 InProgress）
AbortTask(OwnerComp, NodeMemory)       → 被中断时调用
OnTaskFinished(OwnerComp, ...)         → 可选（需设 bNotifyTaskFinished）

// ===== StateTree Task =====
EnterState(Context, Transition)        → Succeeded/Failed/Running
TickState(Context, DeltaTime)          → 每帧（EnterState 返回 Running 后）
ExitState(Context, Transition)         → 离开状态时**一定会被调用**
StateCompleted(Context, ...)           → 状态完成后调用
```

### 核心差异：异步 Task 的清理

**BT 的问题——不主动清理：**

```cpp
// 你的异步 BT Task
EBTNodeResult::Type ExecuteTask(...)
{
    StartMoving();                    // 开始移动
    return EBTNodeResult::InProgress; // 异步执行中
}

// 此时 Decorator 条件变化 → Observer Aborts 触发
// BT 引擎调用 AbortTask()
// ⚠️ 如果你没重写 AbortTask——移动还在继续！没人 StopMovement()
```

BT 中断 Task 时调 `AbortTask`（第 48 行 `BTTaskNode.h`）——但 **`AbortTask` 默认实现什么都不做**。你需要自己重写它：

```cpp
// BT 中正确的清理方式：
virtual EBTNodeResult::Type AbortTask(
    UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override
{
    // 手动清理——这个 Task 被中断时需要做的事
    AAIController* AI = OwnerComp.GetAIOwner();
    if (AI) AI->StopMovement();   // 停止移动
    return EBTNodeResult::Aborted;
}
```

如果你忘了写 `AbortTask`，Task 被中断后角色会继续走——因为没人让它停下来。

**StateTree 的解决——ExitState 保证被调用：**

```cpp
// StateTree Task
EStateTreeRunStatus EnterState(Context, Transition)
{
    StartMoving();
    return EStateTreeRunStatus::Running;
}

void ExitState(Context, Transition)   // ← 不管什么原因离开状态，一定被调用！
{
    StopMovement();  // 清理直接写在这里，不会漏
}

EStateTreeRunStatus TickState(Context, DeltaTime)
{
    if (到达) return EStateTreeRunStatus::Succeeded;
    return EStateTreeRunStatus::Running;
}
```

`ExitState` 在三种情况下都会被调用：
- 状态正常完成（Succeeded/Failed）
- Transition 触发跳到别的状态
- 整棵树被重置

**BT 没有等价的"保证调用"机制。** 你要记得重写 `AbortTask` 和 `OnTaskFinished`，漏一个就可能出 Bug。

### 设计哲学总结

```
BT：Delegates → 委托模式
    你自己注册清理回调（AbortTask），不注册就不调
    给你灵活度，但你要自己兜底

StateTree：Lifecycle → 生命周期模式
    框架保证 EnterState → TickState → ExitState 的调用完整性
    少写代码，少漏清理
```

### 什么时候用

```
用 BT（你的项目当前的 AI 很适合 BT）：
  - 需要每帧重新评估优先级（巡逻 vs 追击 vs 逃跑）
  - 条件变化时需要立即中断当前行为

用 StateTree：
  - 状态切换是明确的（空闲→战斗→结束）
  - 不需要每帧检查所有条件
  - 需要 ExitState 保证清理

实践中两者可以共存：
  复杂的优先级决策 → BT
  明确的阶段切换 → StateTree
```

## 异步 Task 收尾三题

### ① OnTaskFinished vs AbortTask

两者都是异步 Task 可选的清理回调，但触发时机不同：

| | AbortTask | OnTaskFinished |
|---|---|---|
| 正常完成 (Succeeded/Failed) | ❌ 不调 | ✅ 调 |
| 被中断 (Aborted) | ✅ 调 | ✅ 调 |
| 作用 | **紧急停止**当前动作 | **通用收尾**清理 |

源码中 BTTask_MoveTo 的区分：

```cpp
EBTNodeResult::Type UBTTask_MoveTo::AbortTask(...)
{
    // 只在中断时调：取消移动请求
    AIController->GetPathFollowingComponent()->AbortMove(...);
}

void UBTTask_MoveTo::OnTaskFinished(...)
{
    // 不管正常结束还是中断都调：清理引用 + 取消黑板监听
    MyMemory->Task.Reset();
    BlackboardComp->UnregisterObserver(MyMemory->BBObserverDelegateHandle);
}
```

**OnTaskFinished 不会自动被调——必须在构造函数中设标志：**

```cpp
UBTTask_YourName::UBTTask_YourName()
{
    INIT_TASK_NODE_NOTIFY_FLAGS();
    // 这个宏检查你是否重写了 TickTask 和 OnTaskFinished
    // 重写了 → 自动设 bNotifyTaskFinished = true
}
```

宏内部（BTTaskNode.h 第 127-131 行）：

```cpp
bNotifyTick = !std::is_same_v<decltype(&UBTTaskNode::TickTask), TickTask>;
bNotifyTaskFinished = !std::is_same_v<decltype(&UBTTaskNode::OnTaskFinished), OnTaskFinished>;
```

### ② Latent Abort——不能立刻停的中断

某些场景下 Task 不能立刻被中断：

```
例子：播放攻击蒙太奇 → 被中断 → 必须先播完收刀动画
```

AbortTask 返回 InProgress 表示"等一下才能停"：

```cpp
EBTNodeResult::Type AbortTask(...)
{
    if (正在收刀)
    {
        return EBTNodeResult::InProgress;
        // 收刀动画播完后调 FinishLatentAbort(OwnerComp)
    }
    return EBTNodeResult::Aborted;  // 立刻停
}
```

### ③ NodeMemory——共享 Task 实例如何存状态

bCreateNodeInstance = false（默认）时，所有 AI 共享同一个 Task 对象。

**状态不能存在成员变量里，必须存在 NodeMemory 中：**

```cpp
// 第一步：定义内存结构
struct FBTMoveToTaskMemory
{
    FAIRequestID MoveRequestID;
    FDelegateHandle BBObserverDelegateHandle;
    FVector PreviousGoalLocation;
    TWeakObjectPtr<UAITask_MoveTo> Task;
};

// 第二步：告诉 BT 需要多少内存
virtual uint16 GetInstanceMemorySize() const override
{
    return sizeof(FBTMoveToTaskMemory);
}

// 第三步：用 CastInstanceNodeMemory 读写
EBTNodeResult::Type ExecuteTask(..., uint8* NodeMemory)
{
    FBTMoveToTaskMemory* MyMemory = CastInstanceNodeMemory<FBTMoveToTaskMemory>(NodeMemory);
    MyMemory->MoveRequestID = ...;  // 存在 NodeMemory，不是成员变量
}
```

NodeMemory 只在异步 Task 需要时用。同步 Task 不需要。

### 完整异步 Task 模板

```cpp
UCLASS()
class UBTTask_MoveWithCleanup : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_MoveWithCleanup()
    {
        INIT_TASK_NODE_NOTIFY_FLAGS();
        bCreateNodeInstance = false;
        NodeName = TEXT("Move With Cleanup");
    }

    virtual EBTNodeResult::Type ExecuteTask(
        UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override
    {
        AAIController* AI = OwnerComp.GetAIOwner();
        if (!AI) return EBTNodeResult::Failed;
        AI->MoveToLocation(TargetLocation);
        return EBTNodeResult::InProgress;
    }

    virtual EBTNodeResult::Type AbortTask(
        UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override
    {
        AAIController* AI = OwnerComp.GetAIOwner();
        if (AI) AI->StopMovement();
        return EBTNodeResult::Aborted;
    }

    virtual void OnTaskFinished(
        UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory,
        EBTNodeResult::Type TaskResult) override
    {
        // 通用清理——正常完成和中断都调
        UE_LOG(LogTemp, Log, TEXT("Move finished: %d"), TaskResult);
    }

    virtual uint16 GetInstanceMemorySize() const override
    {
        return sizeof(FBTMoveToTaskMemory);
    }
};
```

## 调试工具

### 两种调试工具

```
编辑器 BT Debugger（编辑器中用的）：
  → 打开 BT 资产，点击"Debug"按钮
  → PIE 中 BT 上显示橙色高亮路径
  → 看到当前执行节点、Decorator 红/绿状态

Gameplay Debugger（游戏中按 ` 键）：
  → PIE 中按 ` 键打开覆盖层
  → 按 3 切换到 Behavior Tree 面板
  → 显示黑板所有 Key 的当前值 + 当前节点链
```

### 编辑器 BT Debugger 用法

```
① 打开 BT 资产 → 工具栏点"Debug"（虫子图标）
② 运行 PIE
③ BT 资产上实时显示：
   - 执行中的分支：橙色高亮
   - 执行中的节点：边框闪烁
   - Decorator 条件：绿色（true）/ 红色（false）
   - 等待中的节点："Waiting"
④ 加断点：右键 Task → 添加断点 → 执行到此时暂停
```

### Gameplay Debugger 用法

```
PIE 中按 `（反引号，Tab 键上方）：
  → 屏幕左上角出现调试覆盖层

按 3 → Behavior Tree 面板：
  显示：
  - 当前执行的节点和父节点链
  - 黑板所有 Key 的值（TargetActor、TargetLocation 等）
  - 当前状态（Running / Succeeded / Failed）

再次按 `：关闭覆盖层
```

### 常见调试场景

```
AI 不追玩家：
  ① 按 ` → 3 → 检查黑板上 TargetActor 有没有值
     没有值 → Perception 回调没写黑板
     有值但不动 → BT 结构问题（Decorator 条件？）

  ② 打开 BT 资产 → 调试模式
     Decorator 绿色 → 条件满足，追击分支应该激活
     Decorator 红色 → 条件不满足，停在巡逻

AI 追一半突然回巡逻：
  Gameplay Debugger 看 TargetActor 是否被清空
  BT Debugger 断点打在 OnTargetPerceptionUpdated 看 Stimulus
```

## 感知系统（Perception）深度

### 已有的感知配置

```cpp
// AHSREnemyAIController 构造函数
SightConfig->SightRadius = 1000.0f;              // 看到玩家的最远距离
SightConfig->LoseSightRadius = 1500.0f;           // 丢失目标的距离
SightConfig->PeripheralVisionAngleDegrees = 90.0f; // 视野角度（90° = 半圆）
SightConfig->SetMaxAge(5.0f);                    // 感知信息保存 5 秒
```

当前只用了一种感知：视觉（Sight）。

### 内置感知类型

```
Sight（你正在用的）
  条件：视野圆锥内、无遮挡
  类：UAISenseConfig_Sight
  回调：OnTargetPerceptionUpdated

Hearing（听觉）
  玩家脚步声、枪声触发 AI 警觉
  类：UAISenseConfig_Hearing
  通过 MakeNoise() 触发

Damage（受伤感知）
  AI 被攻击时自动感知攻击者
  类：UAISenseConfig_Damage
  自动触发，不需额外配置

Team（团队感知）
  队友感知到的目标自动共享
  类：UAISenseConfig_Team
  一个敌人看到你，全员警戒
```

### 配置多种感知

```cpp
// 在 AIController 构造函数中加 Hearing
AHSREnemyAIController::AHSREnemyAIController()
{
    SightConfig = CreateDefaultSubobject<...>(TEXT("SightConfig"));
    SightConfig->SightRadius = 1000.0f;
    PerceptionComponent->ConfigureSense(*SightConfig);

    UAISenseConfig_Hearing* HearingConfig =
        CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
    HearingConfig->HearingRange = 2000.0f;
    HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
    PerceptionComponent->ConfigureSense(*HearingConfig);

    PerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
}
```

### FAIStimulus 详解

```cpp
void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
    // 核心字段：
    Stimulus.WasSuccessfullySensed()  // true=感知到 false=丢失
    Stimulus.Type                      // 感知源类型（Sight/Hearing/Damage）
    Stimulus.Strength                  // 强度（Sight=1.0，Hearing 取决于音量）
    Stimulus.StimulusLocation          // 刺激源位置（脚步声位置）
    Stimulus.Tag                       // 自定义 Tag
}
```

## 导航系统（NavMesh）

### NavMesh 是什么

NavMesh = Navigation Mesh（导航网格）。覆盖在关卡地面上的蓝色半透明网格。蓝色区域 = AI 能走的地方，白色区域 = AI 走不了。

**编辑器里按 P 键切换显示/隐藏。**

### 你的项目已经在用

```cpp
// 巡逻找随机点
NavSys->GetRandomReachablePointInRadius(Orgin, Radius, NavLoc);

// 追击（BTTask_MoveTo 内部）
AIController->MoveToLocation(TargetLocation);
// 自动在 NavMesh 上计算路径，绕开障碍物
```

### 动态障碍物

```cpp
// 默认 NavMesh 在游戏开始前烘焙好
// 要让游戏中的物体阻挡 AI 路径：

// 方式一：组件上设置
UBoxComponent* Box = CreateDefaultSubobject<...>(TEXT("Collision"));
Box->SetCanEverAffectNavigation(true);

// 方式二：使用 UNavModifierComponent
UBoxComponent* Obstacle = ...
Obstacle->bDynamicObstacle = true;  // 运行时移动后会自动更新 NavMesh
```

### NavLink（自定义跨越）

两个平台之间有缝隙，AI 跳不过去 → 放置 ANavLinkProxy 连接两个 NavMesh 面。AI 走到起点时自动沿链接移动。你的项目目前是平面场景，暂时不需要。

### 调试

```
编辑器按 P → 显示/隐藏 NavMesh（蓝色）
编辑器按 ' → 显示 AI 当前路径线

代码中：
  NavSys->ProjectPointToNavigation(Location, NavLoc);
  // 检查某点是否在 NavMesh 上
```

## 多 AI 协同

### 问题

你项目中有多个敌人。现在每个敌人独立行动——玩家被敌人 A 追时从敌人 B 旁边跑过去，敌人 B 毫无反应。

### 感知共享（TeamSense）

```cpp
// 让所有敌人共享视野：
// 方法一：使用 UAISenseConfig_Team

UAISenseConfig_Team* TeamConfig =
    CreateDefaultSubobject<UAISenseConfig_Team>(TEXT("TeamConfig"));
TeamConfig->DetectionByAffiliation.bDetectEnemies = true;
PerceptionComponent->ConfigureSense(*TeamConfig);

// 当敌人 A 看到玩家时：
// 敌人 A 的 Stimulus 会自动复制给附近所有同队 AI
// 敌人 B 即使没看到玩家，也会知道玩家的位置
```

### 简单协同逻辑（不写额外系统）

```
合作追击：
  每个敌人独立运行同一个 BT
  黑板上都有 TargetActor（通过 TeamSense 共享）
  每个敌人各自追击

  结果：一个玩家被多个敌人从不同方向包围
  不需要写额外的"协同代码"
```

### 更复杂的协同（排队进攻）

```
如果希望"一个敌人进攻时，另一个在远处包抄"：

  方法：在黑板上加一个"当前攻击者" Key
  Decorator 检查：
    → 如果"当前攻击者"不是自己 → 等外围
    → 如果"当前攻击者"是自己 → 进攻

  攻击者死亡 → 清空"当前攻击者" Key
  → 下一个敌人 BT 检测到"当前攻击者"空了
  → 自己顶上成为新攻击者
```

### 敌人数量的管理

多个敌人同时追玩家 → 可能会出现"叠在一起"的情况。

```
解法 1：EQS 追击位置的 Generator 加 Test
  候选点不要重叠（与队友的距离）

解法 2：每个敌人生成时设一个随机偏移
  BB->SetValueAsFloat("FormationOffset", FMath::FRandRange(-200, 200));
  EQS 中应用这个偏移
```
