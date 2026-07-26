#include "HSRMapSubsystem.h"

#include "../Data/Definitions/HSRMapDefinition.h"
#include "../Data/Definitions/HSRTeleportDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "Misc/PackageName.h"

void UHSRMapSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (GEngine)
	{
		GEngine->OnTravelFailure().AddUObject(this, &UHSRMapSubsystem::HandleTravelFailure);
	}
}

void UHSRMapSubsystem::Deinitialize()
{
	if (GEngine)
	{
		GEngine->OnTravelFailure().RemoveAll(this);
	}
	PendingRequest = FHSRTeleportRequest();
	bTravelPending = false;
	Super::Deinitialize();
}

EHSRMapOperationResult UHSRMapSubsystem::RegisterMapDefinition(const UHSRMapDefinition& Definition)
{
	if (Definition.MapId.IsNone() || Definition.World.IsNull() || Definition.RegionId.IsNone() || Definition.DefaultArrivalId.IsNone())
	{
		return EHSRMapOperationResult::InvalidDefinition;
	}

	const FRegisteredMap Candidate{Definition.World, Definition.RegionId, Definition.DefaultArrivalId};
	if (const FRegisteredMap* Existing = Maps.Find(Definition.MapId))
	{
		const bool bSame = Existing->World == Candidate.World
			&& Existing->RegionId == Candidate.RegionId
			&& Existing->DefaultArrivalId == Candidate.DefaultArrivalId;
		return bSame ? EHSRMapOperationResult::NoOp : EHSRMapOperationResult::DuplicateId;
	}

	Maps.Add(Definition.MapId, Candidate);
	return EHSRMapOperationResult::Success;
}

EHSRMapOperationResult UHSRMapSubsystem::RegisterTeleportDefinition(const UHSRTeleportDefinition& Definition)
{
	if (Definition.TeleportId.IsNone() || Definition.SourceMapId.IsNone() || Definition.DestinationMapId.IsNone()
		|| Definition.DestinationArrivalId.IsNone() || Definition.SourceMapId == Definition.DestinationMapId)
	{
		return EHSRMapOperationResult::InvalidDefinition;
	}
	if (!Maps.Contains(Definition.SourceMapId) || !Maps.Contains(Definition.DestinationMapId))
	{
		return EHSRMapOperationResult::UnknownMap;
	}

	const FRegisteredTeleport Candidate{Definition.SourceMapId, Definition.DestinationMapId,
		Definition.DestinationArrivalId, Definition.bInitiallyUnlocked};
	if (const FRegisteredTeleport* Existing = Teleports.Find(Definition.TeleportId))
	{
		const bool bSame = Existing->SourceMapId == Candidate.SourceMapId
			&& Existing->DestinationMapId == Candidate.DestinationMapId
			&& Existing->DestinationArrivalId == Candidate.DestinationArrivalId
			&& Existing->bInitiallyUnlocked == Candidate.bInitiallyUnlocked;
		return bSame ? EHSRMapOperationResult::NoOp : EHSRMapOperationResult::DuplicateId;
	}

	Teleports.Add(Definition.TeleportId, Candidate);
	if (Definition.bInitiallyUnlocked)
	{
		Snapshot.UnlockedTeleportIds.Add(Definition.TeleportId);
		CommitStateChange();
	}
	return EHSRMapOperationResult::Success;
}

EHSRMapOperationResult UHSRMapSubsystem::RegisterMapAsset(UHSRMapDefinition* Definition)
{
	return Definition ? RegisterMapDefinition(*Definition) : EHSRMapOperationResult::InvalidDefinition;
}

EHSRMapOperationResult UHSRMapSubsystem::RegisterTeleportAsset(UHSRTeleportDefinition* Definition)
{
	return Definition ? RegisterTeleportDefinition(*Definition) : EHSRMapOperationResult::InvalidDefinition;
}

EHSRMapOperationResult UHSRMapSubsystem::SetCurrentLocation(const FName MapId, FName ArrivalId)
{
	const FRegisteredMap* Map = Maps.Find(MapId);
	if (!Map)
	{
		return EHSRMapOperationResult::UnknownMap;
	}
	if (ArrivalId.IsNone())
	{
		ArrivalId = Map->DefaultArrivalId;
	}
	if (ArrivalId.IsNone())
	{
		return EHSRMapOperationResult::InvalidDefinition;
	}
	const FHSRMapLocation Candidate{MapId, ArrivalId};
	if (Snapshot.CurrentLocation == Candidate)
	{
		return EHSRMapOperationResult::NoOp;
	}
	Snapshot.CurrentLocation = Candidate;
	CommitStateChange();
	return EHSRMapOperationResult::Success;
}

