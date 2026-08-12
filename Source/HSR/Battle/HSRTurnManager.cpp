#include "HSRTurnManager.h"

#include "AbilitySystemComponent.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"

// 由速度计算“基础行动距离”（速度越快，基准行动距离越短）。
// 速度非法（非有限值）或 <= 0 时返回 false。
bool UHSRTurnManager::MakeBaseActionDistance(float Speed, float& OutBase)
{
	if (!FMath::IsFinite(Speed))
	{
		return false;
	}
	OutBase = MaximumBaseActionDistance / FMath::Max(Speed, 1.0f);
	return FMath::IsFinite(OutBase) && OutBase > 0.0f;
}

// 初始化回合队列：校验参与者、按速度计算行动距离、按 ParticipantId 排序去重、
// 开启新纪元、绑定速度变化委托，并把第一个合法参与者设为当前回合。
bool UHSRTurnManager::Initialize(const TArray<FHSRBattleParticipant>& InParticipants)
{
	Reset();
	if (InParticipants.IsEmpty())
	{
		return false;
	}
	TArray<FHSRBattleParticipant> Candidate = InParticipants;
	for (FHSRBattleParticipant& Participant : Candidate)
	{
		if (!Participant.IsValid() || Participant.ParticipantId.IsNone())
		{
			return false;
		}
		const float Speed = Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetSpeedAttribute());
		float Base = 0.0f;
		if (!MakeBaseActionDistance(Speed, Base))
		{
			return false;
		}
		Participant.InitiativeSpeed = FMath::Max(Speed, 1.0f);
		Participant.EffectiveSpeed = FMath::Max(Speed, 1.0f);
		Participant.BaseActionDistance = Base;
		Participant.RemainingActionDistance = Base;
	}
	// 稳定排序：保证同速情况下回合顺序可预测（按 ID 字典序）。
	Candidate.Sort([](const FHSRBattleParticipant& A, const FHSRBattleParticipant& B)
	{
		return A.ParticipantId.LexicalLess(B.ParticipantId);
	});
	// 去重：两个参与者不允许同名。
	for (int32 Index = 1; Index < Candidate.Num(); ++Index)
	{
		if (Candidate[Index - 1].ParticipantId == Candidate[Index].ParticipantId)
		{
			return false;
		}
	}
	OrderedParticipants = MoveTemp(Candidate);
	// 开启新的回合纪元（纪元隔离，重置后旧事件不会影响新战斗）。
	++BattleEpochCounter;
	if (BattleEpochCounter == 0)
	{
		++BattleEpochCounter;
	}
	BattleEpoch = BattleEpochCounter;
	if (!BindSpeedDelegates())
	{
		Reset();
		return false;
	}
	if (!AdvanceToNextValidTurn())
	{
		Reset();
		return false;
	}
	return true;
}

// 为每个参与者绑定速度属性变化委托（战斗中速度变化会即时调整行动距离）。
bool UHSRTurnManager::BindSpeedDelegates()
{
	for (const FHSRBattleParticipant& Participant : OrderedParticipants)
	{
	#if WITH_DEV_AUTOMATION_TESTS
		if (SpeedDelegateBindFailureAfter != INDEX_NONE && SpeedDelegateBindings.Num() >= SpeedDelegateBindFailureAfter)
		{
			UnbindSpeedDelegates();
			return false;
		}
	#endif
		UAbilitySystemComponent* ASC = Participant.AbilitySystemComponent.Get();
		if (!ASC)
		{
			UnbindSpeedDelegates();
			return false;
		}
		const FName Id = Participant.ParticipantId;
		const uint64 Epoch = BattleEpoch;
		TWeakObjectPtr<UHSRTurnManager> WeakThis(this);
		// 用弱引用捕获 ASC 与纪元，回调只处理同纪元、且 ASC 仍存活的变化。
		FDelegateHandle Handle = ASC->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetSpeedAttribute()).AddLambda(
			[WeakThis, Id, WeakASC = TWeakObjectPtr<UAbilitySystemComponent>(ASC), Epoch](const FOnAttributeChangeData& Data)
			{
				if (UHSRTurnManager* This = WeakThis.Get())
				{
					This->HandleSpeedChanged(Id, WeakASC.Get(), Epoch, Data.NewValue);
				}
			});
		if (!Handle.IsValid())
		{
			UnbindSpeedDelegates();
			return false;
		}
		FSpeedDelegateBinding& Binding = SpeedDelegateBindings.AddDefaulted_GetRef();
		Binding.ParticipantId = Id;
		Binding.AbilitySystemComponent = ASC;
		Binding.Handle = Handle;
		Binding.Epoch = Epoch;
	}
	return true;
}

