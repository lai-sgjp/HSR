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
	EnergyRefundEffectClass = Definition.EnergyRefundGameplayEffectClass;
	const TSubclassOf<UGameplayEffect> LoadedRefund = EnergyRefundEffectClass.LoadSynchronous();
	float CostMagnitude = 0.0f;
	const bool bValidCost = CostGameplayEffectClass && HasNegativeAttributeModifier(CostGameplayEffectClass->GetDefaultObject<UGameplayEffect>(), UHSRCoreAttributeSet::GetEnergyAttribute(), CostMagnitude);
	float RefundMagnitude = 0.0f;
	const bool bValidRefund = LoadedRefund && LoadedRefund->GetDefaultObject<UGameplayEffect>()->DurationPolicy == EGameplayEffectDurationType::Instant
		&& HasNegativeAttributeModifier(LoadedRefund->GetDefaultObject<UGameplayEffect>(), UHSRCoreAttributeSet::GetEnergyAttribute(), RefundMagnitude) == false;
	// Refund must be a positive additive Energy modifier equal to the Cost.
	bool bPositiveRefund = false;
	if (LoadedRefund) { for (const FGameplayModifierInfo& Modifier : LoadedRefund->GetDefaultObject<UGameplayEffect>()->Modifiers) { float Magnitude = 0.0f; if (Modifier.Attribute == UHSRCoreAttributeSet::GetEnergyAttribute() && Modifier.ModifierOp == EGameplayModOp::Additive && Modifier.ModifierMagnitude.GetStaticMagnitudeIfPossible(1.0f, Magnitude) && FMath::IsNearlyEqual(Magnitude, -CostMagnitude)) { bPositiveRefund = true; break; } } }
	UE_LOG(LogTemp, Log, TEXT("UHSRUltimateAbility::ConfigureFromSkillDefinition - CostGE=%s CostEnergyMagnitude=%.2f ValidCost=%d RefundGE=%s ValidRefund=%d"),
		CostGameplayEffectClass ? *CostGameplayEffectClass->GetName() : TEXT("null"), CostMagnitude, bValidCost ? 1 : 0,
		LoadedRefund ? *LoadedRefund->GetName() : TEXT("null"), (bValidRefund && bPositiveRefund) ? 1 : 0);
	return bValidCost && bValidRefund && bPositiveRefund;
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
	const TSubclassOf<UGameplayEffect> LoadedRefund = EnergyRefundEffectClass.LoadSynchronous();
	if (!SourceASC || !CostGameplayEffectClass || !LoadedRefund)
	{
		ClearPendingTarget();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const float EnergyBeforeCommit = SourceASC->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute());
	FGameplayEffectContextHandle RefundContext = SourceASC->MakeEffectContext();
	FGameplayEffectSpecHandle RefundSpec = SourceASC->MakeOutgoingSpec(LoadedRefund, 1.0f, RefundContext);
	if (!RefundSpec.IsValid()) { ClearPendingTarget(); EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }

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
	bLastActivationSucceeded = ApplyPreparedFormalDamage(SourceASC);
	FHSRFormalDamageExecutionResult& ExecutionResult = GetMutableFormalDamageExecutionResult();
	ExecutionResult.bCostCommitted = true;
	ExecutionResult.EnergyBefore = EnergyBeforeCommit;
	const float EnergyAfterCommit = SourceASC->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute());
	if (!bLastActivationSucceeded)
	{
		const FActiveGameplayEffectHandle RefundApplied = SourceASC->ApplyGameplayEffectSpecToSelf(*RefundSpec.Data.Get());
		ExecutionResult.bRefundApplied = RefundApplied.WasSuccessfullyApplied();
		LastFailureReason = EHSRAbilityFailureReason::EffectFailed;
	}
	ExecutionResult.EnergyAfter = SourceASC->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute());
	UE_LOG(LogTemp, Log, TEXT("UHSRUltimateAbility::ActivateAbility - CostCommitted=1 DamageApplied=%d RefundApplied=%d EnergyBefore=%.2f EnergyAfterCost=%.2f EnergyFinal=%.2f"), bLastActivationSucceeded ? 1 : 0, ExecutionResult.bRefundApplied ? 1 : 0, EnergyBeforeCommit, EnergyAfterCommit, ExecutionResult.EnergyAfter);

	ClearPendingTarget();
	EndAbility(Handle, ActorInfo, ActivationInfo, true, !bLastActivationSucceeded);
}
