#include "HSRStatusComponent.h"

#include "AbilitySystemComponent.h"
#include "../Battle/HSRTurnManager.h"
#include "../Battle/HSRBattleCoordinator.h"
#include "../Data/Definitions/HSRStatusDefinition.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"

// 构造函数：关闭 Tick（状态完全由回合事件驱动）。
UHSRStatusComponent::UHSRStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// 记录最近一次公开操作（供 UI/测试查询“刚刚发生了什么”）。
void UHSRStatusComponent::RecordPublicOperation(EHSRStatusPublicOperation Operation, EHSRStatusOperationResult Result, FName StatusId, FName TargetId)
{
	LastPublicOperation.Operation = Operation;
	LastPublicOperation.Result = Result;
	LastPublicOperation.StatusId = StatusId;
	LastPublicOperation.TargetParticipantId = TargetId;
	LastPublicOperation.Sequence = static_cast<int64>(++PublicOperationSequence);
}

// 组件销毁前：清掉所有状态并解绑回合管理器。
void UHSRStatusComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearStatus();
	UnbindTurnManager();
	Super::EndPlay(EndPlayReason);
}

// 初始化运行时：必须一次性传入参与者 ID 与 ASC，且此前不能有任何状态残留。
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

// 绑定回合管理器：换绑时会先清掉全部状态（旧管理器的回合事件已无关）。
bool UHSRStatusComponent::BindTurnManager(UHSRTurnManager* InTurnManager)
{
	if (!InTurnManager)
	{
		return false;
	}
	// 已是同一管理器且已绑定，直接成功。
	if (BoundTurnManager.Get() == InTurnManager && TurnEndedHandle.IsValid())
	{
		return true;
	}
	ClearStatus();
	UnbindTurnManager();
	BoundTurnManager = InTurnManager;
	TurnEndedHandle = InTurnManager->OnTurnEnded().AddUObject(this, &UHSRStatusComponent::HandleTurnEnded);
	return TurnEndedHandle.IsValid();
}

// 记录协调器引用（DoT 伤害需要协调器走正式伤害管线）。
void UHSRStatusComponent::BindBattleCoordinator(UHSRBattleCoordinator* InCoordinator)
{
	BoundCoordinator = InCoordinator;
}

// 解绑回合管理器并移除委托。
void UHSRStatusComponent::UnbindTurnManager()
{
	if (BoundTurnManager.IsValid() && TurnEndedHandle.IsValid())
	{
		BoundTurnManager->OnTurnEnded().Remove(TurnEndedHandle);
	}
	TurnEndedHandle.Reset();
	BoundTurnManager.Reset();
}

// 施加前校验：定义有效、来源/目标合法、纪元匹配、ASC 存在、目标存活、
// 免疫标签拦截 Debuff，并解析出无限持续的效果 GE 类。
EHSRStatusOperationResult UHSRStatusComponent::ValidateAdd(const UHSRStatusDefinition* Definition, FName SourceParticipantId, FName TargetParticipantId, TSubclassOf<UGameplayEffect>& OutEffectClass, bool bAllowPendingDeferredDefeat) const
{
	if (!Definition)
	{
		return EHSRStatusOperationResult::InvalidDefinition;
	}
	if (const EHSRStatusOperationResult Result = Definition->Validate(); Result != EHSRStatusOperationResult::Success)
	{
		return Result;
	}
	if (SourceParticipantId.IsNone())
	{
		return EHSRStatusOperationResult::InvalidSource;
	}
	if (TargetParticipantId.IsNone() || TargetParticipantId != ParticipantId)
	{
		return EHSRStatusOperationResult::InvalidTarget;
	}
	if (!BoundTurnManager.IsValid() || BoundTurnManager->GetBattleEpoch() == 0)
	{
		return EHSRStatusOperationResult::InvalidEpoch;
	}
	if (!AbilitySystem.IsValid())
	{
		return EHSRStatusOperationResult::MissingAbilitySystem;
	}
	// 目标已败时拒绝（除非是“已承认的延迟死亡”场景）。
	if (!bAllowPendingDeferredDefeat && AbilitySystem->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) <= 0.0f)
	{
		return EHSRStatusOperationResult::DefeatedTarget;
	}
	// Debuff 若带免疫标签且目标身上有该标签，则被免疫。
	if (Definition->Classification == EHSRStatusClassification::Debuff
		&& Definition->ImmunityTag.IsValid()
		&& AbilitySystem->HasMatchingGameplayTag(Definition->ImmunityTag))
	{
		return EHSRStatusOperationResult::Immune;
	}
	OutEffectClass = Definition->InfiniteGameplayEffectClass.LoadSynchronous();
	if (!OutEffectClass)
	{
		return EHSRStatusOperationResult::MissingGameplayEffect;
	}
	// 状态的底层效果必须是 Infinite 持续。
	return OutEffectClass->GetDefaultObject<UGameplayEffect>()->DurationPolicy == EGameplayEffectDurationType::Infinite
		? EHSRStatusOperationResult::Success
		: EHSRStatusOperationResult::GameplayEffectNotInfinite;
}

