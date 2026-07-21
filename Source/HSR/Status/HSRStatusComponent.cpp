#include "HSRStatusComponent.h"

#include "AbilitySystemComponent.h"
#include "../Battle/HSRTurnManager.h"
#include "../Battle/HSRBattleCoordinator.h"
#include "../Data/Definitions/HSRStatusDefinition.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"

UHSRStatusComponent::UHSRStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHSRStatusComponent::RecordPublicOperation(EHSRStatusPublicOperation Operation, EHSRStatusOperationResult Result, FName StatusId, FName TargetId)
{
	LastPublicOperation.Operation = Operation;
	LastPublicOperation.Result = Result;
	LastPublicOperation.StatusId = StatusId;
	LastPublicOperation.TargetParticipantId = TargetId;
	LastPublicOperation.Sequence = static_cast<int64>(++PublicOperationSequence);
}

void UHSRStatusComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearStatus();
	UnbindTurnManager();
	Super::EndPlay(EndPlayReason);
}

bool UHSRStatusComponent::InitializeStatusRuntime(FName InParticipantId, UAbilitySystemComponent* InAbilitySystem)
{
	if (bInitialized || BoundTurnManager.IsValid() || TurnEndedHandle.IsValid() || ActiveStatus.IsSet()
		|| !ParticipantId.IsNone() || AbilitySystem.IsValid() || InParticipantId.IsNone() || !InAbilitySystem)
	{
		return false;
	}
	ParticipantId = InParticipantId;
	AbilitySystem = InAbilitySystem;
	bInitialized = true;
	return true;
}

bool UHSRStatusComponent::BindTurnManager(UHSRTurnManager* InTurnManager)
{
	if (!InTurnManager) return false;
	if (BoundTurnManager.Get() == InTurnManager && TurnEndedHandle.IsValid()) return true;
	ClearStatus();
	UnbindTurnManager();
	BoundTurnManager = InTurnManager;
	TurnEndedHandle = InTurnManager->OnTurnEnded().AddUObject(this, &UHSRStatusComponent::HandleTurnEnded);
	return TurnEndedHandle.IsValid();
}

void UHSRStatusComponent::BindBattleCoordinator(UHSRBattleCoordinator* InCoordinator)
{
	BoundCoordinator = InCoordinator;
}

void UHSRStatusComponent::UnbindTurnManager()
{
	if (BoundTurnManager.IsValid() && TurnEndedHandle.IsValid())
	{
		BoundTurnManager->OnTurnEnded().Remove(TurnEndedHandle);
	}
	TurnEndedHandle.Reset();
	BoundTurnManager.Reset();
}

EHSRStatusOperationResult UHSRStatusComponent::ValidateAdd(const UHSRStatusDefinition* Definition, FName SourceParticipantId, FName TargetParticipantId, TSubclassOf<UGameplayEffect>& OutEffectClass) const
{
	if (!Definition) return EHSRStatusOperationResult::InvalidDefinition;
	const EHSRStatusOperationResult DefinitionResult = Definition->Validate();
	if (DefinitionResult != EHSRStatusOperationResult::Success) return DefinitionResult;
	if (SourceParticipantId.IsNone()) return EHSRStatusOperationResult::InvalidSource;
	if (TargetParticipantId.IsNone() || TargetParticipantId != ParticipantId) return EHSRStatusOperationResult::InvalidTarget;
	if (!BoundTurnManager.IsValid() || BoundTurnManager->GetBattleEpoch() == 0) return EHSRStatusOperationResult::InvalidEpoch;
	if (!AbilitySystem.IsValid()) return EHSRStatusOperationResult::MissingAbilitySystem;
	if (AbilitySystem->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) <= 0.0f) return EHSRStatusOperationResult::DefeatedTarget;
	if (Definition->Classification == EHSRStatusClassification::Debuff && Definition->ImmunityTag.IsValid()
		&& AbilitySystem->HasMatchingGameplayTag(Definition->ImmunityTag)) return EHSRStatusOperationResult::Immune;
	OutEffectClass = Definition->InfiniteGameplayEffectClass.LoadSynchronous();
	if (!OutEffectClass) return EHSRStatusOperationResult::MissingGameplayEffect;
	if (OutEffectClass->GetDefaultObject<UGameplayEffect>()->DurationPolicy != EGameplayEffectDurationType::Infinite)
	{
		return EHSRStatusOperationResult::GameplayEffectNotInfinite;
	}
	return EHSRStatusOperationResult::Success;
}

