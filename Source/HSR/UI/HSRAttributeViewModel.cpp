#include "HSRAttributeViewModel.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"

// 从 ASC（AbilitySystemComponent）初始化：本 VM 把 GAS 的属性值转换成
// UI 可直接显示的字段。它只订阅属性变化事件，数据一变就刷新并广播 OnValuesUpdated，
// Widget 订阅该事件刷新显示，从而把“GAS 数据源”与“UMG 显示”解耦。
void UHSRAttributeViewModel::InitializeFromASC(UAbilitySystemComponent* InASC)
{
	if (!InASC)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRAttributeViewModel::InitializeFromASC - InASC is nullptr"));
		return;
	}

	// 重新初始化前先彻底清理旧的订阅与计数，避免重复绑定或残留统计。
	Teardown();
	ResetDiagnosticCounters();

	ASCWeakPtr = InASC;
	// 先推送一次当前值，保证在订阅事件前界面就有初始数据。
	PushCurrentValues();

	// 没有核心属性集说明 ASC 配置不完整；仍然广播一次（UI 可据此显示空数据）。
	const UHSRCoreAttributeSet* CoreSet = InASC->GetSet<UHSRCoreAttributeSet>();
	if (!CoreSet)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRAttributeViewModel::InitializeFromASC - No CoreAttributeSet on ASC"));
		OnValuesUpdated.Broadcast(this);
		return;
	}

	// 逐个属性注册“值变化”委托：每个属性对应一个 FDelegateHandle，
	// 存放在 DelegateHandles 中，便于 Teardown 时统一移除。
	FDelegateHandle Handle;

	Handle = InASC->GetGameplayAttributeValueChangeDelegate(CoreSet->GetHealthAttribute()).AddUObject(this, &UHSRAttributeViewModel::OnAttributeChanged);
	DelegateHandles.Add(CoreSet->GetHealthAttribute(), Handle);

	Handle = InASC->GetGameplayAttributeValueChangeDelegate(CoreSet->GetMaxHealthAttribute()).AddUObject(this, &UHSRAttributeViewModel::OnAttributeChanged);
	DelegateHandles.Add(CoreSet->GetMaxHealthAttribute(), Handle);

	Handle = InASC->GetGameplayAttributeValueChangeDelegate(CoreSet->GetEnergyAttribute()).AddUObject(this, &UHSRAttributeViewModel::OnAttributeChanged);
	DelegateHandles.Add(CoreSet->GetEnergyAttribute(), Handle);

	Handle = InASC->GetGameplayAttributeValueChangeDelegate(CoreSet->GetMaxEnergyAttribute()).AddUObject(this, &UHSRAttributeViewModel::OnAttributeChanged);
	DelegateHandles.Add(CoreSet->GetMaxEnergyAttribute(), Handle);

	Handle = InASC->GetGameplayAttributeValueChangeDelegate(CoreSet->GetSpeedAttribute()).AddUObject(this, &UHSRAttributeViewModel::OnAttributeChanged);
	DelegateHandles.Add(CoreSet->GetSpeedAttribute(), Handle);

	UE_LOG(LogTemp, Log, TEXT("UHSRAttributeViewModel::InitializeFromASC - Bound %d attribute delegates"), DelegateHandles.Num());

	// 初始化完成后广播一次，让订阅方立刻拿到当前属性值。
	OnValuesUpdated.Broadcast(this);
}


// 复位所有诊断计数器：这些计数用于观察“属性变化事件”与“广播”发生的次数，
// 属于开发期诊断手段，不影响游戏逻辑。
void UHSRAttributeViewModel::ResetDiagnosticCounters()
{
	HealthChangeCount = 0;
	MaxHealthChangeCount = 0;
	EnergyChangeCount = 0;
	MaxEnergyChangeCount = 0;
	SpeedChangeCount = 0;
	TotalBroadcastCount = 0;
	SnapshotBroadcastCount = 0;
	UE_LOG(LogTemp, Log, TEXT("UHSRAttributeViewModel::ResetDiagnosticCounters - All counters reset"));
}

