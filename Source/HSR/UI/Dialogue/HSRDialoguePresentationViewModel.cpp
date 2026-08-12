#include "HSRDialoguePresentationViewModel.h"

#include "../../Dialogue/HSRDialogueSubsystem.h"

// BeginDestroy：UObject 即将销毁前调用。
// 先停机（清空引用与快照状态），再走父类销毁流程，确保不再触发任何广播。
void UHSRDialoguePresentationViewModel::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

// Initialize：把一个有效的 Dialogue 子系统接入本 ViewModel。
// 约定上只允许初始化一次，因此先 Shutdown 复位旧状态，再记录数据源并重置快照；
// 外部没有传入子系统时 LastResult 记为 Unavailable，表示当前无法工作。
void UHSRDialoguePresentationViewModel::Initialize(UHSRDialogueSubsystem* InDialogue)
{
	Shutdown();
	Dialogue = InDialogue;
	Snapshot = FHSRDialoguePresentationSnapshot();
	LastResult = InDialogue ? EHSRDialoguePresentationResult::NoOp : EHSRDialoguePresentationResult::Unavailable;
}

// Shutdown：复位全部状态。
// 清空子系统引用、快照与上一次操作的结果，让本 ViewModel 回到“未初始化”状态，
// 避免残留旧数据影响下一次会话或测试用例。
void UHSRDialoguePresentationViewModel::Shutdown()
{
	Dialogue.Reset();
	Snapshot = FHSRDialoguePresentationSnapshot();
	LastResult = EHSRDialoguePresentationResult::Unavailable;
	LastAuthorityResult = EHSRQuestOperationResult::NoOp;
	LastChoiceResult = FHSRDialogueChoiceResult();
}

// GetSnapshot：把当前内部快照导出给外部（如 Overlay Widget）读取。
// 返回快照是否有效，调用方据此决定是否展示界面。
bool UHSRDialoguePresentationViewModel::GetSnapshot(FHSRDialoguePresentationSnapshot& OutSnapshot) const
{
	OutSnapshot = Snapshot;
	return Snapshot.bIsValid;
}

// MapNodeResult：把“节点级”权威结果枚举翻译成展示层结果枚举。
// 权威层（任务系统）的错误语义更细，但 UI 只需知道少数几类原因，
// 因此这里做一次收敛映射；其余未知错误统一落到 AuthorityRejected。
EHSRDialoguePresentationResult UHSRDialoguePresentationViewModel::MapNodeResult(EHSRQuestOperationResult AuthorityResult)
{
	switch (AuthorityResult)
	{
	case EHSRQuestOperationResult::Success:
		return EHSRDialoguePresentationResult::Success;
	case EHSRQuestOperationResult::UnknownDialogueDefinition:
		return EHSRDialoguePresentationResult::UnknownDialogue;
	case EHSRQuestOperationResult::InvalidEvent:
		return EHSRDialoguePresentationResult::InvalidNode;
	default:
		return EHSRDialoguePresentationResult::AuthorityRejected;
	}
}

// MapBranchResult：把“分支级”权威结果枚举翻译成展示层结果枚举。
// 分支操作包含成功、无操作、各种无效/冲突等细分结果，逐一映射到展示层对应值。
EHSRDialoguePresentationResult UHSRDialoguePresentationViewModel::MapBranchResult(
	EHSRDialogueChoiceOperationResult BranchResult)
{
	switch (BranchResult)
	{
	case EHSRDialogueChoiceOperationResult::Success:
		return EHSRDialoguePresentationResult::Success;
	case EHSRDialogueChoiceOperationResult::NoOp:
		return EHSRDialoguePresentationResult::NoOp;
	case EHSRDialogueChoiceOperationResult::InvalidRequest:
		return EHSRDialoguePresentationResult::InvalidRequest;
	case EHSRDialogueChoiceOperationResult::UnknownDialogueDefinition:
		return EHSRDialoguePresentationResult::UnknownDialogue;
	case EHSRDialogueChoiceOperationResult::InvalidChoice:
		return EHSRDialoguePresentationResult::InvalidChoice;
	case EHSRDialogueChoiceOperationResult::AuthorityUnavailable:
		return EHSRDialoguePresentationResult::AuthorityUnavailable;
	case EHSRDialogueChoiceOperationResult::OperationIdConflict:
		return EHSRDialoguePresentationResult::OperationIdConflict;
	default:
		return EHSRDialoguePresentationResult::AuthorityRejected;
	}
}

