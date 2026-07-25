# C++ 引擎深度

> UE 引擎底层机制：UObject、GC、反射、序列化、容器、智能指针

---

## UObject

### UObject 比普通 C++ 对象多出什么

| 特性 | 普通 C++ 对象 | UObject |
|---|---|---|
| **GC** | 手动 delete 或 shared_ptr | 自动垃圾回收，UPROPERTY 标记的引用会被追踪 |
| **反射** | 无 | 运行时可知所有属性、函数、枚举信息 |
| **CDO** | 无 | 每个 UClass 自动生成一个默认对象 |
| **UClass** | 无 | 每个实例持有 `UClass*` 描述自身类型 |
| **序列化** | 手写 | 自动序列化/反序列化所有 UPROPERTY |
| **子对象系统** | 手动管理 | CreateDefaultSubobject / Outer 链 |
| **网络复制** | 无 | 原生支持 UPROPERTY(Replicated) |

### GC 如何工作

UE 的 GC 基于**标记-清扫（Mark-Sweep）**：

```
① 从根集（Root Set）开始遍历
   根集包括：所有 UObject 全局对象、当前 UWorld 下的 Actor、UPROPERTY 引用的对象

② 标记所有可达对象

③ 清扫所有未被标记的对象（即无人引用的对象）
```

关键：**GC 不回收有 UPROPERTY() 引用的对象**。

```cpp
UPROPERTY() TObjectPtr<UHSRAbilitySystemComponent> ASC;  // GC 知道这个引用
TWeakObjectPtr<AActor> WeakRef;                            // 不阻止 GC
AActor* RawPtr;                                            // GC 不知道这个引用 ← 危险！
```

### UClass / CDO

```cpp
// UClass 是描述 UObject 类型的元类
// UClass 本身也是一个 UObject！
UClass* BPClass = SomeObject->GetClass();
UClass* StaticClass = UHSRCharacterBase::StaticClass();
UClass* Loaded = StaticLoadClass(...);

// CDO — Class Default Object
// 每个 UClass 创建时会自动构造一个默认实例
// 之后所有 new 出来的实例从这个默认实例拷贝初始化
UHSRCharacterBase* CDO = Cast<UHSRCharacterBase>(UHSRCharacterBase::StaticClass()->GetDefaultObject());

// 这就是为什么在蓝图编辑器中改一个属性的默认值
// 只改 CDO，不影响已存在的实例
// 但新 Spawn 的实例会从 CDO 拷贝
```

---

## 反射系统

### 宏的作用链

```cpp
UCLASS()                     → 告诉 UHT：这个类参与反射
class HSR_API UHSRCharacterBase : public UObject
{
    GENERATED_BODY()          → UHT 生成代码的入口点
                              → 没有它，UPROPERTY/UFUNCTION 编译报错
public:
    UPROPERTY(VisibleAnywhere) → 让属性参与 GC、序列化、编辑器显示
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    UFUNCTION(BlueprintCallable) → 暴露给蓝图调用
    void MyFunction();
};
```

### UHT 做了什么

```
MyClass.h
    │  写了 UCLASS() / UPROPERTY() / UFUNCTION()
    │
    ▼
UHT (Unreal Header Tool)
    │  扫描所有带 UE 关键字的头文件
    │  展开 GENERATED_BODY() 的内容
    │  生成 MyClass.generated.h（反射代码）
    │  生成 MyClass.gen.cpp（序列化/反序列化）
    │
    ▼
编译器
    编译 MyClass.h + MyClass.generated.h
    得到完整的反射信息
```

### 反射能做什么

```cpp
// 运行时按名字查找属性
FProperty* HealthProp = GetClass()->FindPropertyByName("Health");
float HealthValue;
HealthProp->GetValue_InContainer(this, &HealthValue);

// 遍历所有属性
for (TFieldIterator<FProperty> It(GetClass()); It; ++It)
{
    UE_LOG(LogTemp, Log, TEXT("Property: %s"), *It->GetName());
}

// 按名字调用函数（不依赖头文件）
UFunction* Func = GetClass()->FindFunctionByName("SomeFunction");
ProcessEvent(Func, &Params);
```

