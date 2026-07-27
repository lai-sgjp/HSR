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
	if (bInitialized || BoundTurnManager.IsValid() || TurnEndedHandle.IsValid() || !Statuses.IsEmpty()
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
	if (BoundTurnManager.IsValid() && TurnEndedHandle.IsValid()) BoundTurnManager->OnTurnEnded().Remove(TurnEndedHandle);
	TurnEndedHandle.Reset();
	BoundTurnManager.Reset();
}

EHSRStatusOperationResult UHSRStatusComponent::ValidateAdd(const UHSRStatusDefinition* Definition, FName SourceParticipantId, FName TargetParticipantId, TSubclassOf<UGameplayEffect>& OutEffectClass) const
{
	if (!Definition) return EHSRStatusOperationResult::InvalidDefinition;
	if (const EHSRStatusOperationResult Result = Definition->Validate(); Result != EHSRStatusOperationResult::Success) return Result;
	if (SourceParticipantId.IsNone()) return EHSRStatusOperationResult::InvalidSource;
	if (TargetParticipantId.IsNone() || TargetParticipantId != ParticipantId) return EHSRStatusOperationResult::InvalidTarget;
	if (!BoundTurnManager.IsValid() || BoundTurnManager->GetBattleEpoch() == 0) return EHSRStatusOperationResult::InvalidEpoch;
	if (!AbilitySystem.IsValid()) return EHSRStatusOperationResult::MissingAbilitySystem;
	if (AbilitySystem->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) <= 0.0f) return EHSRStatusOperationResult::DefeatedTarget;
	if (Definition->Classification == EHSRStatusClassification::Debuff && Definition->ImmunityTag.IsValid() && AbilitySystem->HasMatchingGameplayTag(Definition->ImmunityTag)) return EHSRStatusOperationResult::Immune;
	OutEffectClass = Definition->InfiniteGameplayEffectClass.LoadSynchronous();
	if (!OutEffectClass) return EHSRStatusOperationResult::MissingGameplayEffect;
	return OutEffectClass->GetDefaultObject<UGameplayEffect>()->DurationPolicy == EGameplayEffectDurationType::Infinite
		? EHSRStatusOperationResult::Success : EHSRStatusOperationResult::GameplayEffectNotInfinite;
}

