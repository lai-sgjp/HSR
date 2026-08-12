#include "HSRDialogueSubsystem.h"

#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "../Data/Definitions/HSRDialogueDefinition.h"
#include "../Quest/HSRQuestSubsystem.h"
#include "../Reward/HSRRewardSubsystem.h"

// 对话子系统初始化：缓存三个依赖子系统，并清空分支账本。
// 分支账本用于“同一对话选项只执行一次分支副作用”（防重复领奖/重复触发遭遇）。
void UHSRDialogueSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UGameInstance* GameInstance = GetGameInstance();
	Quest = GameInstance ? GameInstance->GetSubsystem<UHSRQuestSubsystem>() : nullptr;
	Reward = GameInstance ? GameInstance->GetSubsystem<UHSRRewardSubsystem>() : nullptr;
	Encounter = GameInstance ? GameInstance->GetSubsystem<UHSRBattleTransitionSubsystem>() : nullptr;
	BranchLedger.Reset();
}

#if WITH_DEV_AUTOMATION_TESTS
// 自动化测试专用：注入全部三个依赖子系统。
void UHSRDialogueSubsystem::InitializeForAutomation(UHSRQuestSubsystem* InQuest,
	UHSRRewardSubsystem* InReward, UHSRBattleTransitionSubsystem* InEncounter)
{
	Quest = InQuest;
	Reward = InReward;
	Encounter = InEncounter;
	BranchLedger.Reset();
}
#endif

#if WITH_EDITOR
// 编辑器专用：只注入任务子系统，奖励与遭遇仍从 GameInstance 解析。
void UHSRDialogueSubsystem::InitializeForDevelopmentTest(UHSRQuestSubsystem* InQuest)
{
	Quest = InQuest;
	UGameInstance* GameInstance = GetGameInstance();
	Reward = GameInstance ? GameInstance->GetSubsystem<UHSRRewardSubsystem>() : nullptr;
	Encounter = GameInstance ? GameInstance->GetSubsystem<UHSRBattleTransitionSubsystem>() : nullptr;
	BranchLedger.Reset();
}
#endif

// 登记一份对话定义。先校验，通过后只拷贝运行时需要的字段。
EHSRQuestOperationResult UHSRDialogueSubsystem::RegisterDialogueDefinition(const UHSRDialogueDefinition& Definition)
{
	const EHSRQuestOperationResult Validation = CanRegisterDialogueDefinition(Definition);
	if (Validation != EHSRQuestOperationResult::Success)
	{
		return Validation;
	}
	FDialogueRule Rule;
	Rule.DialogueId = Definition.DialogueId;
	Rule.QuestId = Definition.QuestId;
	Rule.StartNodeId = Definition.StartNodeId;
	Rule.Nodes = Definition.Nodes;
	Dialogues.Add(Rule.DialogueId, MoveTemp(Rule));
	return EHSRQuestOperationResult::Success;
}

