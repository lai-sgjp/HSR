# GAS 学习笔记

> 基于 HSR 项目的 Gameplay Ability System 学习记录
> 项目引擎：UE 5.6 | 模块：Runtime（单个 HSR 模块）
> 引擎源码路径：
>   `E:\programs\Epic Games\UE_5.6\Engine\Plugins\Runtime\GameplayAbilities\Source\GameplayAbilities\`

---

# 第 1 站：GAS 宏观架构

## 1.1 四极世界

GAS（Gameplay Ability System）有四个核心类，它们的协作构成了整个系统。

### 核心类角色

| 类 | HSR 项目对应 | 一句话职责 |
|---|---|---|
| **ASC**（AbilitySystemComponent） | `UHSRAbilitySystemComponent` | GAS 的 IoC 容器——能力注册、GE 应用、属性持有、Tag 管理全部汇聚于此 |
| **GameplayAbility** | `UHSRGameplayAbilityBase` 及其 4 个子类 | 描述「角色能做什么」——一个可激活的行为，如技能、攻击、治疗 |
| **GameplayEffect** | `InitialAttributesEffect` 等 GE 蓝图 | 描述「属性如何变化」——纯粹的声明式数据，不从 Actor 派生 |
| **AttributeSet** | `UHSRCoreAttributeSet`（13 个属性） | 定义「角色有什么属性」——纯数据容器 + 变化前/后回调 |

### 关系图

```
┌───────────────────────────────────────────────────────────────────┐
│                    UAbilitySystemComponent (ASC)                   │
│                          ★ 系统中枢 ★                              │
│                                                                   │
│  ┌──────────────┐   ┌──────────────┐   ┌──────────────────────┐  │
│  │ AttributeSet  │   │ GameplayAbil │   │ ActiveGameplayEffect │  │
│  │ (属性集)      │◄──│   ity[]      │   │      List (GE池)     │  │
│  │ Health, Atk…  │   │ (能力列表)   │   │                      │  │
│  └───────┬──────┘   └──────┬───────┘   │ Duration/Infinite GE │  │
│          │                 │            └──────────────────────┘  │
│          │  PostGEExecute  │  CanActivate                         │
│          ▼                 ▼            ┌──────────────────────┐  │
│  ┌──────────────┐   ┌──────────────┐   │ GameplayEffectSpec   │  │
│  │ 属性变化通知  │   │ 激活 → Commit│   │ (Modifiers + 上下文) │  │
│  │ → ViewModel   │   │ → 应用 GE    │   │                      │  │
│  └──────────────┘   └──────────────┘   └──────────────────────┘  │
│                                                                   │
│  ┌───────────────────────────────────────────────────────────┐   │
│  │ AbilityActorInfo                                          │   │
│  │  ┌──────────┐  ┌──────────┐  ┌────────────┐              │   │
│  │  │ OwnerActor│  │AvatarActor│  │PlayerController│         │   │
│  │  │ (逻辑拥有)│  │(执行实体) │  │              │          │   │
│  │  └──────────┘  └──────────┘  └────────────┘              │   │
│  └───────────────────────────────────────────────────────────┘   │
└───────────────────────────────────────────────────────────────────┘
```

### ASC 为什么是「中枢」？

打开 `HSRCharacterBase.cpp` 构造函数：

```cpp
AbilitySystemComponent = CreateDefaultSubobject<UHSRAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
AbilitySystemComponent->SetIsReplicated(false);
AbilitySystemComponent->SetComponentTickEnabled(false);
```

ASC 是一个 `ActorComponent`，但它的意义远超普通组件。引擎源码 `AbilitySystemComponent.h` 中核心成员变量（概念性）：

```
ASC 内部维护的数据：
├── TArray<FGameplayAbilitySpec>          ActivatableAbilities   // 可激活的能力列表
├── FActiveGameplayEffectsContainer       ActiveGameplayEffects  // 当前生效的 GE 池
├── TArray<UAttributeSet*>                SpawnedAttributes      // 拥有的属性集
├── FGameplayTagContainer                 BlockedAbilityTags      // Tag 交互
├── FGameplayTagContainer                 OwnedGameplayTags
├── FOnGameplayAttributeValueChange       委托                    // 属性变化发布
└── TSharedPtr<FGameplayAbilityActorInfo>  AbilityActorInfo       // Owner/Avatar 信息
```

**所有 GAS 相关的状态和操作都聚合在 ASC 上**：

```
├── 注册能力 → GiveAbility() / GrantAbility()
├── 应用 GE  → ApplyGameplayEffectSpecToSelf() / ToTarget()
├── 读取属性 → GetNumericAttribute() / GetSet<UHSRCoreAttributeSet>()
├── 绑定变化 → RegisterGameplayAttributeEvent()
├── Tag 查询 → HasAnyMatchingGameplayTags()
├── Cue 触发 → ExecuteGameplayCue()
└── 信息记录 → AbilityActorInfo（OwnerActor + AvatarActor）
```

**设计哲学**：普通的 UE 设计中，一个 `ACharacter` 可能有 `UHealthComponent`、`UEnergyComponent`、`UBuffComponent`、`USkillComponent`……每新增一个系统就多加一个组件。GAS 的做法是——**用一个 ASC 作为所有属性/效果/能力的行为总线**。AttributeSet 只管定义属性，GameplayAbility 只管定义行为，GameplayEffect 只管定义数据变化——**它们彼此不感知，全通过 ASC 来编排**。

---

## 1.2 InitAbilityActorInfo——最重要的初始化

`HSRCharacterBase.cpp` 第 63 行：

```cpp
AbilitySystemComponent->InitAbilityActorInfo(this, this);
```

两个参数都是 `this`——Owner 和 Avatar 都是 Character 自身。

### Owner vs Avatar 的区别

| 概念 | OwnerActor | AvatarActor |
|---|---|---|
| 语义 | ASC 的逻辑拥有者 | 在世界上执行 GAS 行为的实体 |
| 谁持有 ASC？ | **是**（ASC 在 PlayerState 上时，Owner=PlayerState） | 不持有 ASC |
| 典型拆分 | PlayerState | 角色的 Character/Pawn |
| 为什么不合并？ | PlayerState 在地图切换时不销毁；Pawn 会重生 | — |

**注意**：`this, this` 是简化模式，不是标准模式。多人游戏中 ASC 通常放在 `PlayerState`（作为 Owner），Character 作为 Avatar。这样角色死亡重生时，`InitAbilityActorInfo` 只换 Avatar，已应用的 GE 和属性值不会丢失。

引擎源码 `InitAbilityActorInfo` 的伪码示意：

```cpp
void UAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
    OwnerActor = InOwnerActor;
    AvatarActor = InAvatarActor;
    AbilityActorInfo = MakeShared<FGameplayAbilityActorInfo>();
    AbilityActorInfo->InitFromActor(InOwnerActor, InAvatarActor, this);
    // 通知已激活能力：ActorInfo 刷新了
    for (FGameplayAbilitySpec& Spec : ActivatableAbilities)
        if (Spec.Ability && Spec.Ability->GetInstancingPolicy() == InstancedPerActor)
            Spec.Ability->OnAvatarSet(ActorInfo, Spec);
}
```

### `FGameplayAbilityActorInfo` 有什么

从 `GameplayAbilityTypes.h` 源码中可以看到：

```
OwnerActor           → TWeakObjectPtr<AActor>         // 谁拥有 ASC
AvatarActor          → TWeakObjectPtr<AActor>         // 能力的物理执行实体
PlayerController     → TWeakObjectPtr<APlayerController>  // 从 Avatar 的 GetController() 获得
SkeletalMeshComponent → USkeletalMeshComponent*       // 播放蒙太奇用
AnimInstance         → UAnimInstance*                 // 动画实例
```

注意：`PlayerController` 是 ASC 在初始化时**从 `AvatarActor->GetController()` 自己取的**，不是由 Controller 系统塞进来的。

---

## 1.3 完整的 GAS 数据流链路

```
构造期创建 ASC/AttributeSet
    → 运行期 InitAbilityActorInfo
        → ASC 应用 GE (ApplyGameplayEffectSpecToSelf)
            → 验证 Target Tag 要求
            → 如果 Duration > 0: 加入 ActiveGameplayEffects 池
            → 如果 Duration == 0 (Instant): 立即执行 Modifiers
                → 遍历所有 Modifier: 找到目标 Attribute, 执行运算
                → PreAttributeChange() —— Clamp 预测
                → 设置新值
                → PostGameplayEffectExecute() —— 仅在 Instant GE
                → 广播 OnAttributeChange 委托
                    → ViewModel → Widget
