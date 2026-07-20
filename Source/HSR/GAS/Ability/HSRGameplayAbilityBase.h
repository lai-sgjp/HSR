#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "HSRAbilityTypes.h"
#include "../Damage/HSRDamageTypes.h"
#include "GameplayEffectTypes.h"
#include "HSRGameplayAbilityBase.generated.h"

class UAbilitySystemComponent;
class UHSRSkillDefinition;

UCLASS(Abstract)
class HSR_API UHSRGameplayAbilityBase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UHSRGameplayAbilityBase();

	void SetActionContext(const FGuid& InActionId, FName InSkillId)
	{
		ActionId = InActionId;
		SkillId = InSkillId;
	}

	FGuid GetActionId() const { return ActionId; }
	FName GetSkillId() const { return SkillId; }

	/** Synchronous target handoff for the P6-001 action slice. Derived abilities
	 * that support it must consume and clear the target during activation. */
	virtual bool SetPendingTarget(UAbilitySystemComponent* InTargetAbilitySystem);
	virtual void ClearPendingTarget();
	virtual bool DidLastActivationSucceed() const;
	virtual EHSRAbilityFailureReason GetLastFailureReason() const;
	virtual EHSRAbilityFailureReason GetPreActivationFailureReason(const FGameplayAbilitySpecHandle& Handle, const FGameplayAbilityActorInfo* ActorInfo) const;
	virtual bool ConfigureFromSkillDefinition(const UHSRSkillDefinition& Definition);
	bool PrepareFormalDamage(const FHSRFormalDamageRequest& Request, const FGameplayEffectSpecHandle& Spec, UAbilitySystemComponent* TargetAbilitySystem, FHSRFormalDamagePrepareResult& OutResult);
	const FHSRFormalDamageExecutionResult& GetLastFormalDamageExecutionResult() const { return LastFormalDamageExecutionResult; }
	void ClearPreparedFormalDamage();
	bool ApplyPreparedFormalDamage(UAbilitySystemComponent* SourceAbilitySystem);

protected:
	FHSRFormalDamageExecutionResult& GetMutableFormalDamageExecutionResult() { return LastFormalDamageExecutionResult; }

private:
	FGuid ActionId;
	FName SkillId;
	struct FHSRPreparedFormalDamage
	{
		FHSRFormalDamageRequest Request;
		FGameplayEffectSpecHandle Spec;
		TWeakObjectPtr<UAbilitySystemComponent> Target;
		bool bPrepared = false;
	};
	FHSRPreparedFormalDamage PreparedFormalDamage;
	FHSRFormalDamageExecutionResult LastFormalDamageExecutionResult;
};
