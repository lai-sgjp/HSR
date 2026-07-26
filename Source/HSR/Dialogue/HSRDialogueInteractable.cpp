#include "HSRDialogueInteractable.h"

#include "HSRDialogueSubsystem.h"
#include "../Quest/HSRQuestSubsystem.h"
#include "../Data/Definitions/HSRQuestDefinition.h"
#include "../Data/Definitions/HSRDialogueDefinition.h"

AHSRDialogueInteractable::AHSRDialogueInteractable()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AHSRDialogueInteractable::BeginPlay()
{
	Super::BeginPlay();
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}
	if (QuestDefinition)
	{
		if (UHSRQuestSubsystem* Quest = GI->GetSubsystem<UHSRQuestSubsystem>())
		{
			Quest->RegisterQuestDefinition(*QuestDefinition);
		}
	}
	if (DialogueDefinition)
	{
		if (UHSRDialogueSubsystem* Dialogue = GI->GetSubsystem<UHSRDialogueSubsystem>())
		{
			Dialogue->RegisterDialogueDefinition(*DialogueDefinition);
		}
	}
}

FText AHSRDialogueInteractable::GetInteractionPrompt_Implementation() const
{
	return NSLOCTEXT("HSRDialogueInteractable", "Prompt", "Talk");
}

FHSRInteractionResult AHSRDialogueInteractable::ExecuteInteraction_Implementation(const FHSRInteractionContext& Context)
{
	if (!Context.InteractorActor.IsValid() || DialogueId.IsNone())
	{
		return FHSRInteractionResult::MakeFailure(EHSRInteractionFailureReason::ExecutionFailed, FText::FromString(TEXT("Dialogue is not configured.")));
	}

	FHSRDialogueNodeDefinition StartNode;
	if (!GetStartDialogueNode(StartNode))
	{
		return FHSRInteractionResult::MakeFailure(EHSRInteractionFailureReason::ExecutionFailed, FText::FromString(TEXT("Dialogue definition is unavailable.")));
	}

	UE_LOG(LogTemp, Log, TEXT("AHSRDialogueInteractable::ExecuteInteraction - DialogueId=%s StartNode=%s"), *DialogueId.ToString(), *StartNode.NodeId.ToString());
	return FHSRInteractionResult::MakeSuccess();
}

bool AHSRDialogueInteractable::GetStartDialogueNode(FHSRDialogueNodeDefinition& OutNode) const
{
	const UGameInstance* GI = GetGameInstance();
	const UHSRDialogueSubsystem* Dialogue = GI ? GI->GetSubsystem<UHSRDialogueSubsystem>() : nullptr;
	return Dialogue && !DialogueId.IsNone() && Dialogue->GetStartNode(DialogueId, OutNode);
}

EHSRQuestOperationResult AHSRDialogueInteractable::SelectDialogueChoice(FName NodeId, FName ChoiceId, FHSRDialogueChoiceResult& OutResult)
{
	UGameInstance* GI = GetGameInstance();
	UHSRDialogueSubsystem* Dialogue = GI ? GI->GetSubsystem<UHSRDialogueSubsystem>() : nullptr;
	if (!Dialogue || DialogueId.IsNone())
	{
		OutResult = FHSRDialogueChoiceResult();
		return EHSRQuestOperationResult::UnknownDialogueDefinition;
	}
	return Dialogue->SelectChoice(DialogueId, NodeId, ChoiceId, OutResult);
}