EHSRMapOperationResult UHSRMapSubsystem::UnlockRegion(const FName RegionId)
{
	if (RegionId.IsNone())
	{
		return EHSRMapOperationResult::InvalidDefinition;
	}
	bool bKnown = false;
	for (const TPair<FName, FRegisteredMap>& Pair : Maps)
	{
		if (Pair.Value.RegionId == RegionId)
		{
			bKnown = true;
			break;
		}
	}
	if (!bKnown)
	{
		return EHSRMapOperationResult::UnknownMap;
	}
	if (Snapshot.UnlockedRegionIds.Contains(RegionId))
	{
		return EHSRMapOperationResult::NoOp;
	}
	Snapshot.UnlockedRegionIds.Add(RegionId);
	CommitStateChange();
	return EHSRMapOperationResult::Success;
}

EHSRMapOperationResult UHSRMapSubsystem::UnlockTeleport(const FName TeleportId)
{
	if (!Teleports.Contains(TeleportId))
	{
		return EHSRMapOperationResult::UnknownTeleport;
	}
	if (Snapshot.UnlockedTeleportIds.Contains(TeleportId))
	{
		return EHSRMapOperationResult::NoOp;
	}
	Snapshot.UnlockedTeleportIds.Add(TeleportId);
	CommitStateChange();
	return EHSRMapOperationResult::Success;
}

EHSRMapOperationResult UHSRMapSubsystem::BuildTeleportRequest(const FName TeleportId, FHSRTeleportRequest& OutRequest) const
{
	const FRegisteredTeleport* Teleport = Teleports.Find(TeleportId);
	if (!Teleport)
	{
		return EHSRMapOperationResult::UnknownTeleport;
	}
	if (!Snapshot.UnlockedTeleportIds.Contains(TeleportId))
	{
		return EHSRMapOperationResult::Locked;
	}
	const FRegisteredMap* DestinationMap = Maps.Find(Teleport->DestinationMapId);
	if (!DestinationMap || !Snapshot.UnlockedRegionIds.Contains(DestinationMap->RegionId))
	{
		return EHSRMapOperationResult::Locked;
	}
	if (Snapshot.CurrentLocation.MapId != Teleport->SourceMapId)
	{
		return EHSRMapOperationResult::InvalidSource;
	}

	FHSRTeleportRequest Candidate;
	Candidate.RequestId = FGuid::NewGuid();
	Candidate.TeleportId = TeleportId;
	Candidate.Source = Snapshot.CurrentLocation;
	Candidate.Destination = {Teleport->DestinationMapId, Teleport->DestinationArrivalId};
	OutRequest = MoveTemp(Candidate);
	return EHSRMapOperationResult::Success;
}

EHSRMapOperationResult UHSRMapSubsystem::RequestTeleportTravel(const FName TeleportId)
{
	const UHSRBattleTransitionSubsystem* BattleTravel = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UHSRBattleTransitionSubsystem>() : nullptr;
	if (bTravelPending || (BattleTravel && BattleTravel->HasReturnPending()))
	{
		return EHSRMapOperationResult::AlreadyPending;
	}

	FHSRTeleportRequest Candidate;
	const EHSRMapOperationResult BuildResult = BuildTeleportRequest(TeleportId, Candidate);
	if (BuildResult != EHSRMapOperationResult::Success)
	{
		return BuildResult;
	}
	const FRegisteredMap* DestinationMap = Maps.Find(Candidate.Destination.MapId);
	if (!DestinationMap || DestinationMap->World.IsNull())
	{
		return EHSRMapOperationResult::UnknownMap;
	}
	const FString PackageName = DestinationMap->World.GetLongPackageName();
	if (PackageName.IsEmpty() || !FPackageName::DoesPackageExist(PackageName))
	{
		return EHSRMapOperationResult::InvalidMapPackage;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return EHSRMapOperationResult::InvalidWorld;
	}

	PendingRequest = Candidate;
	bTravelPending = true;
	World->GetTimerManager().SetTimer(TravelTimeoutTimer, this, &UHSRMapSubsystem::HandleTravelTimeout, 5.0f, false);
	UE_LOG(LogTemp, Log, TEXT("HSR Map travel issued RequestId=%s Teleport=%s Source=%s Destination=%s Arrival=%s"),
		*Candidate.RequestId.ToString(), *Candidate.TeleportId.ToString(), *Candidate.Source.MapId.ToString(),
		*Candidate.Destination.MapId.ToString(), *Candidate.Destination.ArrivalId.ToString());
	UGameplayStatics::OpenLevel(World, FName(*PackageName), true);
	return EHSRMapOperationResult::Success;
}

