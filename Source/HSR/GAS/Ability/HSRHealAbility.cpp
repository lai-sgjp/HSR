#include "HSRHealAbility.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "../../Data/HSRSkillDefinition.h"
#include "../Attribute/HSRCoreAttributeSet.h"

// 构造函数：把默认动作上下文固定为 Heal。
UHSRHealAbility::UHSRHealAbility()
{
	SetActionContext(FGuid(), FName(TEXT("Heal")));
}

// 从技能定义配置本能力：校验为合法治疗定义后保存其治疗效果 GE 类。
bool UHSRHealAbility::ConfigureFromSkillDefinition(const UHSRSkillDefinition& D)
{
	Effect.Reset();
	CostGameplayEffectClass = nullptr;
	RefundEffect.Reset();
	ClearPendingTarget();
	bSuccess = false;
	FailureReason = EHSRAbilityFailureReason::DefinitionMissing;
	if (!D.IsValidHealDefinition() || !D.EffectGameplayEffectClass.LoadSynchronous()
		|| (D.CostGameplayEffectClass.IsNull() && !D.EnergyRefundGameplayEffectClass.IsNull()))
	{
		return false;
	}
	if (!D.CostGameplayEffectClass.IsNull())
	{
		const auto Cost = D.CostGameplayEffectClass.LoadSynchronous();
		const auto Refund = D.EnergyRefundGameplayEffectClass.LoadSynchronous();
		float CostAmount = 0, RefundAmount = 0;
		const auto ReadEnergy = [](const UGameplayEffect* GE, float& Out)
		{
			if (!GE || GE->DurationPolicy != EGameplayEffectDurationType::Instant || GE->Modifiers.Num() != 1) return false;
			const FGameplayModifierInfo& Mod = GE->Modifiers[0];
			return Mod.Attribute == UHSRCoreAttributeSet::GetEnergyAttribute()
				&& Mod.ModifierOp == EGameplayModOp::Additive && Mod.ModifierMagnitude.GetStaticMagnitudeIfPossible(1.f, Out);
		};
		if (!Cost || !Refund || !ReadEnergy(Cost->GetDefaultObject<UGameplayEffect>(), CostAmount)
			|| !ReadEnergy(Refund->GetDefaultObject<UGameplayEffect>(), RefundAmount)
			|| !FMath::IsFinite(CostAmount) || !FMath::IsFinite(RefundAmount)
			|| CostAmount >= 0 || !FMath::IsNearlyEqual(-CostAmount, RefundAmount)) return false;
		CostGameplayEffectClass = Cost;
		RefundEffect = D.EnergyRefundGameplayEffectClass;
	}
	Effect = D.EffectGameplayEffectClass;
	FailureReason = EHSRAbilityFailureReason::None;
	return true;
}

EHSRAbilityFailureReason UHSRHealAbility::GetPreActivationFailureReason(const FGameplayAbilitySpecHandle& Handle, const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (Effect.IsNull()) return EHSRAbilityFailureReason::DefinitionMissing;
	return !CostGameplayEffectClass || CheckCost(Handle, ActorInfo, nullptr)
		? EHSRAbilityFailureReason::None : EHSRAbilityFailureReason::InsufficientEnergy;
}

bool UHSRHealAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	return !Effect.IsNull() && Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UHSRHealAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	const UGameplayEffect* Cost = GetCostGameplayEffect();
	bCostApplied = Cost && ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo,
		Cost, GetAbilityLevel(Handle, ActorInfo)).WasSuccessfullyApplied();
}

// 设置待处理目标：校验目标 ASC 非空后暂存。
bool UHSRHealAbility::SetPendingTarget(UAbilitySystemComponent* T)
{
	ClearPendingTarget();
	bSuccess = false;
	FailureReason = EHSRAbilityFailureReason::InvalidTarget;
	if (!T)
	{
		return false;
	}
	Target = T;
	FailureReason = EHSRAbilityFailureReason::None;
	return true;
}

// 清除待处理目标。
void UHSRHealAbility::ClearPendingTarget()
{
	Target.Reset();
}

// 激活能力：直接把治疗 GE 施加到目标（治疗不走“形式化伤害”接缝，直接应用效果）。
void UHSRHealAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle H,
	const FGameplayAbilityActorInfo* I,
	const FGameplayAbilityActivationInfo A,
	const FGameplayEventData* E)
{
	bSuccess = false;
	FailureReason = EHSRAbilityFailureReason::EffectFailed;
	auto* S = I ? I->AbilitySystemComponent.Get() : nullptr;
	auto* T = Target.Get();
	auto C = Effect.LoadSynchronous();
	if (S && T && C)
	{
		auto X = S->MakeEffectContext();
		auto Spec = S->MakeOutgoingSpec(C, 1, X);
		if (Spec.IsValid())
		{
			FGameplayEffectSpecHandle Refund;
			bool bCommitted = false;
			if (CostGameplayEffectClass)
			{
				Refund = S->MakeOutgoingSpec(RefundEffect.LoadSynchronous(), 1, X);
				if (!Refund.IsValid() || !CheckCost(H, I, nullptr))
				{
					FailureReason = Refund.IsValid() ? EHSRAbilityFailureReason::InsufficientEnergy : EHSRAbilityFailureReason::EffectFailed;
					ClearPendingTarget(); EndAbility(H,I,A,true,true); return;
				}
				bCostApplied = false;
				bCommitted = CommitAbility(H,I,A) && bCostApplied;
				if (!bCommitted)
				{
					FailureReason = EHSRAbilityFailureReason::CommitFailed;
					ClearPendingTarget(); EndAbility(H,I,A,true,true); return;
				}
			}
			bSuccess = S->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), T).WasSuccessfullyApplied();
			if (bSuccess) FailureReason = EHSRAbilityFailureReason::None;
			if (!bSuccess && bCommitted)
			{
				const bool bRefunded = S->ApplyGameplayEffectSpecToSelf(*Refund.Data.Get()).WasSuccessfullyApplied();
				ensureMsgf(bRefunded, TEXT("Heal energy refund failed"));
			}
		}
	}
	ClearPendingTarget();
	EndAbility(H, I, A, true, !bSuccess);
}