EHSRStatusOperationResult UHSRStatusComponent::AddOrRefreshStatus(const UHSRStatusDefinition* Definition, FName SourceParticipantId, FName TargetParticipantId, FGuid OperationId)
{
	const bool bScopedStatusOperation = Definition && Definition->StatusId != FName(TEXT("Status.Buff.AttackUp"));
	const FString ScopedOperationKey = bScopedStatusOperation && OperationId.IsValid()
		? FString::Printf(TEXT("%s|%d|%s"), *OperationId.ToString(), static_cast<int32>(Definition->EffectKind), *Definition->StatusId.ToString()) : FString();
	if (OperationId.IsValid() && ((bScopedStatusOperation && ProcessedStatusOperations.Contains(ScopedOperationKey))
		|| (!bScopedStatusOperation && ProcessedAddOperations.Contains(OperationId)))) return LastResult = EHSRStatusOperationResult::IgnoredEvent;
	TSubclassOf<UGameplayEffect> EffectClass;
	LastResult = ValidateAdd(Definition, SourceParticipantId, TargetParticipantId, EffectClass);
	if (LastResult != EHSRStatusOperationResult::Success) return LastResult;
	if (Definition->StatusId != FName(TEXT("Status.Buff.AttackUp")))
	{
		FHSRStatusInstance* Existing = AdditionalStatuses.Find(Definition->StatusId);
		if (Existing)
		{
			if (!Existing->ActiveGameplayEffectHandle.IsValid() || Existing->BattleEpoch != BoundTurnManager->GetBattleEpoch()
				|| !AbilitySystem->GetActiveGameplayEffect(Existing->ActiveGameplayEffectHandle)
				|| AbilitySystem->GetCurrentStackCount(Existing->ActiveGameplayEffectHandle) != Existing->Stacks)
				return LastResult = EHSRStatusOperationResult::InvalidRuntimeInstance;
			if (Definition->RefreshPolicy == EHSRStatusRefreshPolicy::RefreshDuration || Existing->Stacks >= Definition->MaxStacks)
			{
				Existing->SourceParticipantId = SourceParticipantId;
				Existing->RemainingTurns = Definition->DurationTurns;
				LastResult = Definition->RefreshPolicy == EHSRStatusRefreshPolicy::RefreshDuration ? EHSRStatusOperationResult::Refreshed : EHSRStatusOperationResult::AtMaxRefreshed;
				Existing->LastOperationResult = LastResult;
				if (OperationId.IsValid()) ProcessedStatusOperations.Add(ScopedOperationKey);
				NotifyStatusChanged();
				return LastResult;
			}
			const FGameplayEffectSpecHandle StackSpec = AbilitySystem->MakeOutgoingSpec(EffectClass, 1.0f, AbilitySystem->MakeEffectContext());
			if (!StackSpec.IsValid()) return LastResult = EHSRStatusOperationResult::ApplyFailed;
			const FActiveGameplayEffectHandle StackHandle = AbilitySystem->ApplyGameplayEffectSpecToSelf(*StackSpec.Data.Get());
			if (!StackHandle.WasSuccessfullyApplied() || StackHandle != Existing->ActiveGameplayEffectHandle)
			{
				if (StackHandle.IsValid() && !AbilitySystem->RemoveActiveGameplayEffect(StackHandle)) { SecondaryOwnedHandle = StackHandle; return LastResult = EHSRStatusOperationResult::RemoveFailed; }
				return LastResult = EHSRStatusOperationResult::ApplyFailed;
			}
			const int32 ExpectedStacks = Existing->Stacks + 1;
			if (AbilitySystem->GetCurrentStackCount(Existing->ActiveGameplayEffectHandle) != ExpectedStacks)
			{
				const bool bRolledBack = AbilitySystem->RemoveActiveGameplayEffect(Existing->ActiveGameplayEffectHandle, 1);
				return LastResult = bRolledBack ? EHSRStatusOperationResult::ApplyFailed : EHSRStatusOperationResult::RemoveFailed;
			}
			Existing->Stacks = ExpectedStacks; Existing->SourceParticipantId = SourceParticipantId; Existing->RemainingTurns = Definition->DurationTurns; ++ApplyCount;
			LastResult = EHSRStatusOperationResult::StackAdded; Existing->LastOperationResult = LastResult; if (OperationId.IsValid()) ProcessedStatusOperations.Add(ScopedOperationKey); NotifyStatusChanged(); return LastResult;
		}
#if WITH_EDITOR
		if (bForceApplyFailure) return LastResult = EHSRStatusOperationResult::ApplyFailed;
#endif
		const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(EffectClass, 1.0f, AbilitySystem->MakeEffectContext());
		if (!Spec.IsValid()) return LastResult = EHSRStatusOperationResult::ApplyFailed;
		const FActiveGameplayEffectHandle Handle = AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		if (!Handle.WasSuccessfullyApplied()) return LastResult = EHSRStatusOperationResult::ApplyFailed;
		FHSRStatusInstance Instance; Instance.StatusId = Definition->StatusId; Instance.SourceParticipantId = SourceParticipantId; Instance.TargetParticipantId = TargetParticipantId; Instance.BattleEpoch = BoundTurnManager->GetBattleEpoch(); Instance.Stacks = 1; Instance.RemainingTurns = Definition->DurationTurns; Instance.ActiveGameplayEffectHandle = Handle; Instance.LastOperationResult = EHSRStatusOperationResult::Success; AdditionalStatuses.Add(Definition->StatusId, Instance); RuntimeDefinitions.Add(Definition->StatusId, const_cast<UHSRStatusDefinition*>(Definition)); ++ApplyCount;
		LastResult = EHSRStatusOperationResult::Success; if (OperationId.IsValid()) ProcessedStatusOperations.Add(ScopedOperationKey); NotifyStatusChanged(); return LastResult;
	}

	if (ActiveStatus.IsSet())
	{
		FHSRStatusInstance& Existing = ActiveStatus.GetValue();
		if (Existing.StatusId != Definition->StatusId || !Existing.ActiveGameplayEffectHandle.IsValid()
			|| Existing.BattleEpoch != BoundTurnManager->GetBattleEpoch()
			|| !AbilitySystem->GetActiveGameplayEffect(Existing.ActiveGameplayEffectHandle)
			|| AbilitySystem->GetCurrentStackCount(Existing.ActiveGameplayEffectHandle) != Existing.Stacks)
		{
			LastResult = EHSRStatusOperationResult::InvalidRuntimeInstance;
			return LastResult;
		}
		if (Definition->RefreshPolicy == EHSRStatusRefreshPolicy::RefreshDuration)
		{
			Existing.SourceParticipantId = SourceParticipantId;
			Existing.RemainingTurns = Definition->DurationTurns;
			LastResult = EHSRStatusOperationResult::Refreshed;
			Existing.LastOperationResult = LastResult;
			if (OperationId.IsValid()) ProcessedAddOperations.Add(OperationId);
			NotifyStatusChanged();
			return LastResult;
		}
		if (Existing.Stacks >= Definition->MaxStacks)
		{
			Existing.SourceParticipantId = SourceParticipantId;
			Existing.RemainingTurns = Definition->DurationTurns;
			LastResult = EHSRStatusOperationResult::AtMaxRefreshed;
			Existing.LastOperationResult = LastResult;
			if (OperationId.IsValid()) ProcessedAddOperations.Add(OperationId);
			NotifyStatusChanged();
			return LastResult;
		}
		const FGameplayEffectSpecHandle StackSpec = AbilitySystem->MakeOutgoingSpec(EffectClass, 1.0f, AbilitySystem->MakeEffectContext());
		if (!StackSpec.IsValid()) return LastResult = EHSRStatusOperationResult::ApplyFailed;
		const FActiveGameplayEffectHandle StackHandle = AbilitySystem->ApplyGameplayEffectSpecToSelf(*StackSpec.Data.Get());
		const int32 ExpectedStacks = Existing.Stacks + 1;
		if (!StackHandle.WasSuccessfullyApplied()) return LastResult = EHSRStatusOperationResult::ApplyFailed;
		if (StackHandle != Existing.ActiveGameplayEffectHandle)
		{
			if (StackHandle.IsValid() && !AbilitySystem->RemoveActiveGameplayEffect(StackHandle))
			{
				SecondaryOwnedHandle = StackHandle;
				return LastResult = EHSRStatusOperationResult::RemoveFailed;
			}
			return LastResult = EHSRStatusOperationResult::ApplyFailed;
		}
		if (AbilitySystem->GetCurrentStackCount(Existing.ActiveGameplayEffectHandle) != ExpectedStacks)
		{
			const bool bRolledBack = AbilitySystem->RemoveActiveGameplayEffect(Existing.ActiveGameplayEffectHandle, 1);
			return LastResult = bRolledBack && AbilitySystem->GetCurrentStackCount(Existing.ActiveGameplayEffectHandle) == Existing.Stacks
				? EHSRStatusOperationResult::ApplyFailed : EHSRStatusOperationResult::RemoveFailed;
		}
		Existing.Stacks = ExpectedStacks;
		Existing.SourceParticipantId = SourceParticipantId;
		Existing.RemainingTurns = Definition->DurationTurns;
		++ApplyCount;
		LastResult = EHSRStatusOperationResult::StackAdded;
		Existing.LastOperationResult = LastResult;
		if (OperationId.IsValid()) ProcessedAddOperations.Add(OperationId);
		NotifyStatusChanged();
		return LastResult;
	}

#if WITH_EDITOR
	if (bForceApplyFailure)
	{
		LastResult = EHSRStatusOperationResult::ApplyFailed;
		return LastResult;
	}
#endif
	const FGameplayEffectContextHandle Context = AbilitySystem->MakeEffectContext();
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(EffectClass, 1.0f, Context);
	if (!Spec.IsValid())
	{
		LastResult = EHSRStatusOperationResult::ApplyFailed;
		return LastResult;
	}
	const FActiveGameplayEffectHandle Handle = AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	if (!Handle.WasSuccessfullyApplied())
	{
		LastResult = EHSRStatusOperationResult::ApplyFailed;
		return LastResult;
	}

	FHSRStatusInstance Instance;
	Instance.StatusId = Definition->StatusId;
	Instance.SourceParticipantId = SourceParticipantId;
	Instance.TargetParticipantId = TargetParticipantId;
	Instance.BattleEpoch = BoundTurnManager->GetBattleEpoch();
	Instance.Stacks = 1;
	Instance.RemainingTurns = Definition->DurationTurns;
	Instance.ActiveGameplayEffectHandle = Handle;
	Instance.LastOperationResult = EHSRStatusOperationResult::Success;
	ActiveStatus = Instance;
	RuntimeDefinitions.Add(Definition->StatusId, const_cast<UHSRStatusDefinition*>(Definition));
	++ApplyCount;
	LastResult = EHSRStatusOperationResult::Success;
	if (OperationId.IsValid()) ProcessedAddOperations.Add(OperationId);
	NotifyStatusChanged();
	return LastResult;
}

