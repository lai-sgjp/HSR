#include "HSRBattleTransitionSubsystem.h"
#include "../Data/Definitions/HSREncounterDefinition.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Engine/Engine.h"
#include "../Data/Definitions/HSRDropTableDefinition.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Data/Definitions/HSRRewardDefinition.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Reward/HSRRewardSubsystem.h"
#include "../Map/HSRMapSubsystem.h"

void UHSRBattleTransitionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CurrentState = EHSREncounterState::Empty;
	bReturnPending = false;
	bReturnConsumed = false;
	TravelKind = EHSRTravelKind::None;
	TravelRequestId = FGuid();
	TravelSourceMap = NAME_None;
	TravelCompletedEncounterId = NAME_None;

	if (GEngine)
	{
		GEngine->OnTravelFailure().AddUObject(this, &UHSRBattleTransitionSubsystem::HandleTravelFailure);
	}

	UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::Initialize - State=Empty"));
}

void UHSRBattleTransitionSubsystem::Deinitialize()
{
	ClearTravelTimeout();
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

	if (ResolvedEncounterIds.Contains(Definition->EncounterId))
	{
		UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::RequestEncounter - REJECTED resolved EncounterId=%s"), *Definition->EncounterId.ToString());
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::AlreadyConsumed,
			FText::FromString(TEXT("This encounter was already resolved in the current game session.")));
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

	// Pre-flight: verify the map package actually exists on disk
	FString MapPackageName = Definition->BattleMap.GetLongPackageName();
	if (!FPackageName::DoesPackageExist(MapPackageName))
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::RequestEncounter - FAILED InvalidMap (package does not exist: %s)"), *MapPackageName);
		return FHSREncounterResult::MakeFailure(
			EHSREncounterResultType::InvalidMap,
			FText::FromString(TEXT("BattleMap package does not exist on disk.")));
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
	else
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::TravelInitiationFailed, FText::FromString(TEXT("Cannot resolve World for travel.")));
	}

	if (Definition->VictoryRewardDefinition)
	{
		UHSRRewardSubsystem* Reward = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRRewardSubsystem>() : nullptr;
		if (!Reward || !Definition->RewardDropTable)
		{
			return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidDefinition, FText::FromString(TEXT("Encounter reward bundle is incomplete.")));
		}
		const EHSRRewardOperationResult BundleResult = Reward->RegisterBundle(Definition->RewardItemDefinitions, *Definition->RewardDropTable, *Definition->VictoryRewardDefinition);
		if (BundleResult != EHSRRewardOperationResult::Success && BundleResult != EHSRRewardOperationResult::NoOp)
		{
			return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidDefinition, FText::FromString(TEXT("Encounter reward bundle registration failed.")));
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
	if (Definition->VictoryRewardDefinition)
	{
		NewRequest.RewardDefinitionId = Definition->VictoryRewardDefinition->RewardDefinitionId;
		NewRequest.RewardSeed = Definition->RewardSeed;
	}

	PendingRequest = NewRequest;
	CurrentState = EHSREncounterState::Pending;
#if WITH_DEV_AUTOMATION_TESTS
	++AdmissionMutationCountForAutomation;
#endif

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
		TravelRequestId = NewRequestId;
		TravelSourceMap = NewRequest.ExplorationMapPath;
		TravelTargetMap = FName(*Definition->BattleMap.GetLongPackageName());
		StartTravelTimeout();
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

#if WITH_DEV_AUTOMATION_TESTS
FHSRTransitionAutomationSnapshot UHSRBattleTransitionSubsystem::GetAutomationSnapshot(FName EncounterId) const
{
	FHSRTransitionAutomationSnapshot Snapshot;
	Snapshot.State = CurrentState;
	Snapshot.PendingRequest = PendingRequest;
	Snapshot.TravelKind = TravelKind;
	Snapshot.TravelRequestId = TravelRequestId;
	Snapshot.bResolvedMembership = ResolvedEncounterIds.Contains(EncounterId);
	Snapshot.AdmissionMutationCount = AdmissionMutationCountForAutomation;
	return Snapshot;
}