// 校验对话定义：节点 ID 唯一、必须存在起点节点、选择项 ID 唯一且计数合法、
// 带分支的选择必须配操作 ID、分支参数自洽、跳转目标必须指向存在的节点、不可重复登记。
EHSRQuestOperationResult UHSRDialogueSubsystem::CanRegisterDialogueDefinition(const UHSRDialogueDefinition& Definition) const
{
	if (Definition.DialogueId.IsNone())
	{
		return EHSRQuestOperationResult::InvalidDefinitionId;
	}
	if (Definition.StartNodeId.IsNone() || Definition.Nodes.IsEmpty())
	{
		return EHSRQuestOperationResult::InvalidDefinition;
	}
	TSet<FName> NodeIds;
	bool bHasStart = false;
	for (const FHSRDialogueNodeDefinition& Node : Definition.Nodes)
	{
		// 节点 ID 必须非空且不重复。
		if (Node.NodeId.IsNone() || NodeIds.Contains(Node.NodeId))
		{
			return EHSRQuestOperationResult::InvalidDefinition;
		}
		NodeIds.Add(Node.NodeId);
		bHasStart |= Node.NodeId == Definition.StartNodeId;
		// 该节点内，选择项 ID 也必须唯一。
		TSet<FName> ChoiceIds;
		for (const FHSRDialogueChoiceDefinition& Choice : Node.Choices)
		{
			if (Choice.ChoiceId.IsNone() || ChoiceIds.Contains(Choice.ChoiceId) || Choice.EventCount <= 0)
			{
				return EHSRQuestOperationResult::InvalidDefinition;
			}
			// 有分支行为就必须有操作 ID（后续用它做幂等账本）。
			if (Choice.Branch != EHSRDialogueChoiceBranch::None && !Choice.BranchOperationId.IsValid())
			{
				return EHSRQuestOperationResult::InvalidDefinition;
			}
			switch (Choice.Branch)
			{
			case EHSRDialogueChoiceBranch::Quest:
				// 任务分支必须指明要提交的事件。
				if (Choice.QuestEventId.IsNone())
				{
					return EHSRQuestOperationResult::InvalidDefinition;
				}
				break;
			case EHSRDialogueChoiceBranch::Encounter:
				// 遭遇分支必须带完整的遭遇请求参数。
				if (Choice.EncounterRequest.EncounterId.IsNone()
					|| Choice.EncounterRequest.EnemyDefinitionId.IsNone()
					|| Choice.EncounterRequest.BattleMapPath.IsNone()
					|| Choice.EncounterRequest.ExplorationMapPath.IsNone())
				{
					return EHSRQuestOperationResult::InvalidDefinition;
				}
				break;
			case EHSRDialogueChoiceBranch::Reward:
				// 奖励分支必须指向一个有效奖励定义。
				if (Choice.RewardDefinitionId.IsNone() || Choice.RewardSeed < 0)
				{
					return EHSRQuestOperationResult::InvalidDefinition;
				}
				break;
			case EHSRDialogueChoiceBranch::None:
			default:
				break;
			}
			ChoiceIds.Add(Choice.ChoiceId);
		}
	}
	// 必须存在与 StartNodeId 对应的节点，否则对话无法开场。
	if (!bHasStart)
	{
		return EHSRQuestOperationResult::InvalidDefinition;
	}
	// 所有跳转目标都必须指向已存在的节点。
	for (const FHSRDialogueNodeDefinition& Node : Definition.Nodes)
	{
		for (const FHSRDialogueChoiceDefinition& Choice : Node.Choices)
		{
			if (!Choice.TargetNodeId.IsNone() && !NodeIds.Contains(Choice.TargetNodeId))
			{
				return EHSRQuestOperationResult::InvalidDefinition;
			}
		}
	}
	if (Dialogues.Contains(Definition.DialogueId))
	{
		return EHSRQuestOperationResult::DuplicateDefinitionId;
	}
	return EHSRQuestOperationResult::Success;
}

// 预览一个选择项：只回填“选中后会怎样”的结果，不执行任何分支副作用。
// 供 UI 显示选项效果、或做选择前的确认。
EHSRQuestOperationResult UHSRDialogueSubsystem::PreviewChoice(FName DialogueId, FName NodeId, FName ChoiceId, FHSRDialogueChoiceResult& OutResult) const
{
	OutResult = FHSRDialogueChoiceResult();
	const FDialogueRule* Rule = Dialogues.Find(DialogueId);
	if (!Rule)
	{
		return EHSRQuestOperationResult::UnknownDialogueDefinition;
	}
	const FHSRDialogueNodeDefinition* Node = FindNode(*Rule, NodeId);
	const FHSRDialogueChoiceDefinition* Choice = Node ? FindChoice(*Node, ChoiceId) : nullptr;
	if (!Choice)
	{
		return EHSRQuestOperationResult::InvalidEvent;
	}
	// 跳转目标必须存在，防止预览出指向虚空节点的结果。
	if (!Choice->TargetNodeId.IsNone() && !FindNode(*Rule, Choice->TargetNodeId))
	{
		return EHSRQuestOperationResult::InvalidDefinition;
	}
	// 把选择项的全部可见结果复制到 OutResult（不含执行副作用）。
	OutResult.bChoiceAccepted = true;
	OutResult.DialogueId = DialogueId;
	OutResult.ChoiceId = ChoiceId;
	OutResult.NextNodeId = Choice->TargetNodeId;
	OutResult.QuestEventId = Choice->QuestEventId;
	OutResult.EventCount = Choice->EventCount;
	OutResult.Branch = Choice->Branch;
	OutResult.OperationId = Choice->BranchOperationId;
	OutResult.EncounterRequest = Choice->EncounterRequest;
	OutResult.RewardDefinitionId = Choice->RewardDefinitionId;
	OutResult.RewardSeed = Choice->RewardSeed;
	return EHSRQuestOperationResult::Success;
}

