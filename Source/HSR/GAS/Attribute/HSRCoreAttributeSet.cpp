#include "HSRCoreAttributeSet.h"
#include "GameplayEffectExtension.h"

UHSRCoreAttributeSet::UHSRCoreAttributeSet()
{
}

void UHSRCoreAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetMaxEnergyAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetSpeedAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetEnergyAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxEnergy());
	}
}

void UHSRCoreAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// When MaxHealth decreases, clamp Health to [0, new MaxHealth]
	if (Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
	{
		const float NewMaxHealth = GetMaxHealth();
		const float CurrentHealth = GetHealth();
		if (CurrentHealth > NewMaxHealth)
		{
			SetHealth(FMath::Clamp(CurrentHealth, 0.0f, NewMaxHealth));
			UE_LOG(LogTemp, Log, TEXT("UHSRCoreAttributeSet::PostGameplayEffectExecute - MaxHealth decreased; Health clamped from %f to %f"), CurrentHealth, NewMaxHealth);
		}
	}
	// When MaxEnergy decreases, clamp Energy to [0, new MaxEnergy]
	else if (Data.EvaluatedData.Attribute == GetMaxEnergyAttribute())
	{
		const float NewMaxEnergy = GetMaxEnergy();
		const float CurrentEnergy = GetEnergy();
		if (CurrentEnergy > NewMaxEnergy)
		{
			SetEnergy(FMath::Clamp(CurrentEnergy, 0.0f, NewMaxEnergy));
			UE_LOG(LogTemp, Log, TEXT("UHSRCoreAttributeSet::PostGameplayEffectExecute - MaxEnergy decreased; Energy clamped from %f to %f"), CurrentEnergy, NewMaxEnergy);
		}
	}
}

void UHSRCoreAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetMaxEnergyAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetSpeedAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetEnergyAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxEnergy());
	}
}
