#include "HSRUltimateAbility.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "../Attribute/HSRCoreAttributeSet.h"
#include "../../Data/HSRSkillDefinition.h"

namespace
{
	bool HasNegativeAttributeModifier(const UGameplayEffect* Effect, const FGameplayAttribute& Attribute, float& OutMagnitude)
	{
		OutMagnitude = 0.0f;
		if (!Effect)
		{
			return false;
		}

		for (const FGameplayModifierInfo& Modifier : Effect->Modifiers)
		{
			float Magnitude = 0.0f;
			if (Modifier.Attribute == Attribute && Modifier.ModifierOp == EGameplayModOp::Additive
				&& Modifier.ModifierMagnitude.GetStaticMagnitudeIfPossible(1.0f, Magnitude) && Magnitude < 0.0f)
			{
				OutMagnitude = Magnitude;
				return true;
			}
		}
		return false;
	}
}

UHSRUltimateAbility::UHSRUltimateAbility()
{
	SetActionContext(FGuid(), FName(TEXT("Ultimate")));
}

bool UHSRUltimateAbility::ConfigureFromSkillDefinition(const UHSRSkillDefinition& Definition)
{
	if (!Definition.IsValidUltimateDefinition())
	{
		return false;
	}

	CostGameplayEffectClass = Definition.CostGameplayEffectClass.LoadSynchronous();
	UltimateEffectClass = Definition.EffectGameplayEffectClass;
	const TSubclassOf<UGameplayEffect> LoadedEffect = UltimateEffectClass.LoadSynchronous();
	float CostMagnitude = 0.0f;
	float DamageMagnitude = 0.0f;
	const bool bValidCost = CostGameplayEffectClass && HasNegativeAttributeModifier(CostGameplayEffectClass->GetDefaultObject<UGameplayEffect>(), UHSRCoreAttributeSet::GetEnergyAttribute(), CostMagnitude);
	const bool bValidDamage = LoadedEffect && HasNegativeAttributeModifier(LoadedEffect->GetDefaultObject<UGameplayEffect>(), UHSRCoreAttributeSet::GetHealthAttribute(), DamageMagnitude);
	UE_LOG(LogTemp, Log, TEXT("UHSRUltimateAbility::ConfigureFromSkillDefinition - CostGE=%s CostEnergyMagnitude=%.2f ValidCost=%d EffectGE=%s DamageHealthMagnitude=%.2f ValidDamage=%d"),
		CostGameplayEffectClass ? *CostGameplayEffectClass->GetName() : TEXT("null"), CostMagnitude, bValidCost ? 1 : 0,
		LoadedEffect ? *LoadedEffect->GetName() : TEXT("null"), DamageMagnitude, bValidDamage ? 1 : 0);
	return bValidCost && bValidDamage;
}

EHSRAbilityFailureReason UHSRUltimateAbility::GetPreActivationFailureReason(const FGameplayAbilitySpecHandle& Handle, const FGameplayAbilityActorInfo* ActorInfo) const
{
	return CheckCost(Handle, ActorInfo, nullptr) ? EHSRAbilityFailureReason::None : EHSRAbilityFailureReason::InsufficientEnergy;
}

bool UHSRUltimateAbility::SetPendingTarget(UAbilitySystemComponent* InTargetAbilitySystem)
{
	if (!InTargetAbilitySystem)
	{
		return false;
	}
	PendingTargetAbilitySystem = InTargetAbilitySystem;
	return true;
}

void UHSRUltimateAbility::ClearPendingTarget()
{
	PendingTargetAbilitySystem.Reset();
}

void UHSRUltimateAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bLastActivationSucceeded = false;
	LastFailureReason = EHSRAbilityFailureReason::EffectFailed;
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	UAbilitySystemComponent* TargetASC = PendingTargetAbilitySystem.Get();
	TSubclassOf<UGameplayEffect> LoadedEffect = UltimateEffectClass.LoadSynchronous();
	if (!SourceASC || !TargetASC || !CostGameplayEffectClass || !LoadedEffect)
	{
		ClearPendingTarget();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const float EnergyBeforeCommit = SourceASC->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute());
	const float TargetHealthBeforeApply = TargetASC->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());

	// Construct the damage spec before CommitAbility. Once Cost commits, this
	// synchronous prototype has no ordinary failure point before application.
	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(GetAvatarActorFromActorInfo());
	FGameplayEffectSpecHandle EffectSpec = SourceASC->MakeOutgoingSpec(LoadedEffect, 1.0f, EffectContext);
	if (!EffectSpec.IsValid())
	{
		ClearPendingTarget();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CheckCost(Handle, ActorInfo, nullptr))
	{
		LastFailureReason = EHSRAbilityFailureReason::InsufficientEnergy;
		ClearPendingTarget();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		LastFailureReason = EHSRAbilityFailureReason::CommitFailed;
		ClearPendingTarget();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FActiveGameplayEffectHandle AppliedHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*EffectSpec.Data.Get(), TargetASC);
	bLastActivationSucceeded = AppliedHandle.WasSuccessfullyApplied();
	const float EnergyAfterCommit = SourceASC->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute());
	const float TargetHealthAfterApply = TargetASC->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());
	UE_LOG(LogTemp, Log, TEXT("UHSRUltimateAbility::ActivateAbility - CostCommitted=%d EffectApplied=%d CostGE=%s EffectGE=%s EnergyBefore=%.2f EnergyAfter=%.2f TargetHealthBefore=%.2f TargetHealthAfter=%.2f"),
		1, bLastActivationSucceeded ? 1 : 0, *CostGameplayEffectClass->GetName(), *LoadedEffect->GetName(), EnergyBeforeCommit, EnergyAfterCommit, TargetHealthBeforeApply, TargetHealthAfterApply);
	if (!bLastActivationSucceeded)
	{
		ensureMsgf(false, TEXT("UHSRUltimateAbility effect application failed after a successful Cost commit; preflight invariant violated."));
		LastFailureReason = EHSRAbilityFailureReason::EffectFailed;
	}

	ClearPendingTarget();
	EndAbility(Handle, ActorInfo, ActivationInfo, true, !bLastActivationSucceeded);
}
