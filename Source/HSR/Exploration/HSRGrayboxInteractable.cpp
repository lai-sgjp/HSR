#include "HSRGrayboxInteractable.h"
#include "../Interaction/HSRInteractionComponent.h"
#include "../Interaction/HSRInteractableInterface.h"
#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "../Character/HSRExplorationCharacter.h"

AHSRGrayboxInteractable::AHSRGrayboxInteractable()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	SetRootComponent(CollisionComponent);

	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetGenerateOverlapEvents(true);

	bAvailable = true;
}

void AHSRGrayboxInteractable::SetAvailable(bool bInAvailable)
{
	bAvailable = bInAvailable;
}

bool AHSRGrayboxInteractable::IsInteractionAvailable_Implementation() const
{
	return bAvailable && !IsPendingKillPending();
}

FText AHSRGrayboxInteractable::GetInteractionPrompt_Implementation() const
{
	return NSLOCTEXT("HSRGrayboxInteractable", "Prompt", "Graybox Interactable");
}

FHSRInteractionResult AHSRGrayboxInteractable::ExecuteInteraction_Implementation(const FHSRInteractionContext& Context)
{
	AActor* Interactor = Context.InteractorActor.Get();

	// Reject if the interactor is no longer valid
	if (!Interactor)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRGrayboxInteractable::ExecuteInteraction - %s FAILED TargetInvalid (interactor expired)"), *GetName());
		return FHSRInteractionResult::MakeFailure(
			EHSRInteractionFailureReason::TargetInvalid,
			FText::FromString(TEXT("Interactor is no longer valid.")));
	}

	// Verify the interactor is still within range via actual overlap check
	if (!CollisionComponent->IsOverlappingActor(Interactor))
	{
		UE_LOG(LogTemp, Log, TEXT("AHSRGrayboxInteractable::ExecuteInteraction - %s FAILED OutOfRange (interactor=%s not overlapping)"),
			*GetName(), *Interactor->GetName());
		return FHSRInteractionResult::MakeFailure(
			EHSRInteractionFailureReason::OutOfRange,
			FText::FromString(TEXT("Interactor is out of range.")));
	}

	// If an EncounterDefinition is assigned, submit it to the BattleTransitionSubsystem
	if (EncounterDefinition)
	{
		UGameInstance* GI = GetGameInstance();
		if (GI)
		{
			UHSRBattleTransitionSubsystem* Subsystem = GI->GetSubsystem<UHSRBattleTransitionSubsystem>();
			if (Subsystem)
			{
				FHSREncounterResult EncResult = Subsystem->RequestEncounter(EncounterDefinition);
				if (EncResult.ResultType == EHSREncounterResultType::Success)
				{
					UE_LOG(LogTemp, Log, TEXT("AHSRGrayboxInteractable::ExecuteInteraction - %s submitted EncounterRequest %s"),
						*GetName(), *EncResult.RequestId.ToString());
					return FHSRInteractionResult::MakeSuccess();
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("AHSRGrayboxInteractable::ExecuteInteraction - %s EncounterRequest FAILED type=%d msg=%s"),
						*GetName(), static_cast<int32>(EncResult.ResultType), *EncResult.Message.ToString());
					return FHSRInteractionResult::MakeFailure(
						EHSRInteractionFailureReason::ExecutionFailed,
						FText::Format(NSLOCTEXT("Graybox", "EncounterFailed", "Encounter failed: {0}"), EncResult.Message));
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("AHSRGrayboxInteractable::ExecuteInteraction - %s FAILED ExecutionFailed (no subsystem)"), *GetName());
				return FHSRInteractionResult::MakeFailure(
					EHSRInteractionFailureReason::ExecutionFailed,
					FText::FromString(TEXT("Cannot access BattleTransition subsystem.")));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("AHSRGrayboxInteractable::ExecuteInteraction - %s FAILED ExecutionFailed (no GameInstance)"), *GetName());
			return FHSRInteractionResult::MakeFailure(
				EHSRInteractionFailureReason::ExecutionFailed,
				FText::FromString(TEXT("Cannot access GameInstance.")));
		}
	}

	// No EncounterDefinition: fall through to original log-and-success
	UE_LOG(LogTemp, Log, TEXT("AHSRGrayboxInteractable::ExecuteInteraction - %s interacted by %s at location (X=%.0f Y=%.0f Z=%.0f)"),
		*GetName(), *Interactor->GetName(),
		Context.InteractionLocation.X, Context.InteractionLocation.Y, Context.InteractionLocation.Z);
	return FHSRInteractionResult::MakeSuccess();
}

void AHSRGrayboxInteractable::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	if (!OtherActor)
	{
		return;
	}

	AHSRExplorationCharacter* ExplorationChar = Cast<AHSRExplorationCharacter>(OtherActor);
	if (!ExplorationChar)
	{
		return;
	}

	UHSRInteractionComponent* InteractionComp = ExplorationChar->FindComponentByClass<UHSRInteractionComponent>();
	if (InteractionComp)
	{
		InteractionComp->RegisterCandidate(this);
	}
}

void AHSRGrayboxInteractable::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);

	if (!OtherActor)
	{
		return;
	}

	AHSRExplorationCharacter* ExplorationChar = Cast<AHSRExplorationCharacter>(OtherActor);
	if (!ExplorationChar)
	{
		return;
	}

	UHSRInteractionComponent* InteractionComp = ExplorationChar->FindComponentByClass<UHSRInteractionComponent>();
	if (InteractionComp)
	{
		InteractionComp->UnregisterCandidate(this);
	}
}

