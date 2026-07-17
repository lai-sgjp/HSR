#include "HSRAttributeViewModel.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"

void UHSRAttributeViewModel::InitializeFromASC(UAbilitySystemComponent* InASC)
{
	if (!InASC)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRAttributeViewModel::InitializeFromASC - InASC is nullptr"));
		return;
	}

	Teardown();
	ResetDiagnosticCounters();

	ASCWeakPtr = InASC;
	PushCurrentValues();

	const UHSRCoreAttributeSet* CoreSet = InASC->GetSet<UHSRCoreAttributeSet>();
	if (!CoreSet)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRAttributeViewModel::InitializeFromASC - No CoreAttributeSet on ASC"));
		OnValuesUpdated.Broadcast(this);
		return;
	}

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

	OnValuesUpdated.Broadcast(this);
}


void UHSRAttributeViewModel::ResetDiagnosticCounters()
{
	HealthChangeCount = 0;
	MaxHealthChangeCount = 0;
	EnergyChangeCount = 0;
	MaxEnergyChangeCount = 0;
	SpeedChangeCount = 0;
	TotalBroadcastCount = 0;
	UE_LOG(LogTemp, Log, TEXT("UHSRAttributeViewModel::ResetDiagnosticCounters - All counters reset"));
}

void UHSRAttributeViewModel::BroadcastCurrentValues()
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	UE_LOG(LogTemp, Warning, TEXT("UHSRAttributeViewModel::BroadcastCurrentValues - Rejected in Test/Shipping"));
	return;
#else
	PushCurrentValues();
	OnValuesUpdated.Broadcast(this);
	UE_LOG(LogTemp, Log, TEXT("UHSRAttributeViewModel::BroadcastCurrentValues - Manual broadcast triggered with TotalBroadcastCount=%d"), TotalBroadcastCount);
#endif
}

void UHSRAttributeViewModel::Teardown()
{
	if (!ASCWeakPtr.IsValid())
	{
		DelegateHandles.Empty();
		return;
	}

	for (const auto& Pair : DelegateHandles)
	{
		ASCWeakPtr->GetGameplayAttributeValueChangeDelegate(Pair.Key).Remove(Pair.Value);
	}
	DelegateHandles.Empty();
	ASCWeakPtr = nullptr;
	UE_LOG(LogTemp, Log, TEXT("UHSRAttributeViewModel::Teardown - Removed all attribute delegates"));
}

void UHSRAttributeViewModel::PushCurrentValues()
{
	UAbilitySystemComponent* ASC = ASCWeakPtr.Get();
	if (!ASC) return;

	const UHSRCoreAttributeSet* CoreSet = ASC->GetSet<UHSRCoreAttributeSet>();
	if (!CoreSet) return;

	Health = CoreSet->GetHealth();
	MaxHealth = CoreSet->GetMaxHealth();
	Energy = CoreSet->GetEnergy();
	MaxEnergy = CoreSet->GetMaxEnergy();
	Speed = CoreSet->GetSpeed();
}

void UHSRAttributeViewModel::OnAttributeChanged(const FOnAttributeChangeData& Data)
{
	PushCurrentValues();

	if (Data.Attribute == UHSRCoreAttributeSet::GetHealthAttribute()) HealthChangeCount++;
	else if (Data.Attribute == UHSRCoreAttributeSet::GetMaxHealthAttribute()) MaxHealthChangeCount++;
	else if (Data.Attribute == UHSRCoreAttributeSet::GetEnergyAttribute()) EnergyChangeCount++;
	else if (Data.Attribute == UHSRCoreAttributeSet::GetMaxEnergyAttribute()) MaxEnergyChangeCount++;
	else if (Data.Attribute == UHSRCoreAttributeSet::GetSpeedAttribute()) SpeedChangeCount++;

	TotalBroadcastCount++;
	OnValuesUpdated.Broadcast(this);
}