// 添加或刷新状态：已存在时按刷新策略续期或叠层；否则新建实例。
EHSRStatusOperationResult UHSRStatusComponent::AddOrRefreshStatus(const UHSRStatusDefinition* Definition, FName SourceParticipantId, FName TargetParticipantId, FGuid OperationId, bool bAllowPendingDeferredDefeat)
{
	// 幂等：同一操作 ID 只处理一次。
	if (OperationId.IsValid() && ProcessedOperationIds.Contains(OperationId))
	{
		return LastResult = EHSRStatusOperationResult::IgnoredEvent;
	}
	TSubclassOf<UGameplayEffect> EffectClass;
	if ((LastResult = ValidateAdd(Definition, SourceParticipantId, TargetParticipantId, EffectClass, bAllowPendingDeferredDefeat)) != EHSRStatusOperationResult::Success)
	{
		return LastResult;
	}

	FHSRStatusInstance* Existing = Statuses.Find(Definition->StatusId);
	if (Existing)
	{
		// 已有实例必须先与运行时保持一致（句柄有效、同纪元、GE 仍激活、层数吻合）。
		if (!Existing->ActiveGameplayEffectHandle.IsValid() || Existing->BattleEpoch != BoundTurnManager->GetBattleEpoch()
			|| !AbilitySystem->GetActiveGameplayEffect(Existing->ActiveGameplayEffectHandle)
			|| AbilitySystem->GetCurrentStackCount(Existing->ActiveGameplayEffectHandle) != Existing->Stacks)
		{
			return LastResult = EHSRStatusOperationResult::InvalidRuntimeInstance;
		}
		if (Definition->RefreshPolicy == EHSRStatusRefreshPolicy::RefreshDuration || Existing->Stacks >= Definition->MaxStacks)
		{
			// 续期：刷新剩余回合数（叠层到上限时也走这里，标记 AtMaxRefreshed）。
			Existing->SourceParticipantId = SourceParticipantId;
			Existing->RemainingTurns = Definition->DurationTurns;
			LastResult = Definition->RefreshPolicy == EHSRStatusRefreshPolicy::RefreshDuration
				? EHSRStatusOperationResult::Refreshed
				: EHSRStatusOperationResult::AtMaxRefreshed;
		}
		else
		{
			// 叠层：施加同一种 GE（依赖其堆叠规则）并期望层数 +1。
			const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(EffectClass, 1.0f, AbilitySystem->MakeEffectContext());
			if (!Spec.IsValid())
			{
				return LastResult = EHSRStatusOperationResult::ApplyFailed;
			}
			const FActiveGameplayEffectHandle Handle = AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			const int32 ExpectedStacks = Existing->Stacks + 1;
			if (!Handle.WasSuccessfullyApplied() || Handle != Existing->ActiveGameplayEffectHandle)
			{
				if (Handle.IsValid() && !AbilitySystem->RemoveActiveGameplayEffect(Handle))
				{
					return LastResult = EHSRStatusOperationResult::RemoveFailed;
				}
				return LastResult = EHSRStatusOperationResult::ApplyFailed;
			}
			if (AbilitySystem->GetCurrentStackCount(Handle) != ExpectedStacks)
			{
				// 层数未按预期增加：回滚这一层。
				const bool bRolledBack = AbilitySystem->RemoveActiveGameplayEffect(Handle, 1);
				return LastResult = bRolledBack && AbilitySystem->GetCurrentStackCount(Handle) == Existing->Stacks
					? EHSRStatusOperationResult::ApplyFailed
					: EHSRStatusOperationResult::RemoveFailed;
			}
			Existing->Stacks = ExpectedStacks;
			Existing->SourceParticipantId = SourceParticipantId;
			Existing->RemainingTurns = Definition->DurationTurns;
			++ApplyCount;
			LastResult = EHSRStatusOperationResult::StackAdded;
		}
		Existing->LastOperationResult = LastResult;
		if (OperationId.IsValid())
		{
			ProcessedOperationIds.Add(OperationId);
		}
		NotifyStatusChanged();
		return LastResult;
	}

#if WITH_EDITOR
	if (bForceApplyFailure)
	{
		return LastResult = EHSRStatusOperationResult::ApplyFailed;
	}
#endif
	// 新建实例：施加 GE 并登记状态元数据。
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(EffectClass, 1.0f, AbilitySystem->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return LastResult = EHSRStatusOperationResult::ApplyFailed;
	}
	const FActiveGameplayEffectHandle Handle = AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	if (!Handle.WasSuccessfullyApplied())
	{
		return LastResult = EHSRStatusOperationResult::ApplyFailed;
	}
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
	if (OperationId.IsValid())
	{
		ProcessedOperationIds.Add(OperationId);
	}
	LastResult = EHSRStatusOperationResult::Success;
	NotifyStatusChanged();
	return LastResult;
}

