#include "HSRUltimateAbility.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectComponents/AdditionalEffectsGameplayEffectComponent.h"
#include "GameplayEffectComponents/CustomCanApplyGameplayEffectComponent.h"
#include "../Attribute/HSRCoreAttributeSet.h"
#include "../../Data/HSRSkillDefinition.h"

namespace
{
	// Compensation is safe only for a single, static, instant Energy modifier with no
	// executions, cues, application requirements or secondary modifiers to replay.
	bool ReadPureEnergyEffect(const UGameplayEffect* Effect, float& OutMagnitude)
	{
		OutMagnitude = 0.0f;
		if (!Effect || Effect->DurationPolicy != EGameplayEffectDurationType::Instant
			|| Effect->Modifiers.Num() != 1 || !Effect->Executions.IsEmpty()
			|| !Effect->GameplayCues.IsEmpty() || !Effect->GetGrantedTags().IsEmpty()
			|| !Effect->GetBlockedAbilityTags().IsEmpty()
			|| Effect->FindComponent<UAdditionalEffectsGameplayEffectComponent>()
			|| Effect->FindComponent<UCustomCanApplyGameplayEffectComponent>()) return false;
		const FGameplayModifierInfo& Modifier = Effect->Modifiers[0];
		return Modifier.Attribute == UHSRCoreAttributeSet::GetEnergyAttribute()
			&& Modifier.ModifierOp == EGameplayModOp::Additive
			&& Modifier.ModifierMagnitude.GetStaticMagnitudeIfPossible(1.0f, OutMagnitude)
			&& FMath::IsFinite(OutMagnitude);
	}

	bool IsBlocked(const UAbilitySystemComponent* ASC)
	{
		if (!ASC || ASC->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) <= 0.0f) return true;
		for (const TCHAR* Name : {TEXT("State.Stunned"), TEXT("State.UltimateBlocked")})
		{
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(Name), false);
			if (Tag.IsValid() && ASC->HasMatchingGameplayTag(Tag)) return true;
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
	if (bActivationInProgress) return false;
	bConfigured = false;
	bLastActivationSucceeded = false;
	LastFailureReason = EHSRAbilityFailureReason::DefinitionMissing;
	ClearPendingTarget();
	ClearPreparedFormalDamage();
	CostGameplayEffectClass = nullptr;
	UltimateEffectClass.Reset();
	EnergyRefundEffectClass.Reset();
	if (!Definition.IsValidUltimateDefinition()) return false;

	const TSubclassOf<UGameplayEffect> Cost = Definition.CostGameplayEffectClass.LoadSynchronous();
	const TSubclassOf<UGameplayEffect> Refund = Definition.EnergyRefundGameplayEffectClass.LoadSynchronous();
	const TSubclassOf<UGameplayEffect> Effect = Definition.EffectGameplayEffectClass.LoadSynchronous();
	float CostMagnitude = 0.0f, RefundMagnitude = 0.0f;
	if (!Cost || !Refund || !Effect
		|| !ReadPureEnergyEffect(Cost->GetDefaultObject<UGameplayEffect>(), CostMagnitude)
		|| !ReadPureEnergyEffect(Refund->GetDefaultObject<UGameplayEffect>(), RefundMagnitude)
		|| CostMagnitude >= 0.0f || !FMath::IsNearlyEqual(-CostMagnitude, RefundMagnitude)) return false;

	CostGameplayEffectClass = Cost;
	UltimateEffectClass = Definition.EffectGameplayEffectClass;
	EnergyRefundEffectClass = Definition.EnergyRefundGameplayEffectClass;
	EnergyCost = -CostMagnitude;
	ReleaseThreshold = Definition.UltimateReleaseThreshold > 0.0f ? Definition.UltimateReleaseThreshold : EnergyCost;
	bRestoresHealth = Definition.RestoresHealth();
	LastExecutedActionId.Invalidate();
	bConfigured = true;
	LastFailureReason = EHSRAbilityFailureReason::None;
	return true;
}

EHSRAbilityFailureReason UHSRUltimateAbility::GetPreActivationFailureReason(
	const FGameplayAbilitySpecHandle& Handle, const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!bConfigured) return EHSRAbilityFailureReason::DefinitionMissing;
	if (bActivationInProgress || IsBlocked(ASC)) return EHSRAbilityFailureReason::AbilityUnavailable;
	const float Energy = ASC->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute());
	return FMath::IsFinite(Energy) && Energy >= ReleaseThreshold && CheckCost(Handle, ActorInfo, nullptr)
		? EHSRAbilityFailureReason::None : EHSRAbilityFailureReason::InsufficientEnergy;
}

