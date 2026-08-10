#include "HSRDialogueInteractable.h"

#include "HSRDialogueSubsystem.h"
#include "../Quest/HSRQuestSubsystem.h"
#include "../Data/Definitions/HSRQuestDefinition.h"
#include "../Data/Definitions/HSRDialogueDefinition.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AHSRDialogueInteractable::AHSRDialogueInteractable()
{
	PrimaryActorTick.bCanEverTick = false;

	// Give the graybox a discoverable marker and a forgiving interaction radius.
	// The base CollisionComponent starts at radius 32 and is hidden in game, which
	// makes the Dialogue interactable impossible to find or reach during PIE.
	CollisionComponent->SetSphereRadius(260.f);
	CollisionComponent->SetHiddenInGame(false);

	VisualMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMarker"));
	VisualMarker->SetupAttachment(RootComponent);
	VisualMarker->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
	VisualMarker->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.6f));
	VisualMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MarkerMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (MarkerMesh.Succeeded())
	{
		VisualMarker->SetStaticMesh(MarkerMesh.Object);
	}
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
	return FHSRInteractionResult::MakeDialogueSuccess(DialogueId, StartNode.NodeId);
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