// 解绑所有速度变化委托。
void UHSRTurnManager::UnbindSpeedDelegates()
{
	for (const FSpeedDelegateBinding& Binding : SpeedDelegateBindings)
	{
		if (UAbilitySystemComponent* ASC = Binding.AbilitySystemComponent.Get())
		{
			if (Binding.Handle.IsValid())
			{
				ASC->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetSpeedAttribute()).Remove(Binding.Handle);
			}
		}
	}
	SpeedDelegateBindings.Empty();
}

// 速度变化处理：同纪元且 ASC 匹配时，按新速度重算行动距离。
// 当前回合者的距离就地更新；非当前回合者按比例换算剩余距离。
void UHSRTurnManager::HandleSpeedChanged(FName ParticipantId, UAbilitySystemComponent* SourceASC, uint64 BoundEpoch, float NewSpeed)
{
	if (BoundEpoch == 0 || BoundEpoch != BattleEpoch || State == EHSRTurnManagerState::Finished || !SourceASC)
	{
		return;
	}
	if (!FMath::IsFinite(NewSpeed))
	{
		UE_LOG(LogTemp, Warning, TEXT("ActionDistance SpeedRejected Participant=%s Reason=NonFinite Epoch=%llu Sequence=%llu"), *ParticipantId.ToString(), BoundEpoch, TurnSequence);
		return;
	}
	const int32 Index = FindParticipantIndex(ParticipantId);
	if (!OrderedParticipants.IsValidIndex(Index) || OrderedParticipants[Index].AbilitySystemComponent.Get() != SourceASC)
	{
		return;
	}
	FHSRBattleParticipant& Participant = OrderedParticipants[Index];
	float NewBase = 0.0f;
	if (!MakeBaseActionDistance(NewSpeed, NewBase))
	{
		return;
	}
	if (Index == CurrentTurnIndex)
	{
		Participant.EffectiveSpeed = FMath::Max(NewSpeed, 1.0f);
		Participant.BaseActionDistance = NewBase;
		return;
	}
	// 非当前回合者：剩余距离按新旧基准等比例缩放，保持其相对进度。
	const float NewRemaining = Participant.RemainingActionDistance * NewBase / Participant.BaseActionDistance;
	if (!FMath::IsFinite(NewRemaining) || NewRemaining < 0.0f)
	{
		return;
	}
	Participant.EffectiveSpeed = FMath::Max(NewSpeed, 1.0f);
	Participant.BaseActionDistance = NewBase;
	Participant.RemainingActionDistance = FMath::Abs(NewRemaining) <= DistanceEpsilon ? 0.0f : NewRemaining;
}