void UHSRBattleTransitionSubsystem::SeedPendingEncounterForAutomation(const FHSREncounterRequest& InRequest)
{
	ResetEncounterAutomationFixture();
	PendingRequest = InRequest;
	CurrentState = EHSREncounterState::Pending;
	TravelKind = EHSRTravelKind::Encounter;
	TravelRequestId = InRequest.RequestId;
}

void UHSRBattleTransitionSubsystem::SeedResolvedEncounterForAutomation(FName EncounterId)
{
	ResetEncounterAutomationFixture();
	ResolvedEncounterIds.Add(EncounterId);
}

void UHSRBattleTransitionSubsystem::ResetEncounterAutomationFixture()
{
	ClearTravelTimeout();
	CurrentState = EHSREncounterState::Empty;
	PendingRequest = FHSREncounterRequest();
	TravelKind = EHSRTravelKind::None;
	TravelRequestId.Invalidate();
	TravelTargetMap = NAME_None;
	TravelSourceMap = NAME_None;
	TravelCompletedEncounterId = NAME_None;
	ResolvedEncounterIds.Reset();
	AdmissionMutationCountForAutomation = 0;
}
#endif

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
	TravelRequestId = FGuid();
	TravelTargetMap = NAME_None;
	TravelSourceMap = NAME_None;
	ClearTravelTimeout();

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
	TravelRequestId = FGuid();
	TravelTargetMap = NAME_None;
	TravelSourceMap = NAME_None;
	ClearTravelTimeout();
}

bool UHSRBattleTransitionSubsystem::HasPending() const
{
	return CurrentState == EHSREncounterState::Pending || CurrentState == EHSREncounterState::Traveling;
}

void UHSRBattleTransitionSubsystem::ClearReturn()
{
	UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::ClearReturn - Clearing Return context"));
	if (!TravelCompletedEncounterId.IsNone())
	{
		ResolvedEncounterIds.Remove(TravelCompletedEncounterId);
	}
	PendingReturnContext = FHSRExplorationReturnContext();
	bReturnPending = false;
	bReturnConsumed = false;
	TravelKind = EHSRTravelKind::None;
	TravelRequestId = FGuid();
	TravelTargetMap = NAME_None;
	TravelSourceMap = NAME_None;
	ClearTravelTimeout();
}

void UHSRBattleTransitionSubsystem::HandleTravelFailure(UWorld* InWorld, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::HandleTravelFailure - type=%d Error=%s World=%s TargetMap=%s RequestId=%s"),
		static_cast<int32>(FailureType), *ErrorString, InWorld ? *InWorld->GetName() : TEXT("null"),
		*TravelTargetMap.ToString(), *TravelRequestId.ToString());

	// 1. Early-exit if no active transaction (nothing to match)
	if (TravelKind == EHSRTravelKind::None)
	{
		UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::HandleTravelFailure - IGNORED (no active transaction, type=%d)"), static_cast<int32>(FailureType));
		return;
	}

	// 2. Attempt to match InWorld's package path against stored TravelTargetMap
	FString FailureWorldPath;
	if (InWorld && InWorld->GetOutermost())
	{
		FailureWorldPath = UWorld::RemovePIEPrefix(InWorld->GetOutermost()->GetPathName());
	}
	bool bWorldIsNull = (InWorld == nullptr);
	bool bMatchesOurTransaction = bWorldIsNull || DoesTravelFailureMatch(
		FailureWorldPath, TravelSourceMap.ToString(), TravelTargetMap.ToString());

	UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::HandleTravelFailure - MatchCheck: WorldPath=%s TargetMap=%s RequestId=%s bMatch=%d"),
		*FailureWorldPath, *TravelTargetMap.ToString(), *TravelRequestId.ToString(), bMatchesOurTransaction ? 1 : 0);

	// 3. Only clear state if the failure belongs to our tracked transaction
	if (!bMatchesOurTransaction)
	{
		UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::HandleTravelFailure - IGNORED (world/map mismatch). FailureWorld=%s ExpectedMap=%s"),
			*FailureWorldPath, *TravelTargetMap.ToString());
		return;
	}

	// 4. Clear the matching transaction
	if (TravelKind == EHSRTravelKind::Encounter)
	{
		UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::HandleTravelFailure - Clearing Encounter state (RequestId=%s, now clean, retry available)"),
			*PendingRequest.RequestId.ToString());
		ClearPending();
	}
	else if (TravelKind == EHSRTravelKind::Return)
	{
		if (!TravelCompletedEncounterId.IsNone())
		{
			ResolvedEncounterIds.Remove(TravelCompletedEncounterId);
		}
		UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::HandleTravelFailure - Clearing Return context (RequestId=%s, now clean, retry available)"),
			*TravelRequestId.ToString());
		ClearReturn();
	}

	// 5. Clear travel tracking (redundant with ClearPending/ClearReturn but explicit for safety)
	TravelKind = EHSRTravelKind::None;
	TravelRequestId = FGuid();
	TravelTargetMap = NAME_None;
	TravelSourceMap = NAME_None;
	TravelCompletedEncounterId = NAME_None;
	ClearTravelTimeout();
	UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::HandleTravelFailure - State clean. New requests can proceed."));
	return;
}