```

**关键回调时序**：

1. **`PreAttributeChange()`** — 你的 `HSRCoreAttributeSet` 中所有属性的 clamp 都在这里。属性**即将**改变，用于钳位预测。
2. **`PostGameplayEffectExecute()`** — 你的 `HSRCoreAttributeSet` 中 `IncomingDamage → Health` 的处理。只在 `Instant` GE 执行。
3. **`FOnGameplayAttributeValueChange`** — 你的 ViewModel 绑定的是这个。属性**已经**改变后的通知，用于 UI 更新。

### GameplayEffectSpec 的诞生

`HSRCharacterBase::ApplyInitialAttributes()` 第 101-109 行：

```cpp
FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(InitialAttributesEffect, 1.0f, Context);
FActiveGameplayEffectHandle Handle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
```

后台链路：

```
ASC->MakeEffectContext()
    → new FGameplayEffectContext (项目中重写为 FHSRDamageEffectContext)
    → 设置 Instigator, EffectCauser 等

ASC->MakeOutgoingSpec(GE_Class, Level, Context)
    → new FGameplayEffectSpec(GE_Class)
    → 复制所有的 Modifiers, Tags, Duration
    → 在 SetContext 中传递自定义上下文

ASC->ApplyGameplayEffectSpecToSelf(Spec)
    → 验证 Target Tag 要求
    → Duration > 0 → ActiveGE 池 ／ Duration == 0 → 立即执行 Modifiers
        → 遍历 Modifier: 找目标 Attribute, 执行运算 (Add/Multiply/Override)
        → PreAttributeChange → 设新值 → PostGameplayEffectExecute → 广播委托
```

---

## 1.4 Instant vs Duration vs Infinite 深度解析

源码在 `GameplayEffect.h` 第 661-671 行：

```cpp
/** Gameplay effect duration policies */
UENUM()
enum class EGameplayEffectDurationType : uint8
{
    Instant,      // 立即执行，不进 ActiveGameplayEffects 池
    Infinite,     // 永远持续，进了池但不设到期时间
    HasDuration   // 有限持续时间，进了池且有到期时间
};
```

### 关键行为差异

核心源码在 `AbilitySystemComponent.cpp` 第 876-954 行。`ApplyGameplayEffectSpecToSelf` 中：

**Instant（第 950-953 行）— 不进池，直接执行 Modifiers：**

```cpp
else if (Spec.Def->DurationPolicy == EGameplayEffectDurationType::Instant)
{
    ExecuteGameplayEffect(*OurCopyOfSpec, PredictionKey);
}
```

`ExecuteGameplayEffect()` → 到 `GameplayEffect.cpp` 第 2945-3062 行的 `ExecuteActiveEffectsFrom()`：
- 计算 Modifier Magnitudes
- 遍历 Modifiers，对每个调用 `InternalExecuteMod()` → `PreGameplayEffectExecute` → `ApplyModToAttribute` → `PostGameplayEffectExecute`
- 运行 Executions（自定义 ExecCalc，如你项目的 `UHSRDamageExecutionCalculation`）

**Duration/Infinite（第 876-899 行）— 进池，通过 Aggregator 工作：**

```cpp
if (Spec.Def->DurationPolicy != EGameplayEffectDurationType::Instant || bTreatAsInfiniteDuration)
{
    AppliedEffect = ActiveGameplayEffects.ApplyGameplayEffectSpec(Spec, PredictionKey, bFoundExistingStackableGE);
}
```

- `ApplyGameplayEffectSpec()` 把 GE 注册到 `FActiveGameplayEffectsContainer` 中
- Duration 会设一个 `FTimerManager` 定时器，到期调 `CheckDurationExpired`
- Infinite 的 `Duration` 为 `FGameplayEffectConstants::INFINITE_DURATION`（-1.0f），永不到期
- Modifiers 不立即执行——它们通过 **Aggregator 系统**实时计算当前值
- 引用 `GameplayEffect.cpp` 第 2969-2970 行的注释：
  ```
  // Check tag requirements. This code path is for Instant & Periodic effects.
  // Duration effects are a separate code path; they use aggregators and their
  // requirements checks are in FAggregatorMod::UpdateQualifies.
  ```

**预测情况（第 863 行）：**

```cpp
bool bTreatAsInfiniteDuration = GetOwnerRole() != ROLE_Authority
    && PredictionKey.IsLocalClientKey()
    && Spec.Def->DurationPolicy == EGameplayEffectDurationType::Instant;
