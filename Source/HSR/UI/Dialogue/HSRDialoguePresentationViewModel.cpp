#include "HSRDialoguePresentationViewModel.h"

#include "../../Dialogue/HSRDialogueSubsystem.h"

void UHSRDialoguePresentationViewModel::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

void UHSRDialoguePresentationViewModel::Initialize(UHSRDialogueSubsystem* InDialogue)
{
	Shutdown();
	Dialogue = InDialogue;
	Snapshot = FHSRDialoguePresentationSnapshot();
	LastResult = InDialogue ? EHSRDialoguePresentationResult::NoOp : EHSRDialoguePresentationResult::Unavailable;
}

void UHSRDialoguePresentationViewModel::Shutdown()
{
	Dialogue.Reset();
	Snapshot = FHSRDialoguePresentationSnapshot();
	LastResult = EHSRDialoguePresentationResult::Unavailable;
	LastAuthorityResult = EHSRQuestOperationResult::NoOp;
	LastChoiceResult = FHSRDialogueChoiceResult();
}

bool UHSRDialoguePresentationViewModel::GetSnapshot(FHSRDialoguePresentationSnapshot& OutSnapshot) const
{
	OutSnapshot = Snapshot;
	return Snapshot.bIsValid;
}

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

bool UHSRDialoguePresentationViewModel::IsActiveRequest(const FHSRDialoguePresentationRequest& Request) const
{
	return Snapshot.bIsActive && Snapshot.QueryId == Request.QueryId
		&& Snapshot.DialogueId == Request.DialogueId && Snapshot.NodeId == Request.NodeId;
}

bool UHSRDialoguePresentationViewModel::IsActiveRequest(const FHSRDialoguePresentationChoiceRequest& Request) const
{
	return Snapshot.bIsActive && Snapshot.QueryId == Request.QueryId
		&& Snapshot.DialogueId == Request.DialogueId && Snapshot.NodeId == Request.NodeId;
}

EHSRDialoguePresentationResult UHSRDialoguePresentationViewModel::Reject(
	EHSRDialoguePresentationResult Result, EHSRQuestOperationResult AuthorityResult)
{
	LastResult = Result;
	LastAuthorityResult = AuthorityResult;
	return Result;
}

void UHSRDialoguePresentationViewModel::Publish(FHSRDialoguePresentationSnapshot&& Next,
	EHSRDialoguePresentationResult Result, EHSRQuestOperationResult AuthorityResult)
{
	Snapshot = MoveTemp(Next);
	LastResult = Result;
	LastAuthorityResult = AuthorityResult;
	Changed.Broadcast(Snapshot);
	OnSnapshotChanged.Broadcast(Snapshot);
}

EHSRDialoguePresentationResult UHSRDialoguePresentationViewModel::BeginDialogue(
	const FHSRDialoguePresentationRequest& Request)
{
	if (!Request.IsValid())
	{
		return Reject(EHSRDialoguePresentationResult::InvalidRequest, EHSRQuestOperationResult::NoOp);
	}
	if (Snapshot.bIsActive)
	{
		return Reject(IsActiveRequest(Request) ? EHSRDialoguePresentationResult::NoOp : EHSRDialoguePresentationResult::AlreadyActive,
			EHSRQuestOperationResult::NoOp);
	}
	if (!Dialogue.IsValid())
	{
		return Reject(EHSRDialoguePresentationResult::Unavailable, EHSRQuestOperationResult::InvalidState);
	}

	FHSRDialogueNodeDefinition Node;
	const EHSRQuestOperationResult AuthorityResult = Dialogue->GetNode(Request.DialogueId, Request.NodeId, Node);
	if (AuthorityResult != EHSRQuestOperationResult::Success)
	{
		return Reject(MapNodeResult(AuthorityResult), AuthorityResult);
	}

	Publish(BuildSnapshot(Request.QueryId, Request.DialogueId, Node, AuthorityResult),
		EHSRDialoguePresentationResult::Success, AuthorityResult);
	LastChoiceResult = FHSRDialogueChoiceResult();
	return EHSRDialoguePresentationResult::Success;
}

EHSRDialoguePresentationResult UHSRDialoguePresentationViewModel::SubmitChoice(
	const FHSRDialoguePresentationChoiceRequest& Request)
{
	if (!Request.IsValid())
	{
		return Reject(EHSRDialoguePresentationResult::InvalidRequest, EHSRQuestOperationResult::NoOp);
	}
	if (!Snapshot.bIsActive)
	{
		return Reject(EHSRDialoguePresentationResult::Closed, EHSRQuestOperationResult::NoOp);
	}
	if (!IsActiveRequest(Request))
	{
		return Reject(EHSRDialoguePresentationResult::StaleRequest, EHSRQuestOperationResult::NoOp);
	}
	if (!Dialogue.IsValid())
	{
		return Reject(EHSRDialoguePresentationResult::Unavailable, EHSRQuestOperationResult::InvalidState);
	}
	const FHSRDialoguePresentationChoice* PresentationChoice = Snapshot.Choices.FindByPredicate(
		[&Request](const FHSRDialoguePresentationChoice& Choice)
		{
			return Choice.ChoiceId == Request.ChoiceId;
		});
	if (!PresentationChoice || !PresentationChoice->bEnabled)
	{
		return Reject(EHSRDialoguePresentationResult::InvalidChoice, EHSRQuestOperationResult::InvalidEvent);
	}

	FHSRDialogueChoiceResult ChoiceResult;
	FHSRDialogueChoiceRequest AuthorityRequest;
	AuthorityRequest.DialogueId = Request.DialogueId;
	AuthorityRequest.NodeId = Request.NodeId;
	AuthorityRequest.ChoiceId = Request.ChoiceId;
	const EHSRDialogueChoiceOperationResult BranchResult = Dialogue->SelectChoice(
		AuthorityRequest, ChoiceResult);
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

	FHSRDialogueNodeDefinition NextNode;
	const EHSRQuestOperationResult NodeResult = Dialogue->GetNode(
		Request.DialogueId, ChoiceResult.NextNodeId, NextNode);
	if (NodeResult != EHSRQuestOperationResult::Success)
	{
		return Reject(MapNodeResult(NodeResult), NodeResult);
	}

	Publish(BuildSnapshot(Request.QueryId, Request.DialogueId, NextNode, AuthorityResult),
		EHSRDialoguePresentationResult::Success, AuthorityResult);
	return EHSRDialoguePresentationResult::Success;
}

EHSRDialoguePresentationResult UHSRDialoguePresentationViewModel::ExitDialogue(const FGuid& QueryId)
{
	if (!QueryId.IsValid())
	{
		return Reject(EHSRDialoguePresentationResult::InvalidRequest, EHSRQuestOperationResult::NoOp);
	}
	if (!Snapshot.bIsActive)
	{
		return Reject(EHSRDialoguePresentationResult::Closed, EHSRQuestOperationResult::NoOp);
	}
	if (Snapshot.QueryId != QueryId)
	{
		return Reject(EHSRDialoguePresentationResult::StaleRequest, EHSRQuestOperationResult::NoOp);
	}

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
