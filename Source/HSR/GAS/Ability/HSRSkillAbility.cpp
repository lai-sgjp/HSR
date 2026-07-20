#include "HSRSkillAbility.h"
#include "AbilitySystemComponent.h"
#include "../../Data/HSRSkillDefinition.h"

UHSRSkillAbility::UHSRSkillAbility() { SetActionContext(FGuid(), FName(TEXT("Skill"))); }
bool UHSRSkillAbility::ConfigureFromSkillDefinition(const UHSRSkillDefinition& Definition) { if (!Definition.IsValidSkillDefinition()) return false; EffectClass = Definition.EffectGameplayEffectClass; return !EffectClass.IsNull(); }
bool UHSRSkillAbility::SetPendingTarget(UAbilitySystemComponent* InTargetAbilitySystem) { if (!InTargetAbilitySystem) return false; PendingTargetAbilitySystem = InTargetAbilitySystem; return true; }
void UHSRSkillAbility::ClearPendingTarget() { PendingTargetAbilitySystem.Reset(); }
void UHSRSkillAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bLastActivationSucceeded = false;
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!SourceASC) { ClearPendingTarget(); EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	bLastActivationSucceeded = ApplyPreparedFormalDamage(SourceASC);
	ClearPendingTarget(); EndAbility(Handle, ActorInfo, ActivationInfo, true, !bLastActivationSucceeded);
}