```

客户端预测 Instant 效果时，会**临时把它当成 Infinite 处理**，等待服务器确认后才真正生效。

### 对比总结

| 方面 | Instant | HasDuration | Infinite |
|---|---|---|---|
| **进 ActiveGE 池** | ❌ | ✅ | ✅ |
| **Modifier 机制** | 直接调用 `InternalExecuteMod` 修改 Base 值 | 通过 Aggregator | 通过 Aggregator |
| **到期定时器** | 无 | `FTimerManager` 定时 | 无 |
| **手动移除** | 不需要 | 可中途移除 | 必须手动移除 |
| **Stacking** | 不支持 | 支持 | 支持 |
| **Periodic** | 不可能 | 可以 | 可以 |
| **Pre/PostGameplayEffectExecute** | ✅ 走 | ❌ 不走 | ❌ 不走 |
| **GameplayCue OnActive/WhileActive** | ❌ 没有 | ✅ 有 | ✅ 有 |

### InternalExecuteMod 源码解析

`GameplayEffect.cpp` 第 3907-3971 行：

```cpp
bool FActiveGameplayEffectsContainer::InternalExecuteMod(FGameplayEffectSpec& Spec, FGameplayModifierEvaluatedData& ModEvalData)
{
    UAttributeSet* AttributeSet = Owner->GetAttributeSubobject(ModEvalData.Attribute.GetAttributeSetClass());

    if (AttributeSet)
    {
        FGameplayEffectModCallbackData ExecuteData(Spec, ModEvalData, *Owner);

        if (AttributeSet->PreGameplayEffectExecute(ExecuteData))  // ← 可以拒绝此次修改
        {
            float OldValueOfProperty = Owner->GetNumericAttribute(ModEvalData.Attribute);
            ApplyModToAttribute(ModEvalData.Attribute, ModEvalData.ModifierOp, ModEvalData.Magnitude, &ExecuteData);
            // ↑ 修改 Attribute 的 Base 值

            FGameplayEffectModifiedAttribute* ModifiedAttribute = Spec.GetModifiedAttribute(ModEvalData.Attribute);
            if (!ModifiedAttribute)
                ModifiedAttribute = Spec.AddModifiedAttribute(ModEvalData.Attribute);
            ModifiedAttribute->TotalMagnitude += ModEvalData.Magnitude;

            AttributeSet->PostGameplayEffectExecute(ExecuteData);  // ← 你的 IncomingDamage→Health 就在这里
            bExecuted = true;
        }
    }
    return bExecuted;
}
```

---

## 1.5 Context vs Spec vs Handle

这是学习 GAS 最容易混淆的概念之一，它们的职责完全不同。

### FGameplayEffectContext（`GameplayEffectTypes.h` 第 245 行）

```cpp
struct FGameplayEffectContext     // ← 这是 USTRUCT，不是 UObject
{
    TWeakObjectPtr<AActor> Instigator;                         // 谁发起的
    TWeakObjectPtr<AActor> EffectCauser;                       // 物理上造成的实体（武器、子弹）
    TWeakObjectPtr<UGameplayAbility> AbilityCDO;               // 能力 CDO（可复制）
    TWeakObjectPtr<UGameplayAbility> AbilityInstanceNotReplicated; // 能力实例（不复制）
    int32 AbilityLevel;                                        // 等级
    TWeakObjectPtr<UObject> SourceObject;                      // 来源对象
    TWeakObjectPtr<UAbilitySystemComponent> InstigatorAbilitySystemComponent;
    // ... Instigator, HitResult, WorldOrigin, Actors…
};
```

**Context = "谁干的、怎么干的"**。记录来源信息。因为是 `TSharedPtr` 传递，多个 Spec 可以共享同一个 Context。

### FGameplayEffectContextHandle（第 490 行）

```cpp
struct FGameplayEffectContextHandle    // 也是 USTRUCT
{
    TSharedPtr<FGameplayEffectContext> Data;  // 智能指针包裹多态的 Context
};
```

**Handle = 智能指针包装**。Context 是 `USTRUCT` 不走 GC，所以用 `TSharedPtr` 做引用计数。Handle 实现了 `NetSerialize`，能正确序列化多态的 Context 子类（你项目的 `FHSRDamageEffectContext`）。

### FGameplayEffectSpec（`GameplayEffect.h` 第 988 行）

```
struct FGameplayEffectSpec
{
    UGameplayEffect* Def;                            // GE 资产 CDO（只读模板）
    FGameplayEffectContextHandle EffectContext;      // 关联的 Context（谁干的）
    float Duration;                                  // 实例化的持续时间
    float Period;                                    // 实例化的周期
    TArray<FModifierSpec> Modifiers;                 // 运行时 Modifier 值（从 Def 复制）
    FGameplayTagContainer DynamicGrantedTags;        // 运行时 Tag
    int32 StackCount;
    FGameplayEffectAttributeCaptureSpecContainer CapturedRelevantAttributes;
    // ...
};
```

**Spec = "这个 GE 的具体实例"**。同一个 GE 资产可以衍生出无数个 Spec，每个 Spec 有不同的 Magnitude、Duration、SetByCaller 值。

### 类比

```
GE 资产 (UGameplayEffect CDO)  →  蓝图里的模板
  │
  ├── Context: "A 角色用 BasicAttack 发出的"  (被多个 Spec 共享)
  │
  ├── Spec 1: "对 B 角色, 伤害 50, 持续 3 秒"
  └── Spec 2: "对 C 角色, 伤害 100, 持续 5 秒"
```

### 项目中为什么重写 Context

你的 `UHSRAbilitySystemGlobals`（`HSRAbilitySystemGlobals.cpp`）：

```cpp
FGameplayEffectContext* UHSRAbilitySystemGlobals::AllocGameplayEffectContext() const
{
    return new FHSRDamageEffectContext();  // 包含 DamageContext, DamageResult 等自定义数据
}
```

每当你调 `ASC->MakeEffectContext()`，实际创建的就是 `FHSRDamageEffectContext`——里面装着伤害公式的输入（`FHSRDamageContext`）和输出（`FHSRDamageResult`），一路跟着 Spec 传到 ExecCalc。

---

## 1.6 Possess 与 AvatarActor——两个世界的彻底分离

### Controller → Pawn vs ASC → AvatarActor

```
┌───────────────────────────────────────┐
│ UE 游戏框架中的两个独立关系            │
│                                       │
│  [关系一] Controller → Pawn           │
│  ├── "谁控制谁"                      │
│  ├── 属于 PlayerController/AIController│
│  ├── Possess/UnPossess 改变           │
│  ├── 影响输入路由、视角、网络归属      │
│  └── Pawn->Controller 值变化          │
│                                       │
│  [关系二] ASC → AvatarActor           │
│  ├── "GAS 在哪执行"                   │
│  ├── 属于 GameplayAbilities 系统       │
│  ├── InitAbilityActorInfo 设置         │
│  ├── 影响蒙太奇播放位置、Cue 触发位置  │
│  └── FGameplayAbilityActorInfo->       │
│       AvatarActor 值变化              │
└───────────────────────────────────────┘
```

两个系统之间唯一的连接点是：`InitAbilityActorInfo` 内部会**从 Avatar 身上调用 `GetController()`**，把结果存入 `FGameplayAbilityActorInfo::PlayerController`。但这只是信息拷贝，不是绑定。

### 为什么你的 Re-Possess 测试通过了？

看 `HSRCharacterBase::RequestPhase2Repossess()` 第 212-213 行：

```cpp
OriginalController->UnPossess();
OriginalController->Possess(this);
```

- `UnPossess`：`Pawn->Controller = nullptr`（暂时断开）
- `Possess(this)`：`Pawn->Controller = OriginalController`（重新连回）
- **Character 这个 C++ 对象从来就没被销毁过**
- `InitAbilityActorInfo` 在 `BeginPlay` 时设置 `AvatarActor = this`
- Re-Possess 不会重新调 `InitAbilityActorInfo`
- 所以 `AvatarActor` 指向的仍然是同一个 `this`（同一个 Character 对象）
- `FGameplayAbilityActorInfo::PlayerController` 因为 `GetController()` 返回了新的 Controller，所以更新了

**Re-Possess 只改变了 Controller→Pawn 关系，没破坏 ASC→AvatarActor 关系**，所以一切正常。

### 关卡旅行时呢？

这是两者的关键区别：

```
地图旅行发生时：
  1. 当前 World 被卸载
  2. 当前所有 Actor 被 GC 销毁
  3. 新 World 加载，创建新 Actor

  如果 ASC 在 Character 上（你的模式）：
    → Character 被销毁 → ASC 被销毁 → AttributeSet 被销毁
    → 在新地图重新 BeginPlay → 重新 InitAbilityActorInfo
    → 所有属性重置回初始值
    → **关卡变了 AvatarActor 确实会变**——因为 Character 都换了

  如果 ASC 在 PlayerState 上（标准多人模式）：
    → Character 被销毁 → PlayerState 存活（继承自 AInfo）
    → ASC 在 PlayerState 上 → AttributeSet 存活
    → 新 Character 被创建 → InitAbilityActorInfo(PlayerState, NewCharacter)
    → Owner 不变, Avatar 变成新 Character
    → **属性保留**
```

### 一句话总结

- **Re-Possess 不会销毁 Character** → `AvatarActor` 保持不变
- **关卡旅行会销毁 Character** → `AvatarActor` 随 Character 一起消失
- 如果 ASC 在 PlayerState 上，`OwnerActor` 不随关卡旅行改变，属性、GE、能力全部保留

---

## 1.7 PlayerState 是什么

源码路径：`Engine\Source\Runtime\Engine\Classes\GameFramework\PlayerState.h`

```cpp
/**
 * A PlayerState is created for every player on a server (or in a standalone game).
 * PlayerStates are replicated to all clients, and contain network game relevant
 * information about the player, such as playername, score, etc.
 */