EHSRStatusOperationResult UHSRStatusComponent::ReplaceStatus(const UHSRStatusDefinition* Definition, FName SourceParticipantId, FName TargetParticipantId)
{
	if (!ActiveStatus.IsSet() || SecondaryOwnedHandle.IsValid()) return LastResult = EHSRStatusOperationResult::InvalidRuntimeInstance;
	TSubclassOf<UGameplayEffect> EffectClass;
	LastResult = ValidateAdd(Definition, SourceParticipantId, TargetParticipantId, EffectClass);
	if (LastResult != EHSRStatusOperationResult::Success) return LastResult;
	const FActiveGameplayEffect* ExistingEffect = AbilitySystem->GetActiveGameplayEffect(ActiveStatus->ActiveGameplayEffectHandle);
	if (!ExistingEffect || AbilitySystem->GetCurrentStackCount(ActiveStatus->ActiveGameplayEffectHandle) != ActiveStatus->Stacks)
	{
		return LastResult = EHSRStatusOperationResult::InvalidRuntimeInstance;
	}
	if (ExistingEffect->Spec.Def == EffectClass.GetDefaultObject())
	{
		return LastResult = EHSRStatusOperationResult::InvalidRuntimeInstance;
	}
#if WITH_EDITOR
	if (bForceApplyFailure) return LastResult = EHSRStatusOperationResult::ApplyFailed;
#endif
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(EffectClass, 1.0f, AbilitySystem->MakeEffectContext());
	if (!Spec.IsValid()) return LastResult = EHSRStatusOperationResult::ApplyFailed;
	const FActiveGameplayEffectHandle NewHandle = AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	if (!NewHandle.WasSuccessfullyApplied())
	{
		return LastResult = EHSRStatusOperationResult::ApplyFailed;
	}
	if (NewHandle == ActiveStatus->ActiveGameplayEffectHandle)
	{
		const bool bRolledBack = AbilitySystem->RemoveActiveGameplayEffect(NewHandle, 1);
		return LastResult = bRolledBack && AbilitySystem->GetCurrentStackCount(NewHandle) == ActiveStatus->Stacks
			? EHSRStatusOperationResult::ApplyFailed : EHSRStatusOperationResult::RemoveFailed;
	}
	++ApplyCount;
#if WITH_EDITOR
	const bool bOldRemoved = !bForceOldRemoveFailure && AbilitySystem->RemoveActiveGameplayEffect(ActiveStatus->ActiveGameplayEffectHandle);
#else
	const bool bOldRemoved = AbilitySystem->RemoveActiveGameplayEffect(ActiveStatus->ActiveGameplayEffectHandle);
#endif
	if (!bOldRemoved)
	{
		SecondaryOwnedHandle = NewHandle;
		return LastResult = EHSRStatusOperationResult::RemoveFailed;
	}
	++RemoveCount;
	ActiveStatus->StatusId = Definition->StatusId;
	ActiveStatus->SourceParticipantId = SourceParticipantId;
	ActiveStatus->TargetParticipantId = TargetParticipantId;
	ActiveStatus->BattleEpoch = BoundTurnManager->GetBattleEpoch();
	ActiveStatus->Stacks = 1;
	ActiveStatus->RemainingTurns = Definition->DurationTurns;
	ActiveStatus->LastConsumedTurnSequence = 0;
	ActiveStatus->ActiveGameplayEffectHandle = NewHandle;
	LastResult = EHSRStatusOperationResult::Replaced;
	ActiveStatus->LastOperationResult = LastResult;
	NotifyStatusChanged();
	return LastResult;
}

