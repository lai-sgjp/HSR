#include "HSRSkillAbility.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "../../Data/HSRSkillDefinition.h"

UHSRSkillAbility::UHSRSkillAbility() { SetActionContext(FGuid(), FName(TEXT("Skill"))); }
bool UHSRSkillAbility::ConfigureFromSkillDefinition(const UHSRSkillDefinition& Definition) { if (!Definition.IsValidSkillDefinition()) return false; EffectClass = Definition.EffectGameplayEffectClass; return !EffectClass.IsNull(); }
bool UHSRSkillAbility::SetPendingTarget(UAbilitySystemComponent* InTargetAbilitySystem) { if (!InTargetAbilitySystem) return false; PendingTargetAbilitySystem = InTargetAbilitySystem; return true; }
void UHSRSkillAbility::ClearPendingTarget() { PendingTargetAbilitySystem.Reset(); }
void UHSRSkillAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bLastActivationSucceeded = false;
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	UAbilitySystemComponent* TargetASC = PendingTargetAbilitySystem.Get();
	TSubclassOf<UGameplayEffect> LoadedEffect = EffectClass.LoadSynchronous();
	if (!SourceASC || !TargetASC || !LoadedEffect) { ClearPendingTarget(); EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext(); Context.AddSourceObject(GetAvatarActorFromActorInfo());
	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(LoadedEffect, 1.0f, Context);
	if (Spec.IsValid()) bLastActivationSucceeded = SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC).WasSuccessfullyApplied();
	ClearPendingTarget(); EndAbility(Handle, ActorInfo, ActivationInfo, true, !bLastActivationSucceeded);
}