EHSRMapOperationResult UHSRMapSubsystem::CommitPendingArrival(const FName DestinationMapId, const FName ArrivalId,
	APawn* Pawn, const FTransform& ArrivalTransform)
{
	UWorld* World = GetWorld();
	const FString LoadedPackage = World && World->GetOutermost()
		? UWorld::RemovePIEPrefix(World->GetOutermost()->GetPathName()) : FString();
	const EHSRMapOperationResult ContextResult = ValidatePendingArrivalContext(DestinationMapId, ArrivalId, LoadedPackage, 1);
	if (ContextResult != EHSRMapOperationResult::Success)
	{
		return ContextResult;
	}
	if (!Pawn)
	{
		return EHSRMapOperationResult::PawnUnavailable;
	}
	if (ArrivalTransform.ContainsNaN())
	{
		return EHSRMapOperationResult::PlacementFailed;
	}
	if (!Pawn->SetActorTransform(ArrivalTransform, false, nullptr, ETeleportType::TeleportPhysics))
	{
		return EHSRMapOperationResult::PlacementFailed;
	}

	Snapshot.CurrentLocation = PendingRequest.Destination;
	Snapshot.CurrentLocation.WorldTransform = ArrivalTransform;
	const FGuid CompletedRequestId = PendingRequest.RequestId;
	PendingRequest = FHSRTeleportRequest();
	bTravelPending = false;
	if (World)
	{
		World->GetTimerManager().ClearTimer(TravelTimeoutTimer);
	}
	CommitStateChange();
	UE_LOG(LogTemp, Log, TEXT("HSR Map arrival committed RequestId=%s Map=%s Arrival=%s Location=%s"),
		*CompletedRequestId.ToString(), *DestinationMapId.ToString(), *ArrivalId.ToString(),
		*ArrivalTransform.GetLocation().ToString());
	return EHSRMapOperationResult::Success;
}

EHSRMapOperationResult UHSRMapSubsystem::CommitBattleReturnLocation(const FName MapId, APawn* Pawn, const FTransform& ReturnTransform)
{
	const FRegisteredMap* Map = Maps.Find(MapId);
	if (!Map)
	{
		return EHSRMapOperationResult::UnknownMap;
	}
	UWorld* World = GetWorld();
	const FString LoadedPackage = World && World->GetOutermost()
		? UWorld::RemovePIEPrefix(World->GetOutermost()->GetPathName()) : FString();
	if (LoadedPackage != Map->World.GetLongPackageName())
	{
		return EHSRMapOperationResult::InvalidWorld;
	}
	if (!Pawn)
	{
		return EHSRMapOperationResult::PawnUnavailable;
	}
	if (ReturnTransform.ContainsNaN() || !Pawn->SetActorTransform(ReturnTransform, false, nullptr, ETeleportType::TeleportPhysics))
	{
		return EHSRMapOperationResult::PlacementFailed;
	}
	Snapshot.CurrentLocation.MapId = MapId;
	Snapshot.CurrentLocation.ArrivalId = NAME_None;
	Snapshot.CurrentLocation.WorldTransform = ReturnTransform;
	CommitStateChange();
	UE_LOG(LogTemp, Log, TEXT("HSR Battle map location committed Map=%s Location=%s"),
		*MapId.ToString(), *ReturnTransform.GetLocation().ToString());
	return EHSRMapOperationResult::Success;
}

bool UHSRMapSubsystem::ResolveMapIdByPackage(const FName MapPackage, FName& OutMapId) const
{
	const FString Normalized = UWorld::RemovePIEPrefix(MapPackage.ToString());
	for (const TPair<FName, FRegisteredMap>& Pair : Maps)
	{
		if (Pair.Value.World.GetLongPackageName() == Normalized)
		{
			OutMapId = Pair.Key;
			return true;
		}
	}
	OutMapId = NAME_None;
	return false;
}

