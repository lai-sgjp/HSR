#include "HSRDialogueSubsystem.h"

#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "../Data/Definitions/HSRDialogueDefinition.h"
#include "../Quest/HSRQuestSubsystem.h"
#include "../Reward/HSRRewardSubsystem.h"

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
void UHSRDialogueSubsystem::InitializeForDevelopmentTest(UHSRQuestSubsystem* InQuest)
{
	Quest = InQuest;
	UGameInstance* GameInstance = GetGameInstance();
	Reward = GameInstance ? GameInstance->GetSubsystem<UHSRRewardSubsystem>() : nullptr;
	Encounter = GameInstance ? GameInstance->GetSubsystem<UHSRBattleTransitionSubsystem>() : nullptr;
	BranchLedger.Reset();
}
#endif

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
		if (Node.NodeId.IsNone() || NodeIds.Contains(Node.NodeId))
		{
			return EHSRQuestOperationResult::InvalidDefinition;
		}
		NodeIds.Add(Node.NodeId);
		bHasStart |= Node.NodeId == Definition.StartNodeId;
		TSet<FName> ChoiceIds;
		for (const FHSRDialogueChoiceDefinition& Choice : Node.Choices)
		{
			if (Choice.ChoiceId.IsNone() || ChoiceIds.Contains(Choice.ChoiceId) || Choice.EventCount <= 0)
			{
				return EHSRQuestOperationResult::InvalidDefinition;
			}
			if (Choice.Branch != EHSRDialogueChoiceBranch::None && !Choice.BranchOperationId.IsValid())
			{
				return EHSRQuestOperationResult::InvalidDefinition;
			}
			switch (Choice.Branch)
			{
			case EHSRDialogueChoiceBranch::Quest:
				if (Choice.QuestEventId.IsNone())
				{
					return EHSRQuestOperationResult::InvalidDefinition;
				}
				break;
			case EHSRDialogueChoiceBranch::Encounter:
				if (Choice.EncounterRequest.EncounterId.IsNone()
					|| Choice.EncounterRequest.EnemyDefinitionId.IsNone()
					|| Choice.EncounterRequest.BattleMapPath.IsNone()
					|| Choice.EncounterRequest.ExplorationMapPath.IsNone())
				{
					return EHSRQuestOperationResult::InvalidDefinition;
				}
				break;
			case EHSRDialogueChoiceBranch::Reward:
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
	if (!bHasStart)
	{
		return EHSRQuestOperationResult::InvalidDefinition;
	}
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
	if (!Choice->TargetNodeId.IsNone() && !FindNode(*Rule, Choice->TargetNodeId))
	{
		return EHSRQuestOperationResult::InvalidDefinition;
	}
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

	if (OutResult.Branch == EHSRDialogueChoiceBranch::None)
	{
		return EHSRDialogueChoiceOperationResult::Success;
	}
	if (!OutResult.OperationId.IsValid())
	{
		return EHSRDialogueChoiceOperationResult::InvalidDefinition;
	}

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

EHSRDialogueChoiceOperationResult UHSRDialogueSubsystem::DispatchChoiceBranch(
	FHSRDialogueChoiceResult& InOutResult)
{
	switch (InOutResult.Branch)
	{
	case EHSRDialogueChoiceBranch::Quest:
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
		if (!Reward.IsValid())
		{
			InOutResult.RewardResult = EHSRRewardOperationResult::InventoryRejected;
			return EHSRDialogueChoiceOperationResult::AuthorityUnavailable;
		}
		{
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
				static_cast<int32>(Result), *InOutResult.OperationId.ToString(),
				*InOutResult.RewardDefinitionId.ToString());
			return EHSRDialogueChoiceOperationResult::AuthorityRejected;
		}

	case EHSRDialogueChoiceBranch::Encounter:
		if (!Encounter.IsValid())
		{
			InOutResult.EncounterResponse = FHSREncounterResult::MakeFailure(
				EHSREncounterResultType::InvalidRequest);
			return EHSRDialogueChoiceOperationResult::AuthorityUnavailable;
		}
		{
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
				*InOutResult.OperationId.ToString(), *EncounterRequest.EncounterId.ToString());
			return EHSRDialogueChoiceOperationResult::AuthorityRejected;
		}

	case EHSRDialogueChoiceBranch::None:
	default:
		return EHSRDialogueChoiceOperationResult::Success;
	}
}

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

const FHSRDialogueNodeDefinition* UHSRDialogueSubsystem::FindNode(const FDialogueRule& Rule, FName NodeId) const
{
	return Rule.Nodes.FindByPredicate([NodeId](const FHSRDialogueNodeDefinition& Node)
	{
		return Node.NodeId == NodeId;
	});
}

const FHSRDialogueChoiceDefinition* UHSRDialogueSubsystem::FindChoice(const FHSRDialogueNodeDefinition& Node, FName ChoiceId) const
{
	return Node.Choices.FindByPredicate([ChoiceId](const FHSRDialogueChoiceDefinition& Choice)
	{
		return Choice.ChoiceId == ChoiceId;
	});
}