// 解析当前回合者的行动：先广播 TurnEnded，再充值行动距离并推进到下一个合法回合。
bool UHSRTurnManager::ResolveAction(FName ResolvingParticipantId)
{
	if (bSelectingOrResolving || CurrentTurnIndex == INDEX_NONE || State == EHSRTurnManagerState::Finished
		|| GetCurrentParticipantId() != ResolvingParticipantId || !IsCurrentParticipantValid())
	{
		return false;
	}
	bSelectingOrResolving = true;
	const FName ResolvedId = ResolvingParticipantId;
	BroadcastLifecycleEvent(EHSRTurnLifecycleEventType::TurnEnded, ResolvedId);
	bool bSucceeded = ApplyCurrentPendingAfterRecharge();
	if (bSucceeded)
	{
		AdvanceToNextValidTurn();
	}
	bSelectingOrResolving = false;
	if (bSucceeded)
	{
		ActionResolved.Broadcast(ResolvedId);
	}
	return bSucceeded;
}

// 行动结束后为当前参与者“充值”下一次行动距离（基础距离 + 挂起的距离调整）。
bool UHSRTurnManager::ApplyCurrentPendingAfterRecharge()
{
	if (!OrderedParticipants.IsValidIndex(CurrentTurnIndex))
	{
		return false;
	}
	FHSRBattleParticipant& Current = OrderedParticipants[CurrentTurnIndex];
	float Candidate = Current.RemainingActionDistance + Current.BaseActionDistance;
	if (!FMath::IsFinite(Candidate))
	{
		return false;
	}
	// 应用所有挂起的行动距离调整（推进则减、延迟则加）。
	for (const FPendingPostActionOperation& Pending : PendingPostActionOperations)
	{
		Candidate = Pending.Kind == EHSRActionDistanceAdjustmentKind::Advance
			? FMath::Max(0.0f, Candidate - Pending.Distance)
			: Candidate + Pending.Distance;
		if (!FMath::IsFinite(Candidate))
		{
			return false;
		}
	}
	Current.RemainingActionDistance = FMath::Abs(Candidate) <= DistanceEpsilon ? 0.0f : Candidate;
#if WITH_DEV_AUTOMATION_TESTS
	LastPostRechargeDistanceForAutomation = Current.RemainingActionDistance;
#endif
	PendingPostActionOperations.Empty();
	return true;
}

// 请求一次行动距离调整（推进/延迟），默认不允许“已承认的延迟死亡”目标。
FHSRActionDistanceResult UHSRTurnManager::RequestActionDistanceAdjustment(const FHSRActionDistanceRequest& Request)
{
	return RequestActionDistanceAdjustmentInternal(Request, false);
}

