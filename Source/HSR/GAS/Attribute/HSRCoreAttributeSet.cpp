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

	// When MaxHealth changes, clamp Health to [0, new MaxHealth]
	if (Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
	{
		const float NewMaxHealth = GetMaxHealth();
		const float CurHealth = GetHealth();
		const float ClampedHealth = FMath::Clamp(CurHealth, 0.0f, NewMaxHealth);
		if (CurHealth != ClampedHealth)
		{
			SetHealth(ClampedHealth);
			UE_LOG(LogTemp, Log, TEXT("UHSRCoreAttributeSet::PostGameplayEffectExecute - MaxHealth changed; Health clamped from %f to %f"), CurHealth, ClampedHealth);
		}
	}
	// When MaxEnergy changes, clamp Energy to [0, new MaxEnergy]
	else if (Data.EvaluatedData.Attribute == GetMaxEnergyAttribute())
	{
		const float NewMaxEnergy = GetMaxEnergy();
		const float CurEnergy = GetEnergy();
		const float ClampedEnergy = FMath::Clamp(CurEnergy, 0.0f, NewMaxEnergy);
		if (CurEnergy != ClampedEnergy)
		{
			SetEnergy(ClampedEnergy);
			UE_LOG(LogTemp, Log, TEXT("UHSRCoreAttributeSet::PostGameplayEffectExecute - MaxEnergy changed; Energy clamped from %f to %f"), CurEnergy, ClampedEnergy);
		}
	}
	// When Health modified directly, clamp to [0, MaxHealth]
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		const float CurHealth = GetHealth();
		const float MaxH = GetMaxHealth();
		const float Clamped = FMath::Clamp(CurHealth, 0.0f, MaxH);
		if (CurHealth != Clamped)
		{
			SetHealth(Clamped);
			UE_LOG(LogTemp, Log, TEXT("UHSRCoreAttributeSet::PostGameplayEffectExecute - Health clamped from %f to %f"), CurHealth, Clamped);
		}
	}
	// When Energy modified directly, clamp to [0, MaxEnergy]
	else if (Data.EvaluatedData.Attribute == GetEnergyAttribute())
	{
		const float CurEnergy = GetEnergy();
		const float MaxE = GetMaxEnergy();
		const float Clamped = FMath::Clamp(CurEnergy, 0.0f, MaxE);
		if (CurEnergy != Clamped)
		{
			SetEnergy(Clamped);
			UE_LOG(LogTemp, Log, TEXT("UHSRCoreAttributeSet::PostGameplayEffectExecute - Energy clamped from %f to %f"), CurEnergy, Clamped);
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