EHSRStatusOperationResult UHSRStatusComponent::AddOrRefreshStatus(const UHSRStatusDefinition* Definition, FName SourceParticipantId, FName TargetParticipantId, FGuid OperationId)
{
	if (OperationId.IsValid() && ProcessedOperationIds.Contains(OperationId)) return LastResult = EHSRStatusOperationResult::IgnoredEvent;
	TSubclassOf<UGameplayEffect> EffectClass;
	if ((LastResult = ValidateAdd(Definition, SourceParticipantId, TargetParticipantId, EffectClass)) != EHSRStatusOperationResult::Success) return LastResult;

	FHSRStatusInstance* Existing = Statuses.Find(Definition->StatusId);
	if (Existing)
	{
		if (!Existing->ActiveGameplayEffectHandle.IsValid() || Existing->BattleEpoch != BoundTurnManager->GetBattleEpoch()
			|| !AbilitySystem->GetActiveGameplayEffect(Existing->ActiveGameplayEffectHandle)
			|| AbilitySystem->GetCurrentStackCount(Existing->ActiveGameplayEffectHandle) != Existing->Stacks)
		{
			return LastResult = EHSRStatusOperationResult::InvalidRuntimeInstance;
		}
		if (Definition->RefreshPolicy == EHSRStatusRefreshPolicy::RefreshDuration || Existing->Stacks >= Definition->MaxStacks)
		{
			Existing->SourceParticipantId = SourceParticipantId;
			Existing->RemainingTurns = Definition->DurationTurns;
			LastResult = Definition->RefreshPolicy == EHSRStatusRefreshPolicy::RefreshDuration ? EHSRStatusOperationResult::Refreshed : EHSRStatusOperationResult::AtMaxRefreshed;
		}
		else
		{
			const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(EffectClass, 1.0f, AbilitySystem->MakeEffectContext());
			if (!Spec.IsValid()) return LastResult = EHSRStatusOperationResult::ApplyFailed;
			const FActiveGameplayEffectHandle Handle = AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			const int32 ExpectedStacks = Existing->Stacks + 1;
			if (!Handle.WasSuccessfullyApplied() || Handle != Existing->ActiveGameplayEffectHandle)
			{
				if (Handle.IsValid() && !AbilitySystem->RemoveActiveGameplayEffect(Handle)) return LastResult = EHSRStatusOperationResult::RemoveFailed;
				return LastResult = EHSRStatusOperationResult::ApplyFailed;
			}
			if (AbilitySystem->GetCurrentStackCount(Handle) != ExpectedStacks)
			{
				const bool bRolledBack = AbilitySystem->RemoveActiveGameplayEffect(Handle, 1);
				return LastResult = bRolledBack && AbilitySystem->GetCurrentStackCount(Handle) == Existing->Stacks ? EHSRStatusOperationResult::ApplyFailed : EHSRStatusOperationResult::RemoveFailed;
			}
			Existing->Stacks = ExpectedStacks;
			Existing->SourceParticipantId = SourceParticipantId;
			Existing->RemainingTurns = Definition->DurationTurns;
			++ApplyCount;
			LastResult = EHSRStatusOperationResult::StackAdded;
		}
		Existing->LastOperationResult = LastResult;
		if (OperationId.IsValid()) ProcessedOperationIds.Add(OperationId);
		NotifyStatusChanged();
		return LastResult;
	}

#if WITH_EDITOR
	if (bForceApplyFailure) return LastResult = EHSRStatusOperationResult::ApplyFailed;
#endif
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(EffectClass, 1.0f, AbilitySystem->MakeEffectContext());
	if (!Spec.IsValid()) return LastResult = EHSRStatusOperationResult::ApplyFailed;
	const FActiveGameplayEffectHandle Handle = AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	if (!Handle.WasSuccessfullyApplied()) return LastResult = EHSRStatusOperationResult::ApplyFailed;
	FHSRStatusInstance& Instance = Statuses.Add(Definition->StatusId);
	Instance.StatusId = Definition->StatusId;
	Instance.SourceParticipantId = SourceParticipantId;
	Instance.TargetParticipantId = TargetParticipantId;
	Instance.BattleEpoch = BoundTurnManager->GetBattleEpoch();
	Instance.Stacks = 1;
	Instance.RemainingTurns = Definition->DurationTurns;
	Instance.ActiveGameplayEffectHandle = Handle;
	Instance.LastOperationResult = EHSRStatusOperationResult::Success;
	RuntimeDefinitions.Add(Definition->StatusId, const_cast<UHSRStatusDefinition*>(Definition));
	++ApplyCount;
	if (OperationId.IsValid()) ProcessedOperationIds.Add(OperationId);
	LastResult = EHSRStatusOperationResult::Success;
	NotifyStatusChanged();
	return LastResult;
}

