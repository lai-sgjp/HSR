#include "HSRBattleCommandViewModel.h"

#include "../Battle/HSRBattleCoordinator.h"
#include "../Battle/HSRBattleParticipant.h"
#include "../Battle/HSRBattleTypes.h"

// 销毁前必须先解除与协调器的绑定，避免销毁过程中协调器再向本 VM 推送状态。
void UHSRBattleCommandViewModel::BeginDestroy()
{
	UnbindCoordinator();
	Super::BeginDestroy();
}

// SetState 是数据写入入口：外部（协调器）把最新一轮的纯值战斗状态快照推进来。
// 本方法把快照存入 State，并根据快照内容判断“之前挂起的提交”是否已被解析：
// 挂起提交的匹配条件是——战斗 ID 一致，且最后一次解析记录恰好是我们等待的那个动作。
void UHSRBattleCommandViewModel::SetState(const FHSRBattleCommandViewState& InState)
{
	State = InState;
	// 若存在挂起的提交，且满足“战斗已切换/战斗失效/该动作已被解析”之一，则清除挂起状态。
	if (bCommandPending && (!PendingBattleId.IsValid() || State.BattleId != PendingBattleId || !State.BattleId.IsValid()
		|| (State.LastResolution.ActionId == PendingActionId && PendingActionId.IsValid())))
	{
		bCommandPending = false;
		PendingBattleId.Invalidate();
		PendingActionId.Invalidate();
	}
	// 数据更新后刷新所有派生显示字段，并广播新快照通知订阅的 Widget。
	RefreshPresentationAndSelection();
	RefreshCommandState();
	RefreshReadOnlyBattlePresentation();
	Changed.Broadcast(State);
}

// 绑定协调器：VM 需要从协调器读取一些只读的战斗展示信息（如破盾/延迟的注册结果）。
// 绑定前先解绑旧的，避免同一 VM 重复绑定导致状态错乱。
void UHSRBattleCommandViewModel::BindCoordinator(UHSRBattleCoordinator* InCoordinator)
{
	UnbindCoordinator();
	Coordinator = InCoordinator;
	RefreshReadOnlyBattlePresentation();
	UE_LOG(LogTemp, Log, TEXT("P8-005 ViewModel Bind Coordinator=%s Target=%s"), InCoordinator ? TEXT("valid") : TEXT("null"), *SelectedTargetId.ToString());
}

// 解绑协调器：清空引用并清除所有命令锁（挂起提交、展示锁）。
void UHSRBattleCommandViewModel::UnbindCoordinator()
{
	Coordinator.Reset();
	ClearCommandLocks();
	UE_LOG(LogTemp, Log, TEXT("P8-005 ViewModel Unbind"));
}

// 在当前快照中查找“已选技能”的视图数据；未选择或不存在时返回 nullptr。
const FHSRBattleCommandSkillView* UHSRBattleCommandViewModel::FindSelectedSkill() const
{
	return State.FindSkill(SelectedSkillId);
}

