#include "HSRInteractableInterface.h"
#include "HSRInteractionComponent.h"

UHSRInteractionComponent::UHSRInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHSRInteractionComponent::RegisterCandidate(AActor* Candidate)
{
	if (!Candidate)
	{
		return;
	}

	if (CurrentCandidate.IsValid() && CurrentCandidate.Get() == Candidate)
	{
		return;
	}

	if (CurrentCandidate.IsValid() && CurrentCandidate.Get() != Candidate)
	{
		UE_LOG(LogTemp, Log, TEXT("UHSRInteractionComponent::RegisterCandidate - Already has candidate %s, rejecting %s"),
			*CurrentCandidate.Get()->GetName(), *Candidate->GetName());
		return;
	}

	if (!Candidate->Implements<UHSRInteractableInterface>())
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRInteractionComponent::RegisterCandidate - %s does not implement IHSRInteractableInterface"), *Candidate->GetName());
		return;
	}

	CurrentCandidate = Candidate;
	OnCandidateChanged.Broadcast(Candidate);
	UE_LOG(LogTemp, Log, TEXT("UHSRInteractionComponent::RegisterCandidate - Registered %s"), *Candidate->GetName());
}

void UHSRInteractionComponent::UnregisterCandidate(AActor* Candidate)
{
	if (!Candidate || !CurrentCandidate.IsValid())
	{
		return;
	}

	if (CurrentCandidate.Get() == Candidate)
	{
		AActor* OldCandidate = CurrentCandidate.Get();
		CurrentCandidate.Reset();
		OnCandidateChanged.Broadcast(nullptr);
		UE_LOG(LogTemp, Log, TEXT("UHSRInteractionComponent::UnregisterCandidate - Unregistered %s"), *OldCandidate->GetName());
	}
}

bool UHSRInteractionComponent::HasValidCandidate() const
{
	return IsCandidateValid();
}

bool UHSRInteractionComponent::IsCandidateValid() const
{
	AActor* Candidate = CurrentCandidate.Get();
	if (!Candidate)
	{
		return false;
	}

	if (!Candidate->Implements<UHSRInteractableInterface>())
	{
		return false;
	}

	if (!IHSRInteractableInterface::Execute_IsInteractionAvailable(Candidate))
	{
		return false;
	}

	return true;
}

FHSRInteractionResult UHSRInteractionComponent::TryInteract()
{
	AActor* Candidate = CurrentCandidate.Get();

	if (!Candidate)
	{
		FHSRInteractionResult Result = FHSRInteractionResult::MakeFailure(
			EHSRInteractionFailureReason::NoCandidate,
			FText::FromString(TEXT("No interaction candidate available.")));
		OnInteractionCompleted.Broadcast(Result);
		return Result;
	}

	if (!Candidate->Implements<UHSRInteractableInterface>())
	{
		CurrentCandidate.Reset();
		OnCandidateChanged.Broadcast(nullptr);
		FHSRInteractionResult Result = FHSRInteractionResult::MakeFailure(
			EHSRInteractionFailureReason::TargetInvalid,
			FText::FromString(TEXT("Candidate no longer implements the interaction interface.")));
		OnInteractionCompleted.Broadcast(Result);
		return Result;
	}

	if (!IHSRInteractableInterface::Execute_IsInteractionAvailable(Candidate))
	{
		FHSRInteractionResult Result = FHSRInteractionResult::MakeFailure(
			EHSRInteractionFailureReason::Unavailable,
			FText::FromString(TEXT("Candidate is currently unavailable.")));
		OnInteractionCompleted.Broadcast(Result);
		return Result;
	}

	FHSRInteractionContext Context(Candidate);
	FHSRInteractionResult Result = IHSRInteractableInterface::Execute_ExecuteInteraction(Candidate, Context);

	OnInteractionCompleted.Broadcast(Result);
	UE_LOG(LogTemp, Log, TEXT("UHSRInteractionComponent::TryInteract - Executed interaction on %s, success=%d, reason=%d"),
		*Candidate->GetName(), Result.bSuccess, static_cast<int32>(Result.FailureReason));

	return Result;
}

void UHSRInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CurrentCandidate.Reset();
	OnCandidateChanged.Clear();
	OnInteractionCompleted.Clear();
	Super::EndPlay(EndPlayReason);
}