EHSRStatusOperationResult UHSRStatusComponent::ReplaceStatus(const UHSRStatusDefinition* Definition, FName SourceParticipantId, FName TargetParticipantId)
{
	if (!Definition) return LastResult = EHSRStatusOperationResult::InvalidDefinition;
	if (Statuses.Num() != 1) return LastResult = EHSRStatusOperationResult::InvalidRuntimeInstance;
	const FName OldStatusId = Statuses.CreateConstIterator().Key();
	TSubclassOf<UGameplayEffect> EffectClass;
	if ((LastResult = ValidateAdd(Definition, SourceParticipantId, TargetParticipantId, EffectClass)) != EHSRStatusOperationResult::Success) return LastResult;
	FHSRStatusInstance& OldInstance = Statuses.FindChecked(OldStatusId);
	if (!AbilitySystem->GetActiveGameplayEffect(OldInstance.ActiveGameplayEffectHandle)) return LastResult = EHSRStatusOperationResult::InvalidRuntimeInstance;
#if WITH_EDITOR
	if (bForceApplyFailure) return LastResult = EHSRStatusOperationResult::ApplyFailed;
#endif
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(EffectClass, 1.0f, AbilitySystem->MakeEffectContext());
	if (!Spec.IsValid()) return LastResult = EHSRStatusOperationResult::ApplyFailed;
	const FActiveGameplayEffectHandle NewHandle = AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	if (!NewHandle.WasSuccessfullyApplied()) return LastResult = EHSRStatusOperationResult::ApplyFailed;
	++ApplyCount;
#if WITH_EDITOR
	const bool bOldRemoved = !bForceOldRemoveFailure && AbilitySystem->RemoveActiveGameplayEffect(OldInstance.ActiveGameplayEffectHandle);
#else
	const bool bOldRemoved = AbilitySystem->RemoveActiveGameplayEffect(OldInstance.ActiveGameplayEffectHandle);
#endif
	if (!bOldRemoved)
	{
		const bool bRolledBack = AbilitySystem->RemoveActiveGameplayEffect(NewHandle);
		if (bRolledBack) --ApplyCount;
		return LastResult = bRolledBack ? EHSRStatusOperationResult::RemoveFailed : EHSRStatusOperationResult::RemoveFailed;
	}
	++RemoveCount;
	Statuses.Remove(OldStatusId);
	RuntimeDefinitions.Remove(OldStatusId);
	FHSRStatusInstance& NewInstance = Statuses.Add(Definition->StatusId);
	NewInstance.StatusId = Definition->StatusId;
	NewInstance.SourceParticipantId = SourceParticipantId;
	NewInstance.TargetParticipantId = TargetParticipantId;
	NewInstance.BattleEpoch = BoundTurnManager->GetBattleEpoch();
	NewInstance.Stacks = 1;
	NewInstance.RemainingTurns = Definition->DurationTurns;
	NewInstance.ActiveGameplayEffectHandle = NewHandle;
	NewInstance.LastOperationResult = EHSRStatusOperationResult::Replaced;
	RuntimeDefinitions.Add(Definition->StatusId, const_cast<UHSRStatusDefinition*>(Definition));
	LastResult = EHSRStatusOperationResult::Replaced;
	RecordPublicOperation(EHSRStatusPublicOperation::Replace, LastResult, Definition->StatusId, TargetParticipantId);
	NotifyStatusChanged();
	return LastResult;
}

void UHSRStatusComponent::HandleTurnEnded(const FHSRTurnLifecycleEvent& Event)
{
	if (Event.EventType != EHSRTurnLifecycleEventType::TurnEnded) return;
	TArray<FName> ToClear;
	UHSRBattleCoordinator* Coordinator = BoundCoordinator.Get();
	for (TPair<FName, FHSRStatusInstance>& Pair : Statuses)
	{
		FHSRStatusInstance& Instance = Pair.Value;
		if (Event.ParticipantId != Instance.TargetParticipantId || Event.BattleEpoch != Instance.BattleEpoch || Event.TurnSequence <= Instance.LastConsumedTurnSequence) continue;
		const UHSRStatusDefinition* Definition = RuntimeDefinitions.FindRef(Instance.StatusId);
		if (Definition && Definition->EffectKind == EHSRStatusEffectKind::DamageOverTime)
		{
			if (!Coordinator) continue;
			if (Coordinator->ResolveStatusDamage(Instance.SourceParticipantId, Instance.TargetParticipantId, FGuid::NewGuid(), Definition).Result != EHSRDamageResultType::DamageResolved) continue;
			if (AbilitySystem.IsValid() && AbilitySystem->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) <= 0.0f) { ClearStatus(); Coordinator->FinalizeStatusDamage(); return; }
			Coordinator->FinalizeStatusDamage();
		}
		Instance.LastConsumedTurnSequence = Event.TurnSequence;
		Instance.RemainingTurns = FMath::Max(0, Instance.RemainingTurns - 1);
		Instance.LastOperationResult = EHSRStatusOperationResult::Triggered;
		if (Instance.RemainingTurns == 0) ToClear.Add(Instance.StatusId); else NotifyStatusChanged();
	}
	for (const FName StatusId : ToClear) ClearStatusById(StatusId, EHSRStatusPublicOperation::Expire);
}