UCLASS(BlueprintType, Blueprintable, notplaceable, MinimalAPI)
class APlayerState : public AInfo
{
    UPROPERTY(Replicated) float Score;           // 得分
    UPROPERTY(Replicated) int32 PlayerId;        // 网络 ID
    UPROPERTY(Replicated) uint8 CompressedPing;  // 延迟
    // ...
};
```

关键特征：

| 特性 | 说明 |
|---|---|
| **跨地图存活** | 继承自 `AInfo`，地图旅行不销毁 |
| **网络复制** | 所有客户端都能看到 |
| **一个玩家一个** | 从创建到结束，唯一且稳定 |
| **无物理** | 没有碰撞、没有网格 |
| **单机也有** | 即使单机，UE 也会创建 PlayerState |

在 GAS 的标准多人模式中，ASC 放在 PlayerState 上是因为角色的 Character 会因死亡/重生而销毁，但玩家不会被销毁。你的单机项目放在 Character 上，`this, this` 完全合理——只要你知道标准设计是为了什么。

---

## 1.8 本书架架构决策

| 特征 | 项目选择 | UE 常规推荐 | 理由 |
|---|---|---|---|
| ASC 存放位置 | Character | PlayerState（多人） | 单机、无网络复制 |
| Owner==Avatar | `this, this` | 通常分离 | 同上 |
| 属性初始化 | 一个 GE 在 BeginPlay | GE 或 HardCoded | 开发者友好 |
| 敌人 ASC | 战斗中动态创建 | 同玩家 | 敌人不存在于探索世界 |
| Tick | 关闭 | 关闭 | 纯事件驱动 |

---

## 1.9 练习回顾

1. **在源码中追踪**（引擎源码路径已确认）：
   - `GameplayAbilityTypes.h` → `FGameplayAbilityActorInfo` 的结构体字段
   - `AbilitySystemComponent.h` → `InitAbilityActorInfo` 的声明和注释
   - `GameplayEffect.h` → `EGameplayEffectDurationType`, `UGameplayEffect::Modifiers`

2. **在你的项目里打断点**：
   - `HSRCharacterBase::BeginPlay` 第 63 行 → 观察 `AbilityActorInfo` 三个字段的值
   - `HSRCoreAttributeSet::PreAttributeChange` → 观察属性变化前的 clamp
   - `HSRCoreAttributeSet::PostGameplayEffectExecute` → 观察 `IncomingDamage` 转移

3. **思考题答案**：
   > 如果把 `InitAbilityActorInfo` 的两个 `this` 换成 `(GetOwner(), this)`，且在构造时把 Owner 设为 PlayerState——同样的 GE 应用链**不会失败**。
   > 
   > 原因：`MakeEffectContext()` 只依赖 ASC，`MakeOutgoingSpec()` 只复制 GE 数据，`ApplyGameplayEffectSpecToSelf()` 最终调用的 `InternalExecuteMod()` 通过 `GetAttributeSubobject()` 在 ASC 的 `SpawnedAttributes` 中查找 AttributeSet。**只要 ASC 存在且 AttributeSet 注册在 ASC 上，Owner 是 PlayerState 还是 Character 不改变 GE 的执行逻辑**。
   >
   > 这正是 PlayerState 方案的优势：OwnerActor 不变，AvatarActor 随重生变化，属性和 GE 保留。

---

# 第 2 站：GameplayAbility 深度

## 2.1 什么是 GameplayAbility

> 引擎源码：`.../GameplayAbilities/Public/Abilities/GameplayAbility.h`

```cpp
/** GameplayAbility 定义了一个角色的"能力"——一个可激活的行为。
 *  它包含了激活条件、价格、冷却、执行逻辑等。
 *  它不包含属性——属性在 AttributeSet 中。
 *  它不包含直接的数据修改——数据修改通过 GameplayEffect 完成。
 */
```

**核心设计原则**：Ability 是"行为脚本"——它知道你"想做什么"、你能不能做、怎么做——但它自己不持有数据。所有属性读取和修改都通过 ASC 中转。

---

## 2.2 激活链路图

```
GiveAbility(AbilityClass)
  │ 在 ASC 中注册一个 FGameplayAbilitySpec
  ▼
TryActivateAbility(Handle)
  │ 用户或代码触发了激活请求
  ▼
CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)
  │ ① AvatarActor 有效?
  │ ② ASC 有效、不处于 Inhibited 状态?
  │ ③ Cooldown 检查?
  │ ④ Cost 检查?
  │ ⑤ Tag 要求检查 (BlockingTags, RequiredTags, CancelTags, IgnoreTags)?
  │ ⑥ K2_CanActivateAbility (BP 覆盖)?
  ▼  (任一失败 → 返回 false)
ActivateAbility(Handle, ActorInfo, ActivationInfo, EventData)
  │ 这里有三种写法：
  │ ① 同步执行（你的 BasicAttack, Skill）：直接 Apply GE，然后 EndAbility
  │ ② 异步执行（最典型）：启动 AbilityTask 等待事件
  │ ③ 混合执行
  ▼
CommitAbility(Handle, ActorInfo, ActivationInfo)
  │ ① CommitCheck (再次检查 Cost + Cooldown)
  │ ② CommitExecute (应用 Cost + Cooldown)
  │ ③ K2_CommitExecute (BP 事件)
  │ ④ NotifyAbilityCommit (广播)
  ▼
[AbilityTask 异步执行] (可选)
  │ 例如等待动画播放完 → 在回调中 Apply GE
  ▼
EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled)
```

---

## 2.3 CanActivateAbility 详细检查链

> 源码路径：`GameplayAbilities\Private\GameplayAbility.cpp`

函数签名：

```cpp
virtual bool CanActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags = nullptr,
    const FGameplayTagContainer* TargetTags = nullptr,
    OUT FGameplayTagContainer* OptionalRelevantTags = nullptr
) const;
```

检查顺序：

```
1. ActorInfo 是否有效, ActorInfo->AvatarActor 是否有效
   → 没有 Avatar，无法执行能力

2. ActorInfo->IsLocallyControlled() 检查 (对于需要本机控制的能力)
   → 网络相关

3. AbilitySystemComponent::GetUserAbilityActivationInhibited() 是否阻挡
   → 用于全局性的阻挡（死亡、对话等）

4. CheckCooldown(Handle, ActorInfo, OptionalRelevantTags)
   → 检查 Cooldown GameplayEffect 是否存在
   → 如果 CD 未过期，返回 false

5. CheckCost(Handle, ActorInfo, OptionalRelevantTags)
   → 检查 Cost GameplayEffect 是否可以应用
   → 如果资源不足（HP 不够、Energy 不够），返回 false

6. DoesAbilitySatisfyTagRequirements(ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)
   → 检查这个能力的 Tag 与目标的 Tag 交互
   → 检查 Owner 身上的 Tag 是否包含 Blocked/Required 等

7. K2_CanActivateAbility() (仅在 bHasBlueprintCanUse 为 true 时)
   → 在蓝图中覆盖额外检查条件
```

---

## 2.4 Tag 交互规则

> 在 `GameplayAbility.h` 中，每个能力包含四个 Tag 容器：

```cpp
/** 如果激活者身上有这个 Tag，能力会被阻塞 */
UPROPERTY(EditDefaultsOnly, Category = "Tags")
FGameplayTagContainer ActivationBlockedTags;

/** 激活者必须具备所有这些 Tag，能力才能激活 */
UPROPERTY(EditDefaultsOnly, Category = "Tags")
FGameplayTagContainer ActivationRequiredTags;