void UHSRStatusComponent::HandleTurnEnded(const FHSRTurnLifecycleEvent& Event)
{
	if (Event.EventType != EHSRTurnLifecycleEventType::TurnEnded) return;
	TArray<FName> StatusIdsToClear;
	bool bLethalDamage = false;
	UHSRBattleCoordinator* Coordinator = BoundCoordinator.Get();
	for (TPair<FName, FHSRStatusInstance>& Pair : AdditionalStatuses)
	{
		FHSRStatusInstance& Instance = Pair.Value;
		if (Event.ParticipantId != Instance.TargetParticipantId || Event.BattleEpoch != Instance.BattleEpoch || Event.TurnSequence <= Instance.LastConsumedTurnSequence) continue;
		const UHSRStatusDefinition* Definition = RuntimeDefinitions.FindRef(Instance.StatusId);
		if (Definition && Definition->EffectKind == EHSRStatusEffectKind::DamageOverTime)
		{
			if (!Coordinator) continue;
			const FHSRDamageResult Damage = Coordinator->ResolveStatusDamage(Instance.SourceParticipantId, Instance.TargetParticipantId, FGuid::NewGuid(), Definition);
			if (Damage.Result != EHSRDamageResultType::DamageResolved) continue;
			bLethalDamage = AbilitySystem.IsValid() && AbilitySystem->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) <= 0.0f;
			if (bLethalDamage) break;
			Instance.LastConsumedTurnSequence = Event.TurnSequence;
			Instance.RemainingTurns = FMath::Max(0, Instance.RemainingTurns - 1);
			Instance.LastOperationResult = EHSRStatusOperationResult::Triggered;
			if (Instance.RemainingTurns == 0) StatusIdsToClear.Add(Instance.StatusId);
			else NotifyStatusChanged();
			Coordinator->FinalizeStatusDamage();
			continue;
		}
		Instance.LastConsumedTurnSequence = Event.TurnSequence;
		Instance.RemainingTurns = FMath::Max(0, Instance.RemainingTurns - 1);
		Instance.LastOperationResult = EHSRStatusOperationResult::Triggered;
		if (Instance.RemainingTurns == 0) StatusIdsToClear.Add(Instance.StatusId);
		else NotifyStatusChanged();
	}
	if (bLethalDamage)
	{
		ClearStatus();
		Coordinator->FinalizeStatusDamage();
		return;
	}
	for (const FName StatusId : StatusIdsToClear) ClearStatusById(StatusId, EHSRStatusPublicOperation::Expire);
	if (!ActiveStatus.IsSet()) return;
	FHSRStatusInstance& Instance = ActiveStatus.GetValue();
	if (Event.ParticipantId != Instance.TargetParticipantId || Event.BattleEpoch != Instance.BattleEpoch
		|| Event.TurnSequence <= Instance.LastConsumedTurnSequence)
	{
		return;
	}
	Instance.LastConsumedTurnSequence = Event.TurnSequence;
	Instance.RemainingTurns = FMath::Max(0, Instance.RemainingTurns - 1);
	Instance.LastOperationResult = EHSRStatusOperationResult::Triggered;
	if (Instance.RemainingTurns == 0)
	{
		ClearStatus();
	}
	else
	{
		NotifyStatusChanged();
	}
}