// MapBranchAuthorityResult：反向收敛——把一次分支选择的具体结果，折成一个
// 任务系统层面的结果枚举，便于上层统一判断成败。
// 不同分支类型（任务/奖励/遭遇）各自换算：奖励分支把成功/无操作/失败折成对应结果，
// 遭遇分支只看遭遇响应是否成功；无分支（None）视为成功。
EHSRQuestOperationResult UHSRDialoguePresentationViewModel::MapBranchAuthorityResult(
	const FHSRDialogueChoiceResult& Result)
{
	switch (Result.Branch)
	{
	case EHSRDialogueChoiceBranch::Quest:
		return Result.QuestResult;
	case EHSRDialogueChoiceBranch::Reward:
		return Result.RewardResult == EHSRRewardOperationResult::Success
			? EHSRQuestOperationResult::Success
			: (Result.RewardResult == EHSRRewardOperationResult::NoOp
				? EHSRQuestOperationResult::NoOp
				: EHSRQuestOperationResult::RewardRejected);
	case EHSRDialogueChoiceBranch::Encounter:
		return Result.EncounterResponse.ResultType == EHSREncounterResultType::Success
			? EHSRQuestOperationResult::Success
			: EHSRQuestOperationResult::InvalidState;
	case EHSRDialogueChoiceBranch::None:
	default:
		return EHSRQuestOperationResult::Success;
	}
}

// BuildSnapshot：根据当前节点定义与权威结果，构造一份“展示层快照”。
// 快照是纯值 DTO：包含状态/有效标记、当前对话与节点 ID、说话人/正文文本、
// 权威结果，并把节点里的选项定义逐一转换为带“可用性”的展示选项。
FHSRDialoguePresentationSnapshot UHSRDialoguePresentationViewModel::BuildSnapshot(
	const FGuid& QueryId, FName DialogueId, const FHSRDialogueNodeDefinition& Node,
	EHSRQuestOperationResult AuthorityResult)
{
	FHSRDialoguePresentationSnapshot Next;
	Next.Status = EHSRDialoguePresentationStatus::Active;
	Next.bIsValid = true;
	Next.bIsActive = true;
	Next.QueryId = QueryId;
	Next.DialogueId = DialogueId;
	Next.NodeId = Node.NodeId;
	Next.SpeakerText = Node.SpeakerText;
	Next.BodyText = Node.Text;
	Next.AuthorityResult = AuthorityResult;
	for (const FHSRDialogueChoiceDefinition& Choice : Node.Choices)
	{
		// 每个选项复制出展示层副本；ChoiceId 为空视为不可点（占位选项）。
		FHSRDialoguePresentationChoice& ViewChoice = Next.Choices.AddDefaulted_GetRef();
		ViewChoice.ChoiceId = Choice.ChoiceId;
		ViewChoice.DisplayText = Choice.DisplayText;
		ViewChoice.TargetNodeId = Choice.TargetNodeId;
		ViewChoice.bEnabled = !Choice.ChoiceId.IsNone();
		ViewChoice.Branch = Choice.Branch;
		ViewChoice.OperationId = Choice.BranchOperationId;
	}
	return Next;
}

// IsActiveRequest（节点请求）：判断某次“打开节点”请求是否就是当前正在进行的会话。
// 通过快照的活动标记与请求的三元组（QueryId/DialogueId/NodeId）逐项比对，
// 用于识别“重复请求”或“过期请求”（Stale）。
bool UHSRDialoguePresentationViewModel::IsActiveRequest(const FHSRDialoguePresentationRequest& Request) const
{
	return Snapshot.bIsActive && Snapshot.QueryId == Request.QueryId
		&& Snapshot.DialogueId == Request.DialogueId && Snapshot.NodeId == Request.NodeId;
}

// IsActiveRequest（选项请求）：对“提交选项”请求做同样的会话匹配判定。
bool UHSRDialoguePresentationViewModel::IsActiveRequest(const FHSRDialoguePresentationChoiceRequest& Request) const
{
	return Snapshot.bIsActive && Snapshot.QueryId == Request.QueryId
		&& Snapshot.DialogueId == Request.DialogueId && Snapshot.NodeId == Request.NodeId;
}

// Reject：登记一次“拒绝”结果并返回给调用方。
// 把展示层结果与对应的权威层结果同时记录，方便上层日志/测试追溯失败原因。
EHSRDialoguePresentationResult UHSRDialoguePresentationViewModel::Reject(
	EHSRDialoguePresentationResult Result, EHSRQuestOperationResult AuthorityResult)
{
	LastResult = Result;
	LastAuthorityResult = AuthorityResult;
	return Result;
}

