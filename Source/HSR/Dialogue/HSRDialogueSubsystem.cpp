#include "HSRDialogueSubsystem.h"

#include "../Data/Definitions/HSRDialogueDefinition.h"
#include "../Quest/HSRQuestSubsystem.h"

void UHSRDialogueSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Quest = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRQuestSubsystem>() : nullptr;
}

#if WITH_DEV_AUTOMATION_TESTS
void UHSRDialogueSubsystem::InitializeForAutomation(UHSRQuestSubsystem* InQuest)
{
	Quest = InQuest;
}
#endif

#if WITH_EDITOR
void UHSRDialogueSubsystem::InitializeForDevelopmentTest(UHSRQuestSubsystem* InQuest)
{
	Quest = InQuest;
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

EHSRQuestOperationResult UHSRDialogueSubsystem::SelectChoice(FName DialogueId, FName NodeId, FName ChoiceId, FHSRDialogueChoiceResult& OutResult)
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
	if (!Choice->QuestEventId.IsNone())
	{
		if (!Quest.IsValid())
		{
			return EHSRQuestOperationResult::InvalidState;
		}
		TArray<FHSRQuestRuntimeState> Changed;
		const EHSRQuestOperationResult Result = Quest->SubmitEvent({Choice->QuestEventId, Choice->EventCount}, Changed);
		OutResult.bQuestEventSubmitted = Result == EHSRQuestOperationResult::Success || Result == EHSRQuestOperationResult::NoOp;
		return Result == EHSRQuestOperationResult::InvalidEvent ? EHSRQuestOperationResult::NoOp : Result;
	}
	return EHSRQuestOperationResult::Success;
}

bool UHSRDialogueSubsystem::GetStartNode(FName DialogueId, FHSRDialogueNodeDefinition& OutNode) const
{
	const FDialogueRule* Rule = Dialogues.Find(DialogueId);
	const FHSRDialogueNodeDefinition* Node = Rule ? FindNode(*Rule, Rule->StartNodeId) : nullptr;
	if (!Node)
	{
		return false;
	}
	OutNode = *Node;
	return true;
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