---

## 容器与智能指针

### UE 容器

| 容器 | 特点 | 对比 STL |
|---|---|---|
| `TArray<T>` | 动态数组，紧凑内存 | 类似 vector，但更多内联函数 |
| `TMap<K, V>` | 哈希映射，不可重复 Key | 类似 unordered_map |
| `TSet<T>` | 哈希集合，不可重复元素 | 类似 unordered_set |
| `TQueue<T>` | 线程安全队列 | / |

UE 容器相比 STL 的优势：**集成 GC 和序列化**。

```cpp
// TArray 中存 UObject 指针
UPROPERTY() TArray<TObjectPtr<UObject>> Items;  // GC 知道这些引用
TArray<AActor*> RawItems;                        // GC 不知道 ← 引用可能被回收
```

### UE 智能指针

| 类型 | 用途 | 是否可 UPROPERTY | 关联 GC |
|---|---|---|---|
| `TSharedPtr<T>` | 共享所有权，引用计数 | ❌ | ❌ |
| `TWeakPtr<T>` | 弱引用，不增加计数 | ❌ | ❌ |
| `TUniquePtr<T>` | 独占所有权 | ❌ | ❌ |
| `TWeakObjectPtr<T>` | 弱引用 UObject | ❌ | ✅ 追踪 GC |
| `TObjectPtr<T>` | 自动管理，替代裸指针 | ✅ | ✅ |
| `TStrongObjectPtr<T>` | 阻止 GC | ❌ | ✅ 阻止回收 |

你项目中的典型用法：

```cpp
UPROPERTY() TObjectPtr<UHSRAbilitySystemComponent> AbilitySystemComponent;  // 子对象，阻止 GC
TWeakObjectPtr<UAbilitySystemComponent> Target;  // 不阻止 GC，但知道"对象是否还活着"
TOptional<FHSRStatusInstance> ActiveStatus;       // Optional（不是指针，是值类型 + 有无状态）
```

### TObjectPtr vs TWeakObjectPtr——核心区别

| 类型 | 是否阻止 GC | 可否 UPROPERTY | 用途 |
|---|---|---|---|
| `TObjectPtr<T>` | ✅ 阻止 | ✅ | 子对象、UPROPERTY 成员变量 |
| `TWeakObjectPtr<T>` | ❌ 不阻止 | ❌ | 弱引用、观察者模式 |

**TObjectPtr**——强引用，GC 不会回收。通常用于 `UPROPERTY()` 成员变量：

```cpp
UPROPERTY() TObjectPtr<UHSRAbilitySystemComponent> AbilitySystemComponent;
// 这个 ASC 只要对象还在，GC 就不会回收它
```

如果没有 `UPROPERTY()`，GC 可能认为这个对象"无人引用"而回收，导致崩溃。

**TWeakObjectPtr**——弱引用，GC 可以随时回收。不阻塞销毁：

```cpp
TWeakObjectPtr<AActor> Actor;
// 战斗不拥有角色的生命周期，Actor 可以被正常销毁
```

`TWeakObjectPtr` 可以检测指针是否有效：

```cpp
if (Participant.Actor.IsValid()) { /* 对象还在 */ }
// 不是：if (Participant.Actor != nullptr) ← 这不准确，野指针可能非空但已释放
```

### 你项目中的设计意图

`FHSRBattleParticipant` 用 `TWeakObjectPtr<AActor>` 而不是 `TObjectPtr<AActor>`：

- 参与者由 Battle World（UWorld）管理生命周期
- `FHSRBattleParticipant` 只是观察者——不拥有这些 Actor
- 如果用 `TObjectPtr`，Actors 被 Destroy 后 GC 因为仍有引用而不回收 → **内存泄漏**
- `TWeakObjectPtr` 的设计哲学是：**"我需要知道你在不在，但我不阻止你离开"**

### 其他智能指针

