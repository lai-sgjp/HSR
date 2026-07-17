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
	OnValuesUpdated.Broadcast(this);
}