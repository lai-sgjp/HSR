#include "HSRBasicAttackAbility.h"

#include "AbilitySystemComponent.h"

// 构造函数：把默认动作上下文固定为 BasicAttack。
// 正式伤害由 UHSRBattleCoordinator 准备，这里不再绑定旧的固定伤害 GE；
// 本能力只消费那份准备好的 Spec。
UHSRBasicAttackAbility::UHSRBasicAttackAbility()
{
	SetActionContext(FGuid(), FName(TEXT("BasicAttack")));
}

// 设置待处理目标：校验目标 ASC 非空后暂存。
bool UHSRBasicAttackAbility::SetPendingTarget(UAbilitySystemComponent* InTargetAbilitySystem)
{
	if (!InTargetAbilitySystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBasicAttackAbility::SetPendingTarget - REJECTED null target ASC"));
		return false;
	}

	PendingTargetAbilitySystem = InTargetAbilitySystem;
	return true;
}

// 清除待处理目标。
void UHSRBasicAttackAbility::ClearPendingTarget()
{
	PendingTargetAbilitySystem.Reset();
}

// 激活能力：应用已准备的正式伤害（由 Coordinator 预先装配好 Spec），
// 结束后按结果标记成功/失败并结束能力。
void UHSRBasicAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bLastActivationSucceeded = false;
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!SourceASC)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBasicAttackAbility::ActivateAbility - REJECTED SourceASC=null"));
		ClearPendingTarget();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bLastActivationSucceeded = ApplyPreparedFormalDamage(SourceASC);

	ClearPendingTarget();
	EndAbility(Handle, ActorInfo, ActivationInfo, true, !bLastActivationSucceeded);
}
