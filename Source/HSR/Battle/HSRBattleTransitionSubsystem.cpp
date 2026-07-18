#include "HSRBattleTransitionSubsystem.h"
#include "../Data/Definitions/HSREncounterDefinition.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

void UHSRBattleTransitionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CurrentState = EHSREncounterState::Empty;
	UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::Initialize - State=Empty"));
}

FHSREncounterResult UHSRBattleTransitionSubsystem::RequestEncounter(UHSREncounterDefinition* Definition)
{
	// Pre-travel validation: must not pollute Pending state
	if (!Definition)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::RequestEncounter - FAILED InvalidDefinition (null)"));
		return FHSREncounterResult::MakeFailure(
			EHSREncounterResultType::InvalidDefinition,
			FText::FromString(TEXT("EncounterDefinition is null.")));
	}

	if (Definition->EncounterId.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::RequestEncounter - FAILED InvalidRequest (EncounterId=None)"));
		return FHSREncounterResult::MakeFailure(
			EHSREncounterResultType::InvalidRequest,
			FText::FromString(TEXT("EncounterId is not set.")));
	}

	if (Definition->EnemyDefinitionId.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::RequestEncounter - FAILED InvalidRequest (EnemyDefinitionId=None)"));
		return FHSREncounterResult::MakeFailure(
			EHSREncounterResultType::InvalidRequest,
			FText::FromString(TEXT("EnemyDefinitionId is not set.")));
	}

	if (Definition->BattleMap.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::RequestEncounter - FAILED InvalidMap (BattleMap=null)"));
		return FHSREncounterResult::MakeFailure(
			EHSREncounterResultType::InvalidMap,
			FText::FromString(TEXT("BattleMap is not set.")));
	}

	// Reject if a transition is already in progress
	if (CurrentState == EHSREncounterState::Pending || CurrentState == EHSREncounterState::Traveling)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::RequestEncounter - FAILED AlreadyPending (state=%d, existing=%s)"),
			static_cast<int32>(CurrentState), *PendingRequest.RequestId.ToString());
		return FHSREncounterResult::MakeFailure(
			EHSREncounterResultType::AlreadyPending,
			FText::FromString(TEXT("A battle transition is already pending or traveling.")));
	}

	// Build pure-data request DTO (no World object references, no pointers to runtime instances)
	FGuid NewRequestId;
	FPlatformMisc::CreateGuid(NewRequestId);

	FTransform ReturnTransform = FTransform::Identity;
	UWorld* World = GetWorld();
	if (World)
	{
		APlayerController* PC = World->GetFirstPlayerController();
		if (PC && PC->GetPawn())
		{
			ReturnTransform = PC->GetPawn()->GetActorTransform();
		}
	}

	FHSREncounterRequest NewRequest;
	NewRequest.RequestId = NewRequestId;
	NewRequest.EncounterId = Definition->EncounterId;
	NewRequest.EnemyDefinitionId = Definition->EnemyDefinitionId;
	NewRequest.Initiative = EHSREncounterInitiative::Neutral;
	NewRequest.BattleMapPath = FName(*Definition->BattleMap.GetLongPackageName());
	NewRequest.ReturnTransform = ReturnTransform;

	// Record the exploration map soft path (strip PIE prefix for pure data path)
	if (World)
	{
		FString WorldPath = World->GetOutermost()->GetPathName();
		NewRequest.ExplorationMapPath = FName(*UWorld::RemovePIEPrefix(WorldPath));
	}

	PendingRequest = NewRequest;
	CurrentState = EHSREncounterState::Pending;

	UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::RequestEncounter - RequestId=%s EncounterId=%s EnemyDefId=%s ExplorationMap=%s"),
		*NewRequestId.ToString(), *Definition->EncounterId.ToString(), *Definition->EnemyDefinitionId.ToString(),
		*NewRequest.ExplorationMapPath.ToString());
	UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::RequestEncounter - BattleMap=%s ReturnLoc=%s"),
		*Definition->BattleMap.GetLongPackageName(),
		*ReturnTransform.GetLocation().ToString());

	// Initiate level travel. OpenLevel returns void, so synchronous success check is not available.
	if (World)
	{
		CurrentState = EHSREncounterState::Traveling;
		UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::RequestEncounter - Traveling to %s"),
			*Definition->BattleMap.GetLongPackageName());

		UGameplayStatics::OpenLevel(World, FName(*Definition->BattleMap.GetLongPackageName()), true);

		UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::RequestEncounter - OpenLevel issued for %s"),
			*Definition->BattleMap.GetLongPackageName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UHSRBattleTransitionSubsystem::RequestEncounter - FAILED TravelInitiationFailed (no World)"));
		CurrentState = EHSREncounterState::Empty;
		PendingRequest = FHSREncounterRequest();
		return FHSREncounterResult::MakeFailure(
			EHSREncounterResultType::TravelInitiationFailed,
			FText::FromString(TEXT("Cannot resolve World for travel.")));
	}

	return FHSREncounterResult::MakeSuccess(NewRequestId);
}