// 显式替换状态：要求当前恰好有一个状态，把它整体替换成新状态。
// 替换是原子的：旧状态移除失败时会回滚新状态。
EHSRStatusOperationResult UHSRStatusComponent::ReplaceStatus(const UHSRStatusDefinition* Definition, FName SourceParticipantId, FName TargetParticipantId)
{
	if (!Definition)
	{
		return LastResult = EHSRStatusOperationResult::InvalidDefinition;
	}
	if (Statuses.Num() != 1)
	{
		return LastResult = EHSRStatusOperationResult::InvalidRuntimeInstance;
	}
	const FName OldStatusId = Statuses.CreateConstIterator().Key();
	TSubclassOf<UGameplayEffect> EffectClass;
	if ((LastResult = ValidateAdd(Definition, SourceParticipantId, TargetParticipantId, EffectClass, false)) != EHSRStatusOperationResult::Success)
	{
		return LastResult;
	}
	FHSRStatusInstance& OldInstance = Statuses.FindChecked(OldStatusId);
	if (!AbilitySystem->GetActiveGameplayEffect(OldInstance.ActiveGameplayEffectHandle))
	{
		return LastResult = EHSRStatusOperationResult::InvalidRuntimeInstance;
	}
#if WITH_EDITOR
	if (bForceApplyFailure)
	{
		return LastResult = EHSRStatusOperationResult::ApplyFailed;
	}
#endif
	// 先施加新状态。
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(EffectClass, 1.0f, AbilitySystem->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return LastResult = EHSRStatusOperationResult::ApplyFailed;
	}
	const FActiveGameplayEffectHandle NewHandle = AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	if (!NewHandle.WasSuccessfullyApplied())
	{
		return LastResult = EHSRStatusOperationResult::ApplyFailed;
	}
	++ApplyCount;
	// 再移除旧状态。
#if WITH_EDITOR
	const bool bOldRemoved = !bForceOldRemoveFailure && AbilitySystem->RemoveActiveGameplayEffect(OldInstance.ActiveGameplayEffectHandle);
#else
	const bool bOldRemoved = AbilitySystem->RemoveActiveGameplayEffect(OldInstance.ActiveGameplayEffectHandle);
#endif
	if (!bOldRemoved)
	{
		// 旧状态移除失败：回滚新状态。
		const bool bRolledBack = AbilitySystem->RemoveActiveGameplayEffect(NewHandle);
		if (bRolledBack)
		{
			--ApplyCount;
		}
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

// 回合结束回调：只有目标自己的回合结束才消耗持续时间；DoT 在此时结算伤害。
void UHSRStatusComponent::HandleTurnEnded(const FHSRTurnLifecycleEvent& Event)
{
	if (Event.EventType != EHSRTurnLifecycleEventType::TurnEnded)
	{
		return;
	}
	TArray<FName> ToClear;
	UHSRBattleCoordinator* Coordinator = BoundCoordinator.Get();
	for (TPair<FName, FHSRStatusInstance>& Pair : Statuses)
	{
		FHSRStatusInstance& Instance = Pair.Value;
		// 只处理目标自己的回合结束，且同纪元、且回合序号必须递增（防重复事件）。
		if (Event.ParticipantId != Instance.TargetParticipantId
			|| Event.BattleEpoch != Instance.BattleEpoch
			|| Event.TurnSequence <= Instance.LastConsumedTurnSequence)
		{
			continue;
		}
		const UHSRStatusDefinition* Definition = RuntimeDefinitions.FindRef(Instance.StatusId);
		// DoT 类型：回合结束时结算一次伤害（走协调器的正式伤害管线）。
		if (Definition && Definition->EffectKind == EHSRStatusEffectKind::DamageOverTime)
		{
			if (!Coordinator)
			{
				continue;
			}
			if (Coordinator->ResolveStatusDamage(Instance.SourceParticipantId, Instance.TargetParticipantId, FGuid::NewGuid(), Definition).Result != EHSRDamageResultType::DamageResolved)
			{
				continue;
			}
			// 结算伤害后目标死亡：清状态并让协调器处理死亡。
			if (AbilitySystem.IsValid() && AbilitySystem->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) <= 0.0f)
			{
				ClearStatus();
				Coordinator->FinalizeStatusDamage();
				return;
			}
			Coordinator->FinalizeStatusDamage();
		}
		// 消耗一回合持续时间。
		Instance.LastConsumedTurnSequence = Event.TurnSequence;
		Instance.RemainingTurns = FMath::Max(0, Instance.RemainingTurns - 1);
		Instance.LastOperationResult = EHSRStatusOperationResult::Triggered;
		if (Instance.RemainingTurns == 0)
		{
			ToClear.Add(Instance.StatusId);
		}
		else
		{
			NotifyStatusChanged();
		}
	}
	// 到期的状态按“过期”语义清理。
	for (const FName StatusId : ToClear)
	{
		ClearStatusById(StatusId, EHSRStatusPublicOperation::Expire);
	}
}

// 按状态 ID 清理（过期或净化共用）。
EHSRStatusOperationResult UHSRStatusComponent::ClearStatusById(FName StatusId, EHSRStatusPublicOperation Operation)
{
	FHSRStatusInstance* Instance = Statuses.Find(StatusId);
	if (!Instance)
	{
		return LastResult = EHSRStatusOperationResult::NoDispelCandidate;
	}
#if WITH_EDITOR
	if (bForceDispelRemoveFailure)
	{
		return LastResult = EHSRStatusOperationResult::RemoveFailed;
	}
#endif
	// 记录清理前的剩余回合/序号（供测试断言）。
	LastRemovedRemainingTurns = Instance->RemainingTurns;
	LastRemovedTurnSequence = Instance->LastConsumedTurnSequence;
	if (AbilitySystem.IsValid() && Instance->ActiveGameplayEffectHandle.IsValid() && AbilitySystem->GetActiveGameplayEffect(Instance->ActiveGameplayEffectHandle))
	{
		if (!AbilitySystem->RemoveActiveGameplayEffect(Instance->ActiveGameplayEffectHandle))
		{
			return LastResult = EHSRStatusOperationResult::RemoveFailed;
		}
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

// 净化一个可净化的 Debuff（按状态 ID 字典序取第一个）。
EHSRStatusOperationResult UHSRStatusComponent::DispelOneStatus()
{
	TArray<FName> Candidates;
	for (const TPair<FName, FHSRStatusInstance>& Pair : Statuses)
	{
		const UHSRStatusDefinition* Definition = RuntimeDefinitions.FindRef(Pair.Key);
		if (Definition && Definition->Classification == EHSRStatusClassification::Debuff && Definition->bDispellable)
		{
			Candidates.Add(Pair.Key);
		}
	}
	Candidates.Sort([](const FName& A, const FName& B)
	{
		return A.LexicalLess(B);
	});
	return Candidates.IsEmpty() ? LastResult = EHSRStatusOperationResult::NoDispelCandidate
		: ClearStatusById(Candidates[0], EHSRStatusPublicOperation::Dispel);
}

// 来源失效处理：按 SourceInvalidPolicy=Remove 的策略移除来源已败/无效的状态。
int32 UHSRStatusComponent::HandleSourceInvalid(FName SourceParticipantId)
{
	if (SourceParticipantId.IsNone())
	{
		return 0;
	}
	// 幂等：同一纪元 + 来源只处理一次。
	const FString Key = FString::Printf(TEXT("%llu|%s"), BoundTurnManager.IsValid() ? BoundTurnManager->GetBattleEpoch() : 0, *SourceParticipantId.ToString());
	if (ProcessedInvalidSources.Contains(Key))
	{
		return 0;
	}
	TArray<FName> ToRemove;
	for (const TPair<FName, FHSRStatusInstance>& Pair : Statuses)
	{
		const UHSRStatusDefinition* Definition = RuntimeDefinitions.FindRef(Pair.Key);
		if (Pair.Value.SourceParticipantId == SourceParticipantId
			&& Definition && Definition->SourceInvalidPolicy == EHSRSourceInvalidPolicy::Remove)
		{
			ToRemove.Add(Pair.Key);
		}
	}
	int32 Removed = 0;
	for (const FName StatusId : ToRemove)
	{
		if (ClearStatusById(StatusId) == EHSRStatusOperationResult::Success)
		{
			++Removed;
		}
		else
		{
			return Removed;
		}
	}
	ProcessedInvalidSources.Add(Key);
	return Removed;
}

// 清空全部状态。
EHSRStatusOperationResult UHSRStatusComponent::ClearStatus()
{
	FName StatusId = NAME_None;
	FName TargetId = ParticipantId;
	bool bFailed = false;
	for (auto It = Statuses.CreateIterator(); It; ++It)
	{
		const TPair<FName, FHSRStatusInstance>& Pair = *It;
		StatusId = StatusId.IsNone() ? Pair.Key : StatusId;
		TargetId = Pair.Value.TargetParticipantId;
		LastRemovedRemainingTurns = Pair.Value.RemainingTurns;
		LastRemovedTurnSequence = Pair.Value.LastConsumedTurnSequence;
		if (AbilitySystem.IsValid() && Pair.Value.ActiveGameplayEffectHandle.IsValid() && AbilitySystem->GetActiveGameplayEffect(Pair.Value.ActiveGameplayEffectHandle))
		{
			bool bRemoved = false;
	#if WITH_EDITOR
			bRemoved = !bForceClearRemoveFailure && AbilitySystem->RemoveActiveGameplayEffect(Pair.Value.ActiveGameplayEffectHandle);
	#else
			bRemoved = AbilitySystem->RemoveActiveGameplayEffect(Pair.Value.ActiveGameplayEffectHandle);
	#endif
			if (!bRemoved)
			{
				bFailed = true;
				continue;
			}
			++RemoveCount;
		}
		RuntimeDefinitions.Remove(Pair.Key);
		It.RemoveCurrent();
	}
	LastResult = bFailed ? EHSRStatusOperationResult::RemoveFailed : EHSRStatusOperationResult::Success;
	RecordPublicOperation(EHSRStatusPublicOperation::Clear, LastResult, StatusId, TargetId);
	NotifyStatusChanged();
	return LastResult;
}

// 运行时快照（测试/诊断用，纯值 DTO）。
FHSRStatusRuntimeSnapshot UHSRStatusComponent::GetSnapshot(FName StatusId) const
{
	FHSRStatusRuntimeSnapshot Snapshot;
	Snapshot.Result = LastResult;
	Snapshot.ApplyCount = ApplyCount;
	Snapshot.RemoveCount = RemoveCount;
	Snapshot.LastRemovedRemainingTurns = LastRemovedRemainingTurns;
	Snapshot.LastRemovedTurnSequence = LastRemovedTurnSequence;
	// 未指定 StatusId 时取第一个实例。
	const FHSRStatusInstance* Instance = StatusId.IsNone()
		? (Statuses.IsEmpty() ? nullptr : &Statuses.CreateConstIterator().Value())
		: Statuses.Find(StatusId);
	if (!Instance)
	{
		return Snapshot;
	}
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

// 面向 UI 的公开快照列表（只含展示所需字段，按状态 ID 排序）。
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
	Result.Sort([](const FHSRStatusPublicSnapshot& A, const FHSRStatusPublicSnapshot& B)
	{
		return A.StatusId.LexicalLess(B.StatusId);
	});
	return Result;
}

// 取单个状态的公开快照。
EHSRStatusOperationResult UHSRStatusComponent::GetPublicSnapshot(FName StatusId, FHSRStatusPublicSnapshot& OutSnapshot) const
{
	OutSnapshot = FHSRStatusPublicSnapshot();
	OutSnapshot.StatusId = StatusId;
	if (StatusId.IsNone())
	{
		return OutSnapshot.LastResult = EHSRStatusOperationResult::UnknownStatus;
	}
	const FHSRStatusInstance* Instance = Statuses.Find(StatusId);
	if (!Instance)
	{
		return OutSnapshot.LastResult = EHSRStatusOperationResult::UnknownStatus;
	}
	const UHSRStatusDefinition* Definition = RuntimeDefinitions.FindRef(StatusId);
	OutSnapshot.TargetParticipantId = Instance->TargetParticipantId;
	OutSnapshot.DisplayName = Definition && !Definition->DisplayName.IsEmpty() ? Definition->DisplayName : FText::FromName(StatusId);
	OutSnapshot.Classification = Definition ? Definition->Classification : EHSRStatusClassification::Buff;
	OutSnapshot.Stacks = Instance->Stacks;
	OutSnapshot.RemainingTurns = Instance->RemainingTurns;
	OutSnapshot.LastResult = Instance->LastOperationResult;
	return EHSRStatusOperationResult::Success;
}
