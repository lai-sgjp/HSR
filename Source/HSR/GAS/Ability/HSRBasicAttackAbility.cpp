#include "HSRBasicAttackAbility.h"

#include "AbilitySystemComponent.h"

UHSRBasicAttackAbility::UHSRBasicAttackAbility()
{
	SetActionContext(FGuid(), FName(TEXT("BasicAttack")));
	// Formal damage is prepared by UHSRBattleCoordinator.  Do not bind the
	// retired fixed-damage GE here: this ability only consumes that prepared Spec.
}

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

void UHSRBasicAttackAbility::ClearPendingTarget()
{
	PendingTargetAbilitySystem.Reset();
}

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