// 行动距离调整核心：先校验请求合法性/幂等/纪元，再按当前回合与否分别处理。
FHSRActionDistanceResult UHSRTurnManager::RequestActionDistanceAdjustmentInternal(const FHSRActionDistanceRequest& Request, bool bAllowAdmittedDeferredDefeat)
{
	// 统一收尾：记录结果日志并返回结果对象。
	const auto Complete = [this, &Request](EHSRActionDistanceAdjustmentResult Result, int32 Index = INDEX_NONE, const FHSRActionDistanceResult* OldSnapshot = nullptr)
	{
		const FHSRActionDistanceResult Out = MakeAdjustmentResult(Result, Index, OldSnapshot);
		const float Speed = OrderedParticipants.IsValidIndex(Index) ? OrderedParticipants[Index].EffectiveSpeed : 0.0f;
		const TCHAR* Reason = TEXT("InvalidRequest");
		switch (Out.Result)
		{
		case EHSRActionDistanceAdjustmentResult::Accepted: Reason = TEXT("Accepted"); break;
		case EHSRActionDistanceAdjustmentResult::DuplicateOperation: Reason = TEXT("DuplicateOperation"); break;
		case EHSRActionDistanceAdjustmentResult::InvalidEpoch: Reason = TEXT("InvalidEpoch"); break;
		case EHSRActionDistanceAdjustmentResult::InvalidTarget: Reason = TEXT("InvalidTarget"); break;
		case EHSRActionDistanceAdjustmentResult::DefeatedTarget: Reason = TEXT("DefeatedTarget"); break;
		case EHSRActionDistanceAdjustmentResult::Finished: Reason = TEXT("Finished"); break;
		case EHSRActionDistanceAdjustmentResult::ArithmeticFailure: Reason = TEXT("ArithmeticFailure"); break;
		default: break;
		}
		UE_LOG(LogTemp, Log, TEXT("ActionDistance Result=%d Reason=%s OperationId=%s Target=%s Kind=%d Ratio=%.6f OldSpeed=%.6f NewSpeed=%.6f OldBase=%.6f NewBase=%.6f OldRemaining=%.6f NewRemaining=%.6f OldPending=%d NewPending=%d Current=%s Next=%s Epoch=%llu Sequence=%llu"),
			static_cast<int32>(Out.Result), Reason, *Request.OperationId.ToString(), *Request.TargetParticipantId.ToString(), static_cast<int32>(Request.Kind), Request.Ratio,
			Speed, Speed, Out.OldBase, Out.NewBase, Out.OldRemaining, Out.NewRemaining, Out.OldPendingOperationCount, Out.NewPendingOperationCount,
			*Out.CurrentParticipantId.ToString(), *Out.NextParticipantId.ToString(), Out.BattleEpoch, Out.TurnSequence);
		return Out;
	};
	// 请求合法性：操作 ID 有效、目标非空、比例在 [0,1] 内、类型为 Advance 或 Delay。
	if (!Request.OperationId.IsValid() || Request.TargetParticipantId.IsNone()
		|| !FMath::IsFinite(Request.Ratio) || Request.Ratio < 0.0f || Request.Ratio > 1.0f
		|| (Request.Kind != EHSRActionDistanceAdjustmentKind::Advance && Request.Kind != EHSRActionDistanceAdjustmentKind::Delay))
	{
		return Complete(EHSRActionDistanceAdjustmentResult::InvalidRequest);
	}
	const int32 Index = FindParticipantIndex(Request.TargetParticipantId);
	// 幂等：同一操作 ID 只接受一次。
	if (ConsumedOperationIds.Contains(Request.OperationId))
	{
		return Complete(EHSRActionDistanceAdjustmentResult::DuplicateOperation, Index);
	}
	ConsumedOperationIds.Add(Request.OperationId);
	// 纪元必须匹配当前战斗。
	if (Request.BattleEpoch != BattleEpoch || BattleEpoch == 0)
	{
		return Complete(EHSRActionDistanceAdjustmentResult::InvalidEpoch, Index);
	}
	if (State == EHSRTurnManagerState::Finished)
	{
		return Complete(EHSRActionDistanceAdjustmentResult::Finished, Index);
	}
	if (!OrderedParticipants.IsValidIndex(Index))
	{
		return Complete(EHSRActionDistanceAdjustmentResult::InvalidTarget);
	}
	FHSRBattleParticipant& Target = OrderedParticipants[Index];
	if (!Target.IsValid())
	{
		return Complete(EHSRActionDistanceAdjustmentResult::InvalidTarget, Index);
	}
	// 目标已败（或被移除）时拒绝，除非是“已承认的延迟死亡”场景。
	if (!IsParticipantTurnEligible(Target) && !bAllowAdmittedDeferredDefeat)
	{
		return Complete(EHSRActionDistanceAdjustmentResult::DefeatedTarget, Index);
	}
	const float Distance = Target.BaseActionDistance * Request.Ratio;
	if (!FMath::IsFinite(Distance))
	{
		return Complete(EHSRActionDistanceAdjustmentResult::ArithmeticFailure, Index);
	}
	const FHSRActionDistanceResult Before = MakeAdjustmentResult(EHSRActionDistanceAdjustmentResult::Accepted, Index);
	if (Index == CurrentTurnIndex)
	{
		// 目标正在行动：调整挂起，等行动结束后随充值一并应用。
		// 先对“最大充值量 + 挂起调整 + 本次调整”做有限性预演，防止数值爆炸。
		float Preview = Target.RemainingActionDistance + MaximumBaseActionDistance;
		for (const FPendingPostActionOperation& Pending : PendingPostActionOperations)
		{
			Preview = Pending.Kind == EHSRActionDistanceAdjustmentKind::Advance
				? FMath::Max(0.0f, Preview - Pending.Distance)
				: Preview + Pending.Distance;
			if (!FMath::IsFinite(Preview))
			{
				return Complete(EHSRActionDistanceAdjustmentResult::ArithmeticFailure, Index);
			}
		}
		Preview = Request.Kind == EHSRActionDistanceAdjustmentKind::Advance
			? FMath::Max(0.0f, Preview - Distance)
			: Preview + Distance;
		if (!FMath::IsFinite(Preview))
		{
			return Complete(EHSRActionDistanceAdjustmentResult::ArithmeticFailure, Index);
		}
		PendingPostActionOperations.Add({ Request.Kind, Distance });
	}
	else
	{
		// 目标不在行动：直接改它的剩余行动距离。
		const float Value = Request.Kind == EHSRActionDistanceAdjustmentKind::Advance
			? FMath::Max(0.0f, Target.RemainingActionDistance - Distance)
			: Target.RemainingActionDistance + Distance;
		if (!FMath::IsFinite(Value))
		{
			return Complete(EHSRActionDistanceAdjustmentResult::ArithmeticFailure, Index);
		}
		Target.RemainingActionDistance = FMath::Abs(Value) <= DistanceEpsilon ? 0.0f : Value;
	}
	return Complete(EHSRActionDistanceAdjustmentResult::Accepted, Index, &Before);
}