// Publish：把新快照提交为当前状态并广播给订阅者（Widget 层）。
// 这是数据从“计算完成”到“UI 可显示”的关键一跳：快照被移动进内部状态，
// 同时通过两个委托广播出去（Changed 供强绑定，OnSnapshotChanged 供动态绑定）。
void UHSRDialoguePresentationViewModel::Publish(FHSRDialoguePresentationSnapshot&& Next,
	EHSRDialoguePresentationResult Result, EHSRQuestOperationResult AuthorityResult)
{
	Snapshot = MoveTemp(Next);
	LastResult = Result;
	LastAuthorityResult = AuthorityResult;
	Changed.Broadcast(Snapshot);
	OnSnapshotChanged.Broadcast(Snapshot);
}

// BeginDialogue：请求打开一段对话并定位到指定节点。
// 数据流：先做一系列前置校验（请求合法性、是否已有活动会话、子系统是否可用），
// 再从 Dialogue 子系统读取节点定义，最后构建快照并广播（Publish）。
// 每次打开对话都会清空上次分支选择结果。
EHSRDialoguePresentationResult UHSRDialoguePresentationViewModel::BeginDialogue(
	const FHSRDialoguePresentationRequest& Request)
{
	if (!Request.IsValid())
	{
		// 请求本身不合法（缺 QueryId/DialogueId/NodeId）直接拒绝。
		return Reject(EHSRDialoguePresentationResult::InvalidRequest, EHSRQuestOperationResult::NoOp);
	}
	if (Snapshot.bIsActive)
	{
		// 已有活动会话：若请求与当前会话一致则视为重复的无操作请求，否则拒绝为新开。
		return Reject(IsActiveRequest(Request) ? EHSRDialoguePresentationResult::NoOp : EHSRDialoguePresentationResult::AlreadyActive,
			EHSRQuestOperationResult::NoOp);
	}
	if (!Dialogue.IsValid())
	{
		// 子系统未接入时无法提供服务。
		return Reject(EHSRDialoguePresentationResult::Unavailable, EHSRQuestOperationResult::InvalidState);
	}

	// 从权威层读取节点定义；失败则把权威结果映射为展示层结果后拒绝。
	FHSRDialogueNodeDefinition Node;
	const EHSRQuestOperationResult AuthorityResult = Dialogue->GetNode(Request.DialogueId, Request.NodeId, Node);
	if (AuthorityResult != EHSRQuestOperationResult::Success)
	{
		return Reject(MapNodeResult(AuthorityResult), AuthorityResult);
	}

	// 构建展示快照并广播；同时清空上次分支选择结果，避免串会话。
	Publish(BuildSnapshot(Request.QueryId, Request.DialogueId, Node, AuthorityResult),
		EHSRDialoguePresentationResult::Success, AuthorityResult);
	LastChoiceResult = FHSRDialogueChoiceResult();
	return EHSRDialoguePresentationResult::Success;
}