EHSRStatusOperationResult UHSRStatusComponent::ClearStatusById(FName StatusId, EHSRStatusPublicOperation Operation)
{
	FHSRStatusInstance* Instance = Statuses.Find(StatusId);
	if (!Instance) return LastResult = EHSRStatusOperationResult::NoDispelCandidate;
#if WITH_EDITOR
	if (bForceDispelRemoveFailure) return LastResult = EHSRStatusOperationResult::RemoveFailed;
#endif
	LastRemovedRemainingTurns = Instance->RemainingTurns;
	LastRemovedTurnSequence = Instance->LastConsumedTurnSequence;
	if (AbilitySystem.IsValid() && Instance->ActiveGameplayEffectHandle.IsValid() && AbilitySystem->GetActiveGameplayEffect(Instance->ActiveGameplayEffectHandle))
	{
		if (!AbilitySystem->RemoveActiveGameplayEffect(Instance->ActiveGameplayEffectHandle)) return LastResult = EHSRStatusOperationResult::RemoveFailed;
		++RemoveCount;
	}
	const FName TargetId = Instance->TargetParticipantId;
	Statuses.Remove(StatusId);
	RuntimeDefinitions.Remove(StatusId);
	LastResult = Operation == EHSRStatusPublicOperation::Dispel ? EHSRStatusOperationResult::Dispelled : EHSRStatusOperationResult::Success;
	RecordPublicOperation(Operation, LastResult, StatusId, TargetId);
	NotifyStatusChanged();
	return LastResult;
}

EHSRStatusOperationResult UHSRStatusComponent::DispelOneStatus()
{
	TArray<FName> Candidates;
	for (const TPair<FName, FHSRStatusInstance>& Pair : Statuses)
	{
		const UHSRStatusDefinition* Definition = RuntimeDefinitions.FindRef(Pair.Key);
		if (Definition && Definition->Classification == EHSRStatusClassification::Debuff && Definition->bDispellable) Candidates.Add(Pair.Key);
	}
	Candidates.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
	return Candidates.IsEmpty() ? LastResult = EHSRStatusOperationResult::NoDispelCandidate : ClearStatusById(Candidates[0], EHSRStatusPublicOperation::Dispel);
}

int32 UHSRStatusComponent::HandleSourceInvalid(FName SourceParticipantId)
{
	if (SourceParticipantId.IsNone()) return 0;
	const FString Key = FString::Printf(TEXT("%llu|%s"), BoundTurnManager.IsValid() ? BoundTurnManager->GetBattleEpoch() : 0, *SourceParticipantId.ToString());
	if (ProcessedInvalidSources.Contains(Key)) return 0;
	TArray<FName> ToRemove;
	for (const TPair<FName, FHSRStatusInstance>& Pair : Statuses)
	{
		const UHSRStatusDefinition* Definition = RuntimeDefinitions.FindRef(Pair.Key);
		if (Pair.Value.SourceParticipantId == SourceParticipantId && Definition && Definition->SourceInvalidPolicy == EHSRSourceInvalidPolicy::Remove) ToRemove.Add(Pair.Key);
	}
	int32 Removed = 0;
	for (const FName StatusId : ToRemove) { if (ClearStatusById(StatusId) == EHSRStatusOperationResult::Success) ++Removed; else return Removed; }
	ProcessedInvalidSources.Add(Key);
	return Removed;
}

