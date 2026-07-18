#include "HSRGrayboxInteractable.h"
#include "../Interaction/HSRInteractionComponent.h"

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
	UE_LOG(LogTemp, Log, TEXT("AHSRGrayboxInteractable::ExecuteInteraction - %s interacted"),
		*GetName());
	return FHSRInteractionResult::MakeSuccess();
}

void AHSRGrayboxInteractable::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	if (!OtherActor)
	{
		return;
	}

	UHSRInteractionComponent* InteractionComp = OtherActor->FindComponentByClass<UHSRInteractionComponent>();
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

	UHSRInteractionComponent* InteractionComp = OtherActor->FindComponentByClass<UHSRInteractionComponent>();
	if (InteractionComp)
	{
		InteractionComp->UnregisterCandidate(this);
	}
}

