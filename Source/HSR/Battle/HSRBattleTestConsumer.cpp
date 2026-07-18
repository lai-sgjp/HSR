#include "HSRBattleTestConsumer.h"
#include "HSRBattleTransitionSubsystem.h"

AHSRBattleTestConsumer::AHSRBattleTestConsumer()
{
	PrimaryActorTick.bCanEverTick = false;
	bEnableTestReturn = false;
	ReturnDelay = 0.3f;
}

void AHSRBattleTestConsumer::BeginPlay()
{
	Super::BeginPlay();

	UHSRBattleTransitionSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UHSRBattleTransitionSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("AHSRBattleTestConsumer::BeginPlay - FAILED: Cannot access UHSRBattleTransitionSubsystem"));
		return;
	}

	// --- First consumption: should succeed ---
	// Use the Consume return value (ConsumedRequest), NOT GetPendingRequest()
	FHSREncounterResult FirstResult = Subsystem->ConsumePendingEncounter();
	if (FirstResult.ResultType == EHSREncounterResultType::Success)
	{
		const FHSREncounterRequest ConsumedReq = FirstResult.ConsumedRequest;
		UE_LOG(LogTemp, Log, TEXT("AHSRBattleTestConsumer::BeginPlay - First Consume SUCCESS"));
		UE_LOG(LogTemp, Log, TEXT("  RequestId       = %s"), *ConsumedReq.RequestId.ToString());
		UE_LOG(LogTemp, Log, TEXT("  EncounterId     = %s"), *ConsumedReq.EncounterId.ToString());
		UE_LOG(LogTemp, Log, TEXT("  EnemyDefId      = %s"), *ConsumedReq.EnemyDefinitionId.ToString());
		UE_LOG(LogTemp, Log, TEXT("  Initiative      = %d"), static_cast<int32>(ConsumedReq.Initiative));
		UE_LOG(LogTemp, Log, TEXT("  BattleMap       = %s"), *ConsumedReq.BattleMapPath.ToString());
		UE_LOG(LogTemp, Log, TEXT("  ExplorationMap  = %s"), *ConsumedReq.ExplorationMapPath.ToString());
		UE_LOG(LogTemp, Log, TEXT("  ReturnLoc       = %s"), *ConsumedReq.ReturnTransform.GetLocation().ToString());
		UE_LOG(LogTemp, Log, TEXT("  Subsystem State = %d (should be 3 = Consumed)"), static_cast<int32>(Subsystem->GetCurrentState()));

		// Store consumed request and schedule test return if enabled
		StoredConsumedRequest = ConsumedReq;
		if (bEnableTestReturn && !ConsumedReq.ExplorationMapPath.IsNone())
		{
			UE_LOG(LogTemp, Log, TEXT("AHSRBattleTestConsumer::BeginPlay - Scheduling test return in %.1f seconds"), ReturnDelay);
			GetWorld()->GetTimerManager().SetTimer(ReturnTimerHandle, this, &AHSRBattleTestConsumer::RequestTestReturn, ReturnDelay, false);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AHSRBattleTestConsumer::BeginPlay - First Consume FAILED (type=%d)"),
			static_cast<int32>(FirstResult.ResultType));
		UE_LOG(LogTemp, Error, TEXT("  Message: %s"), *FirstResult.Message.ToString());
	}

	// --- Second consumption: must FAIL with AlreadyConsumed ---
	// After first consume, the internal payload is cleared; second consume must fail
	// and must not return any ConsumedRequest data.
	FHSREncounterResult SecondResult = Subsystem->ConsumePendingEncounter();
	if (SecondResult.ResultType == EHSREncounterResultType::AlreadyConsumed)
	{
		UE_LOG(LogTemp, Log, TEXT("AHSRBattleTestConsumer::BeginPlay - Second Consume correctly FAILED AlreadyConsumed"));

		// Verify no old payload data through ConsumedRequest on failure
		const FHSREncounterRequest& EmptyReq = SecondResult.ConsumedRequest;
		UE_LOG(LogTemp, Log, TEXT("  ConsumedRequest.EncounterId (should be None): %s"), *EmptyReq.EncounterId.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AHSRBattleTestConsumer::BeginPlay - Second Consume unexpected result type=%d (expected AlreadyConsumed)"),
			static_cast<int32>(SecondResult.ResultType));
	}

	// --- Clear to restore Empty state ---
	Subsystem->ClearPending();
	UE_LOG(LogTemp, Log, TEXT("AHSRBattleTestConsumer::BeginPlay - Final State=%d (Empty)"),
		static_cast<int32>(Subsystem->GetCurrentState()));
}

void AHSRBattleTestConsumer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(ReturnTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void AHSRBattleTestConsumer::RequestTestReturn()
{
	UHSRBattleTransitionSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UHSRBattleTransitionSubsystem>();
	if (!Subsystem)
		return;

	// The consumed request was already logged in BeginPlay.
	// Now trigger a return through the subsystem.
	UE_LOG(LogTemp, Log, TEXT("AHSRBattleTestConsumer::RequestTestReturn - TEST: Calling RequestTestReturn..."));
	FHSRExplorationReturnResult RetResult = Subsystem->RequestTestReturn(StoredConsumedRequest);
	if (RetResult.ResultType == EHSREncounterReturnResultType::Success)
	{
		UE_LOG(LogTemp, Log, TEXT("AHSRBattleTestConsumer::RequestTestReturn - Return initiated, traveling back to exploration map"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRBattleTestConsumer::RequestTestReturn - Failed type=%d"),
			static_cast<int32>(RetResult.ResultType));
	}
}