/** 激活时，会自动取消激活者身上这些 Tag 对应的能力 */
UPROPERTY(EditDefaultsOnly, Category = "Tags")
FGameplayTagContainer CancelAbilitiesWithTag;

/** 激活时需要忽略的 Tag（用于父类能力的继承排除） */
UPROPERTY(EditDefaultsOnly, Category = "Tags")
FGameplayTagContainer BlockedAbilityTags;
```

**源码中 `DoesAbilitySatisfyTagRequirements` 的逻辑**（`GameplayAbility.cpp`）：

检查顺序：
```
① 如果 Owner 有任何 ActivationBlockedTags 中的 Tag → 失败
② 如果 Owner 没有所有 ActivationRequiredTags 中的 Tag → 失败
③ 如果有 SourceTags（来源目标）且与 SourceBlockedTags 重叠 → 失败
④ 如果有 TargetTags（目标）且与 TargetBlockedTags 重叠 → 失败
⑤ 如果有 CancelAbilitiesWithTag → 在激活前取消对应能力
```

---

## 2.5 Commit 机制——资源消耗的正式路径

`CommitAbility` 源码在 `GameplayAbility.cpp`：

```cpp
bool UGameplayAbility::CommitAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    OUT FGameplayTagContainer* OptionalRelevantTags)
{
    // 最后一次检查
    if (!CommitCheck(Handle, ActorInfo, ActivationInfo, OptionalRelevantTags))
        return false;

    // 正式提交
    CommitExecute(Handle, ActorInfo, ActivationInfo);

    // BP 事件
    K2_CommitExecute();

    // 广播
    NotifyAbilityCommit(ActorInfo);

    return true;
}
```

`CommitExecute` 内部：

```cpp
void UGameplayAbility::CommitExecute(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo)
{
    // 应用 Cooldown GE
    CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, false);
    // 应用 Cost GE
    CommitAbilityCost(Handle, ActorInfo, ActivationInfo, false);
}
```

关键理解：
- Cost 和 Cooldown 在 GAS 中都是 **GameplayEffect**
- `CommitAbilityCooldown` = 应用一个 Duration GE（冷却期间该能力被阻挡）
- `CommitAbilityCost` = 应用一个 Instant GE（扣除 HP、Energy 等）
- 所以你在 `HSRUltimateAbility` 中看到的 Energy Cost/Refund 模式是：
  ```
  CommitAbility → 自动应用 Cost GE（扣除 Energy）
  → 如果伤害执行失败 → 手动应用 Refund GE（加回 Energy）
  ```

---

## 2.6 实例化策略（Instancing Policy）

在 `GameplayAbility.h` 中的枚举：

```cpp
UENUM()
enum class EGameplayAbilityInstancingPolicy : uint8
{
    NonInstanced,        // 使用 CDO 本身，不能有状态（性能最优）
    InstancedPerActor,   // 每个 ASC 一个实例（你的项目使用的）
    InstancedPerExecution // 每次激活创建一个新实例
};
```

你的 `UHSRGameplayAbilityBase` 构造函数（在 `HSRGameplayAbilityBase.cpp` 中）使用的是 `InstancedPerActor`：

```cpp
UHSRGameplayAbilityBase::UHSRGameplayAbilityBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    // ...
}
```

这意味着：
- 每个 ASC（每个角色）对每种能力只创建一个实例
- 每次激活时都使用同一个实例对象
- 实例中的状态变量（你项目中的 `PreparedFormalDamage`、`ActionId`、`SkillId` 等）**在一次激活后必须清干净**
- 你的 `ClearPreparedTarget()` 和 `ClearPreparedFormalDamage()` 就是这个作用

---

## 2.7 AbilityTask——异步能力的基础

> 源码路径：`.../GameplayAbilities/Public/Abilities/Tasks/`

对于异步能力（播放动画 → 等待完成 → 应用效果），GAS 提供了 `AbilityTask`：

```cpp
UCLASS(abstract)
class UAbilityTask : public UGameplayTask
{
    GENERATED_BODY()
    // 可以等待：动画播放完、目标数据到达、延迟、Tag 变化、GE 应用…
};
```

你项目中的 4 个能力都是**同步**行为——激活后立即 `ApplyPreparedFormalDamage` 然后 `EndAbility`。但在更复杂的技能中，你会用到：

| Task | 作用 |
|---|---|
| `AbilityTask_PlayMontageAndWait` | 播放蒙太奇 → 等待完成后触发回调 |
| `AbilityTask_WaitTargetData` | 等待玩家选择目标 → 返回目标数据 |
| `AbilityTask_WaitDelay` | 等待指定时间 |
| `AbilityTask_WaitGameplayTagAdded` | 等待某个 Tag 出现 |
| `AbilityTask_WaitGameplayEffectApplied` | 等待某个 GE 被应用到目标 |

**典型异步能力模式**：

```
ActivateAbility
  → PlayMontageAndWait("攻击动画")
    → OnCompleted 回调:
      → ApplyGameplayEffectToTarget (伤害 GE)
      → EndAbility
```

---

## 2.8 你项目中的 4 个能力子类对比

| 能力 | 文件 | 类别 | 行为模式 | Commit? | 确认执行 |
|---|---|---|---|---|---|
| `UHSRBasicAttackAbility` | `Ability/HSRBasicAttackAbility.h` | 基础攻击 | 同步：Prepare → Apply | 不 Commit | 直接 ApplyPreparedFormalDamage |
| `UHSRSkillAbility` | `Ability/HSRSkillAbility.h` | 战技 | 同步：Prepare → Apply，消耗 1 SP | 不 Commit（SP 由 Coordinator 管理） | 同 |
| `UHSRUltimateAbility` | `Ability/HSRUltimateAbility.h` | 终结技 | 同步：Commit → Prepare → Apply → Refund | ✅ CommitAbility | 失败时手动 Apply Refund |
| `UHSRHealAbility` | `Ability/HSRHealAbility.h` | 治疗 | 同步：Prepare 治疗 GE → Apply | 不 Commit | 直接 Apply 到目标 |

**关键区别在 Ultimate**：

`HSRUltimateAbility` 中：
```cpp
// 1. 先 Commit（扣 Energy）
if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    return false;

// 2. PrepFormalDamage（准备伤害 Spec）
PrepareFormalDamage(...);

// 3. Apply（应用伤害）
bool bApplied = ApplyPreparedFormalDamage(SourceASC);