FHSREncounterResult UHSRBattleTransitionSubsystem::ConsumePendingEncounter()
{
	if (CurrentState == EHSREncounterState::Empty)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::ConsumePendingEncounter - FAILED NothingPending"));
		return FHSREncounterResult::MakeFailure(
			EHSREncounterResultType::NothingPending,
			FText::FromString(TEXT("No pending encounter to consume.")));
	}

	if (CurrentState == EHSREncounterState::Consumed)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::ConsumePendingEncounter - FAILED AlreadyConsumed (request=%s)"),
			*PendingRequest.RequestId.ToString());
		return FHSREncounterResult::MakeFailure(
			EHSREncounterResultType::AlreadyConsumed,
			FText::FromString(TEXT("This encounter has already been consumed.")));
	}

	if (CurrentState == EHSREncounterState::Pending)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::ConsumePendingEncounter - FAILED NothingPending (still Pending, not yet Traveling)"));
		return FHSREncounterResult::MakeFailure(
			EHSREncounterResultType::NothingPending,
			FText::FromString(TEXT("Travel has not completed yet.")));
	}

	// Capture the full DTO before clearing internal payload
	FHSREncounterRequest Consumed = PendingRequest;
	FGuid ConsumedId = Consumed.RequestId;

	// Immediately clear internal payload (consume invariant: payload is no longer readable)
	PendingRequest = FHSREncounterRequest();
	CurrentState = EHSREncounterState::Consumed;

	UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::ConsumePendingEncounter - SUCCESS RequestId=%s EncounterId=%s EnemyDefId=%s"),
		*Consumed.RequestId.ToString(), *Consumed.EncounterId.ToString(), *Consumed.EnemyDefinitionId.ToString());
	UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::ConsumePendingEncounter - Initiative=%d BattleMapPath=%s ExplorationMap=%s ReturnLoc=%s"),
		static_cast<int32>(Consumed.Initiative), *Consumed.BattleMapPath.ToString(),
		*Consumed.ExplorationMapPath.ToString(),
		*Consumed.ReturnTransform.GetLocation().ToString());

	// Return the full consumed DTO in the result so Consumer does not re-read from Subsystem
	FHSREncounterResult Result = FHSREncounterResult::MakeSuccess(ConsumedId);
	Result.ConsumedRequest = Consumed;
	return Result;
}

void UHSRBattleTransitionSubsystem::ClearPending()
{
	if (CurrentState != EHSREncounterState::Empty)
	{
		UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::ClearPending - Cleared state=%d request=%s"),
			static_cast<int32>(CurrentState), *PendingRequest.RequestId.ToString());
	}

	CurrentState = EHSREncounterState::Empty;
	PendingRequest = FHSREncounterRequest();
}

bool UHSRBattleTransitionSubsystem::HasPending() const
{
	return CurrentState == EHSREncounterState::Pending || CurrentState == EHSREncounterState::Traveling;
}