EHSRStatusOperationResult UHSRStatusComponent::ClearStatusById(FName StatusId, EHSRStatusPublicOperation Operation)
{
	if (StatusId == FName(TEXT("Status.Buff.AttackUp"))) return ClearStatus();
	if (FHSRStatusInstance* Instance = AdditionalStatuses.Find(StatusId))
	{
		#if WITH_EDITOR
		if (bForceDispelRemoveFailure) return LastResult = EHSRStatusOperationResult::RemoveFailed;
		#endif
		LastRemovedRemainingTurns = Instance->RemainingTurns;
		LastRemovedTurnSequence = Instance->LastConsumedTurnSequence;
		if (AbilitySystem.IsValid() && Instance->ActiveGameplayEffectHandle.IsValid() && AbilitySystem->GetActiveGameplayEffect(Instance->ActiveGameplayEffectHandle))
		{
			if (AbilitySystem->RemoveActiveGameplayEffect(Instance->ActiveGameplayEffectHandle)) ++RemoveCount;
			else return LastResult = EHSRStatusOperationResult::RemoveFailed;
		}
		const FName RemovedTargetId = Instance->TargetParticipantId;
		AdditionalStatuses.Remove(StatusId); RuntimeDefinitions.Remove(StatusId);
		RecordPublicOperation(Operation, Operation == EHSRStatusPublicOperation::Dispel ? EHSRStatusOperationResult::Dispelled : EHSRStatusOperationResult::Success, StatusId, RemovedTargetId);
		NotifyStatusChanged();
		return LastResult = EHSRStatusOperationResult::Success;
	}
	return LastResult = EHSRStatusOperationResult::NoDispelCandidate;
}

