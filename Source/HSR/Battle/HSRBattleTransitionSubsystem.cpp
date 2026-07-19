#include "HSRBattleTransitionSubsystem.h"
#include "../Data/Definitions/HSREncounterDefinition.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

void UHSRBattleTransitionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CurrentState = EHSREncounterState::Empty;
	bReturnPending = false;
	TravelKind = EHSRTravelKind::None;

	if (GEngine)
	{
		GEngine->OnTravelFailure().AddUObject(this, &UHSRBattleTransitionSubsystem::HandleTravelFailure);
	}

	UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::Initialize - State=Empty"));
}

void UHSRBattleTransitionSubsystem::Deinitialize()
{
	if (GEngine)
	{
		GEngine->OnTravelFailure().RemoveAll(this);
	}

	Super::Deinitialize();
	UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::Deinitialize"));
}

FHSREncounterResult UHSRBattleTransitionSubsystem::RequestEncounter(UHSREncounterDefinition* Definition, EHSREncounterInitiative Initiative)
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
	NewRequest.Initiative = Initiative;
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
		TravelKind = EHSRTravelKind::Encounter;
		TravelTargetMap = FName(*Definition->BattleMap.GetLongPackageName());
		UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::RequestEncounter - Traveling to %s (kind=Encounter, map=%s)"),
			*Definition->BattleMap.GetLongPackageName(), *TravelTargetMap.ToString());

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
	TravelKind = EHSRTravelKind::None;
	TravelTargetMap = NAME_None;

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
	TravelKind = EHSRTravelKind::None;
	TravelTargetMap = NAME_None;
}

bool UHSRBattleTransitionSubsystem::HasPending() const
{
	return CurrentState == EHSREncounterState::Pending || CurrentState == EHSREncounterState::Traveling;
}

void UHSRBattleTransitionSubsystem::ClearReturn()
{
	UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::ClearReturn - Clearing Return context"));
	PendingReturnContext = FHSRExplorationReturnContext();
	bReturnPending = false;
	TravelKind = EHSRTravelKind::None;
	TravelTargetMap = NAME_None;
}

void UHSRBattleTransitionSubsystem::HandleTravelFailure(UWorld* InWorld, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::HandleTravelFailure - type=%d Error=%s World=%s TargetMap=%s"),
		static_cast<int32>(FailureType), *ErrorString, InWorld ? *InWorld->GetName() : TEXT("null"), *TravelTargetMap.ToString());

	// Only clear the matching pending transaction (not unrelated new requests)
	if (TravelKind == EHSRTravelKind::Encounter)
	{
		UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::HandleTravelFailure - Clearing Encounter state (RequestId=%s)"),
			*PendingRequest.RequestId.ToString());
		ClearPending();
	}
	else if (TravelKind == EHSRTravelKind::Return)
	{
		UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::HandleTravelFailure - Clearing Return context"));
		ClearReturn();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::HandleTravelFailure - No matching pending transaction, ignoring"));
	}

	// Clear travel tracking
	TravelKind = EHSRTravelKind::None;
	TravelTargetMap = NAME_None;
}


FHSRExplorationReturnResult UHSRBattleTransitionSubsystem::RequestTestReturn(const FHSREncounterRequest& FromConsumedRequest)
{
	// Validate BEFORE writing (must not pollute Pending)
	if (bReturnPending)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::RequestTestReturn - FAILED AlreadyPending"));
		return FHSRExplorationReturnResult::MakeFailure(
			EHSREncounterReturnResultType::AlreadyPending,
			FText::FromString(TEXT("A return is already pending.")));
	}

	if (FromConsumedRequest.ExplorationMapPath.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::RequestTestReturn - FAILED InvalidReturnContext (no path)"));
		return FHSRExplorationReturnResult::MakeFailure(
			EHSREncounterReturnResultType::InvalidReturnContext,
			FText::FromString(TEXT("No exploration map path in return context.")));
	}

	// Also reject invalid RequestId
	if (!FromConsumedRequest.RequestId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::RequestTestReturn - FAILED InvalidReturnContext (invalid RequestId)"));
		return FHSRExplorationReturnResult::MakeFailure(
			EHSREncounterReturnResultType::InvalidReturnContext,
			FText::FromString(TEXT("Invalid RequestId in return context.")));
	}

	// Build pure-data return context from consumed request
	FHSRExplorationReturnContext ReturnCtx;
	ReturnCtx.RequestId = FromConsumedRequest.RequestId;
	ReturnCtx.ExplorationMapPath = FromConsumedRequest.ExplorationMapPath;
	ReturnCtx.ReturnTransform = FromConsumedRequest.ReturnTransform;

	// Check World availability BEFORE writing Pending
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::RequestTestReturn - FAILED: no World, rejecting"));
		return FHSRExplorationReturnResult::MakeFailure(
			EHSREncounterReturnResultType::InvalidReturnContext,
			FText::FromString(TEXT("Cannot resolve World for travel.")));
	}

	PendingReturnContext = ReturnCtx;
	bReturnPending = true;

	TravelKind = EHSRTravelKind::Return;
	TravelTargetMap = ReturnCtx.ExplorationMapPath;

	UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::RequestTestReturn - SUCCESS RequestId=%s ExplorationMap=%s (kind=Return)"),
		*ReturnCtx.RequestId.ToString(), *ReturnCtx.ExplorationMapPath.ToString());

	UGameplayStatics::OpenLevel(World, ReturnCtx.ExplorationMapPath, true);
	UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::RequestTestReturn - Traveling back to %s"), *ReturnCtx.ExplorationMapPath.ToString());

	return FHSRExplorationReturnResult::MakeSuccess();
}

FHSRExplorationReturnResult UHSRBattleTransitionSubsystem::ConsumeReturnContext()
{
	if (!bReturnPending)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::ConsumeReturnContext - FAILED NothingPending"));
		return FHSRExplorationReturnResult::MakeFailure(
			EHSREncounterReturnResultType::NothingPending,
			FText::FromString(TEXT("No pending return context.")));
	}

	// Capture the full DTO before clearing internal payload
	FHSRExplorationReturnContext Consumed = PendingReturnContext;
	PendingReturnContext = FHSRExplorationReturnContext();
	bReturnPending = false;
	TravelKind = EHSRTravelKind::None;
	TravelTargetMap = NAME_None;

	UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::ConsumeReturnContext - SUCCESS RequestId=%s ReturnLoc=%s"),
		*Consumed.RequestId.ToString(), *Consumed.ReturnTransform.GetLocation().ToString());

	// Return the full consumed context in the Result so consumer does NOT re-read from Subsystem
	FHSRExplorationReturnResult Result = FHSRExplorationReturnResult::MakeSuccess();
	Result.ConsumedContext = Consumed;
	return Result;
}