// 4. 如果失败 → 退还 Energy
if (!bApplied)
{
    // Apply Refund GE
    SourceASC->ApplyGameplayEffectSpecToSelf(*RefundSpec.Data);
}
```

这是典型的设计模式：**所有消耗在 Commit 中前扣，失败时必须回滚**。

---

## 2.9 在项目源码中追踪

打开以下文件对照阅读：

1. **打开 `HSRGameplayAbilityBase.h/.cpp` 和 4 个子类**
   - 看看 `SetPendingTarget` → `PrepareFormalDamage` → `ApplyPreparedFormalDamage` 的调用链
   - 注意 `PreparedFormalDamage` 结构体如何保证"准备一次，应用一次"

2. **打开引擎源码 `GameplayAbility.h` 和 `GameplayAbility.cpp`**
   - 找到 `CanActivateAbility` 的完整检查顺序
   - 找到 `CommitAbility` 的 Cooldown/Cost 应用逻辑
   - 找到 `EGameplayAbilityInstancingPolicy` 的定义

3. **在你的项目里打断点看看激活链路：**
   - `HSRGameplayAbilityBase::SetPendingTarget` → 看 ActionId 如何传递
   - `HSRBasicAttackAbility::ActivateAbility` → 看同步能力如何直接 EndAbility
   - `HSRUltimateAbility::ActivateAbility` → 看 Commit 前后的 Energy 变化

---

---

# 第 3 站：GameplayEffect 与执行管道

> 核心源码文件：
> - `GameplayEffect.h` — GE 数据结构、ModifierInfo、DurationPolicy
> - `GameplayEffectTypes.h` — EGameplayModOp 聚合公式、ModifierEvaluatedData
> - `GameplayEffect.cpp` — 执行实现
> - `GameplayEffectAggregator.h/.cpp` — 属性聚合器
> - **你的项目**: `HSRDamageExecutionCalculation.h/.cpp`、`HSRDamageTypes.h`、`HSRDamageEffectContext.h/.cpp`

## 3.1 GameplayEffect 的本质

```
GameplayEffect = 属性变化的声明式描述
├── DurationPolicy (Instant / HasDuration / Infinite)
├── Modifiers[] ─── 对 Attribute 的数值变化声明
│   ├── Attribute (目标属性)
│   ├── ModifierOp (Add / Multiply 等)
│   └── Magnitude (ScalableFloat / AttributeBased / CustomCalc / SetByCaller)
├── Executions[] ─── 自定义执行逻辑 (如你的 HSRDamageExecutionCalculation)
├── Stacking ─── 堆叠规则 (None / AggregateByTarget / AggregateBySource)
├── Period ─── 周期执行
├── Tags
│   ├── GameplayCues ─── 触发视听反馈
│   ├── GrantedTags ─── GE 生效期间授予的 Tag
│   └── Requirements ─── Activation/Application tag 要求
└── Components[] (UE 5.3+) ─── 各种功能组件代替旧属性字段
```

**GE 是一个资产（`UPrimaryDataAsset` 的子类）**，运行时不修改。每次"应用"GE 时，ASC 会创建一个 `FGameplayEffectSpec` 作为该资产的运行时快照，里面的 Modifier 值可以被运行时重写。

## 3.2 Modifier 聚合公式（EGameplayModOp）

源码在 `GameplayEffectTypes.h` 第 111-137 行：

```cpp
/**
 * ((BaseValue + AddBase) * MultiplyAdditive / DivideAdditive * MultiplyCompound) + AddFinal
 */