// 核心展示数据刷新：把 State 中的纯值数据翻译成 UI 可直接显示的文字。
// 这包括当前行动者、能量、技能点、最近一次解析结果、状态列表、回合顺序、
// 参与者列表、战斗事件流水等。UI 只消费这里生成的 FText，不直接触碰战斗对象。
void UHSRBattleCommandViewModel::RefreshPresentationAndSelection()
{
	CurrentActorText = FText::Format(NSLOCTEXT("HSRCommand", "CurrentActor", "Actor: {0}"), State.GetParticipantLabel(State.CurrentActorId));
	EnergyText = FText::Format(NSLOCTEXT("HSRCommand", "Energy", "Energy: {0} / {1}"), FText::AsNumber(FMath::RoundToInt(State.Energy)), FText::AsNumber(FMath::RoundToInt(State.MaxEnergy)));
	SkillPointsText = FText::Format(NSLOCTEXT("HSRCommand", "SkillPoints", "Skill Points: {0} / {1}"), FText::AsNumber(State.SkillPoints), FText::AsNumber(State.MaxSkillPoints));
	LastResolutionText = FText::Format(NSLOCTEXT("HSRCommand", "Resolution", "Last Resolution: {0} ({1})"), FText::AsNumber(static_cast<int32>(State.LastResolution.Status)), FText::AsNumber(static_cast<int32>(State.LastResolution.FailureReason)));
	// 把每个状态实例拼成一行文本，多行之间用换行连接。
	TArray<FString> StatusLines;
	for (const FHSRStatusPublicSnapshot& Status : State.Statuses)
	{
		StatusLines.Add(FString::Printf(TEXT("%s | %s | %s | %s | x%d | %d"), *State.GetParticipantLabel(Status.TargetParticipantId).ToString(), *Status.StatusId.ToString(), *Status.DisplayName.ToString(),
			Status.Classification == EHSRStatusClassification::Buff ? TEXT("Buff") : TEXT("Debuff"), Status.Stacks, Status.RemainingTurns));
	}
	StatusText = FText::FromString(FString::Join(StatusLines, TEXT("\n")));
	// 最近一次状态操作事件：有有效序列号才生成文本，否则为空。
	const FHSRStatusPublicOperationEvent& Operation = State.LastStatusOperation;
	StatusOperationText = Operation.Sequence > 0
		? FText::FromString(FString::Printf(TEXT("%s | %s | op=%d | result=%d | #%lld"), *Operation.TargetParticipantId.ToString(), *Operation.StatusId.ToString(), static_cast<int32>(Operation.Operation), static_cast<int32>(Operation.Result), Operation.Sequence))
		: FText::GetEmpty();
	// 回合顺序：参与者 ID 依次展开成“A -> B -> C”文本。
	TArray<FString> OrderLines;
	for (const FName ParticipantId : State.TurnOrderParticipantIds)
	{
		OrderLines.Add(State.GetParticipantLabel(ParticipantId).ToString());
	}
	TurnOrderText = FText::FromString(FString::Join(OrderLines, TEXT(" -> ")));
	// 参与者列表：每个参与者的 HP/能量/韧性/弱点排成一行，弱点标签按字典序排序后展示。
	TArray<FString> ParticipantLines;
	for (const FHSRBattleParticipantView& Participant : State.Participants)
	{
		TArray<FString> WeaknessNames;
		for (const FGameplayTag& WeaknessTag : Participant.WeaknessTags)
		{
			WeaknessNames.Add(WeaknessTag.ToString());
		}
		WeaknessNames.Sort();
		const FString ParticipantWeaknessText = WeaknessNames.IsEmpty() ? TEXT("None") : FString::Join(WeaknessNames, TEXT(", "));
		ParticipantLines.Add(FString::Printf(TEXT("%s | HP %.0f/%.0f | Energy %.0f/%.0f | Toughness %.0f/%.0f | Weakness %s%s"),
			*Participant.GetDisplayLabel().ToString(), Participant.Health, Participant.MaxHealth, Participant.Energy, Participant.MaxEnergy,
			Participant.Toughness, Participant.MaxToughness, *ParticipantWeaknessText, Participant.bDefeated ? TEXT(" | Defeated") : TEXT("")));
	}
	ParticipantsText = FText::FromString(FString::Join(ParticipantLines, TEXT("\n")));
	// 战斗事件流水：来源 -> 目标 | 事件类型 数值 (暴击/破盾标记)。
	TArray<FString> PresentationLines;
	for (const FHSRBattlePresentationEvent& Event : State.PresentationEvents)
	{
		PresentationLines.Add(FString::Printf(TEXT("%s -> %s | %s %.0f%s%s"),
			*State.GetParticipantLabel(Event.SourceParticipantId).ToString(),
			*State.GetParticipantLabel(Event.TargetParticipantId).ToString(),
			*Event.GetEventTypeLabel().ToString(),
			Event.Value,
			Event.bCritical ? TEXT(" | Critical") : TEXT(""),
			Event.bBreak ? TEXT(" | Break") : TEXT("")));
	}
	PresentationText = FText::FromString(FString::Join(PresentationLines, TEXT("\n")));

	// 选择收敛：若当前技能指针已失效（技能列表变化），回退到第一个技能；
	// 若当前目标不在所选技能的候选目标中，回退到第一个候选目标。
	const FHSRBattleCommandSkillView* SelectedSkill = FindSelectedSkill();
	if (!SelectedSkill)
	{
		SelectedSkillId = State.Skills.Num() > 0 ? State.Skills[0].SkillId : NAME_None;
		SelectedSkill = FindSelectedSkill();
	}
	if (!SelectedSkill || !SelectedSkill->CandidateTargetIds.Contains(SelectedTargetId))
	{
		SelectedTargetId = SelectedSkill && SelectedSkill->CandidateTargetIds.Num() > 0 ? SelectedSkill->CandidateTargetIds[0] : NAME_None;
	}
	RefreshReadOnlyBattlePresentation();
	RefreshCommandState();
}

