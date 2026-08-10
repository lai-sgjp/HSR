#include "HSRRewardChest.h"

#include "../Character/HSRExplorationCharacter.h"
#include "../Data/Definitions/HSRDropTableDefinition.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Data/Definitions/HSRRewardDefinition.h"
#include "../Interaction/HSRInteractionComponent.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Reward/HSRRewardSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

AHSRRewardChest::AHSRRewardChest()
{
	PrimaryActorTick.bCanEverTick = false;
	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	SetRootComponent(CollisionComponent);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetGenerateOverlapEvents(true);
	CollisionComponent->SetSphereRadius(80.0f);

	// Visible chest body so the reward is discoverable and interactive in the world.
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CollisionComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (MeshFinder.Succeeded())
	{
		MeshComponent->SetStaticMesh(MeshFinder.Object);
	}
	MeshComponent->SetRelativeScale3D(FVector(1.2f, 1.2f, 1.0f));
	MeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, -10.0f));
}

void AHSRRewardChest::BeginPlay()
{
	Super::BeginPlay();
	bClaimConfigurationValid = StableClaimId.IsValid();
	if (bClaimConfigurationValid)
	{
		for (TActorIterator<AHSRRewardChest> It(GetWorld()); It; ++It)
		{
			const AHSRRewardChest* Other = *It;
			if (Other != this && Other->StableClaimId == StableClaimId && GetPathName().Compare(Other->GetPathName()) > 0)
			{
				bClaimConfigurationValid = false;
				UE_LOG(LogTemp, Error, TEXT("P13-003 RewardChest Configuration=INVALID Reason=DuplicateStableClaimId ClaimId=%s Actor=%s Other=%s"), *StableClaimId.ToString(), *GetPathName(), *Other->GetPathName());
				break;
			}
		}
	}
	UGameInstance* GameInstance = GetGameInstance();
	UHSRRewardSubsystem* Reward = GameInstance ? GameInstance->GetSubsystem<UHSRRewardSubsystem>() : nullptr;
	if (bClaimConfigurationValid && Reward && DropTableDefinition && RewardDefinition)
	{
		const EHSRRewardOperationResult Result = Reward->RegisterBundle(ItemDefinitions, *DropTableDefinition, *RewardDefinition);
		bRewardBundleRegistered = Result == EHSRRewardOperationResult::Success || Result == EHSRRewardOperationResult::NoOp;
		if (!bRewardBundleRegistered)
		{
			UE_LOG(LogTemp, Error, TEXT("P13-004 RewardChest DefinitionRegistration=FAILED Result=%d Actor=%s"), static_cast<int32>(Result), *GetPathName());
		}
	}
}

bool AHSRRewardChest::IsInteractionAvailable_Implementation() const { return bClaimConfigurationValid && !bClaimed && !IsPendingKillPending(); }
FText AHSRRewardChest::GetInteractionPrompt_Implementation() const { return NSLOCTEXT("HSRRewardChest", "Prompt", "Open reward chest"); }

FHSRInteractionResult AHSRRewardChest::ExecuteInteraction_Implementation(const FHSRInteractionContext& Context)
{
	AActor* Interactor = Context.InteractorActor.Get();
	if (!Interactor) return FHSRInteractionResult::MakeFailure(EHSRInteractionFailureReason::TargetInvalid);
	if (!CollisionComponent->IsOverlappingActor(Interactor)) return FHSRInteractionResult::MakeFailure(EHSRInteractionFailureReason::OutOfRange);
	if (bClaimed) return FHSRInteractionResult::MakeFailure(EHSRInteractionFailureReason::Unavailable);
	UGameInstance* GameInstance = GetGameInstance();
	UHSRRewardSubsystem* Reward = GameInstance ? GameInstance->GetSubsystem<UHSRRewardSubsystem>() : nullptr;
	if (!bClaimConfigurationValid || !bRewardBundleRegistered || !StableClaimId.IsValid() || !Reward || !DropTableDefinition || !RewardDefinition)
	{
		return FHSRInteractionResult::MakeFailure(EHSRInteractionFailureReason::ExecutionFailed);
	}
	FHSRRewardRequest Request;
	Request.ClaimId = StableClaimId;
	Request.RewardDefinitionId = RewardDefinition->RewardDefinitionId;
	Request.Seed = RewardSeed;
	FHSRRewardReceipt Receipt;
	const EHSRRewardOperationResult Result = Reward->SubmitReward(Request, Receipt);
	if (Result != EHSRRewardOperationResult::Success && Result != EHSRRewardOperationResult::NoOp) return FHSRInteractionResult::MakeFailure(EHSRInteractionFailureReason::ExecutionFailed);
	bClaimed = true;
	if (UHSRInteractionComponent* Interaction = Interactor->FindComponentByClass<UHSRInteractionComponent>())
	{
		Interaction->UnregisterCandidate(this);
	}
	UE_LOG(LogTemp, Log, TEXT("P13-003 RewardChest Result=%s ClaimId=%s Revision=%lld"), Result == EHSRRewardOperationResult::Success ? TEXT("SUCCESS") : TEXT("NOOP"), *StableClaimId.ToString(), Receipt.Revision);
	return FHSRInteractionResult::MakeSuccess();
}

void AHSRRewardChest::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
	if (AHSRExplorationCharacter* Character = Cast<AHSRExplorationCharacter>(OtherActor)) if (UHSRInteractionComponent* Interaction = Character->FindComponentByClass<UHSRInteractionComponent>()) Interaction->RegisterCandidate(this);
}

void AHSRRewardChest::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);
	if (AHSRExplorationCharacter* Character = Cast<AHSRExplorationCharacter>(OtherActor)) if (UHSRInteractionComponent* Interaction = Character->FindComponentByClass<UHSRInteractionComponent>()) Interaction->UnregisterCandidate(this);
}