// 消费一次破韧延迟（把目标延后一整段基础距离）。
bool UHSRTurnManager::ConsumeBreakDelay(const FHSRTurnDelayRequest& Request)
{
	FHSRActionDistanceRequest DistanceRequest;
	DistanceRequest.BattleEpoch = BattleEpoch;
	DistanceRequest.OperationId = Request.ActionId;
	DistanceRequest.TargetParticipantId = Request.TargetParticipantId;
	DistanceRequest.Ratio = 1.0f;
	DistanceRequest.Kind = EHSRActionDistanceAdjustmentKind::Delay;
	return RequestActionDistanceAdjustment(DistanceRequest).Result == EHSRActionDistanceAdjustmentResult::Accepted;
}

// 与 ConsumeBreakDelay 相同，但允许“已承认的延迟死亡”目标（用于伤害即死亡 + 破韧同帧场景）。
bool UHSRTurnManager::ConsumeAdmittedBreakDelay(const FHSRTurnDelayRequest& Request)
{
	FHSRActionDistanceRequest DistanceRequest;
	DistanceRequest.BattleEpoch = BattleEpoch;
	DistanceRequest.OperationId = Request.ActionId;
	DistanceRequest.TargetParticipantId = Request.TargetParticipantId;
	DistanceRequest.Ratio = 1.0f;
	DistanceRequest.Kind = EHSRActionDistanceAdjustmentKind::Delay;
	return RequestActionDistanceAdjustmentInternal(DistanceRequest, true).Result == EHSRActionDistanceAdjustmentResult::Accepted;
}

// 结束战斗：清空挂起操作、解绑委托、置为 Finished。
void UHSRTurnManager::FinishBattle()
{
	PendingPostActionOperations.Empty();
	UnbindSpeedDelegates();
	State = EHSRTurnManagerState::Finished;
	CurrentTurnIndex = INDEX_NONE;
}

// 重置回合管理器到初始状态（Waiting）。
void UHSRTurnManager::Reset()
{
	UnbindSpeedDelegates();
	OrderedParticipants.Empty();
	PendingPostActionOperations.Empty();
	ConsumedOperationIds.Empty();
	CurrentTurnIndex = INDEX_NONE;
	BattleEpoch = 0;
	TurnSequence = 0;
	State = EHSRTurnManagerState::Waiting;
	bSelectingOrResolving = false;
#if WITH_DEV_AUTOMATION_TESTS
	LastPostRechargeDistanceForAutomation = 0.0f;
#endif
}