```cpp
// FHSRBattleParticipant 中的用法：
struct FHSRBattleParticipant
{
    TWeakObjectPtr<AActor> Actor;                    // 弱引用——战斗不拥有角色的生命周期
    TWeakObjectPtr<UAbilitySystemComponent> ASC;     // 同上
};

// TWeakObjectPtr 的好处：
// ① 不阻止 GC——当角色死亡被销毁时，Actor 可以被正常回收
// ② 能检测是否还存活：
if (Participant.Actor.IsValid())   // ← 而不是 "if (Participant.Actor != nullptr)"
{
    // Actor 还存在
}
```

如果这里用了 `TObjectPtr<T>`（即强引用），角色即使被 Destroy 也不会被回收——因为 `FHSRBattleParticipant` 仍然持有引用。`TWeakObjectPtr` 设计为"我只想知道你在不在，但我不阻止你离开"。

---

## Actor 生命周期

```
构造
  → 构造函数（C++ 对象创建）
  → CreateDefaultSubobject（创建子对象）
  → PostInitProperties（CDO 初始化完成）
  → BeginPlay（World 中激活，游戏逻辑从这里开始）
  → Tick（每帧更新）
  → EndPlay（即将从 World 中移除）
  → BeginDestroy（开始销毁）
  → FinishDestroy（GC 最终回收）
```

你项目中的顺序：

```cpp
AHSRCharacterBase::AHSRCharacterBase()
{
    // 构造函数：CreateDefaultSubobject，设置默认值
    AbilitySystemComponent = CreateDefaultSubobject<...>(...);
    CoreAttributeSet = CreateDefaultSubobject<...>(...);
}

void AHSRCharacterBase::BeginPlay()
{
    Super::BeginPlay();
    // 运行时初始化：ActorInfo、GE、Delegate
    InitializeAbilityActorInfo();
    ApplyInitialAttributes();
    BindAttributeDelegates();
}

void AHSRCharacterBase::EndPlay(...)
{
    // 清理：解除委托绑定
    AttributeViewModel->Teardown();
    Super::EndPlay(EndPlayReason);
}
```

---

## 委托（Delegate）

类型安全的函数指针容器。将"事件发生"和"谁处理事件"解耦。

### 三种委托类型

| 类型 | 宏前缀 | 返回值 | 多绑定 | 蓝图可用 |
|---|---|---|---|---|
| 单播 | `DECLARE_DELEGATE` | ✅ | ❌ | ❌ |
| 多播 | `DECLARE_MULTICAST_DELEGATE` | ❌ | ✅ | ❌ |
| 动态多播 | `DECLARE_DYNAMIC_MULTICAST_DELEGATE` | ❌ | ✅ | ✅ |

你项目全部使用**多播**——战斗系统的事件有多个监听者。

### 你项目中的委托

```cpp
DECLARE_MULTICAST_DELEGATE(FHSRStatusChangedDelegate);
DECLARE_MULTICAST_DELEGATE_OneParam(FHSRActionResolvedDelegate, FName);
DECLARE_MULTICAST_DELEGATE_OneParam(FHSRTurnLifecycleDelegate, const FHSRTurnLifecycleEvent&);
DECLARE_MULTICAST_DELEGATE_OneParam(FHSRBattleResultReadyDelegate, const FHSRBattleResult&);
DECLARE_MULTICAST_DELEGATE_OneParam(FHSRBattleCommandStateReadyDelegate, const FHSRBattleCommandViewState&);
```

### 绑定方式

| 方法 | 安全 | 自动解绑 |
|---|---|---|
| `AddUObject(UObject*, &Func)` | ✅ GC 感知 | ✅ 对象销毁时自动解绑 |
| `AddRaw(void*, &Func)` | ❌ | ❌ 必须手动移除，否则野指针崩溃 |
| `AddLambda(Function)` | 取决于闭包 | ❌ |

### 调用链

```
BroadcastLifecycleEvent(TurnEnded, ParticipantId)
  → 构造 FHSRTurnLifecycleEvent 结构体
  → TurnEnded.Broadcast(Event)     ← 委托变量名
    → 遍历绑定列表，调用每个绑定的函数
```

