#include "HSRInteractionViewModel.h"
#include "../Interaction/HSRInteractionComponent.h"
#include "../Interaction/HSRInteractableInterface.h"

void UHSRInteractionViewModel::Observe(UHSRInteractionComponent* InComponent)
{
	// Always teardown old observation first
	if (UHSRInteractionComponent* OldComp = ObservedComponent.Get())
	{
		OldComp->OnCandidateChanged.RemoveDynamic(this, &UHSRInteractionViewModel::OnComponentCandidateChanged);
	}
	ObservedComponent.Reset();

	if (!InComponent)
	{
		BroadcastCurrentState(nullptr);
		return;
	}

	ObservedComponent = InComponent;
	InComponent->OnCandidateChanged.AddUniqueDynamic(this, &UHSRInteractionViewModel::OnComponentCandidateChanged);

	// Broadcast initial snapshot based on current candidate
	BroadcastCurrentState(InComponent->GetCurrentCandidate());
	UE_LOG(LogTemp, Log, TEXT("UHSRInteractionViewModel::Observe - Component=%s"), *InComponent->GetName());
}

void UHSRInteractionViewModel::Teardown()
{
	if (UHSRInteractionComponent* OldComp = ObservedComponent.Get())
	{
		OldComp->OnCandidateChanged.RemoveDynamic(this, &UHSRInteractionViewModel::OnComponentCandidateChanged);
	}
	ObservedComponent.Reset();
	BroadcastCurrentState(nullptr);
	UE_LOG(LogTemp, Log, TEXT("UHSRInteractionViewModel::Teardown - Observation cleared"));
}

void UHSRInteractionViewModel::OnComponentCandidateChanged(AActor* NewCandidate)
{
	BroadcastCurrentState(NewCandidate);
}

void UHSRInteractionViewModel::BroadcastCurrentState(AActor* Candidate)
{
	if (IsValid(Candidate) && Candidate->Implements<UHSRInteractableInterface>())
	{
		FText Prompt = IHSRInteractableInterface::Execute_GetInteractionPrompt(Candidate);
		OnPromptChanged.Broadcast(true, Prompt);
		UE_LOG(LogTemp, Verbose, TEXT("UHSRInteractionViewModel::BroadcastCurrentState - visible=true prompt=%s"),
			*Prompt.ToString());
	}
	else
	{
		OnPromptChanged.Broadcast(false, FText::GetEmpty());
		UE_LOG(LogTemp, Verbose, TEXT("UHSRInteractionViewModel::BroadcastCurrentState - visible=false"));
	}
}