EHSRStatusOperationResult UHSRStatusComponent::DispelOneStatus()
{
	TArray<FName> Candidates;
	for (const TPair<FName, FHSRStatusInstance>& Pair : AdditionalStatuses)
	{
		const UHSRStatusDefinition* Definition = RuntimeDefinitions.FindRef(Pair.Key);
		if (Definition && Definition->Classification == EHSRStatusClassification::Debuff && Definition->bDispellable) Candidates.Add(Pair.Key);
	}
	Candidates.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
	if (Candidates.IsEmpty()) return LastResult = EHSRStatusOperationResult::NoDispelCandidate;
	const EHSRStatusOperationResult RemoveResult = ClearStatusById(Candidates[0], EHSRStatusPublicOperation::Dispel);
	return LastResult = RemoveResult == EHSRStatusOperationResult::Success ? EHSRStatusOperationResult::Dispelled : RemoveResult;
}

int32 UHSRStatusComponent::HandleSourceInvalid(FName SourceParticipantId)
{
	if (SourceParticipantId.IsNone()) return 0;
	const FString InvalidSourceKey = FString::Printf(TEXT("%llu|%s"), BoundTurnManager.IsValid() ? BoundTurnManager->GetBattleEpoch() : 0, *SourceParticipantId.ToString());
	if (ProcessedInvalidSources.Contains(InvalidSourceKey)) return 0;
	TArray<FName> ToRemove;
	for (const TPair<FName, FHSRStatusInstance>& Pair : AdditionalStatuses)
	{
		const UHSRStatusDefinition* Definition = RuntimeDefinitions.FindRef(Pair.Key);
		if (Pair.Value.SourceParticipantId == SourceParticipantId && Definition && Definition->SourceInvalidPolicy == EHSRSourceInvalidPolicy::Remove) ToRemove.Add(Pair.Key);
	}
	if (ActiveStatus.IsSet() && ActiveStatus->SourceParticipantId == SourceParticipantId)
	{
		const UHSRStatusDefinition* Definition = RuntimeDefinitions.FindRef(ActiveStatus->StatusId);
		if (Definition && Definition->SourceInvalidPolicy == EHSRSourceInvalidPolicy::Remove) ToRemove.Add(ActiveStatus->StatusId);
	}
	int32 Removed = 0;
	bool bAllRemoved = true;
	for (const FName StatusId : ToRemove)
	{
		if (ClearStatusById(StatusId) == EHSRStatusOperationResult::Success) ++Removed;
		else bAllRemoved = false;
	}
	if (bAllRemoved) ProcessedInvalidSources.Add(InvalidSourceKey);
	return Removed;
}