FHSRExplorationReturnResult UHSRBattleTransitionSubsystem::RequestTestReturn(const FHSREncounterRequest& FromConsumedRequest)
{
	FHSRBattleReturnContext BattleReturnContext;
	BattleReturnContext.RequestId = FromConsumedRequest.RequestId;
	BattleReturnContext.ExplorationMapPath = FromConsumedRequest.ExplorationMapPath;
	BattleReturnContext.ReturnTransform = FromConsumedRequest.ReturnTransform;
	FHSRBattleResult TestResult;
	TestResult.RequestId = BattleReturnContext.RequestId;
	TestResult.ReturnContext = BattleReturnContext;
	return RequestBattleReturn(TestResult);
}

FHSRExplorationReturnResult UHSRBattleTransitionSubsystem::RequestBattleReturn(const FHSRBattleResult& BattleResult)
{
	const FHSRExplorationReturnResult Validation = ValidateBattleReturn(BattleResult);
	if (Validation.ResultType != EHSREncounterReturnResultType::Success)
	{
		return Validation;
	}

	const FHSRBattleReturnContext& BattleReturnContext = BattleResult.ReturnContext;
	FHSRExplorationReturnContext ReturnCtx;
	ReturnCtx.RequestId = BattleReturnContext.RequestId;
	ReturnCtx.ExplorationMapPath = BattleReturnContext.ExplorationMapPath;
	UHSRMapSubsystem* MapSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRMapSubsystem>() : nullptr;
	if (!MapSubsystem || !MapSubsystem->ResolveMapIdByPackage(BattleReturnContext.ExplorationMapPath, ReturnCtx.ExplorationMapId))
	{
		return FHSRExplorationReturnResult::MakeFailure(EHSREncounterReturnResultType::InvalidReturnContext,
			FText::FromString(TEXT("Exploration map path is not registered with MapSubsystem.")));
	}
	ReturnCtx.ReturnTransform = BattleReturnContext.ReturnTransform;

	PendingReturnContext = ReturnCtx;
	bReturnPending = true;
	bReturnConsumed = false;

	TravelKind = EHSRTravelKind::Return;
	TravelRequestId = ReturnCtx.RequestId;
	TravelTargetMap = ReturnCtx.ExplorationMapPath;
	if (UWorld* World = GetWorld())
	{
		TravelSourceMap = FName(*UWorld::RemovePIEPrefix(World->GetOutermost()->GetPathName()));
	}
	StartTravelTimeout();
	TravelCompletedEncounterId = ShouldResolveEncounter(BattleResult.Outcome) ? BattleResult.EncounterId : NAME_None;
	if (!TravelCompletedEncounterId.IsNone())
	{
		ResolvedEncounterIds.Add(TravelCompletedEncounterId);
	}

	UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::RequestTestReturn - SUCCESS RequestId=%s ExplorationMap=%s (kind=Return)"),
		*ReturnCtx.RequestId.ToString(), *ReturnCtx.ExplorationMapPath.ToString());

	UGameplayStatics::OpenLevel(GetWorld(), ReturnCtx.ExplorationMapPath, true);
	UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::RequestTestReturn - Traveling back to %s"), *ReturnCtx.ExplorationMapPath.ToString());

	return FHSRExplorationReturnResult::MakeSuccess();
}