namespace EGameplayModOp
{
    enum Type : int
    {
        AddBase,            // 第一步：加到底值上
        MultiplyAdditive,   // 第二步：所有 Additive 乘数相加后乘以当前值 (1.5 + 1.5 → 2.0)
        DivideAdditive,     // 第三步：所有 Additive 除数相加后被当前值除 (2 + 2 → 1/3)
        MultiplyCompound,   // 第四步：所有 Compound 乘数连乘 (1.5 * 1.5 → 2.25)
        Override = 3,       // 直接覆盖（注意数值 3 是为了序列化兼容）
        AddFinal,           // 最后再加一个定值
        Max
    };
}
```

执行公式的逻辑在 `GameplayEffectAggregator.cpp` 的 `EvaluateWithBase` 函数中（简化）：

```
result = BaseValue
result += Σ(AddBase)
result *= Π(1.0 + Σ(MultiplyAdditive))
result /= Π(1.0 + Σ(DivideAdditive))
result *= Π(MultiplyCompound)
result += Σ(AddFinal)
```

### Instant 执行下的简化

对 Instant 效果，`ApplyModToAttribute` 使用的是 `StaticExecModOnBaseValue`（在 `GameplayEffect.cpp` 第 3977 行），这个函数**不是完整聚合**：

```cpp
float FAggregator::StaticExecModOnBaseValue(float BaseValue, TEnumAsByte<EGameplayModOp::Type> ModifierOp, float EvaluatedMagnitude)
{
    switch (ModifierOp)
    {
        case EGameplayModOp::Override:      BaseValue = EvaluatedMagnitude; break;
        case EGameplayModOp::AddBase:
        case EGameplayModOp::AddFinal:      BaseValue += EvaluatedMagnitude; break;
        case EGameplayModOp::MultiplyAdditive:
        case EGameplayModOp::MultiplyCompound: BaseValue *= EvaluatedMagnitude; break;
        case EGameplayModOp::DivideAdditive:
            if (!FMath::IsNearlyZero(EvaluatedMagnitude))
                BaseValue /= EvaluatedMagnitude;
            break;
    }
    return BaseValue;
}
```

注意：Instant 下 **AddBase 和 AddFinal 行为一样（都是 +=）**，**MultiplyAdditive 和 MultiplyCompound 也一样（都是 *=）**。完整的通道区分只在 Duration/Infinite 的 Aggregator 系统中真正生效。

### ModifierInfo 的结构

`GameplayEffect.h` 第 542 行：

```cpp
struct FGameplayModifierInfo
{
    FGameplayAttribute Attribute;                    // 目标属性（如 Attack, Defense）
    TEnumAsByte<EGameplayModOp::Type> ModifierOp;    // 运算类型
    FGameplayEffectModifierMagnitude ModifierMagnitude; // 数值来源
    FGameplayModEvaluationChannelSettings EvaluationChannelSettings;
    FGameplayTagRequirements SourceTags;             // 来源 Tag 要求
    FGameplayTagRequirements TargetTags;             // 目标 Tag 要求
};
```

### ModifierMagnitude 的计算方式

```cpp
enum class EGameplayEffectMagnitudeCalculation : uint8
{
    ScalableFloat,        // 简单的可缩放浮点数 (最常用)
    AttributeBased,       // 基于另一个属性值计算
    CustomCalculationClass, // 自定义 MMC (UGameplayModMagnitudeCalculation)
    SetByCaller,          // 运行时由代码设置 (如你项目的 FinalDamage)
};
```

你项目中的伤害 GE 使用的是 `SetByCaller`——因为 FinalDamage 是在 ExecCalc 中算出来的，通过输出 Modifier 传入：

```cpp
// HSRDamageExecutionCalculation.cpp 第 75 行
OutExecutionOutput.AddOutputModifier(
    FGameplayModifierEvaluatedData(
        UHSRCoreAttributeSet::GetIncomingDamageAttribute(),
        EGameplayModOp::Additive,
        FinalDamage
    )
);
```

而 `IncomingDamage` 是 Meta 属性，在 `PostGameplayEffectExecute` 中被消耗掉。

## 3.3 执行管道总览

当 `ApplyGameplayEffectSpecToSelf` 被调用时，最重要的是看**分支判断逻辑**（在 `AbilitySystemComponent.cpp` 第 876-954 行）：

```
ApplyGameplayEffectSpecToSelf(Spec)
│
├─ Is Instant? ──────────────────────────────────────────────────────┐
│   YES (DurationPolicy == Instant)                                  │
│   → ExecuteGameplayEffect(Spec)                                    │
│     → FActiveGameplayEffectsContainer::ExecuteActiveEffectsFrom()  │
│       ├── 计算所有 Modifier Magnitudes                             │
│       ├── 遍历 Modifiers[]                                         │
│       │   └── InternalExecuteMod(Spec, EvalData)                  │
│       │       ├── PreGameplayEffectExecute(ExecuteData)            │
│       │       ├── ApplyModToAttribute() ← 直接改 Base 值           │
│       │       └── PostGameplayEffectExecute(ExecuteData)           │
│       ├── 遍历 Executions[]                                        │
│       │   └── ExecCDO->Execute(Params, Output)                    │
│       │       └── 你的 HSRDamageExecutionCalculation 就在这里      │
│       │       └── 输出 Modifier → 再次 InternalExecuteMod          │
│       └── 触发 GameplayCue Executed                                │
│                                                                   │
├─ Is Duration or Infinite? ────────────────────────────────────────┤
│   → ActiveGameplayEffects.ApplyGameplayEffectSpec(Spec)            │
│     ├── 创建 FActiveGameplayEffect 条目                            │
│     ├── 注册到 ActiveGameplayEffects 容器                          │
│     ├── Duration 效果：注册 FTimerManager 到期定时器               │
│     ├── Modifiers 不立即执行——注册到 Aggregator                    │
│     └── 触发 GameplayCue OnActive + WhileActive                    │
│                                                                   │
├─ Is Periodic? (Duration/Infinite + Period > 0) ───────────────────┤
│   → 在 ActiveGameplayEffects 中注册 Period 定时器                  │
│   → 每周期触发 ExecuteActiveEffectsFrom() ← 走 Instant 的同一路径  │
│   → 每次执行都是瞬时的（就像 Instant 一样走 Modifiers + Executions）│
└───────────────────────────────────────────────────────────────────┘
```

### InternalExecuteMod 的完整流程

`GameplayEffect.cpp` 第 3907 行：

```cpp
bool FActiveGameplayEffectsContainer::InternalExecuteMod(
    FGameplayEffectSpec& Spec,
    FGameplayModifierEvaluatedData& ModEvalData)
{
    // 1. 找到属性所属的 AttributeSet
    UAttributeSet* AttributeSet = Owner->GetAttributeSubobject(
        ModEvalData.Attribute.GetAttributeSetClass());

    if (!AttributeSet) return false;  // 没有这个属性集 → 跳过

    // 2. 创建回调数据
    FGameplayEffectModCallbackData ExecuteData(Spec, ModEvalData, *Owner);

    // 3. PreGameplayEffectExecute → 可以拒绝
    if (AttributeSet->PreGameplayEffectExecute(ExecuteData))
    {
        // 4. 记录旧值
        float OldValue = Owner->GetNumericAttribute(ModEvalData.Attribute);

        // 5. 修改 Base 值
        ApplyModToAttribute(ModEvalData.Attribute, ModEvalData.ModifierOp,
            ModEvalData.Magnitude, &ExecuteData);

        // 6. 记录变化量到 Spec
        Spec.GetModifiedAttribute(ModEvalData.Attribute)->TotalMagnitude
            += ModEvalData.Magnitude;

        // 7. PostGameplayEffectExecute → 你的 IncomingDamage 处理入口
        AttributeSet->PostGameplayEffectExecute(ExecuteData);

        return true;
    }
    return false;
}
```

## 3.4 ExecutionCalculation（ExecCalc）——自定义执行逻辑

ExecCalc 是 GAS 中最强大的扩展点之一。你不需要 Modifier 的声明式操作，你可以**在 C++ 中完全控制伤害计算**。

### 你的项目的 ExecCalc

`HSRDamageExecutionCalculation`：

```cpp
void UHSRDamageExecutionCalculation::Execute_Implementation(
    const FGameplayEffectCustomExecutionParameters& ExecutionParams,
    FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
    // 1. 从 Spec 中获取自定义 Context
    const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
    FHSRDamageEffectContext* Context =
        static_cast<FHSRDamageEffectContext*>(Spec.GetContext().Get());

    // 2. 从目标捕获属性
    FAggregatorEvaluateParameters Params;
    float Attack, Defense, CritRate, CritDamage;
    ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
        Statics.AttackDef, Params, Attack);
    // ... 捕获 Defense, CritRate, CritDamage

    // 3. 计算伤害
    const float NormalAttack = FMath::Max(0.0f, Attack);
    const float NormalDefense = FMath::Max(0.0f, Defense);
    const bool bCritical = CritRoll < Clamp(CritRate, 0, 1);
    const float RawDamage = Max(MinDamage,
        NormalAttack * AbilityMultiplier - NormalDefense * DefenseCoefficient);
    const float CritMultiplier = bCritical ? 1.0 + CritDamage : 1.0;
    const float FinalDamage = Max(MinDamage, Round(RawDamage * CritMultiplier));

    // 4. 输出结果：加到 IncomingDamage Meta 属性上
    OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
        UHSRCoreAttributeSet::GetIncomingDamageAttribute(),
        EGameplayModOp::Additive,
        FinalDamage));
}
```

### ExecCalc 的运行位置

ExecCalc 在 `ExecuteActiveEffectsFrom` 中运行（`GameplayEffect.cpp` 第 2993-3027 行）：

```cpp
for (const FGameplayEffectExecutionDefinition& CurExecDef : SpecToUse.Def->Executions)
{
    if (CurExecDef.CalculationClass)
    {
        const UGameplayEffectExecutionCalculation* ExecCDO =
            CurExecDef.CalculationClass->GetDefaultObject<...>();

        FGameplayEffectCustomExecutionParameters ExecutionParams(...);
        FGameplayEffectCustomExecutionOutput ExecutionOutput;
        ExecCDO->Execute(ExecutionParams, ExecutionOutput);

        // 把 ExecCalc 输出的 Modifiers 再通过 InternalExecuteMod 执行
        for (FGameplayModifierEvaluatedData& CurExecMod : OutputModifiers)
        {
            InternalExecuteMod(SpecToUse, CurExecMod);
        }
    }
}
```

所以你的 ExecCalc 输出 `IncomingDamage` → `InternalExecuteMod` 被调用 → 你的 `HSRCoreAttributeSet::PostGameplayEffectExecute` 中：

```cpp
// 在 PostGameplayEffectExecute 中：
if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
{
    const float LocalDamageDone = GetHealth() - OldHealth;
    SetHealth(FMath::Clamp(GetHealth() - GetIncomingDamage(), 0.0f, GetMaxHealth()));
    // ...
}
```

**整条计算链路**：
```
Ability 激活
  → ApplyGameplayEffectSpecToTarget(Spec, TargetASC)
    → TargetASC->ApplyGameplayEffectSpecToSelf(Spec)
      → 因为是 Instant → ExecuteGameplayEffect()
        → ExecuteActiveEffectsFrom()
          → 遍历 Modifiers (直接 Modifier)
          → 遍历 Executions
            → HSRDamageExecCalc->Execute()
              → 捕获 Attack/Defense/Crit
              → 计算 FinalDamage
              → 输出 IncomingDamage = FinalDamage (Additive)
              → InternalExecuteMod(IncomingDamage)
                → PreGameplayEffectExecute
                → ApplyModToAttribute(IncomingDamage)   ← 设置 IncomingDamage 值
                → PostGameplayEffectExecute
                  → Health -= IncomingDamage
                  → Clamp Health
                  → 广播 HealthChange 委托
```

## 3.5 捕捉（Attribute Capture）

ExecCalc 不能直接读 Attribute，必须通过**捕捉**机制。捕捉是为了在多人游戏中保证值的一致性（尤其是 snapshot 场景）。

你的 ExecCalc 构造函数注册了捕捉定义：

```cpp
UHSRDamageExecutionCalculation::UHSRDamageExecutionCalculation()
{
    const HSRDamageCapture::FStatics& Statics = HSRDamageCapture::Get();
    RelevantAttributesToCapture.Add(Statics.AttackDef);
    RelevantAttributesToCapture.Add(Statics.CritRateDef);
    RelevantAttributesToCapture.Add(Statics.CritDamageDef);
    RelevantAttributesToCapture.Add(Statics.DefenseDef);
}
```

每个捕捉定义的结构：

```cpp
DECLARE_ATTRIBUTE_CAPTUREDEF(Attack);   // 展开后：
// FGameplayEffectAttributeCaptureDefinition AttackDef;
// 包含: Attribute (Attack), AttributeSource (Source/Target), bSnapshot