// 当前回合参与者的 ID；无当前回合时返回 None。
FName UHSRTurnManager::GetCurrentParticipantId() const
{
	return OrderedParticipants.IsValidIndex(CurrentTurnIndex) ? OrderedParticipants[CurrentTurnIndex].ParticipantId : NAME_None;
}

// 按参与者 ID 查找其在有序队列中的下标。
int32 UHSRTurnManager::FindParticipantIndex(FName ParticipantId) const
{
	return OrderedParticipants.IndexOfByPredicate([ParticipantId](const FHSRBattleParticipant& P)
	{
		return P.ParticipantId == ParticipantId;
	});
}

// 推进到下一个合法回合：找出所有存活参与者中剩余距离最小的，
// 全体减去该最小值（相当于时间推进），同速者按 ID 字典序取第一个，并广播 TurnStarted。
bool UHSRTurnManager::AdvanceToNextValidTurn()
{
	TArray<int32> Eligible;
	float Minimum = TNumericLimits<float>::Max();
	for (int32 I = 0; I < OrderedParticipants.Num(); ++I)
	{
		if (IsParticipantTurnEligible(OrderedParticipants[I]))
		{
			// 剩余距离非有限值时直接终止（数值错误）。
			if (!FMath::IsFinite(OrderedParticipants[I].RemainingActionDistance))
			{
				State = EHSRTurnManagerState::Finished;
				CurrentTurnIndex = INDEX_NONE;
				return false;
			}
			Eligible.Add(I);
			Minimum = FMath::Min(Minimum, OrderedParticipants[I].RemainingActionDistance);
		}
	}
	if (Eligible.IsEmpty())
	{
		State = EHSRTurnManagerState::Finished;
		CurrentTurnIndex = INDEX_NONE;
		return false;
	}
	// 收集“冻结”候选：剩余距离等于最小值的参与者（本回合的行动者）。
	TArray<int32> FrozenCandidates;
	for (int32 I : Eligible)
	{
		if (FMath::Abs(OrderedParticipants[I].RemainingActionDistance - Minimum) <= DistanceEpsilon)
		{
			FrozenCandidates.Add(I);
		}
	}
	// 全体减去最小值（时间推进到下一个行动时刻）。
	for (int32 I : Eligible)
	{
		float& Value = OrderedParticipants[I].RemainingActionDistance;
		Value -= Minimum;
		if (FMath::Abs(Value) <= DistanceEpsilon)
		{
			Value = 0.0f;
		}
	}
	// 同速打破平局：按 ID 字典序。
	FrozenCandidates.Sort([this](int32 A, int32 B)
	{
		return OrderedParticipants[A].ParticipantId.LexicalLess(OrderedParticipants[B].ParticipantId);
	});
	CurrentTurnIndex = FrozenCandidates[0];
	State = OrderedParticipants[CurrentTurnIndex].Team == EHSRBattleParticipantTeam::Player
		? EHSRTurnManagerState::PlayerTurn
		: EHSRTurnManagerState::EnemyTurn;
	++TurnSequence;
	BroadcastLifecycleEvent(EHSRTurnLifecycleEventType::TurnStarted, GetCurrentParticipantId());
	return true;
}

// 广播回合生命周期事件（回合开始/结束）。
void UHSRTurnManager::BroadcastLifecycleEvent(EHSRTurnLifecycleEventType Type, FName Id)
{
	if (BattleEpoch == 0 || TurnSequence == 0 || Id.IsNone())
	{
		return;
	}
	FHSRTurnLifecycleEvent Event;
	Event.BattleEpoch = BattleEpoch;
	Event.ParticipantId = Id;
	Event.TurnSequence = TurnSequence;
	Event.EventType = Type;
	if (Type == EHSRTurnLifecycleEventType::TurnStarted)
	{
		TurnStarted.Broadcast(Event);
	}
	else
	{
		TurnEnded.Broadcast(Event);
	}
}