// 按技能 ID 选择技能：找到技能后更新选择并广播新快照。返回是否选择成功。
bool UHSRBattleCommandViewModel::SelectSkillById(FName SkillId)
{
	const FHSRBattleCommandSkillView* Skill = State.FindSkill(SkillId);
	if (!Skill)
	{
		return false;
	}

	SelectedSkillId = Skill->SkillId;
	SelectedTargetId = Skill->CandidateTargetIds.Num() > 0 ? Skill->CandidateTargetIds[0] : NAME_None;

	RefreshReadOnlyBattlePresentation();
	RefreshCommandState();
	Changed.Broadcast(State);
	return true;
}

// Category-keyed selection is first-match-wins, so it cannot reach a second skill sharing a
// category. Kept as a shim for existing callers; new code should use SelectSkillById.
// 按类别选择技能：同一类别只会命中第一个技能（类别键不唯一），仅作为兼容入口保留；
// 新代码应优先使用 SelectSkillById 精确选择。
bool UHSRBattleCommandViewModel::SelectSkill(EHSRSkillCategory Category)
{
	const FHSRBattleCommandSkillView* Skill = State.FindSkillByCategory(Category);
	if (Skill)
	{
		return SelectSkillById(Skill->SkillId);
	}
	return false;
}

// 返回当前所选技能的候选目标列表；未选中技能时返回空列表。
TArray<FName> UHSRBattleCommandViewModel::GetTargetOptions() const
{
	const FHSRBattleCommandSkillView* Skill = FindSelectedSkill();
	if (Skill)
	{
		return Skill->CandidateTargetIds;
	}
	return TArray<FName>();
}

// 选择目标：目标必须是当前所选技能的合法候选目标，否则拒绝。
bool UHSRBattleCommandViewModel::SelectTarget(FName TargetId)
{
	if (!GetTargetOptions().Contains(TargetId))
	{
		return false;
	}
	SelectedTargetId = TargetId;
	RefreshReadOnlyBattlePresentation();
	RefreshCommandState();
	Changed.Broadcast(State);
	return true;
}

// 提交战斗命令的前置校验：只有满足全部条件（合法动作 ID、战斗进行中、
// 当前行动者是玩家控制、无挂起提交、无展示锁、技能可用、技能与目标选择一致）
// 才登记“挂起提交”，等待后续 ResolveCommandSubmit 带回解析结果。
bool UHSRBattleCommandViewModel::BeginCommandSubmit(const FGuid& ActionId, FName ActorParticipantId, FName SkillId, FName TargetParticipantId)
{
	const FHSRBattleCommandSkillView* Skill = State.FindSkill(SkillId);
	if (!ActionId.IsValid() || !State.BattleId.IsValid() || !State.bCurrentActorPlayerControlled || bCommandPending || bPresentationLocked || ActorParticipantId != State.CurrentActorId
		|| !Skill || !Skill->bAvailable || SkillId != SelectedSkillId || TargetParticipantId != SelectedTargetId || !Skill->CandidateTargetIds.Contains(TargetParticipantId))
	{
		return false;
	}
	PendingBattleId = State.BattleId;
	PendingActionId = ActionId;
	bCommandPending = true;
	RefreshCommandState();
	Changed.Broadcast(State);
	return true;
}

