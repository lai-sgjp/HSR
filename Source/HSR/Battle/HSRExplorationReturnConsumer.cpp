#include "HSRExplorationReturnConsumer.h"
#include "HSRBattleTransitionSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

AHSRExplorationReturnConsumer::AHSRExplorationReturnConsumer()
{
	PrimaryActorTick.bCanEverTick = false;
	RetryCount = 0;
}

void AHSRExplorationReturnConsumer::BeginPlay()
{
	Super::BeginPlay();

	// Wait for player pawn to exist, then consume return and apply transform
	GetWorld()->GetTimerManager().SetTimer(ReturnTimerHandle, this, &AHSRExplorationReturnConsumer::TryConsumeAndReturn, RetryInterval, false);
}

void AHSRExplorationReturnConsumer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(ReturnTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void AHSRExplorationReturnConsumer::TryConsumeAndReturn()
{
	UHSRBattleTransitionSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UHSRBattleTransitionSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	if (!Subsystem->HasReturnPending())
	{
		// No return pending - nothing to do
		UE_LOG(LogTemp, Log, TEXT("AHSRExplorationReturnConsumer::TryConsumeAndReturn - No return pending"));
		return;
	}

	// Wait for player pawn to be available
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;

	if (!PlayerPawn && RetryCount < MaxRetries)
	{
		RetryCount++;
		GetWorld()->GetTimerManager().SetTimer(ReturnTimerHandle, this, &AHSRExplorationReturnConsumer::TryConsumeAndReturn, RetryInterval, false);
		UE_LOG(LogTemp, Log, TEXT("AHSRExplorationReturnConsumer::TryConsumeAndReturn - waiting for pawn (attempt %d/%d)"), RetryCount, MaxRetries);
		return;
	}

	if (!PlayerPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRExplorationReturnConsumer::TryConsumeAndReturn - FAILED: no player pawn after %d retries"), MaxRetries);
		Subsystem->ClearPending();
		return;
	}

	// Consume return context
	const FHSRExplorationReturnContext& Ctx = Subsystem->GetReturnContext();
	FHSRExplorationReturnResult Result = Subsystem->ConsumeReturnContext();

	if (Result.ResultType == EHSREncounterReturnResultType::Success)
	{
		// Apply the return transform to the player pawn
		PlayerPawn->SetActorTransform(Ctx.ReturnTransform, false, nullptr, ETeleportType::TeleportPhysics);

		UE_LOG(LogTemp, Log, TEXT("AHSRExplorationReturnConsumer::TryConsumeAndReturn - SUCCESS: Teleported pawn to %s"),
			*Ctx.ReturnTransform.GetLocation().ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRExplorationReturnConsumer::TryConsumeAndReturn - FAILED: ConsumeReturnContext returned type=%d"),
			static_cast<int32>(Result.ResultType));
	}
}
