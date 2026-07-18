#include "HSRInteractionComponent.h"
#include "HSRInteractableInterface.h"

UHSRInteractionComponent::UHSRInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bCandidateEverRegistered = false;
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
	bCandidateEverRegistered = true;
	OnCandidateChanged.Broadcast(Candidate);
	UE_LOG(LogTemp, Log, TEXT("UHSRInteractionComponent::RegisterCandidate - Owner=%s Registered %s"),
		*GetOwner()->GetName(), *Candidate->GetName());
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
		bCandidateEverRegistered = false;
		OnCandidateChanged.Broadcast(nullptr);
		UE_LOG(LogTemp, Log, TEXT("UHSRInteractionComponent::UnregisterCandidate - Owner=%s Unregistered %s"),
			*GetOwner()->GetName(), *OldCandidate->GetName());
	}
}

bool UHSRInteractionComponent::HasValidCandidate() const
{
	return IsCandidateValid();
}


FText UHSRInteractionComponent::GetCurrentPrompt() const
{
	AActor* Candidate = CurrentCandidate.Get();
	if (!Candidate || !Candidate->Implements<UHSRInteractableInterface>())
	{
		return FText::GetEmpty();
	}
	return IHSRInteractableInterface::Execute_GetInteractionPrompt(Candidate);
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
	AActor* OwnerActor = GetOwner();

	// Candidate existed but the weak pointer expired (target destroyed)
	if (!Candidate && bCandidateEverRegistered)
	{
		CurrentCandidate.Reset();
		bCandidateEverRegistered = false;
		OnCandidateChanged.Broadcast(nullptr);
		FHSRInteractionResult Result = FHSRInteractionResult::MakeFailure(
			EHSRInteractionFailureReason::TargetInvalid,
			FText::FromString(TEXT("Candidate was destroyed.")));
		OnInteractionCompleted.Broadcast(Result);
		UE_LOG(LogTemp, Log, TEXT("UHSRInteractionComponent::TryInteract - Owner=%s FAILED TargetInvalid (candidate destroyed)"),
			*GetOwner()->GetName());
		return Result;
	}

	// Never had a candidate, or candidate was properly unregistered
	if (!Candidate)
	{
		FHSRInteractionResult Result = FHSRInteractionResult::MakeFailure(
			EHSRInteractionFailureReason::NoCandidate,
			FText::FromString(TEXT("No interaction candidate available.")));
		OnInteractionCompleted.Broadcast(Result);
		UE_LOG(LogTemp, Log, TEXT("UHSRInteractionComponent::TryInteract - Owner=%s FAILED NoCandidate"),
			*GetOwner()->GetName());
		return Result;
	}

	if (!Candidate->Implements<UHSRInteractableInterface>())
	{
		CurrentCandidate.Reset();
		bCandidateEverRegistered = false;
		OnCandidateChanged.Broadcast(nullptr);
		FHSRInteractionResult Result = FHSRInteractionResult::MakeFailure(
			EHSRInteractionFailureReason::TargetInvalid,
			FText::FromString(TEXT("Candidate no longer implements the interaction interface.")));
		OnInteractionCompleted.Broadcast(Result);
		UE_LOG(LogTemp, Log, TEXT("UHSRInteractionComponent::TryInteract - Owner=%s Candidate=%s FAILED TargetInvalid (no interface)"),
			*GetOwner()->GetName(), *Candidate->GetName());
		return Result;
	}

	if (!IHSRInteractableInterface::Execute_IsInteractionAvailable(Candidate))
	{
		FHSRInteractionResult Result = FHSRInteractionResult::MakeFailure(
			EHSRInteractionFailureReason::Unavailable,
			FText::FromString(TEXT("Candidate is currently unavailable.")));
		OnInteractionCompleted.Broadcast(Result);
		UE_LOG(LogTemp, Log, TEXT("UHSRInteractionComponent::TryInteract - Owner=%s Candidate=%s FAILED Unavailable"),
			*GetOwner()->GetName(), *Candidate->GetName());
		return Result;
	}

	FHSRInteractionContext Context(OwnerActor, OwnerActor->GetActorLocation());
	FHSRInteractionResult Result = IHSRInteractableInterface::Execute_ExecuteInteraction(Candidate, Context);

	if (!Result.bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("UHSRInteractionComponent::TryInteract - Owner=%s Candidate=%s FAILED ExecutionFailed reason=%d"),
			*GetOwner()->GetName(), *Candidate->GetName(), static_cast<int32>(Result.FailureReason));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("UHSRInteractionComponent::TryInteract - Owner=%s Candidate=%s SUCCESS"),
			*GetOwner()->GetName(), *Candidate->GetName());
	}

	OnInteractionCompleted.Broadcast(Result);
	return Result;
}

void UHSRInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CurrentCandidate.Reset();
	bCandidateEverRegistered = false;
	OnCandidateChanged.Clear();
	OnInteractionCompleted.Clear();
	Super::EndPlay(EndPlayReason);
}