bool UHSRBattleTransitionSubsystem::ShouldResolveEncounter(const EHSRBattleOutcome Outcome)
{
	return Outcome == EHSRBattleOutcome::PlayerVictory;
}

bool UHSRBattleTransitionSubsystem::DoesTravelFailureMatch(const FString& FailureWorldPackage,
	const FString& SourcePackage, const FString& TargetPackage)
{
	if (FailureWorldPackage.IsEmpty())
	{
		return false;
	}
	const FString NormalizedFailure = UWorld::RemovePIEPrefix(FailureWorldPackage);
	return (!SourcePackage.IsEmpty() && NormalizedFailure == UWorld::RemovePIEPrefix(SourcePackage))
		|| (!TargetPackage.IsEmpty() && NormalizedFailure == UWorld::RemovePIEPrefix(TargetPackage));
}

void UHSRBattleTransitionSubsystem::StartTravelTimeout()
{
	ClearTravelTimeout();
	TravelTimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UHSRBattleTransitionSubsystem::HandleTravelTimeout), 5.0f);
}

void UHSRBattleTransitionSubsystem::ClearTravelTimeout()
{
	if (TravelTimeoutHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TravelTimeoutHandle);
		TravelTimeoutHandle.Reset();
	}
}

bool UHSRBattleTransitionSubsystem::HandleTravelTimeout(float)
{
	TravelTimeoutHandle.Reset();
	if (TravelKind == EHSRTravelKind::Encounter)
	{
		UE_LOG(LogTemp, Warning, TEXT("HSR Battle encounter travel timed out RequestId=%s; cleared for retry"), *TravelRequestId.ToString());
		ClearPending();
	}
	else if (TravelKind == EHSRTravelKind::Return)
	{
		UE_LOG(LogTemp, Warning, TEXT("HSR Battle return travel timed out RequestId=%s; cleared for retry"), *TravelRequestId.ToString());
		ClearReturn();
	}
	return false;
}

FHSRExplorationReturnResult UHSRBattleTransitionSubsystem::ValidateBattleReturn(const FHSRBattleResult& BattleResult) const
{
	const FHSRBattleReturnContext& BattleReturnContext = BattleResult.ReturnContext;
	// Validate BEFORE writing (must not pollute Pending)
	if (bReturnPending)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::RequestTestReturn - FAILED AlreadyPending"));
		return FHSRExplorationReturnResult::MakeFailure(
			EHSREncounterReturnResultType::AlreadyPending,
			FText::FromString(TEXT("A return is already pending.")));
	}

	if (BattleReturnContext.ExplorationMapPath.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::RequestTestReturn - FAILED InvalidReturnContext (no path)"));
		return FHSRExplorationReturnResult::MakeFailure(
			EHSREncounterReturnResultType::InvalidReturnContext,
			FText::FromString(TEXT("No exploration map path in return context.")));
	}

	// Also reject invalid RequestId
	if (!BattleReturnContext.RequestId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::RequestTestReturn - FAILED InvalidReturnContext (invalid RequestId)"));
		return FHSRExplorationReturnResult::MakeFailure(
			EHSREncounterReturnResultType::InvalidReturnContext,
			FText::FromString(TEXT("Invalid RequestId in return context.")));
	}

	if (!FPackageName::DoesPackageExist(BattleReturnContext.ExplorationMapPath.ToString()))
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::RequestBattleReturn - FAILED invalid map=%s"), *BattleReturnContext.ExplorationMapPath.ToString());
		return FHSRExplorationReturnResult::MakeFailure(EHSREncounterReturnResultType::InvalidReturnContext, FText::FromString(TEXT("Exploration map package does not exist.")));
	}
	UHSRMapSubsystem* MapSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRMapSubsystem>() : nullptr;
	FName ResolvedMapId;
	if (!MapSubsystem || MapSubsystem->HasPendingTravel()
		|| !MapSubsystem->ResolveMapIdByPackage(BattleReturnContext.ExplorationMapPath, ResolvedMapId))
	{
		return FHSRExplorationReturnResult::MakeFailure(EHSREncounterReturnResultType::InvalidReturnContext,
			FText::FromString(TEXT("MapSubsystem cannot authorize the exploration return.")));
	}

	// Check World availability BEFORE writing Pending
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::RequestTestReturn - FAILED: no World, rejecting"));
		return FHSRExplorationReturnResult::MakeFailure(
			EHSREncounterReturnResultType::InvalidReturnContext,
			FText::FromString(TEXT("Cannot resolve World for travel.")));
	}

	return FHSRExplorationReturnResult::MakeSuccess();
}