// SubmitChoice：玩家选择了一个对话选项。
// 数据流：先做请求/会话状态校验，再在当前快照的选项里按 ChoiceId 找到展示选项，
// 交给 Dialogue 子系统实际执行分支选择；若结果指向下一个节点则读取并广播新节点快照，
// 若结果表示对话结束则广播一个关闭态快照。
EHSRDialoguePresentationResult UHSRDialoguePresentationViewModel::SubmitChoice(
	const FHSRDialoguePresentationChoiceRequest& Request)
{
	if (!Request.IsValid())
	{
		// 请求不合法（如 ChoiceId 为空）直接拒绝。
		return Reject(EHSRDialoguePresentationResult::InvalidRequest, EHSRQuestOperationResult::NoOp);
	}
	if (!Snapshot.bIsActive)
	{
		// 没有活动会话，选择无从谈起。
		return Reject(EHSRDialoguePresentationResult::Closed, EHSRQuestOperationResult::NoOp);
	}
	if (!IsActiveRequest(Request))
	{
		// 会话三元组对不上，说明请求已过期，拒绝执行。
		return Reject(EHSRDialoguePresentationResult::StaleRequest, EHSRQuestOperationResult::NoOp);
	}
	if (!Dialogue.IsValid())
	{
		// 子系统未接入时无法提供服务。
		return Reject(EHSRDialoguePresentationResult::Unavailable, EHSRQuestOperationResult::InvalidState);
	}
	// 在当前快照的选项列表里按 ChoiceId 定位展示选项。
	const FHSRDialoguePresentationChoice* PresentationChoice = Snapshot.Choices.FindByPredicate(
		[&Request](const FHSRDialoguePresentationChoice& Choice)
		{
			return Choice.ChoiceId == Request.ChoiceId;
		});
	if (!PresentationChoice || !PresentationChoice->bEnabled)
	{
		// 选项不存在或已被禁用，拒绝本次选择。
		return Reject(EHSRDialoguePresentationResult::InvalidChoice, EHSRQuestOperationResult::InvalidEvent);
	}

	// 组装权威层请求并交给 Dialogue 子系统执行分支选择。
	FHSRDialogueChoiceResult ChoiceResult;
	FHSRDialogueChoiceRequest AuthorityRequest;
	AuthorityRequest.DialogueId = Request.DialogueId;
	AuthorityRequest.NodeId = Request.NodeId;
	AuthorityRequest.ChoiceId = Request.ChoiceId;
	const EHSRDialogueChoiceOperationResult BranchResult = Dialogue->SelectChoice(
		AuthorityRequest, ChoiceResult);
	// 记录分支结果，供上层（日志/测试）追溯。
	LastChoiceResult = ChoiceResult;
	const EHSRQuestOperationResult AuthorityResult = MapBranchAuthorityResult(ChoiceResult);
	if (BranchResult != EHSRDialogueChoiceOperationResult::Success
		&& BranchResult != EHSRDialogueChoiceOperationResult::NoOp)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Dialogue Presentation branch rejected Result=%d Branch=%d QuestResult=%d RewardResult=%d EncounterResult=%d OperationId=%s"),
			static_cast<int32>(BranchResult), static_cast<int32>(ChoiceResult.Branch),
			static_cast<int32>(ChoiceResult.QuestResult), static_cast<int32>(ChoiceResult.RewardResult),
			static_cast<int32>(ChoiceResult.EncounterResponse.ResultType), *ChoiceResult.OperationId.ToString());
		return Reject(MapBranchResult(BranchResult), AuthorityResult);
	}

	if (ChoiceResult.NextNodeId.IsNone())
	{
		// 分支结果没有指向下一个节点，表示对话结束：广播一个关闭态快照。
		FHSRDialoguePresentationSnapshot Closed = Snapshot;
		Closed.Status = EHSRDialoguePresentationStatus::Closed;
		Closed.bIsValid = false;
		Closed.bIsActive = false;
		Closed.SpeakerText = FText();
		Closed.BodyText = FText();
		Closed.Choices.Reset();
		Closed.AuthorityResult = AuthorityResult;
		Publish(MoveTemp(Closed), EHSRDialoguePresentationResult::Success, AuthorityResult);
		return EHSRDialoguePresentationResult::Success;
	}

	// 分支结果指向下一个节点：从权威层读取该节点并广播新快照。
	FHSRDialogueNodeDefinition NextNode;
	const EHSRQuestOperationResult NodeResult = Dialogue->GetNode(
		Request.DialogueId, ChoiceResult.NextNodeId, NextNode);
	if (NodeResult != EHSRQuestOperationResult::Success)
	{
		// 下一节点读取失败，按节点级错误拒绝。
		return Reject(MapNodeResult(NodeResult), NodeResult);
	}

	Publish(BuildSnapshot(Request.QueryId, Request.DialogueId, NextNode, AuthorityResult),
		EHSRDialoguePresentationResult::Success, AuthorityResult);
	return EHSRDialoguePresentationResult::Success;
}

// ExitDialogue：主动结束一段对话（如玩家按下退出键）。
// 校验请求后，把当前快照标记为关闭态并广播，让 UI 收起界面；
// 权威层没有“显式退出”动作，因此这里只改展示层状态。
EHSRDialoguePresentationResult UHSRDialoguePresentationViewModel::ExitDialogue(const FGuid& QueryId)
{
	if (!QueryId.IsValid())
	{
		// 无效 QueryId 直接拒绝。
		return Reject(EHSRDialoguePresentationResult::InvalidRequest, EHSRQuestOperationResult::NoOp);
	}
	if (!Snapshot.bIsActive)
	{
		// 已没有活动会话，无需再退出。
		return Reject(EHSRDialoguePresentationResult::Closed, EHSRQuestOperationResult::NoOp);
	}
	if (Snapshot.QueryId != QueryId)
	{
		// QueryId 不匹配当前会话，属于过期请求。
		return Reject(EHSRDialoguePresentationResult::StaleRequest, EHSRQuestOperationResult::NoOp);
	}

	// 复用当前快照，仅把状态改为关闭：清空文本与选项，保留 QueryId 便于追踪。
	FHSRDialoguePresentationSnapshot Closed = Snapshot;
	Closed.Status = EHSRDialoguePresentationStatus::Closed;
	Closed.bIsValid = false;
	Closed.bIsActive = false;
	Closed.SpeakerText = FText();
	Closed.BodyText = FText();
	Closed.Choices.Reset();
	Publish(MoveTemp(Closed), EHSRDialoguePresentationResult::Success, LastAuthorityResult);
	return EHSRDialoguePresentationResult::Success;
}