bool UHSRUltimateAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	return GetPreActivationFailureReason(Handle, ActorInfo) == EHSRAbilityFailureReason::None
		&& Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

bool UHSRUltimateAbility::SetPendingTarget(UAbilitySystemComponent* Target)
{
	return SetPendingTargets({Target});
}

bool UHSRUltimateAbility::SetPendingTargets(const TArray<UAbilitySystemComponent*>& Targets)
{
	if (bActivationInProgress) return false;
	ClearPendingTarget();
	ClearPreparedFormalDamage();
	bLastActivationSucceeded = false;
	LastFailureReason = EHSRAbilityFailureReason::InvalidTarget;
	if (Targets.IsEmpty()) return false;
	for (UAbilitySystemComponent* Target : Targets)
	{
		if (!IsValid(Target) || Target->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) <= 0.0f
			|| PendingTargetAbilitySystems.Contains(Target))
		{
			ClearPendingTarget();
			return false;
		}
		PendingTargetAbilitySystems.Add(Target);
	}
	LastFailureReason = EHSRAbilityFailureReason::None;
	return true;
}

void UHSRUltimateAbility::ClearPendingTarget()
{
	PendingTargetAbilitySystems.Reset();
}

void UHSRUltimateAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// CommitAbility is synchronous. Neither a reentrant delegate nor a duplicated commit
	// is allowed to apply a second Cost within this execution.
	if (bCostAttempted) return;
	bCostAttempted = true;
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC || !CostGameplayEffectClass) return;
	const float Before = ASC->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute());
	const FActiveGameplayEffectHandle Applied = ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo,
		GetCostGameplayEffect(), GetAbilityLevel(Handle, ActorInfo));
	const float After = ASC->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute());
	// UE5.6 AbilitySystemComponent.cpp creates MyHandle(INDEX_NONE) for an instant GE
	// with bPassedFiltersAndWasExecuted=true. WasSuccessfullyApplied is the execution
	// result; IsValid would wrongly require a persistent active handle. Net Energy
	// can include legitimate GE gains from synchronous attribute-change delegates.
	bCostApplied = Applied.WasSuccessfullyApplied();
	UE_LOG(LogTemp, Verbose, TEXT("UltimateCost Applied=%d Authored=%.2f NetEnergyDelta=%.2f"),
		bCostApplied, EnergyCost, After - Before);
}

void UHSRUltimateAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bLastActivationSucceeded = false;
	GetMutableFormalDamageExecutionResult() = FHSRFormalDamageExecutionResult();
	LastFailureReason = GetPreActivationFailureReason(Handle, ActorInfo);
	// Action identity is checked only at execution: UI preflight still carries the
	// previous action context and must remain a pure resource/status availability query.
	if (GetActionId().IsValid() && GetActionId() == LastExecutedActionId)
		LastFailureReason = EHSRAbilityFailureReason::DuplicateAction;
	UAbilitySystemComponent* Source = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const bool bTargetsValid = !PendingTargetAbilitySystems.IsEmpty()
		&& !PendingTargetAbilitySystems.ContainsByPredicate([](const auto& Target)
		{
			return !Target.IsValid() || Target->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) <= 0.0f;
		});
	if (LastFailureReason != EHSRAbilityFailureReason::None || !bTargetsValid)
	{
		if (LastFailureReason == EHSRAbilityFailureReason::None) LastFailureReason = EHSRAbilityFailureReason::InvalidTarget;
		ClearPendingTarget(); ClearPreparedFormalDamage();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bActivationInProgress = true;
	const bool bPreviouslyCancelable = CanBeCanceled();
	// Cost and instant effects form a synchronous atomic action. External cancellation
	// (including ASC delegates fired by Cost) cannot cut it between spend and settlement.
	SetCanBeCanceled(false);
	bCostAttempted = false;
	bCostApplied = false;
	const float EnergyBefore = Source->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute());
	const auto RefundClass = EnergyRefundEffectClass.LoadSynchronous();
	const auto Context = Source->MakeEffectContext();
	const FGameplayEffectSpecHandle Refund = Source->MakeOutgoingSpec(RefundClass, 1.0f, Context);
	TArray<FGameplayEffectSpecHandle> HealSpecs;
	bool bHealPreflightPassed = true;
	if (bRestoresHealth)
	{
		const auto HealClass = UltimateEffectClass.LoadSynchronous();
		for (const auto& Target : PendingTargetAbilitySystems)
		{
			FGameplayEffectSpecHandle Heal = Source->MakeOutgoingSpec(HealClass, 1.0f, Context);
			if (!Target.IsValid() || !Heal.IsValid()) { bHealPreflightPassed = false; break; }
			const auto& Container = Target->GetActiveGameplayEffects();
			if (!Heal.Data->Def->CanApply(Container, *Heal.Data)) { bHealPreflightPassed = false; break; }
			for (const auto& Query : Target->GameplayEffectApplicationQueries)
			{
				if (!Query.Execute(Container, *Heal.Data)) { bHealPreflightPassed = false; break; }
			}
			if (!bHealPreflightPassed) break;
			HealSpecs.Add(Heal);
		}
	}

	if (!Refund.IsValid() || !bHealPreflightPassed)
	{
		LastFailureReason = EHSRAbilityFailureReason::EffectFailed;
	}
	else if (!CommitAbility(Handle, ActorInfo, ActivationInfo) || !bCostApplied)
	{
		LastFailureReason = EHSRAbilityFailureReason::CommitFailed;
	}
	else
	{
		if (bRestoresHealth)
		{
			int32 AppliedCount = 0;
			for (int32 Index = 0; Index < HealSpecs.Num(); ++Index)
			{
				UAbilitySystemComponent* Target = PendingTargetAbilitySystems[Index].Get();
				if (!IsValid(Target) || Target->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) <= 0.f) continue;
				if (Source->ApplyGameplayEffectSpecToTarget(*HealSpecs[Index].Data.Get(), Target).WasSuccessfullyApplied()) ++AppliedCount;
			}
			// Preflight handles ordinary refusal before Cost. If a callback changes a later
			// target after the commit, already settled heals remain authoritative: consume
			// once and report the partial settlement, never refund a partly applied action.
			bLastActivationSucceeded = AppliedCount > 0;
			if (AppliedCount != HealSpecs.Num())
				UE_LOG(LogTemp, Warning, TEXT("UltimateHeal PartialSettlement ActionId=%s Applied=%d Requested=%d CostCommitted=1"),
					*GetActionId().ToString(), AppliedCount, HealSpecs.Num());
		}
		else bLastActivationSucceeded = ApplyPreparedFormalDamage(Source);
		LastFailureReason = bLastActivationSucceeded ? EHSRAbilityFailureReason::None : EHSRAbilityFailureReason::EffectFailed;
	}

	FHSRFormalDamageExecutionResult& Result = GetMutableFormalDamageExecutionResult();
	if (bRestoresHealth) Result.bSucceeded = bLastActivationSucceeded;
	Result.bCostCommitted = bCostApplied;
	Result.EnergyBefore = EnergyBefore;
	Result.bRefundApplied = false;
	if (!bLastActivationSucceeded && bCostApplied)
	{
		const FActiveGameplayEffectHandle AppliedRefund = Source->ApplyGameplayEffectSpecToSelf(*Refund.Data.Get());
		Result.bRefundApplied = AppliedRefund.WasSuccessfullyApplied();
		ensureMsgf(Result.bRefundApplied, TEXT("Ultimate compensation GE was rejected for ActionId=%s"), *GetActionId().ToString());
	}
	Result.EnergyAfter = Source->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute());
	if (GetActionId().IsValid()) LastExecutedActionId = GetActionId();
	UE_LOG(LogTemp, Verbose, TEXT("UltimateGAS ActionId=%s CostCommitted=%d EffectResolved=%d Compensation=%d EnergyBefore=%.2f EnergyAfter=%.2f"),
		*GetActionId().ToString(), bCostApplied, bLastActivationSucceeded, Result.bRefundApplied, EnergyBefore, Result.EnergyAfter);
	ClearPendingTarget(); ClearPreparedFormalDamage();
	bActivationInProgress = false;
	SetCanBeCanceled(bPreviouslyCancelable);
	if (IsActive()) EndAbility(Handle, ActorInfo, ActivationInfo, true, !bLastActivationSucceeded);
}
