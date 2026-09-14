#include "HSRSceneInteraction.h"
#include "Components/StaticMeshComponent.h"
#include "../Map/HSRMapSubsystem.h"
#include "../Quest/HSRQuestSubsystem.h"
#include "Engine/GameInstance.h"

AHSRSceneInteraction::AHSRSceneInteraction()
{
    DisplayMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayMesh"));
    DisplayMesh->SetupAttachment(RootComponent);
    DisplayMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CollisionComponent->SetSphereRadius(180);
}
FText AHSRSceneInteraction::GetInteractionPrompt_Implementation() const { return Prompt; }
FHSRInteractionResult AHSRSceneInteraction::ExecuteInteraction_Implementation(const FHSRInteractionContext& Context)
{
    if (!Context.InteractorActor.IsValid() || !GetGameInstance()) return FHSRInteractionResult::MakeFailure(EHSRInteractionFailureReason::TargetInvalid);
    auto* Maps=GetGameInstance()->GetSubsystem<UHSRMapSubsystem>();
    if (!TeleportId.IsNone())
    {
        const auto Result=Maps ? Maps->RequestTeleportTravel(TeleportId) : EHSRMapOperationResult::InvalidWorld;
        return Result==EHSRMapOperationResult::Success ? FHSRInteractionResult::MakeSuccess() : FHSRInteractionResult::MakeFailure(EHSRInteractionFailureReason::ExecutionFailed);
    }
    if (!QuestEventId.IsNone())
    {
        auto* Quest=GetGameInstance()->GetSubsystem<UHSRQuestSubsystem>();
        if (!Quest) return FHSRInteractionResult::MakeFailure(EHSRInteractionFailureReason::ExecutionFailed);
        FHSRQuestDomainEvent Event; Event.EventId=QuestEventId;
        TArray<FHSRQuestRuntimeState> Changed;
        const auto Result=Quest->SubmitEvent(Event,Changed);
        if (Result!=EHSRQuestOperationResult::Success && Result!=EHSRQuestOperationResult::NoOp) return FHSRInteractionResult::MakeFailure(EHSRInteractionFailureReason::ExecutionFailed);
        if (Maps && !DiscoveryId.IsNone()) Maps->SetExplorationFlag(DiscoveryId);
        auto Success=FHSRInteractionResult::MakeSuccess(); Success.Message=NSLOCTEXT("HSRScene","Investigated","调查记录已更新"); return Success;
    }
    return Super::ExecuteInteraction_Implementation(Context);
}
