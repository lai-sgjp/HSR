#include "HSRCoreAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "../Damage/HSRDamageEffectContext.h"

UHSRCoreAttributeSet::UHSRCoreAttributeSet()
{
}

// 属性“基础值”即将变化前的钳制：把属性限制在合理范围内。
// 基础值变化通常来自 GE 的持续时间/堆叠刷新，需要保证属性始终合法。
void UHSRCoreAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	if (Attribute == GetMaxHealthAttribute())
	{
		// 最大血量不得为负。
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetMaxEnergyAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetMaxToughnessAttribute())
	{
		NewValue = FMath::IsFinite(NewValue) ? FMath::Max(NewValue, 0.0f) : 0.0f;
	}
	else if (Attribute == GetSpeedAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetAttackAttribute() || Attribute == GetDefenseAttribute() || Attribute == GetCritDamageAttribute())
	{
		NewValue = FMath::IsFinite(NewValue) ? FMath::Max(NewValue, 0.0f) : 0.0f;
	}
	else if (Attribute == GetCritRateAttribute())
	{
		// 暴击率钳制在 [0,1]。
		NewValue = FMath::IsFinite(NewValue) ? FMath::Clamp(NewValue, 0.0f, 1.0f) : 0.0f;
	}
	else if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetEnergyAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxEnergy());
	}
	else if (Attribute == GetToughnessAttribute())
	{
		NewValue = FMath::IsFinite(NewValue) ? FMath::Clamp(NewValue, 0.0f, GetMaxToughness()) : 0.0f;
	}
}

// GE 执行后的处理：把“传入伤害/传入韧性伤害”这类元属性转换成实际扣血/扣韧性，
// 并把上限变化后的当前值重新钳制。
void UHSRCoreAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	// 处理“传入伤害”元属性：取伤害量、清零元属性、真正扣血。
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float Damage = FMath::IsFinite(GetIncomingDamage()) ? FMath::Max(0.0f, GetIncomingDamage()) : 0.0f;
		SetIncomingDamage(0.0f);
		if (Damage > 0.0f)
		{
			const float HealthBefore = GetHealth();
			SetHealth(FMath::Clamp(GetHealth() - Damage, 0.0f, GetMaxHealth()));
			// 把“实际生效伤害”写回伤害上下文（供伤害结果 DTO 使用）。
			if (FHSRDamageEffectContext* DamageContext = static_cast<FHSRDamageEffectContext*>(Data.EffectSpec.GetContext().Get()))
			{
				DamageContext->DamageResult.Breakdown.AppliedDamage = FMath::Max(0.0f, HealthBefore - GetHealth());
			}
		}
		return;
	}
	// 处理“传入韧性伤害”元属性：清零元属性后真正扣韧性。
	if (Data.EvaluatedData.Attribute == GetIncomingToughnessDamageAttribute())
	{
		const float Damage = FMath::IsFinite(GetIncomingToughnessDamage()) ? FMath::Max(0.0f, GetIncomingToughnessDamage()) : 0.0f;
		SetIncomingToughnessDamage(0.0f);
		if (Damage > 0.0f)
		{
			SetToughness(FMath::Clamp(GetToughness() - Damage, 0.0f, GetMaxToughness()));
		}
		return;
	}

	// 最大血量变化时，把当前血量钳制到 [0, 新上限]。
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
	// 最大能量变化时，把当前能量钳制到 [0, 新上限]。
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
	// 直接修改血量时钳制到 [0, MaxHealth]。
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
	// 直接修改能量时钳制到 [0, MaxEnergy]。
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

	// 韧性（当前或上限）变化后统一钳制到 [0, MaxToughness]。
	if (Data.EvaluatedData.Attribute == GetMaxToughnessAttribute() || Data.EvaluatedData.Attribute == GetToughnessAttribute())
	{
		const float CurrentToughness = GetToughness();
		const float ClampedToughness = FMath::Clamp(CurrentToughness, 0.0f, GetMaxToughness());
		if (CurrentToughness != ClampedToughness)
		{
			SetToughness(ClampedToughness);
			UE_LOG(LogTemp, Log, TEXT("UHSRCoreAttributeSet::PostGameplayEffectExecute - Toughness clamped from %f to %f"), CurrentToughness, ClampedToughness);
		}
	}
}

// 属性“当前值”即将变化前的钳制（与基础值变化规则一致，保证任何入口都合法）。
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
	else if (Attribute == GetMaxToughnessAttribute())
	{
		NewValue = FMath::IsFinite(NewValue) ? FMath::Max(NewValue, 0.0f) : 0.0f;
	}
	else if (Attribute == GetSpeedAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetAttackAttribute() || Attribute == GetDefenseAttribute() || Attribute == GetCritDamageAttribute())
	{
		NewValue = FMath::IsFinite(NewValue) ? FMath::Max(NewValue, 0.0f) : 0.0f;
	}
	else if (Attribute == GetCritRateAttribute())
	{
		NewValue = FMath::IsFinite(NewValue) ? FMath::Clamp(NewValue, 0.0f, 1.0f) : 0.0f;
	}
	else if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetEnergyAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxEnergy());
	}
	else if (Attribute == GetToughnessAttribute())
	{
		NewValue = FMath::IsFinite(NewValue) ? FMath::Clamp(NewValue, 0.0f, GetMaxToughness()) : 0.0f;
	}
}