// 解析挂起的提交：当解析结果对应的战斗与动作正好匹配挂起记录时，
// 说明该命令已真正落地，清除挂起状态并广播。
void UHSRBattleCommandViewModel::ResolveCommandSubmit(const FGuid& BattleId, const FHSRAbilityResolution& Resolution)
{
	if (bCommandPending && BattleId == PendingBattleId && Resolution.ActionId == PendingActionId)
	{
		bCommandPending = false;
		PendingBattleId.Invalidate();
		PendingActionId.Invalidate();
		RefreshCommandState();
		Changed.Broadcast(State);
	}
}

// 清除所有命令锁：挂起提交、展示锁、挂起 ID 一并复位。
void UHSRBattleCommandViewModel::ClearCommandLocks()
{
	bCommandPending = false;
	bPresentationLocked = false;
	PendingBattleId.Invalidate();
	PendingActionId.Invalidate();
	RefreshCommandState();
}

// 展示战斗结果：只有结果合法、对应本场战斗、且结果界面尚未显示时才接受。
bool UHSRBattleCommandViewModel::ShowBattleResult(const FHSRBattleResult& Result)
{
	if (!Result.IsValid() || State.BattleId != Result.RequestId || State.ResultViewState.bVisible || State.ResultViewState.RequestId.IsValid())
	{
		return false;
	}
	State.ResultViewState.RequestId = Result.RequestId;
	State.ResultViewState.Outcome = Result.Outcome;
	State.ResultViewState.DefeatedParticipantId = Result.DefeatedParticipantId;
	State.ResultViewState.RewardDefinitionId = Result.RewardDefinitionId;
	State.ResultViewState.RewardGrants.Reset();
	State.ResultViewState.RewardRevision = 0;
	State.ResultViewState.bVisible = true;
	State.ResultViewState.bConfirmPending = false;
	ClearCommandLocks();
	RefreshCommandState();
	Changed.Broadcast(State);
	return true;
}

// 设置战斗结果的奖励数据：结果界面可见且领奖请求合法时才更新奖励展示字段。
void UHSRBattleCommandViewModel::SetBattleResultReward(const FHSRRewardReceipt& Receipt)
{
	if (!State.ResultViewState.bVisible || !Receipt.Request.ClaimId.IsValid())
	{
		return;
	}
	State.ResultViewState.RewardDefinitionId = Receipt.Request.RewardDefinitionId;
	State.ResultViewState.RewardGrants = Receipt.Grants;
	State.ResultViewState.RewardRevision = Receipt.Revision;
	Changed.Broadcast(State);
}

// 请求确认战斗结果：触发一次“确认请求”广播，供外部（如奖励发放逻辑）响应。
bool UHSRBattleCommandViewModel::RequestBattleResultConfirm()
{
	if (!State.ResultViewState.bVisible || State.ResultViewState.bConfirmPending || !State.ResultViewState.RequestId.IsValid())
	{
		return false;
	}
	State.ResultViewState.bConfirmPending = true;
	RefreshCommandState();
	Changed.Broadcast(State);
	ResultConfirmRequested.Broadcast(State.ResultViewState.RequestId);
	return true;
}

// 拒绝确认请求：仅当当前正处于挂起确认且请求 ID 匹配时生效。
void UHSRBattleCommandViewModel::RejectBattleResultConfirm(const FGuid& RequestId)
{
	if (!State.ResultViewState.bVisible || !State.ResultViewState.bConfirmPending || State.ResultViewState.RequestId != RequestId)
	{
		return;
	}
	State.ResultViewState.bConfirmPending = false;
	RefreshCommandState();
	Changed.Broadcast(State);
}

// 清除战斗结果展示：把结果视图状态整体重置为默认值。
void UHSRBattleCommandViewModel::ClearBattleResult()
{
	State.ResultViewState = FHSRBattleResultViewState();
	RefreshCommandState();
}

