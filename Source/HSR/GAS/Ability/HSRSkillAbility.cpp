#include "HSRSkillAbility.h"
#include "AbilitySystemComponent.h"
#include "../../Data/HSRSkillDefinition.h"

// 构造函数：把默认动作上下文固定为 Skill。
UHSRSkillAbility::UHSRSkillAbility()
{
	SetActionContext(FGuid(), FName(TEXT("Skill")));
}

// 从技能定义配置本能力：校验定义有效性后保存其效果 GE 类。
bool UHSRSkillAbility::ConfigureFromSkillDefinition(const UHSRSkillDefinition& Definition)
{
	if (!Definition.IsValidSkillDefinition())
	{
		return false;
	}
	EffectClass = Definition.EffectGameplayEffectClass;
	return !EffectClass.IsNull();
}

// 设置待处理目标：校验目标 ASC 非空后暂存。
bool UHSRSkillAbility::SetPendingTarget(UAbilitySystemComponent* InTargetAbilitySystem)
{
	if (!InTargetAbilitySystem)
	{
		return false;
	}
	PendingTargetAbilitySystem = InTargetAbilitySystem;
	return true;
}

// 清除待处理目标。
void UHSRSkillAbility::ClearPendingTarget()
{
	PendingTargetAbilitySystem.Reset();
}

// 激活能力：应用已准备的正式伤害，结束后标记结果并结束能力。
void UHSRSkillAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bLastActivationSucceeded = false;
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!SourceASC)
	{
		ClearPendingTarget();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	bLastActivationSucceeded = ApplyPreparedFormalDamage(SourceASC);
	ClearPendingTarget();
	EndAbility(Handle, ActorInfo, ActivationInfo, true, !bLastActivationSucceeded);
}
