#include "HSRInteractionViewModel.h"
#include "../Interaction/HSRInteractionComponent.h"
#include "../Interaction/HSRInteractableInterface.h"

static int32 NextViewModelInstanceId = 1;

UHSRInteractionViewModel::UHSRInteractionViewModel()
{
	InstanceId = NextViewModelInstanceId++;
	bLastVisible = false;
	LastPrompt = FText::GetEmpty();
	BroadcastCount = 0;
	SkippedDedupCount = 0;
	TeardownCount = 0;
}

void UHSRInteractionViewModel::Observe(UHSRInteractionComponent* InComponent)
{
	// Same component: pure no-op — candidate changes handled by OnCandidateChanged event
	if (ObservedComponent.Get() == InComponent && InComponent)
	{
		UE_LOG(LogTemp, Verbose, TEXT("UHSRInteractionViewModel[%d]::Observe - Same component, pure no-op"), InstanceId);
		return;
	}

	// Different or null component: full teardown + bind
	Teardown();

	if (InComponent)
	{
		ObservedComponent = InComponent;
		InComponent->OnCandidateChanged.AddUniqueDynamic(this, &UHSRInteractionViewModel::OnComponentCandidateChanged);
		BroadcastCurrentState(InComponent->GetCurrentCandidate());
		UE_LOG(LogTemp, Log, TEXT("UHSRInteractionViewModel[%d]::Observe - Bound to Component=%s"),
			InstanceId, *InComponent->GetName());
	}
}

void UHSRInteractionViewModel::Teardown()
{
	UHSRInteractionComponent* OldComp = ObservedComponent.Get();
	if (!OldComp)
	{
		TeardownCount++;
		UE_LOG(LogTemp, Verbose, TEXT("UHSRInteractionViewModel[%d]::Teardown - Already torn down (teardown#%d)"),
			InstanceId, TeardownCount);
		return;
	}

	OldComp->OnCandidateChanged.RemoveDynamic(this, &UHSRInteractionViewModel::OnComponentCandidateChanged);
	ObservedComponent.Reset();
	bLastVisible = false;
	LastPrompt = FText::GetEmpty();
	TeardownCount++;
	UE_LOG(LogTemp, Log, TEXT("UHSRInteractionViewModel[%d]::Teardown - Unbound from Component=%s, totalBroadcast=%d, skippedDedup=%d, teardown#%d"),
		InstanceId, *OldComp->GetName(), BroadcastCount, SkippedDedupCount, TeardownCount);
}

void UHSRInteractionViewModel::ForceCurrentSnapshot()
{
	AActor* Candidate = ObservedComponent.IsValid() ? ObservedComponent.Get()->GetCurrentCandidate() : nullptr;
	FText Prompt;
	bool bVisible = false;
	if (IsValid(Candidate) && Candidate->Implements<UHSRInteractableInterface>())
	{
		Prompt = IHSRInteractableInterface::Execute_GetInteractionPrompt(Candidate);
		bVisible = true;
	}

	bLastVisible = bVisible;
	LastPrompt = Prompt;
	OnPromptChanged.Broadcast(bVisible, Prompt);
	BroadcastCount++;
	UE_LOG(LogTemp, Log, TEXT("UHSRInteractionViewModel[%d]::ForceCurrentSnapshot - visible=%d prompt=%s (totalBroadcast=%d)"),
		InstanceId, bVisible, *Prompt.ToString(), BroadcastCount);
}

void UHSRInteractionViewModel::OnComponentCandidateChanged(AActor* NewCandidate)
{
	BroadcastCurrentState(NewCandidate);
}

void UHSRInteractionViewModel::BroadcastCurrentState(AActor* Candidate)
{
	FText Prompt;
	bool bVisible = false;

	if (IsValid(Candidate) && Candidate->Implements<UHSRInteractableInterface>())
	{
		Prompt = IHSRInteractableInterface::Execute_GetInteractionPrompt(Candidate);
		bVisible = true;
	}

	if (bVisible == bLastVisible && Prompt.EqualTo(LastPrompt))
	{
		SkippedDedupCount++;
		UE_LOG(LogTemp, Verbose, TEXT("UHSRInteractionViewModel[%d]::BroadcastCurrentState - skipped (skip#%d)"),
			InstanceId, SkippedDedupCount);
		return;
	}

	bLastVisible = bVisible;
	LastPrompt = Prompt;
	OnPromptChanged.Broadcast(bVisible, Prompt);
	BroadcastCount++;
	UE_LOG(LogTemp, Log, TEXT("UHSRInteractionViewModel[%d]::BroadcastCurrentState - visible=%d prompt=%s (totalBroadcast=%d)"),
		InstanceId, bVisible, *Prompt.ToString(), BroadcastCount);
}