// 刷新“可提交”标志：综合战斗状态、锁状态、协调器有效性与技能/目标选择，
// 决定“执行”按钮是否可用。UI 只读这个汇总后的布尔值。
void UHSRBattleCommandViewModel::RefreshCommandState()
{
	State.SelectedSkillId = SelectedSkillId;
	State.SelectedTargetId = SelectedTargetId;
	State.bCommandPending = bCommandPending;
	State.bPresentationLocked = bPresentationLocked;
	State.PendingActionId = PendingActionId;
	const FHSRBattleCommandSkillView* Skill = FindSelectedSkill();
	State.bCanSubmit = !State.ResultViewState.bVisible && !bCommandPending && !bPresentationLocked && Coordinator.IsValid() && State.BattleId.IsValid() && State.bCurrentActorPlayerControlled
		&& Skill && Skill->bAvailable && Skill->CandidateTargetIds.Contains(SelectedTargetId);
}

// 刷新只读的战斗展示（弱点/韧性/破盾/延迟）。
// 这些数据全部来自快照中的参与者视图或协调器，UI 不直接观察任何 ASC。
void UHSRBattleCommandViewModel::RefreshReadOnlyBattlePresentation()
{
	WeaknessText = FText::GetEmpty();
	ToughnessText = FText::GetEmpty();
	BreakText = FText::GetEmpty();
	DelayText = FText::GetEmpty();

	// Toughness and weaknesses come from the participant view the Coordinator already baked from the
	// ASC, so the UI never observes a live ASC of its own. bHasAttributes distinguishes "never read"
	// from a genuine zero, which is why an ASC-less participant still renders no text at all.
	// 韧性与弱点来自协调器烘焙好的参与者视图，因此 UI 从不自行观察 ASC；
	// bHasAttributes 用于区分“从未读取”与“真实的 0 值”，没有 ASC 的参与者因此不显示任何文本。
	const FHSRBattleParticipantView* TargetView = State.Participants.FindByPredicate(
		[this](const FHSRBattleParticipantView& Candidate)
		{
			return Candidate.ParticipantId == SelectedTargetId;
		});
	if (TargetView && TargetView->bHasAttributes)
	{
		FString Weaknesses;
		for (const FGameplayTag& Tag : TargetView->WeaknessTags)
		{
			if (!Weaknesses.IsEmpty())
			{
				Weaknesses += TEXT(", ");
			}
			Weaknesses += Tag.ToString();
		}
		WeaknessText = FText::Format(
			NSLOCTEXT("HSRCommand", "Weakness", "Weakness: {0}"),
			FText::FromString(Weaknesses.IsEmpty() ? TEXT("None") : Weaknesses));
		ToughnessText = FText::Format(
			NSLOCTEXT("HSRCommand", "Toughness", "Toughness: {0} / {1}"),
			FText::AsNumber(FMath::RoundToInt(TargetView->Toughness)),
			FText::AsNumber(FMath::RoundToInt(TargetView->MaxToughness)));
	}

	// 破盾/延迟信息需要协调器配合：破盾是否触发、延迟是否登记成功。
	if (!Coordinator.IsValid())
	{
		return;
	}
	if (State.LastResolution.bHasBreakResult)
	{
		const FHSRBreakResult& Break = State.LastResolution.BreakResult;
		BreakText = FText::Format(NSLOCTEXT("HSRCommand", "Break", "Break: {0}"), FText::FromString(Break.bTriggered ? TEXT("Triggered") : TEXT("Not triggered")));
		const bool bDelayRegistered = Break.bTriggered && Coordinator->GetLastBreakDelayActionId() == Break.ActionId && Coordinator->WasLastBreakDelayRegistered();
		DelayText = FText::Format(NSLOCTEXT("HSRCommand", "Delay", "Delay: {0}"), FText::FromString(Break.bTriggered ? (bDelayRegistered ? TEXT("Registered") : TEXT("Rejected")) : TEXT("Not requested")));
	}
	UE_LOG(LogTemp, Log, TEXT("P8-005 View Target=%s Weakness=%s Toughness=%s Break=%s Delay=%s"), *SelectedTargetId.ToString(), *WeaknessText.ToString(), *ToughnessText.ToString(), *BreakText.ToString(), *DelayText.ToString());
}