// 正式选中一个选择项。无分支时直接成功；有分支时先走账本幂等，再分发分支副作用。
EHSRDialogueChoiceOperationResult UHSRDialogueSubsystem::SelectChoice(
	const FHSRDialogueChoiceRequest& Request, FHSRDialogueChoiceResult& OutResult)
{
	OutResult = FHSRDialogueChoiceResult();
	if (!Request.IsValid())
	{
		return EHSRDialogueChoiceOperationResult::InvalidRequest;
	}

	const EHSRQuestOperationResult PreviewResult = PreviewChoice(
		Request.DialogueId, Request.NodeId, Request.ChoiceId, OutResult);
	if (PreviewResult != EHSRQuestOperationResult::Success)
	{
		// 把旧式结果码映射成新式分支结果码。
		switch (PreviewResult)
		{
		case EHSRQuestOperationResult::UnknownDialogueDefinition:
			return EHSRDialogueChoiceOperationResult::UnknownDialogueDefinition;
		case EHSRQuestOperationResult::InvalidDefinition:
			return EHSRDialogueChoiceOperationResult::InvalidDefinition;
		case EHSRQuestOperationResult::InvalidEvent:
		default:
			return EHSRDialogueChoiceOperationResult::InvalidChoice;
		}
	}

	// 纯对话（无分支）不需要操作 ID，直接成功。
	if (OutResult.Branch == EHSRDialogueChoiceBranch::None)
	{
		return EHSRDialogueChoiceOperationResult::Success;
	}
	if (!OutResult.OperationId.IsValid())
	{
		return EHSRDialogueChoiceOperationResult::InvalidDefinition;
	}

	// 账本命中：说明这个选项的分支副作用之前已经执行过。
	// 若上下文（对话/节点/选项）一致则回放旧结果，不一致说明操作 ID 被复用，拒绝。
	if (const FDialogueBranchLedgerEntry* Existing = BranchLedger.Find(OutResult.OperationId))
	{
		if (Existing->DialogueId != Request.DialogueId || Existing->NodeId != Request.NodeId
			|| Existing->ChoiceId != Request.ChoiceId)
		{
			return EHSRDialogueChoiceOperationResult::OperationIdConflict;
		}
		OutResult = Existing->Result;
		return EHSRDialogueChoiceOperationResult::NoOp;
	}

	const EHSRDialogueChoiceOperationResult BranchResult = DispatchChoiceBranch(OutResult);
	// 分支执行成功或幂等命中后，把结果记入账本，保证下次同选项不再重复执行副作用。
	if (BranchResult == EHSRDialogueChoiceOperationResult::Success
		|| BranchResult == EHSRDialogueChoiceOperationResult::NoOp)
	{
		FDialogueBranchLedgerEntry& Entry = BranchLedger.FindOrAdd(OutResult.OperationId);
		Entry.DialogueId = Request.DialogueId;
		Entry.NodeId = Request.NodeId;
		Entry.ChoiceId = Request.ChoiceId;
		Entry.Result = OutResult;
	}
	return BranchResult;
}

// 旧式重载：先预览，再按旧式结果码路径处理（供老的调用方使用）。
EHSRQuestOperationResult UHSRDialogueSubsystem::SelectChoice(FName DialogueId, FName NodeId, FName ChoiceId, FHSRDialogueChoiceResult& OutResult)
{
	const EHSRQuestOperationResult PreviewResult = PreviewChoice(DialogueId, NodeId, ChoiceId, OutResult);
	if (PreviewResult != EHSRQuestOperationResult::Success)
	{
		return PreviewResult;
	}
	if (OutResult.Branch != EHSRDialogueChoiceBranch::None)
	{
		FHSRDialogueChoiceRequest Request;
		Request.DialogueId = DialogueId;
		Request.NodeId = NodeId;
		Request.ChoiceId = ChoiceId;
		const EHSRDialogueChoiceOperationResult BranchResult = SelectChoice(Request, OutResult);
		if (BranchResult == EHSRDialogueChoiceOperationResult::Success
			|| BranchResult == EHSRDialogueChoiceOperationResult::NoOp)
		{
			return BranchResult == EHSRDialogueChoiceOperationResult::NoOp
				? EHSRQuestOperationResult::NoOp
				: EHSRQuestOperationResult::Success;
		}
		return MapLegacyBranchResult(OutResult);
	}
	// 无分支：仅当选择了带任务事件的选项时，直接向任务系统提交事件。
	if (!OutResult.QuestEventId.IsNone())
	{
		if (!Quest.IsValid())
		{
			return EHSRQuestOperationResult::InvalidState;
		}
		TArray<FHSRQuestRuntimeState> Changed;
		const EHSRQuestOperationResult Result = Quest->SubmitEvent({OutResult.QuestEventId, OutResult.EventCount}, Changed);
		OutResult.bQuestEventSubmitted = Result == EHSRQuestOperationResult::Success || Result == EHSRQuestOperationResult::NoOp;
		return Result == EHSRQuestOperationResult::InvalidEvent ? EHSRQuestOperationResult::NoOp : Result;
	}
	return EHSRQuestOperationResult::Success;
}