FHSRExplorationReturnResult UHSRBattleTransitionSubsystem::CommitReturnContext(APawn* PlayerPawn)
{
	if (!bReturnPending)
	{
		return FHSRExplorationReturnResult::MakeFailure(bReturnConsumed
			? EHSREncounterReturnResultType::AlreadyConsumed : EHSREncounterReturnResultType::NothingPending);
	}
	UHSRMapSubsystem* Maps = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRMapSubsystem>() : nullptr;
	if (!Maps || PendingReturnContext.ExplorationMapId.IsNone())
	{
		return FHSRExplorationReturnResult::MakeFailure(EHSREncounterReturnResultType::InvalidReturnContext);
	}
	const EHSRMapOperationResult PlacementResult = Maps->CommitBattleReturnLocation(
		PendingReturnContext.ExplorationMapId, PlayerPawn, PendingReturnContext.ReturnTransform);
	if (PlacementResult != EHSRMapOperationResult::Success)
	{
		return FHSRExplorationReturnResult::MakeFailure(EHSREncounterReturnResultType::InvalidReturnContext,
			FText::FromString(TEXT("Battle return placement has not committed.")));
	}

	FHSRExplorationReturnResult Result = FHSRExplorationReturnResult::MakeSuccess();
	Result.ConsumedContext = PendingReturnContext;
	UE_LOG(LogTemp, Log, TEXT("HSR Battle return committed RequestId=%s MapId=%s Location=%s"),
		*PendingReturnContext.RequestId.ToString(), *PendingReturnContext.ExplorationMapId.ToString(),
		*PendingReturnContext.ReturnTransform.GetLocation().ToString());
	PendingReturnContext = FHSRExplorationReturnContext();
	bReturnPending = false;
	bReturnConsumed = true;
	TravelKind = EHSRTravelKind::None;
	TravelRequestId = FGuid();
	TravelTargetMap = NAME_None;
	TravelSourceMap = NAME_None;
	TravelCompletedEncounterId = NAME_None;
	ClearTravelTimeout();
	return Result;
}

FHSRExplorationReturnResult UHSRBattleTransitionSubsystem::ConsumeReturnContext()
{
	if (!bReturnPending)
	{
		if (bReturnConsumed)
		{
			UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::ConsumeReturnContext - FAILED AlreadyConsumed (request=%s)"),
				*PendingReturnContext.RequestId.ToString());
			return FHSRExplorationReturnResult::MakeFailure(
				EHSREncounterReturnResultType::AlreadyConsumed,
				FText::FromString(TEXT("Return context has already been consumed.")));
		}
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::ConsumeReturnContext - FAILED NothingPending"));
		return FHSRExplorationReturnResult::MakeFailure(
			EHSREncounterReturnResultType::NothingPending,
			FText::FromString(TEXT("No pending return context.")));
	}

	// Capture the full DTO before clearing internal payload
	FHSRExplorationReturnContext Consumed = PendingReturnContext;
	PendingReturnContext = FHSRExplorationReturnContext();
	bReturnPending = false;
	bReturnConsumed = true;
	TravelKind = EHSRTravelKind::None;
	TravelRequestId = FGuid();
	TravelTargetMap = NAME_None;
	TravelSourceMap = NAME_None;
	TravelCompletedEncounterId = NAME_None;
	ClearTravelTimeout();

	UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::ConsumeReturnContext - SUCCESS RequestId=%s ReturnLoc=%s"),
		*Consumed.RequestId.ToString(), *Consumed.ReturnTransform.GetLocation().ToString());

	// Return the full consumed context in the Result so consumer does NOT re-read from Subsystem
	FHSRExplorationReturnResult Result = FHSRExplorationReturnResult::MakeSuccess();
	Result.ConsumedContext = Consumed;
	return Result;
}