// 当前回合参与者是否仍然“有资格行动”（存活）。
bool UHSRTurnManager::IsCurrentParticipantValid() const
{
	return OrderedParticipants.IsValidIndex(CurrentTurnIndex)
		&& IsParticipantTurnEligible(OrderedParticipants[CurrentTurnIndex]);
}

// 回合资格判定：只有存活者才有资格行动（唯一的判定入口）。
bool UHSRTurnManager::IsParticipantTurnEligible(const FHSRBattleParticipant& P)
{
	return P.IsAlive();
}

// 构建未来若干回合的预估顺序（在副本上模拟，不改变真实回合状态）。
TArray<FHSRTurnForecastEntry> UHSRTurnManager::BuildTurnForecast(int32 SlotCount) const
{
	TArray<FHSRTurnForecastEntry> Forecast;
	if (SlotCount <= 0 || State == EHSRTurnManagerState::Finished)
	{
		return Forecast;
	}

	// 用副本模拟，所以真实回合状态不受影响。
	struct FForecastLane
	{
		FName ParticipantId;
		bool bPlayerTeam = false;
		float Remaining = 0.0f;
		float Base = 0.0f;
	};

	TArray<FForecastLane> Lanes;
	Lanes.Reserve(OrderedParticipants.Num());
	for (const FHSRBattleParticipant& Participant : OrderedParticipants)
	{
		if (!IsParticipantTurnEligible(Participant) || !FMath::IsFinite(Participant.RemainingActionDistance))
		{
			continue;
		}
		FForecastLane& Lane = Lanes.AddDefaulted_GetRef();
		Lane.ParticipantId = Participant.ParticipantId;
		Lane.bPlayerTeam = Participant.Team == EHSRBattleParticipantTeam::Player;
		Lane.Remaining = Participant.RemainingActionDistance;
		Lane.Base = Participant.BaseActionDistance > DistanceEpsilon ? Participant.BaseActionDistance : MaximumBaseActionDistance;
	}
	if (Lanes.Num() == 0)
	{
		return Forecast;
	}

	// 正在行动的参与者占据第 0 格（尽管它的距离已被消耗）。
	const FName ActingId = GetCurrentParticipantId();
	TSet<FName> Seen;
	Forecast.Reserve(SlotCount);
	float ElapsedDistance = 0.0f;

	for (int32 Slot = 0; Slot < SlotCount; ++Slot)
	{
		int32 WinnerIndex = INDEX_NONE;
		// 第 0 格固定是当前行动者。
		if (Slot == 0 && !ActingId.IsNone())
		{
			WinnerIndex = Lanes.IndexOfByPredicate([&ActingId](const FForecastLane& Lane)
			{
				return Lane.ParticipantId == ActingId;
			});
		}
		if (WinnerIndex == INDEX_NONE)
		{
			// 平局按登记顺序（与 AdvanceToNextValidTurn 的冻结候选规则一致）。
			for (int32 Index = 0; Index < Lanes.Num(); ++Index)
			{
				if (WinnerIndex == INDEX_NONE || Lanes[Index].Remaining < Lanes[WinnerIndex].Remaining - DistanceEpsilon)
				{
					WinnerIndex = Index;
				}
			}
		}
		if (WinnerIndex == INDEX_NONE)
		{
			break;
		}

		FForecastLane& Winner = Lanes[WinnerIndex];
		ElapsedDistance += FMath::Max(0.0f, Winner.Remaining);

		FHSRTurnForecastEntry& Entry = Forecast.AddDefaulted_GetRef();
		Entry.ParticipantId = Winner.ParticipantId;
		Entry.bPlayerTeam = Winner.bPlayerTeam;
		Entry.SlotIndex = Slot;
		Entry.DistanceUntilAction = ElapsedDistance;
		Entry.bRepeatAction = Seen.Contains(Winner.ParticipantId);
		Seen.Add(Winner.ParticipantId);

		// 与 AdvanceTurn 一样归一化：全体减去消耗值，然后把刚行动者充值回基础距离。
		const float Consumed = FMath::Max(0.0f, Winner.Remaining);
		for (FForecastLane& Lane : Lanes)
		{
			Lane.Remaining = FMath::Max(0.0f, Lane.Remaining - Consumed);
		}
		Winner.Remaining = Winner.Base;
	}

	return Forecast;
}

