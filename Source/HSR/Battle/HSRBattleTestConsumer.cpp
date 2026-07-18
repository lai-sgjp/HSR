#include "HSRBattleTestConsumer.h"
#include "HSRBattleTransitionSubsystem.h"

AHSRBattleTestConsumer::AHSRBattleTestConsumer()
{
	PrimaryActorTick.bCanEverTick = false;
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
	FHSREncounterResult FirstResult = Subsystem->ConsumePendingEncounter();
	if (FirstResult.ResultType == EHSREncounterResultType::Success)
	{
		UE_LOG(LogTemp, Log, TEXT("AHSRBattleTestConsumer::BeginPlay - First Consume SUCCESS"));
		UE_LOG(LogTemp, Log, TEXT("  RequestId    = %s"), *FirstResult.RequestId.ToString());

		const FHSREncounterRequest& Req = Subsystem->GetPendingRequest();
		UE_LOG(LogTemp, Log, TEXT("  EncounterId  = %s"), *Req.EncounterId.ToString());
		UE_LOG(LogTemp, Log, TEXT("  EnemyDefId   = %s"), *Req.EnemyDefinitionId.ToString());
		UE_LOG(LogTemp, Log, TEXT("  Initiative   = %d"), static_cast<int32>(Req.Initiative));
		UE_LOG(LogTemp, Log, TEXT("  BattleMap    = %s"), *Req.BattleMapPath.ToString());
		UE_LOG(LogTemp, Log, TEXT("  ReturnLoc    = %s"), *Req.ReturnTransform.GetLocation().ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AHSRBattleTestConsumer::BeginPlay - First Consume FAILED (type=%d)"),
			static_cast<int32>(FirstResult.ResultType));
		UE_LOG(LogTemp, Error, TEXT("  Message: %s"), *FirstResult.Message.ToString());
	}

	// --- Second consumption: must FAIL with AlreadyConsumed ---
	FHSREncounterResult SecondResult = Subsystem->ConsumePendingEncounter();
	if (SecondResult.ResultType == EHSREncounterResultType::AlreadyConsumed)
	{
		UE_LOG(LogTemp, Log, TEXT("AHSRBattleTestConsumer::BeginPlay - Second Consume correctly FAILED AlreadyConsumed"));
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