```cpp
// 变量名叫 TurnEnded，getter 叫 OnTurnEnded
FHSRTurnLifecycleDelegate TurnEnded;
FHSRTurnLifecycleDelegate& OnTurnEnded() { return TurnEnded; }

// 绑定（返回 Handle）
FDelegateHandle Handle = OnTurnEnded().AddUObject(this, &MyClass::MyFunc);

// 广播
TurnEnded.Broadcast(Event);

// 解绑
OnTurnEnded().Remove(Handle);
```

### FDelegateHandle

绑定委托时返回的整数 ID，用于在解绑时标识要移除的绑定项。多播可以有多个绑定，每个有自己的 Handle。

---

## Actor 生命周期

### 完整顺序

```
① 构造函数（C++ 对象创建）
   CreateDefaultSubobject（创建默认子对象）
   不能依赖 World、其他 Actor

② PostInitProperties（CDO 初始化完成）

③ BeginPlay（进入 World，游戏逻辑正式开始）
   可以访问 World、其他 Actor
   你项目：InitAbilityActorInfo、ApplyGE、BindDelegate

④ Tick（每帧更新）
   你项目 Tick 全关（bCanEverTick = false）

⑤ EndPlay（离开 World）
   你项目：ViewModel->Teardown()

⑥ BeginDestroy → FinishDestroy（GC 回收）

⑦ 析构函数
```

### 构造函数 vs BeginPlay

```
构造函数：
  → 只执行一次
  → 不能依赖 World
  → CreateDefaultSubobject、设默认值

BeginPlay：
  → 每次 Actor 进入 World 都执行
  → 可以依赖 World、其他 Actor
  → 初始化 ASC、Apply GE、绑定委托
```

### 你项目的生命周期映射

```cpp
AHSRCharacterBase 构造函数：
  → CreateDefaultSubobject<UHSRAbilitySystemComponent>(...)
  → CreateDefaultSubobject<UHSRCoreAttributeSet>(...)
  → CreateDefaultSubobject<UHSRAttributeViewModel>(...)
  // 子对象注册完成，但不能 GetWorld()

AHSRCharacterBase::BeginPlay：
  → ASC->InitAbilityActorInfo(this, this)  // ActorInfo 生效
  → ApplyInitialAttributes()               // GE 生效
  → BindAttributeDelegates()               // 委托绑定

AHSRCharacterBase::EndPlay：
  → AttributeViewModel->Teardown()         // 解除绑定
```

---

## 容器内部实现（选读）

### TArray

动态数组，连续内存。和 std::vector 几乎一样，但 UE 加了很多内联函数。

```
TArray<int32> Arr;
Arr.Add(10);      // 末尾添加
Arr.Insert(5, 0); // 指定位置插入（后面的元素后移）
Arr.Remove(10);   // 移除所有值为 10 的元素
Arr.Reset();      // 清空但不释放内存
Arr.Empty();      // 清空并释放内存
Arr.Shrink();     // 缩容到实际大小
Arr.IsValidIndex(0);  // 检查索引是否有效
```

### TMap

哈希映射。键不可重复。

```
TMap<FName, int32> Map;
Map.Add("Health", 100);     // 添加
Map.Find("Health");          // 查找，返回指针，不存在返回 nullptr
Map.FindRef("Health");       // 查找，返回值，不存在返回默认值
Map.Contains("Health");      // 判断是否存在
Map.Remove("Health");        // 移除
```

### TSet

哈希集合，元素唯一。

```
TSet<int32> Set;
Set.Add(10);
Set.Add(10);       // 重复添加不生效
Set.Contains(10);  // true
```

### UE 容器 vs STL 容器

```
TArray<int32> Arr;     // std::vector
TMap<FName, int32>     // std::unordered_map
TSet<int32>            // std::unordered_set

UE 容器的好处：
  → 和 UPROPERTY 集成（支持 GC、序列化）
  → 更安全的迭代器（任何修改都会使迭代器失效并触发 assert）
  → 空检查更友好（IsValidIndex、Contains）
```

> 你在 `HSRBattleParticipant` 中用了 `TWeakObjectPtr<AActor>`。如果改成 `TObjectPtr<AActor>`，会有什么具体问题？在什么场景下会出 Bug？