DEFINE_ATTRIBUTE_CAPTUREDEF(UHSRCoreAttributeSet, Attack, Source, false);
// Source = 来源方, Target = 目标方
// false = 不 Snapshot（运行时读取当前值）
// true  = Snapshot（在 Spec 创建时就读好值，后续不变）
```

在 `Execute_Implementation` 中通过 `ExecutionParams.AttemptCalculateCapturedAttributeMagnitude()` 获取捕捉的值。

**为什么你项目中的 bSnapshot = false**？因为伤害计算需要在应用瞬间读取当前攻防值。在回合制游戏中不存在多人游戏的时序问题，Snapshot 没有意义。

## 3.6 Duration/Infinite 的 Aggregator 机制

Duration/Infinite 效果**不直接修改 Base 值**——它们通过 `FAggregator` 系统叠加影响。

```
每个 Attribute 有一个 Aggregator：
┌─────────────────────────────────────────────┐
│ FAggregator (for Attack)                    │
│                                             │
│  BaseValue = 100 (直接由 Instant GE 或代码设)│
│                                             │
│  Mods 列表:                                  │
│  ┌─────────────────────────────────────┐   │
│  │ 来自 GE-1 (Buff): AddBase, +20     │   │
│  │ 来自 GE-2 (Debuff): MultiplyAdditive,│   │
│  │  ×0.8                              │   │
│  │ 来自 GE-3 (Buff): AddFinal, +10    │   │
│  └─────────────────────────────────────┘   │
│                                             │
│  Evaluate() =                               │
│    (100 + 20) × 0.8 + 10 = 106             │
└─────────────────────────────────────────────┘
```

当 GE 被移除时，Aggregator 自动重新评估，属性值**回退**到剩余 Mods 的状态。这正是 Buff/Debuff 系统的基础——你的 `UHSRStatusComponent` 通过 Infinite GE 实现状态效果，移除 GE 时属性自动恢复。

## 3.7 堆叠（Stacking）

`GameplayEffect.h` 第 2372 行：

```cpp
/** How this GameplayEffect stacks with other instances of this same GameplayEffect */
UPROPERTY(EditDefaultsOnly, Category = Stacking)
EGameplayEffectStackingType StackingType;  // None / AggregateByTarget / AggregateBySource

/** Stack limit */
int32 StackLimitCount;

/** Duration refresh policy on stack */
EGameplayEffectStackingDurationPolicy StackDurationRefreshPolicy;

/** Period reset policy on stack */
EGameplayEffectStackingPeriodPolicy StackPeriodResetPolicy;

/** Expiration policy on stack */
EGameplayEffectStackingExpirationPolicy StackExpirationPolicy;
```

在你的项目中，`UHSRStatusComponent` 使用了 GAS 的 Stacking 机制：`GetCurrentStackCount()`、`AddOrRefreshStatus` 会通过 `GetActiveGameplayEffect` 检查当前堆叠数，然后决定叠加还是刷新。

## 3.8 周期性（Periodic）

当 GE 设置了 `Period > 0`，ASC 会在每个 Period 间隔调用 `ExecutePeriodicEffect()`：

```cpp
void UAbilitySystemComponent::ExecutePeriodicEffect(FActiveGameplayEffectHandle Handle)
{
    ActiveGameplayEffects.ExecutePeriodicGameplayEffect(Handle);
}
```

这触发 `ExecuteActiveEffectsFrom()`，**和 Instant 走完全相同的 Modifiers/Executions 路径**。所以：

- 周期伤害 = 每 X 秒执行一次 Instant 伤害
- 周期治疗 = 每 X 秒执行一次 Instant 治疗

**你的项目中的 Period 应用场景**：DamageOverTime 类型的 Buff（`Status.Debuff.DamageOverTime`）就是周期性的——每个 Tick 应用一个 Instant 伤害 GE。

## 3.9 你项目中的完整伤害管道映射

```
Coordinator 发起命令
  │
  ▼
HSRBasicAttackAbility::ActivateAbility
  │ SetPendingTarget(TargetASC)
  │ ConfigureFromSkillDefinition(SkillDef)  ← 读 DamageRuleDefinition
  │
  ▼
Ability->PrepareFormalDamage()
  │ 创建 FHSRFormalDamageRequest
  │ 设置 CritRoll, AbilityMultiplier, DefenseCoefficient 等
  │ 把数据写入 Spec 的 FHSRDamageEffectContext
  │
  ▼
Ability->ApplyPreparedFormalDamage(SourceASC)
  │
  ▼
ASC->ApplyGameplayEffectSpecToTarget(Spec, TargetASC)
  │
  ▼
TargetASC->ApplyGameplayEffectSpecToSelf(Spec)
  │ Instant GE → ExecuteGameplayEffect()
  │
  ▼
FActiveGameplayEffectsContainer::ExecuteActiveEffectsFrom(Spec)
  │
  ├── Modifiers: (可能没有，看 GE 定义)
  │
  └── Executions:
      └── HSRDamageExecutionCalculation::Execute_Implementation()
          ├── 获取 FHSRDamageEffectContext (含你的自定义数据)
          ├── 捕捉 Source 的 Attack/CritRate/CritDamage
          ├── 捕捉 Target 的 Defense
          ├── 计算伤害 (确定性公式)
          ├── 填写 DamageResult (含公式追溯 Breakdown)
          └── 输出 IncomingDamage += FinalDamage
              │
              ▼
          InternalExecuteMod(IncomingDamage)
              │
              ▼
          PreGameplayEffectExecute(ExecuteData)  ← 用于前置规则
              │
              ▼
          ApplyModToAttribute(IncomingDamage, Add, FinalDamage) ← 设置值
              │
              ▼
          PostGameplayEffectExecute(ExecuteData)
              │
              ├── Health = Clamp(Health - IncomingDamage, 0, MaxHealth)
              │
              └── (同样处理 IncomingToughnessDamage)
                  │
                  ▼
          广播 OnHealthChanged 委托 → ViewModel → Widget
```

## 3.10 在源码中追踪

1. **打开 `GameplayEffect.h` 查看完整结构**
   - 找到 `EGameplayEffectDurationType` 枚举
   - 找到 `EGameplayModOp::Type` 及其公式注释
   - 找到 `UGameplayEffect` 的所有属性字段（DurationPolicy, Period, Modifiers, Executions 等）

2. **打开 `GameplayEffect.cpp`**
   - 找到 3907 行的 `InternalExecuteMod` → 看 Pre/Post 回调链
   - 找到 2945 行的 `ExecuteActiveEffectsFrom` → 看 Modifiers 和 Executions 的执行顺序
   - 找到 447 行的 `StaticExecModOnBaseValue` → 看 Instant 的简化公式

3. **在你的项目中**
   - `HSRDamageExecutionCalculation::Execute_Implementation` → 完整的伤害计算
   - `HSRCoreAttributeSet::PostGameplayEffectExecute` → IncomingDamage 的处理
   - `HSRGameplayAbilityBase::ApplyPreparedFormalDamage` → 准备和应用的完整事务

4. **思考题**：
   > ExecCalc 输出的 `IncomingDamage` 是通过 Modifier 传递到 AttributeSet 的。如果在同一个 GE 中既有一个直接 Modifier（+50 Health）又有一个 ExecCalc（输出 -100 IncomingDamage），它们的执行顺序是什么？最终 Health 变化是多少？
   >
   > 提示：看 `ExecuteActiveEffectsFrom` 中 Modifiers 循环和 Executions 循环的先后顺序。