// 构造一次调整的结果对象：填入当前纪元/序号/参与者及目标的前后距离快照。
FHSRActionDistanceResult UHSRTurnManager::MakeAdjustmentResult(EHSRActionDistanceAdjustmentResult Result, int32 Index, const FHSRActionDistanceResult* OldSnapshot) const
{
	FHSRActionDistanceResult Out;
	Out.Result = Result;
	Out.BattleEpoch = BattleEpoch;
	Out.TurnSequence = TurnSequence;
	Out.CurrentParticipantId = GetCurrentParticipantId();
	Out.NextParticipantId = GetCurrentParticipantId();
	Out.PendingOperationCount = PendingPostActionOperations.Num();
	Out.OldPendingOperationCount = Out.NewPendingOperationCount = Out.PendingOperationCount;
	if (OrderedParticipants.IsValidIndex(Index))
	{
		const FHSRBattleParticipant& P = OrderedParticipants[Index];
		Out.OldBase = P.BaseActionDistance;
		Out.NewBase = P.BaseActionDistance;
		Out.OldRemaining = P.RemainingActionDistance;
		Out.NewRemaining = P.RemainingActionDistance;
	}
	if (OldSnapshot)
	{
		Out.OldBase = OldSnapshot->OldBase;
		Out.OldRemaining = OldSnapshot->OldRemaining;
		Out.OldPendingOperationCount = OldSnapshot->PendingOperationCount;
	}
	return Out;
}
#if WITH_EDITOR
// 开发测试用：把当前回合者的 Actor 弱引用清空，模拟“无效当前参与者”。
bool UHSRTurnManager::InvalidateCurrentParticipantForDevelopmentTest()
{
	if (!OrderedParticipants.IsValidIndex(CurrentTurnIndex))
	{
		return false;
	}
	OrderedParticipants[CurrentTurnIndex].Actor.Reset();
	return true;
}
#endif
#if WITH_DEV_AUTOMATION_TESTS
// 自动化测试用：读取某参与者的速度/基准距离/剩余距离/挂起数。
bool UHSRTurnManager::GetActionDistanceForAutomation(FName ParticipantId, float& OutSpeed, float& OutBase, float& OutRemaining, int32& OutPending) const
{
	const int32 Index = FindParticipantIndex(ParticipantId);
	if (!OrderedParticipants.IsValidIndex(Index))
	{
		return false;
	}
	const FHSRBattleParticipant& P = OrderedParticipants[Index];
	OutSpeed = P.EffectiveSpeed;
	OutBase = P.BaseActionDistance;
	OutRemaining = P.RemainingActionDistance;
	OutPending = PendingPostActionOperations.Num();
	return true;
}
// 自动化测试用：直接写入某参与者的速度/基准距离/剩余距离。
bool UHSRTurnManager::SetActionDistanceForAutomation(FName ParticipantId, float InSpeed, float InBase, float InRemaining)
{
	const int32 Index = FindParticipantIndex(ParticipantId);
	if (!OrderedParticipants.IsValidIndex(Index))
	{
		return false;
	}
	FHSRBattleParticipant& P = OrderedParticipants[Index];
	P.EffectiveSpeed = InSpeed;
	P.BaseActionDistance = InBase;
	P.RemainingActionDistance = InRemaining;
	return true;
}
#endif