// 按分支类型分发真正的副作用执行。
EHSRDialogueChoiceOperationResult UHSRDialogueSubsystem::DispatchChoiceBranch(
	FHSRDialogueChoiceResult& InOutResult)
{
	switch (InOutResult.Branch)
	{
	case EHSRDialogueChoiceBranch::Quest:
		// 任务子系统不可用：无法提交事件，只能拒绝。
		if (!Quest.IsValid())
		{
			InOutResult.QuestResult = EHSRQuestOperationResult::InvalidState;
			return EHSRDialogueChoiceOperationResult::AuthorityUnavailable;
		}
		{
			TArray<FHSRQuestRuntimeState> Changed;
			const EHSRQuestOperationResult Result = Quest->SubmitEvent(
				{InOutResult.QuestEventId, InOutResult.EventCount}, Changed);
			InOutResult.QuestResult = Result;
			InOutResult.bQuestEventSubmitted = Result == EHSRQuestOperationResult::Success
				|| Result == EHSRQuestOperationResult::NoOp;
			// 事件已入账（无论新增还是幂等）都视为分支提交成功。
			if (Result == EHSRQuestOperationResult::Success)
			{
				InOutResult.bBranchSubmitted = true;
				return EHSRDialogueChoiceOperationResult::Success;
			}
			if (Result == EHSRQuestOperationResult::NoOp)
			{
				InOutResult.bBranchSubmitted = true;
				return EHSRDialogueChoiceOperationResult::NoOp;
			}
			return EHSRDialogueChoiceOperationResult::AuthorityRejected;
		}

	case EHSRDialogueChoiceBranch::Reward:
		// 奖励子系统不可用：拒绝发奖。
		if (!Reward.IsValid())
		{
			InOutResult.RewardResult = EHSRRewardOperationResult::InventoryRejected;
			return EHSRDialogueChoiceOperationResult::AuthorityUnavailable;
		}
		{
			// 用操作 ID 作为领取 ID，保证同一选项重复选择时奖励幂等。
			FHSRRewardRequest RewardRequest;
			RewardRequest.ClaimId = InOutResult.OperationId;
			RewardRequest.RewardDefinitionId = InOutResult.RewardDefinitionId;
			RewardRequest.Seed = InOutResult.RewardSeed;
			const EHSRRewardOperationResult Result = Reward->SubmitReward(RewardRequest,
				InOutResult.RewardReceipt);
			InOutResult.RewardResult = Result;
			if (Result == EHSRRewardOperationResult::Success)
			{
				InOutResult.bBranchSubmitted = true;
				return EHSRDialogueChoiceOperationResult::Success;
			}
			if (Result == EHSRRewardOperationResult::NoOp)
			{
				InOutResult.bBranchSubmitted = true;
				return EHSRDialogueChoiceOperationResult::NoOp;
			}
			UE_LOG(LogTemp, Warning, TEXT("Dialogue branch Reward rejected Result=%d OperationId=%s RewardId=%s"),
				static_cast<int32>(Result),
				*InOutResult.OperationId.ToString(),
				*InOutResult.RewardDefinitionId.ToString());
			return EHSRDialogueChoiceOperationResult::AuthorityRejected;
		}

	case EHSRDialogueChoiceBranch::Encounter:
		// 遭遇子系统不可用：构造失败响应。
		if (!Encounter.IsValid())
		{
			InOutResult.EncounterResponse = FHSREncounterResult::MakeFailure(
				EHSREncounterResultType::InvalidRequest);
			return EHSRDialogueChoiceOperationResult::AuthorityUnavailable;
		}
		{
			// 用操作 ID 作为遭遇请求 ID，方便追溯是哪次选择触发的遭遇。
			FHSREncounterRequest EncounterRequest = InOutResult.EncounterRequest;
			EncounterRequest.RequestId = InOutResult.OperationId;
			InOutResult.EncounterResponse = Encounter->SubmitEncounterRequestFromUI(EncounterRequest);
			if (InOutResult.EncounterResponse.ResultType == EHSREncounterResultType::Success)
			{
				InOutResult.bBranchSubmitted = true;
				return EHSRDialogueChoiceOperationResult::Success;
			}
			UE_LOG(LogTemp, Warning, TEXT("Dialogue branch Encounter rejected Result=%d OperationId=%s EncounterId=%s"),
				static_cast<int32>(InOutResult.EncounterResponse.ResultType),
				*InOutResult.OperationId.ToString(),
				*EncounterRequest.EncounterId.ToString());
			return EHSRDialogueChoiceOperationResult::AuthorityRejected;
		}

	case EHSRDialogueChoiceBranch::None:
	default:
		return EHSRDialogueChoiceOperationResult::Success;
	}
}