EHSRStatusOperationResult UHSRStatusComponent::ClearStatus()
{
	FName StatusId = NAME_None;
	FName TargetId = ParticipantId;
	bool bFailed = false;
	for (const TPair<FName, FHSRStatusInstance>& Pair : Statuses)
	{
		StatusId = StatusId.IsNone() ? Pair.Key : StatusId;
		TargetId = Pair.Value.TargetParticipantId;
		LastRemovedRemainingTurns = Pair.Value.RemainingTurns;
		LastRemovedTurnSequence = Pair.Value.LastConsumedTurnSequence;
		if (AbilitySystem.IsValid() && Pair.Value.ActiveGameplayEffectHandle.IsValid() && AbilitySystem->GetActiveGameplayEffect(Pair.Value.ActiveGameplayEffectHandle))
		{
			if (AbilitySystem->RemoveActiveGameplayEffect(Pair.Value.ActiveGameplayEffectHandle)) ++RemoveCount; else bFailed = true;
		}
	}
	Statuses.Empty();
	RuntimeDefinitions.Empty();
	LastResult = bFailed ? EHSRStatusOperationResult::RemoveFailed : EHSRStatusOperationResult::Success;
	RecordPublicOperation(EHSRStatusPublicOperation::Clear, LastResult, StatusId, TargetId);
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
	const FHSRStatusInstance* Instance = StatusId.IsNone() ? (Statuses.IsEmpty() ? nullptr : &Statuses.CreateConstIterator().Value()) : Statuses.Find(StatusId);
	if (!Instance) return Snapshot;
	Snapshot.StatusId = Instance->StatusId;
	Snapshot.SourceParticipantId = Instance->SourceParticipantId;
	Snapshot.TargetParticipantId = Instance->TargetParticipantId;
	Snapshot.BattleEpoch = Instance->BattleEpoch;
	Snapshot.LastConsumedTurnSequence = Instance->LastConsumedTurnSequence;
	Snapshot.RemainingTurns = Instance->RemainingTurns;
	Snapshot.Stacks = Instance->Stacks;
	Snapshot.InstanceCount = 1;
	Snapshot.bHandleValid = Instance->ActiveGameplayEffectHandle.IsValid();
	Snapshot.ActiveHandleIdentity = Instance->ActiveGameplayEffectHandle.ToString();
	Snapshot.bHandleActiveInAbilitySystem = AbilitySystem.IsValid() && AbilitySystem->GetActiveGameplayEffect(Instance->ActiveGameplayEffectHandle) != nullptr;
	Snapshot.GameplayEffectStackCount = AbilitySystem.IsValid() ? AbilitySystem->GetCurrentStackCount(Instance->ActiveGameplayEffectHandle) : 0;
	return Snapshot;
}

TArray<FHSRStatusPublicSnapshot> UHSRStatusComponent::GetPublicSnapshots() const
{
	TArray<FHSRStatusPublicSnapshot> Result;
	for (const TPair<FName, FHSRStatusInstance>& Pair : Statuses)
	{
		const UHSRStatusDefinition* Definition = RuntimeDefinitions.FindRef(Pair.Key);
		FHSRStatusPublicSnapshot& Snapshot = Result.AddDefaulted_GetRef();
		Snapshot.StatusId = Pair.Key;
		Snapshot.TargetParticipantId = Pair.Value.TargetParticipantId;
		Snapshot.DisplayName = Definition && !Definition->DisplayName.IsEmpty() ? Definition->DisplayName : FText::FromName(Pair.Key);
		Snapshot.Classification = Definition ? Definition->Classification : EHSRStatusClassification::Buff;
		Snapshot.Stacks = Pair.Value.Stacks;
		Snapshot.RemainingTurns = Pair.Value.RemainingTurns;
		Snapshot.LastResult = Pair.Value.LastOperationResult;
	}
	Result.Sort([](const FHSRStatusPublicSnapshot& A, const FHSRStatusPublicSnapshot& B) { return A.StatusId.LexicalLess(B.StatusId); });
	return Result;
}

EHSRStatusOperationResult UHSRStatusComponent::GetPublicSnapshot(FName StatusId, FHSRStatusPublicSnapshot& OutSnapshot) const
{
	OutSnapshot = FHSRStatusPublicSnapshot();
	OutSnapshot.StatusId = StatusId;
	if (StatusId.IsNone()) return OutSnapshot.LastResult = EHSRStatusOperationResult::UnknownStatus;
	const FHSRStatusInstance* Instance = Statuses.Find(StatusId);
	if (!Instance) return OutSnapshot.LastResult = EHSRStatusOperationResult::UnknownStatus;
	const UHSRStatusDefinition* Definition = RuntimeDefinitions.FindRef(StatusId);
	OutSnapshot.TargetParticipantId = Instance->TargetParticipantId;
	OutSnapshot.DisplayName = Definition && !Definition->DisplayName.IsEmpty() ? Definition->DisplayName : FText::FromName(StatusId);
	OutSnapshot.Classification = Definition ? Definition->Classification : EHSRStatusClassification::Buff;
	OutSnapshot.Stacks = Instance->Stacks;
	OutSnapshot.RemainingTurns = Instance->RemainingTurns;
	OutSnapshot.LastResult = Instance->LastOperationResult;
	return EHSRStatusOperationResult::Success;
}