EHSRStatusOperationResult UHSRStatusComponent::ClearStatus()
{
	FName ClearedStatusId = ActiveStatus.IsSet() ? ActiveStatus->StatusId : NAME_None;
	FName ClearedTargetId = ActiveStatus.IsSet() ? ActiveStatus->TargetParticipantId : ParticipantId;
	if (ClearedStatusId.IsNone() && !AdditionalStatuses.IsEmpty()) { const FHSRStatusInstance& First = AdditionalStatuses.CreateConstIterator().Value(); ClearedStatusId = First.StatusId; ClearedTargetId = First.TargetParticipantId; }
	bool bAnyRemoveFailed = false;
	if (ActiveStatus.IsSet())
	{
		LastRemovedRemainingTurns = ActiveStatus->RemainingTurns;
		LastRemovedTurnSequence = ActiveStatus->LastConsumedTurnSequence;
		const FActiveGameplayEffectHandle Handle = ActiveStatus->ActiveGameplayEffectHandle;
		if (AbilitySystem.IsValid() && Handle.IsValid() && AbilitySystem->GetActiveGameplayEffect(Handle))
		{
			if (AbilitySystem->RemoveActiveGameplayEffect(Handle)) ++RemoveCount;
			else bAnyRemoveFailed = true;
		}
	}
	if (SecondaryOwnedHandle.IsValid())
	{
		if (AbilitySystem.IsValid() && AbilitySystem->GetActiveGameplayEffect(SecondaryOwnedHandle))
		{
			if (AbilitySystem->RemoveActiveGameplayEffect(SecondaryOwnedHandle)) ++RemoveCount;
			else bAnyRemoveFailed = true;
		}
		SecondaryOwnedHandle.Invalidate();
	}
	ActiveStatus.Reset();
	for (const TPair<FName, FHSRStatusInstance>& Pair : AdditionalStatuses)
	{
		LastRemovedRemainingTurns = Pair.Value.RemainingTurns;
		LastRemovedTurnSequence = Pair.Value.LastConsumedTurnSequence;
		if (AbilitySystem.IsValid() && Pair.Value.ActiveGameplayEffectHandle.IsValid() && AbilitySystem->GetActiveGameplayEffect(Pair.Value.ActiveGameplayEffectHandle))
		{
			if (AbilitySystem->RemoveActiveGameplayEffect(Pair.Value.ActiveGameplayEffectHandle)) ++RemoveCount; else bAnyRemoveFailed = true;
		}
	}
	AdditionalStatuses.Empty(); RuntimeDefinitions.Empty();
	LastResult = bAnyRemoveFailed ? EHSRStatusOperationResult::RemoveFailed : EHSRStatusOperationResult::Success;
	RecordPublicOperation(EHSRStatusPublicOperation::Clear, LastResult, ClearedStatusId, ClearedTargetId);
	NotifyStatusChanged();
	return LastResult;
}