EHSRMapOperationResult UHSRMapSubsystem::ValidatePendingArrivalContext(const FName DestinationMapId, const FName ArrivalId,
	const FString& LoadedWorldPackage, const int32 MatchingArrivalCount) const
{
	if (!bTravelPending)
	{
		return EHSRMapOperationResult::NothingPending;
	}
	if (PendingRequest.Destination.MapId != DestinationMapId || PendingRequest.Destination.ArrivalId != ArrivalId)
	{
		return EHSRMapOperationResult::WrongDestination;
	}
	const FRegisteredMap* DestinationMap = Maps.Find(DestinationMapId);
	const FString ExpectedPackage = DestinationMap ? DestinationMap->World.GetLongPackageName() : FString();
	const FString NormalizedLoadedPackage = UWorld::RemovePIEPrefix(LoadedWorldPackage);
	if (ExpectedPackage.IsEmpty() || NormalizedLoadedPackage.IsEmpty() || NormalizedLoadedPackage != ExpectedPackage)
	{
		return EHSRMapOperationResult::InvalidWorld;
	}
	if (MatchingArrivalCount == 0)
	{
		return EHSRMapOperationResult::ArrivalNotFound;
	}
	if (MatchingArrivalCount != 1)
	{
		return EHSRMapOperationResult::ArrivalAmbiguous;
	}
	return EHSRMapOperationResult::Success;
}

EHSRMapOperationResult UHSRMapSubsystem::CancelPendingTravel(const FGuid& RequestId)
{
	if (!bTravelPending)
	{
		return EHSRMapOperationResult::NothingPending;
	}
	if (!RequestId.IsValid() || PendingRequest.RequestId != RequestId)
	{
		return EHSRMapOperationResult::RequestMismatch;
	}
	PendingRequest = FHSRTeleportRequest();
	bTravelPending = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TravelTimeoutTimer);
	}
	return EHSRMapOperationResult::Success;
}

bool UHSRMapSubsystem::GetPendingRequest(FHSRTeleportRequest& OutRequest) const
{
	if (!bTravelPending)
	{
		return false;
	}
	OutRequest = PendingRequest;
	return true;
}

void UHSRMapSubsystem::HandleTravelFailure(UWorld* InWorld, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (!bTravelPending)
	{
		return;
	}
	const FRegisteredMap* DestinationMap = Maps.Find(PendingRequest.Destination.MapId);
	const FString ExpectedPackage = DestinationMap ? DestinationMap->World.GetLongPackageName() : FString();
	const FString FailurePackage = InWorld && InWorld->GetOutermost()
		? UWorld::RemovePIEPrefix(InWorld->GetOutermost()->GetPathName()) : FString();
	if (InWorld && FailurePackage != ExpectedPackage)
	{
		UE_LOG(LogTemp, Log, TEXT("HSR Map ignored unrelated travel failure World=%s Expected=%s"), *FailurePackage, *ExpectedPackage);
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("HSR Map travel failed RequestId=%s Type=%d Error=%s; pending cleared for retry"),
		*PendingRequest.RequestId.ToString(), static_cast<int32>(FailureType), *ErrorString);
	CancelPendingTravel(PendingRequest.RequestId);
}

void UHSRMapSubsystem::HandleTravelTimeout()
{
	if (!bTravelPending)
	{
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("HSR Map travel timed out RequestId=%s; pending cleared for retry"), *PendingRequest.RequestId.ToString());
	CancelPendingTravel(PendingRequest.RequestId);
}

#if WITH_DEV_AUTOMATION_TESTS
EHSRMapOperationResult UHSRMapSubsystem::StageTeleportForAutomation(const FName TeleportId)
{
	if (bTravelPending)
	{
		return EHSRMapOperationResult::AlreadyPending;
	}
	FHSRTeleportRequest Candidate;
	const EHSRMapOperationResult Result = BuildTeleportRequest(TeleportId, Candidate);
	if (Result == EHSRMapOperationResult::Success)
	{
		PendingRequest = Candidate;
		bTravelPending = true;
	}
	return Result;
}

bool UHSRMapSubsystem::DoesRegisteredMapPackageExistForAutomation(const FName MapId) const
{
	const FRegisteredMap* Map = Maps.Find(MapId);
	return Map && !Map->World.IsNull() && FPackageName::DoesPackageExist(Map->World.GetLongPackageName());
}
#endif

bool UHSRMapSubsystem::IsRegionUnlocked(const FName RegionId) const
{
	return Snapshot.UnlockedRegionIds.Contains(RegionId);
}

bool UHSRMapSubsystem::IsTeleportUnlocked(const FName TeleportId) const
{
	return Snapshot.UnlockedTeleportIds.Contains(TeleportId);
}

void UHSRMapSubsystem::CommitStateChange()
{
	++Snapshot.Revision;
	MapStateChanged.Broadcast(Snapshot);
}