// 手动推送一次当前属性值并广播。
// 该接口仅限开发/测试构建使用（Test/Shipping 中直接拒绝），
// 用途是让界面在不受 GAS 事件驱动时也能手动刷新一次快照。
void UHSRAttributeViewModel::BroadcastCurrentValues()
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	UE_LOG(LogTemp, Warning, TEXT("UHSRAttributeViewModel::BroadcastCurrentValues - Rejected in Test/Shipping"));
	return;
#else
	PushCurrentValues();
	SnapshotBroadcastCount++;
	OnValuesUpdated.Broadcast(this);
	UE_LOG(LogTemp, Log, TEXT("UHSRAttributeViewModel::BroadcastCurrentValues - Manual snapshot broadcast #%d (TotalBroadcastCount=%d)"), SnapshotBroadcastCount, TotalBroadcastCount);
#endif
}

// 解除与 ASC 的所有关联：逐个移除属性变化委托，并清空弱引用。
void UHSRAttributeViewModel::Teardown()
{
	// ASC 已失效（对象被销毁）时没有可移除的委托，直接清空容器即可。
	if (!ASCWeakPtr.IsValid())
	{
		DelegateHandles.Empty();
		return;
	}

	// 遍历所有已注册的属性委托并逐一移除，确保不再收到任何属性变化通知。
	for (const auto& Pair : DelegateHandles)
	{
		ASCWeakPtr->GetGameplayAttributeValueChangeDelegate(Pair.Key).Remove(Pair.Value);
	}
	DelegateHandles.Empty();
	ASCWeakPtr = nullptr;
	UE_LOG(LogTemp, Log, TEXT("UHSRAttributeViewModel::Teardown - Removed all attribute delegates"));
}

// 从 ASC 的核心属性集读取当前值并写入本地字段。这是“数据源 -> 显示字段”的映射，
// 把 FGameplayAttributeData 的浮点值同步到 VM 的简单标量成员上。
void UHSRAttributeViewModel::PushCurrentValues()
{
	UAbilitySystemComponent* ASC = ASCWeakPtr.Get();
	if (!ASC)
	{
		return;
	}

	const UHSRCoreAttributeSet* CoreSet = ASC->GetSet<UHSRCoreAttributeSet>();
	if (!CoreSet)
	{
		return;
	}

	Health = CoreSet->GetHealth();
	MaxHealth = CoreSet->GetMaxHealth();
	Energy = CoreSet->GetEnergy();
	MaxEnergy = CoreSet->GetMaxEnergy();
	Speed = CoreSet->GetSpeed();
}

// 属性变化回调：GAS 中任一已订阅属性变化都会进入这里。
// 先同步刷新所有值（一次快照，保证各字段一致），再统计是哪个属性变化，
// 最后广播通知订阅方。刷新全部而非只刷新变化属性，是为了避免多属性联动时显示不一致。
void UHSRAttributeViewModel::OnAttributeChanged(const FOnAttributeChangeData& Data)
{
	PushCurrentValues();

	// 根据具体变化属性累加对应的诊断计数。
	if (Data.Attribute == UHSRCoreAttributeSet::GetHealthAttribute())
	{
		HealthChangeCount++;
	}
	else if (Data.Attribute == UHSRCoreAttributeSet::GetMaxHealthAttribute())
	{
		MaxHealthChangeCount++;
	}
	else if (Data.Attribute == UHSRCoreAttributeSet::GetEnergyAttribute())
	{
		EnergyChangeCount++;
	}
	else if (Data.Attribute == UHSRCoreAttributeSet::GetMaxEnergyAttribute())
	{
		MaxEnergyChangeCount++;
	}
	else if (Data.Attribute == UHSRCoreAttributeSet::GetSpeedAttribute())
	{
		SpeedChangeCount++;
	}

	TotalBroadcastCount++;
	OnValuesUpdated.Broadcast(this);
}