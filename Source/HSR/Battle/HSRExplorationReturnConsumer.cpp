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
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(ReturnTimerHandle);
	}
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

	if (!PlayerPawn && ScheduleRetry(TEXT("player pawn unavailable")))
	{
		return;
	}

	if (!PlayerPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRExplorationReturnConsumer::TryConsumeAndReturn - FAILED: no player pawn after %d retries"), MaxRetries);
		Subsystem->ClearReturn();
		return;
	}

	// Commit only after MapSubsystem validates the loaded World and places the pawn.
	FHSRExplorationReturnResult Result = Subsystem->CommitReturnContext(PlayerPawn);

	if (Result.ResultType == EHSREncounterReturnResultType::Success)
	{
	UE_LOG(LogTemp, Log, TEXT("AHSRExplorationReturnConsumer::TryConsumeAndReturn - SUCCESS: Teleported pawn to %s"),
		*Result.ConsumedContext.ReturnTransform.GetLocation().ToString());

	// A4c: Test second ConsumeReturnContext — must return AlreadyConsumed
	FHSRExplorationReturnResult SecondResult = Subsystem->CommitReturnContext(PlayerPawn);
	if (SecondResult.ResultType == EHSREncounterReturnResultType::AlreadyConsumed)
	{
		UE_LOG(LogTemp, Log, TEXT("A4c: Second ConsumeReturnContext correctly returned AlreadyConsumed"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("A4c: Second ConsumeReturnContext unexpected type=%d (expected AlreadyConsumed)"),
			static_cast<int32>(SecondResult.ResultType));
	}
}
else
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRExplorationReturnConsumer::TryConsumeAndReturn - FAILED: ConsumeReturnContext returned type=%d"),
			static_cast<int32>(Result.ResultType));
		if (!ScheduleRetry(TEXT("return commit rejected")))
		{
			UE_LOG(LogTemp, Warning, TEXT("AHSRExplorationReturnConsumer::TryConsumeAndReturn - retries exhausted; clearing return for retry"));
			Subsystem->ClearReturn();
		}
	}
}

bool AHSRExplorationReturnConsumer::ScheduleRetry(const TCHAR* Reason)
{
	if (RetryCount >= MaxRetries || !GetWorld())
	{
		return false;
	}
	++RetryCount;
	GetWorld()->GetTimerManager().SetTimer(ReturnTimerHandle, this,
		&AHSRExplorationReturnConsumer::TryConsumeAndReturn, RetryInterval, false);
	UE_LOG(LogTemp, Log, TEXT("AHSRExplorationReturnConsumer::TryConsumeAndReturn - waiting: %s (attempt %d/%d)"),
		Reason, RetryCount, MaxRetries);
	return true;
}