// 把新式分支结果映射回旧式任务结果码（兼容旧调用方）。
EHSRQuestOperationResult UHSRDialogueSubsystem::MapLegacyBranchResult(
	const FHSRDialogueChoiceResult& Result) const
{
	if (Result.bBranchSubmitted)
	{
		return EHSRQuestOperationResult::Success;
	}
	if (Result.Branch == EHSRDialogueChoiceBranch::Quest)
	{
		return Result.QuestResult;
	}
	if (Result.Branch == EHSRDialogueChoiceBranch::Reward)
	{
		return Result.RewardResult == EHSRRewardOperationResult::NoOp
			? EHSRQuestOperationResult::NoOp
			: EHSRQuestOperationResult::RewardRejected;
	}
	return EHSRQuestOperationResult::InvalidState;
}

// 获取指定节点定义；对话不存在或节点不存在都返回对应错误。
EHSRQuestOperationResult UHSRDialogueSubsystem::GetNode(FName DialogueId, FName NodeId, FHSRDialogueNodeDefinition& OutNode) const
{
	OutNode = FHSRDialogueNodeDefinition();
	const FDialogueRule* Rule = Dialogues.Find(DialogueId);
	if (!Rule)
	{
		return EHSRQuestOperationResult::UnknownDialogueDefinition;
	}
	const FHSRDialogueNodeDefinition* Node = FindNode(*Rule, NodeId);
	if (!Node)
	{
		return EHSRQuestOperationResult::InvalidEvent;
	}
	OutNode = *Node;
	return EHSRQuestOperationResult::Success;
}

// 取得对话的起点节点；对话不存在时返回 false 并清空输出。
bool UHSRDialogueSubsystem::GetStartNode(FName DialogueId, FHSRDialogueNodeDefinition& OutNode) const
{
	const FDialogueRule* Rule = Dialogues.Find(DialogueId);
	if (!Rule)
	{
		OutNode = FHSRDialogueNodeDefinition();
		return false;
	}
	return GetNode(DialogueId, Rule->StartNodeId, OutNode) == EHSRQuestOperationResult::Success;
}

// 线性查找节点：对话节点数量不多，直接按 ID 谓词查找即可。
const FHSRDialogueNodeDefinition* UHSRDialogueSubsystem::FindNode(const FDialogueRule& Rule, FName NodeId) const
{
	return Rule.Nodes.FindByPredicate([NodeId](const FHSRDialogueNodeDefinition& Node)
	{
		return Node.NodeId == NodeId;
	});
}

// 在节点内按选择项 ID 查找选择定义（数量少，线性查找足够）。
const FHSRDialogueChoiceDefinition* UHSRDialogueSubsystem::FindChoice(const FHSRDialogueNodeDefinition& Node, FName ChoiceId) const
{
	return Node.Choices.FindByPredicate([ChoiceId](const FHSRDialogueChoiceDefinition& Choice)
	{
		return Choice.ChoiceId == ChoiceId;
	});
}
