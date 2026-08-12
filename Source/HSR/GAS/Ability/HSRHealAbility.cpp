#include "HSRHealAbility.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "../../Data/HSRSkillDefinition.h"

// 构造函数：把默认动作上下文固定为 Heal。
UHSRHealAbility::UHSRHealAbility()
{
	SetActionContext(FGuid(), FName(TEXT("Heal")));
}

// 从技能定义配置本能力：校验为合法治疗定义后保存其治疗效果 GE 类。
bool UHSRHealAbility::ConfigureFromSkillDefinition(const UHSRSkillDefinition& D)
{
	if (!D.IsValidHealDefinition())
	{
		return false;
	}
	Effect = D.EffectGameplayEffectClass;
	return !Effect.IsNull();
}

// 设置待处理目标：校验目标 ASC 非空后暂存。
bool UHSRHealAbility::SetPendingTarget(UAbilitySystemComponent* T)
{
	if (!T)
	{
		return false;
	}
	Target = T;
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
	auto* S = I ? I->AbilitySystemComponent.Get() : nullptr;
	auto* T = Target.Get();
	auto C = Effect.LoadSynchronous();
	if (S && T && C)
	{
		auto X = S->MakeEffectContext();
		auto Spec = S->MakeOutgoingSpec(C, 1, X);
		if (Spec.IsValid())
		{
			bSuccess = S->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), T).WasSuccessfullyApplied();
		}
	}
	ClearPendingTarget();
	EndAbility(H, I, A, true, !bSuccess);
}
