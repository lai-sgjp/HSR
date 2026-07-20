#include "HSRBasicAttackAbility.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

UHSRBasicAttackAbility::UHSRBasicAttackAbility()
{
	SetActionContext(FGuid(), FName(TEXT("BasicAttack")));
	DamageEffectClass = TSoftClassPtr<UGameplayEffect>(FSoftObjectPath(TEXT("/Game/GameplayEffects/BP_GE_P5_BasicAttackDamage.BP_GE_P5_BasicAttackDamage_C")));
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
	UAbilitySystemComponent* TargetASC = PendingTargetAbilitySystem.Get();
	TSubclassOf<UGameplayEffect> LoadedEffect = DamageEffectClass.LoadSynchronous();

	if (!SourceASC || !TargetASC || !LoadedEffect)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBasicAttackAbility::ActivateAbility - REJECTED SourceASC=%s TargetASC=%s DamageEffect=%s"),
			SourceASC ? *SourceASC->GetName() : TEXT("null"),
			TargetASC ? *TargetASC->GetName() : TEXT("null"),
			LoadedEffect ? *LoadedEffect->GetName() : TEXT("missing"));
		ClearPendingTarget();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(GetAvatarActorFromActorInfo());
	FGameplayEffectSpecHandle EffectSpec = SourceASC->MakeOutgoingSpec(LoadedEffect, 1.0f, EffectContext);
	if (!EffectSpec.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBasicAttackAbility::ActivateAbility - REJECTED MakeOutgoingSpec failed"));
		ClearPendingTarget();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FActiveGameplayEffectHandle AppliedHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*EffectSpec.Data.Get(), TargetASC);
	bLastActivationSucceeded = AppliedHandle.WasSuccessfullyApplied();
	if (bLastActivationSucceeded)
	{
		UE_LOG(LogTemp, Log, TEXT("UHSRBasicAttackAbility::ActivateAbility - SUCCESS Source=%s Target=%s Effect=%s"), *SourceASC->GetName(), *TargetASC->GetName(), *LoadedEffect->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBasicAttackAbility::ActivateAbility - FAILED Source=%s Target=%s Effect=%s"), *SourceASC->GetName(), *TargetASC->GetName(), *LoadedEffect->GetName());
	}

	ClearPendingTarget();
	EndAbility(Handle, ActorInfo, ActivationInfo, true, !bLastActivationSucceeded);
}