FHSRStatusRuntimeSnapshot UHSRStatusComponent::GetSnapshot(FName StatusId) const
{
	FHSRStatusRuntimeSnapshot Snapshot;
	Snapshot.Result = LastResult;
	Snapshot.ApplyCount = ApplyCount;
	Snapshot.RemoveCount = RemoveCount;
	Snapshot.LastRemovedRemainingTurns = LastRemovedRemainingTurns;
	Snapshot.LastRemovedTurnSequence = LastRemovedTurnSequence;
	const FHSRStatusInstance* SelectedInstance = nullptr;
	if (!StatusId.IsNone())
	{
		SelectedInstance = ActiveStatus.IsSet() && ActiveStatus->StatusId == StatusId ? &ActiveStatus.GetValue() : AdditionalStatuses.Find(StatusId);
	}
	else if (ActiveStatus.IsSet())
	{
		SelectedInstance = &ActiveStatus.GetValue();
	}
	else if (!AdditionalStatuses.IsEmpty())
	{
		SelectedInstance = &AdditionalStatuses.CreateConstIterator().Value();
	}
	if (SelectedInstance)
	{
		const FHSRStatusInstance& Instance = *SelectedInstance;
		Snapshot.StatusId = Instance.StatusId;
		Snapshot.SourceParticipantId = Instance.SourceParticipantId;
		Snapshot.TargetParticipantId = Instance.TargetParticipantId;
		Snapshot.BattleEpoch = Instance.BattleEpoch;
		Snapshot.LastConsumedTurnSequence = Instance.LastConsumedTurnSequence;
		Snapshot.RemainingTurns = Instance.RemainingTurns;
		Snapshot.Stacks = Instance.Stacks;
		Snapshot.InstanceCount = 1;
		Snapshot.bHandleValid = Instance.ActiveGameplayEffectHandle.IsValid();
		Snapshot.ActiveHandleIdentity = Instance.ActiveGameplayEffectHandle.ToString();
		Snapshot.bHandleActiveInAbilitySystem = AbilitySystem.IsValid()
			&& AbilitySystem->GetActiveGameplayEffect(Instance.ActiveGameplayEffectHandle) != nullptr;
		Snapshot.GameplayEffectStackCount = AbilitySystem.IsValid() ? AbilitySystem->GetCurrentStackCount(Instance.ActiveGameplayEffectHandle) : 0;
	}
	Snapshot.bSecondaryHandleValid = SecondaryOwnedHandle.IsValid();
	Snapshot.SecondaryHandleIdentity = SecondaryOwnedHandle.IsValid() ? SecondaryOwnedHandle.ToString() : FString();
	Snapshot.bSecondaryHandleActiveInAbilitySystem = AbilitySystem.IsValid() && SecondaryOwnedHandle.IsValid()
		&& AbilitySystem->GetActiveGameplayEffect(SecondaryOwnedHandle) != nullptr;
	return Snapshot;
}

TArray<FHSRStatusPublicSnapshot> UHSRStatusComponent::GetPublicSnapshots() const
{
	TArray<FHSRStatusPublicSnapshot> Result;
	const auto Append = [this, &Result](const FHSRStatusInstance& Instance)
	{
		const UHSRStatusDefinition* Definition = RuntimeDefinitions.FindRef(Instance.StatusId);
		FHSRStatusPublicSnapshot& Snapshot = Result.AddDefaulted_GetRef();
		Snapshot.StatusId = Instance.StatusId;
		Snapshot.TargetParticipantId = Instance.TargetParticipantId;
		Snapshot.DisplayName = Definition && !Definition->DisplayName.IsEmpty() ? Definition->DisplayName : FText::FromName(Instance.StatusId);
		Snapshot.Classification = Definition ? Definition->Classification : EHSRStatusClassification::Buff;
		Snapshot.Stacks = Instance.Stacks;
		Snapshot.RemainingTurns = Instance.RemainingTurns;
		Snapshot.LastResult = Instance.LastOperationResult;
	};
	if (ActiveStatus.IsSet()) Append(ActiveStatus.GetValue());
	for (const TPair<FName, FHSRStatusInstance>& Pair : AdditionalStatuses) Append(Pair.Value);
	Result.Sort([](const FHSRStatusPublicSnapshot& A, const FHSRStatusPublicSnapshot& B) { return A.StatusId.LexicalLess(B.StatusId); });
	return Result;
}
