#include "HSRMapSubsystem.h"

#include "../Data/Definitions/HSRMapDefinition.h"
#include "../Data/Definitions/HSRTeleportDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "../UI/HSRUIManagerSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Misc/PackageName.h"

namespace
{
bool IsValidMapTransform(const FTransform& Transform)
{
	const FVector Location = Transform.GetLocation();
	const FVector Scale = Transform.GetScale3D();
	const FQuat Rotation = Transform.GetRotation();
	return !Transform.ContainsNaN()
		&& FMath::IsFinite(Location.X) && FMath::IsFinite(Location.Y) && FMath::IsFinite(Location.Z)
		&& FMath::IsFinite(Scale.X) && FMath::IsFinite(Scale.Y) && FMath::IsFinite(Scale.Z)
		&& FMath::IsFinite(Rotation.X) && FMath::IsFinite(Rotation.Y)
		&& FMath::IsFinite(Rotation.Z) && FMath::IsFinite(Rotation.W);
}

bool AddUniqueNames(const TArray<FName>& Source, TSet<FName>& Target)
{
	for (const FName Id : Source)
	{
		if (Id.IsNone() || Target.Contains(Id))
		{
			return false;
		}
		Target.Add(Id);
	}
	return true;
}

bool NameSetsEqual(const TSet<FName>& Left, const TSet<FName>& Right)
{
	if (Left.Num() != Right.Num())
	{
		return false;
	}
	for (const FName Id : Left)
	{
		if (!Right.Contains(Id))
		{
			return false;
		}
	}
	return true;
}
}

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

	const FRegisteredMap Candidate{Definition.World, Definition.RegionId, Definition.DefaultArrivalId, Definition.DisplayName};
	if (const FRegisteredMap* Existing = Maps.Find(Definition.MapId))
	{
		const bool bSame = Existing->World == Candidate.World
			&& Existing->RegionId == Candidate.RegionId
			&& Existing->DefaultArrivalId == Candidate.DefaultArrivalId
			&& Existing->DisplayName.EqualTo(Candidate.DisplayName);
		return bSame ? EHSRMapOperationResult::NoOp : EHSRMapOperationResult::DuplicateId;
	}

	Maps.Add(Definition.MapId, Candidate);
	return EHSRMapOperationResult::Success;
}

EHSRMapOperationResult UHSRMapSubsystem::RegisterTeleportDefinition(const UHSRTeleportDefinition& Definition)
{
	if (Definition.TeleportId == TEXT("Save.Restore"))
	{
		return EHSRMapOperationResult::InvalidDefinition;
	}
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

bool UHSRMapSubsystem::HasRegionDefinition(const FName RegionId) const
{
	for (const TPair<FName, FRegisteredMap>& Pair : Maps)
	{
		if (Pair.Value.RegionId == RegionId)
		{
			return true;
		}
	}
	return false;
}

FText UHSRMapSubsystem::GetMapDisplayName(const FName MapId) const
{
	const FRegisteredMap* Map = Maps.Find(MapId);
	if (!Map || Map->DisplayName.IsEmpty())
	{
		return FText::FromName(MapId);
	}
	return Map->DisplayName;
}

void UHSRMapSubsystem::GetAvailableTeleports(TArray<FHSRTeleportProjection>& OutTeleports) const
{
	OutTeleports.Reset();
	const FName CurrentMapId = Snapshot.CurrentLocation.MapId;
	for (const TPair<FName, FRegisteredTeleport>& Pair : Teleports)
	{
		const FRegisteredTeleport& Teleport = Pair.Value;
		FHSRTeleportProjection Projection;
		Projection.TeleportId = Pair.Key;
		Projection.SourceMapId = Teleport.SourceMapId;
		Projection.DestinationMapId = Teleport.DestinationMapId;
		Projection.DestinationDisplayName = GetMapDisplayName(Teleport.DestinationMapId);

		const FRegisteredMap* DestinationMap = Maps.Find(Teleport.DestinationMapId);
		const bool bFromCurrentMap = (CurrentMapId == Teleport.SourceMapId);
		const bool bTeleportUnlocked = Snapshot.UnlockedTeleportIds.Contains(Pair.Key);
		const bool bRegionUnlocked = DestinationMap && Snapshot.UnlockedRegionIds.Contains(DestinationMap->RegionId);
		Projection.bUsable = bFromCurrentMap && bTeleportUnlocked && bRegionUnlocked;

		OutTeleports.Add(Projection);
	}
}

int32 UHSRMapSubsystem::GetReachableTeleportCount() const
{
	TArray<FHSRTeleportProjection> All;
	GetAvailableTeleports(All);
	int32 Count = 0;
	for (const FHSRTeleportProjection& Projection : All)
	{
		if (Projection.bUsable)
		{
			++Count;
		}
	}
	return Count;
}

bool UHSRMapSubsystem::GetReachableTeleport(const int32 Index, FHSRTeleportProjection& OutTeleport) const
{
	TArray<FHSRTeleportProjection> All;
	GetAvailableTeleports(All);
	int32 Seen = 0;
	for (const FHSRTeleportProjection& Projection : All)
	{
		if (!Projection.bUsable)
		{
			continue;
		}
		if (Seen == Index)
		{
			OutTeleport = Projection;
			return true;
		}
		++Seen;
	}
	return false;
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

EHSRMapOperationResult UHSRMapSubsystem::SetExplorationFlag(const FName FlagId)
{
	if (FlagId.IsNone())
	{
		return EHSRMapOperationResult::InvalidDefinition;
	}
	if (Snapshot.ExplorationFlags.Contains(FlagId))
	{
		return EHSRMapOperationResult::NoOp;
	}
	Snapshot.ExplorationFlags.Add(FlagId);
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
	if (ULocalPlayer* LocalPlayer = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr)
	{
		if (UHSRUIManagerSubsystem* UIManager = LocalPlayer->GetSubsystem<UHSRUIManagerSubsystem>())
		{
			if (UIManager->PrepareExplorationTravel() != EHSRUIScreenResult::Success)
			{
				return EHSRMapOperationResult::UIPreparationFailed;
			}
		}
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

EHSRMapOperationResult UHSRMapSubsystem::RequestRestoreTravel()
{
	return RequestRestoreTravel(Snapshot);
}

EHSRMapOperationResult UHSRMapSubsystem::RequestRestoreTravel(const FHSRMapRuntimeSnapshot& RestoreTarget)
{
	if (RestoreTarget.CurrentLocation.MapId.IsNone())
	{
		return EHSRMapOperationResult::NoOp;
	}
	if (Snapshot.CurrentLocation.MapId == RestoreTarget.CurrentLocation.MapId)
	{
		return EHSRMapOperationResult::NoOp;
	}
	UWorld* World = GetWorld();
	if (!World || bTravelPending) return bTravelPending ? EHSRMapOperationResult::AlreadyPending : EHSRMapOperationResult::InvalidWorld;
	const FRegisteredMap* Destination = Maps.Find(RestoreTarget.CurrentLocation.MapId);
	if (!Destination || Destination->World.IsNull()) return EHSRMapOperationResult::UnknownMap;
	const FString Package = Destination->World.GetLongPackageName();
	if (Package.IsEmpty() || !FPackageName::DoesPackageExist(Package)) return EHSRMapOperationResult::InvalidMapPackage;
	FName Loaded; if (World->GetOutermost() && ResolveMapIdByPackage(World->GetOutermost()->GetFName(), Loaded) && Loaded == RestoreTarget.CurrentLocation.MapId) return EHSRMapOperationResult::NoOp;
	if (ULocalPlayer* LP = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr) if (UHSRUIManagerSubsystem* UI = LP->GetSubsystem<UHSRUIManagerSubsystem>()) if (UI->PrepareExplorationTravel() != EHSRUIScreenResult::Success) return EHSRMapOperationResult::UIPreparationFailed;
	PendingRequest.RequestId=FGuid::NewGuid(); PendingRequest.TeleportId=TEXT("Save.Restore"); PendingRequest.Source.MapId=Loaded; PendingRequest.Destination=RestoreTarget.CurrentLocation; bTravelPending=true;
	World->GetTimerManager().SetTimer(TravelTimeoutTimer,this,&UHSRMapSubsystem::HandleTravelTimeout,5.0f,false);
	UE_LOG(LogTemp,Log,TEXT("HSR Map restore travel issued RequestId=%s Destination=%s"),*PendingRequest.RequestId.ToString(),*PendingRequest.Destination.MapId.ToString());
	UGameplayStatics::OpenLevel(World,FName(*Package),true); return EHSRMapOperationResult::Success;
}

EHSRMapOperationResult UHSRMapSubsystem::ApplyRestoreLocation(const FHSRMapRuntimeSnapshot& RestoreTarget)
{
	if (RestoreTarget.CurrentLocation.MapId.IsNone()) return EHSRMapOperationResult::NoOp;
	UWorld* World = GetWorld();
	if (!World) return EHSRMapOperationResult::NoOp;
	FName LoadedMapId;
	if (!World->GetOutermost() || !ResolveMapIdByPackage(World->GetOutermost()->GetFName(), LoadedMapId)
		|| LoadedMapId != RestoreTarget.CurrentLocation.MapId) return EHSRMapOperationResult::InvalidWorld;
	APlayerController* Controller = World->GetFirstPlayerController();
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Pawn) return EHSRMapOperationResult::PawnUnavailable;
	if (RestoreTarget.CurrentLocation.WorldTransform.ContainsNaN()
		|| !Pawn->SetActorTransform(RestoreTarget.CurrentLocation.WorldTransform, false, nullptr, ETeleportType::TeleportPhysics))
	{
		return EHSRMapOperationResult::PlacementFailed;
	}
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
	return CommitPendingArrivalValidated(DestinationMapId, ArrivalId, Pawn, ArrivalTransform);
}

EHSRMapOperationResult UHSRMapSubsystem::CommitPendingRestoreArrival(const FName DestinationMapId, APawn* Pawn,
	const FTransform& SavedTransform)
{
	if (!bTravelPending)
	{
		return EHSRMapOperationResult::NothingPending;
	}
	if (PendingRequest.TeleportId != TEXT("Save.Restore") || PendingRequest.Destination.MapId != DestinationMapId)
	{
		return EHSRMapOperationResult::WrongDestination;
	}
	UWorld* World = GetWorld();
	const FString LoadedPackage = World && World->GetOutermost()
		? UWorld::RemovePIEPrefix(World->GetOutermost()->GetPathName()) : FString();
	const FRegisteredMap* DestinationMap = Maps.Find(DestinationMapId);
	if (!DestinationMap || LoadedPackage.IsEmpty() || LoadedPackage != DestinationMap->World.GetLongPackageName())
	{
		return EHSRMapOperationResult::InvalidWorld;
	}
	return CommitPendingArrivalValidated(DestinationMapId, PendingRequest.Destination.ArrivalId, Pawn, SavedTransform);
}

EHSRMapOperationResult UHSRMapSubsystem::CommitPendingArrivalValidated(const FName DestinationMapId, const FName ArrivalId,
	APawn* Pawn, const FTransform& ArrivalTransform)
{
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
	LastCommittedRequestId = CompletedRequestId;
	PendingRequest = FHSRTeleportRequest();
	bTravelPending = false;
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(TravelTimeoutTimer);
	}
	CommitStateChange();
	PublishArrivalCommitted(DestinationMapId, ArrivalId, EHSRMapArrivalCommitKind::OrdinaryTravel);
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
	PublishArrivalCommitted(MapId, NAME_None, EHSRMapArrivalCommitKind::BattleReturn);
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
	const FHSRTeleportRequest CanceledRequest = PendingRequest;
	PendingRequest = FHSRTeleportRequest();
	bTravelPending = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TravelTimeoutTimer);
	}
	if (CanceledRequest.TeleportId == TEXT("Save.Restore"))
	{
		RestoreTravelFailed.Broadcast(CanceledRequest.RequestId);
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

void UHSRMapSubsystem::ExportSaveData(FHSRMapSaveData& OutData) const
{
	OutData = FHSRMapSaveData();
	OutData.CurrentLocation = Snapshot.CurrentLocation;
	OutData.UnlockedRegionIds = Snapshot.UnlockedRegionIds.Array();
	OutData.UnlockedTeleportIds = Snapshot.UnlockedTeleportIds.Array();
	OutData.ExplorationFlags = Snapshot.ExplorationFlags.Array();
	OutData.UnlockedRegionIds.Sort(FNameLexicalLess());
	OutData.UnlockedTeleportIds.Sort(FNameLexicalLess());
	OutData.ExplorationFlags.Sort(FNameLexicalLess());
	OutData.Revision = Snapshot.Revision;
}

bool UHSRMapSubsystem::PrepareRestore(const FHSRMapSaveData& Data, FHSRMapRuntimeSnapshot& OutCandidate) const
{
	const UHSRBattleTransitionSubsystem* BattleTravel = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UHSRBattleTransitionSubsystem>() : nullptr;
	if (!CanRestoreState(bTravelPending, BattleTravel && BattleTravel->HasReturnPending())
		|| Data.Revision < 0 || !IsValidMapTransform(Data.CurrentLocation.WorldTransform))
	{
		return false;
	}
	FHSRMapRuntimeSnapshot Candidate;
	Candidate.CurrentLocation = Data.CurrentLocation;
	Candidate.Revision = Data.Revision;
	if (!AddUniqueNames(Data.UnlockedRegionIds, Candidate.UnlockedRegionIds)
		|| !AddUniqueNames(Data.UnlockedTeleportIds, Candidate.UnlockedTeleportIds)
		|| !AddUniqueNames(Data.ExplorationFlags, Candidate.ExplorationFlags))
	{
		return false;
	}
	if (Candidate.CurrentLocation.MapId.IsNone())
	{
		if (!Candidate.CurrentLocation.ArrivalId.IsNone() || !Candidate.UnlockedRegionIds.IsEmpty()
			|| !Candidate.UnlockedTeleportIds.IsEmpty() || !Candidate.ExplorationFlags.IsEmpty())
		{
			return false;
		}
		OutCandidate = MoveTemp(Candidate);
		return true;
	}
	if (!Maps.Contains(Candidate.CurrentLocation.MapId))
	{
		return false;
	}
	for (const FName RegionId : Candidate.UnlockedRegionIds)
	{
		bool bKnown = false;
		for (const TPair<FName, FRegisteredMap>& Pair : Maps)
		{
			bKnown |= Pair.Value.RegionId == RegionId;
		}
		if (!bKnown)
		{
			return false;
		}
	}
	for (const FName TeleportId : Candidate.UnlockedTeleportIds)
	{
		if (!Teleports.Contains(TeleportId))
		{
			return false;
		}
	}
	OutCandidate = MoveTemp(Candidate);
	return true;
}

bool UHSRMapSubsystem::IsRestoreDifferent(const FHSRMapRuntimeSnapshot& Candidate) const
{
	return !(Snapshot.CurrentLocation == Candidate.CurrentLocation)
		|| !NameSetsEqual(Snapshot.UnlockedRegionIds, Candidate.UnlockedRegionIds)
		|| !NameSetsEqual(Snapshot.UnlockedTeleportIds, Candidate.UnlockedTeleportIds)
		|| !NameSetsEqual(Snapshot.ExplorationFlags, Candidate.ExplorationFlags)
		|| Snapshot.Revision != Candidate.Revision;
}

void UHSRMapSubsystem::CommitRestore(FHSRMapRuntimeSnapshot&& Candidate, const bool bNotify)
{
	Snapshot = MoveTemp(Candidate);
	if (bNotify)
	{
		MapStateChanged.Broadcast(Snapshot);
	}
}

void UHSRMapSubsystem::CommitStateChange()
{
	++Snapshot.Revision;
	MapStateChanged.Broadcast(Snapshot);
}

void UHSRMapSubsystem::PublishArrivalCommitted(const FName MapId, const FName ArrivalId,
	const EHSRMapArrivalCommitKind Kind)
{
	FHSRMapArrivalCommitInfo Info;
	Info.CommitGeneration = ++ArrivalCommitGeneration;
	Info.MapId = MapId;
	Info.ArrivalId = ArrivalId;
	Info.Kind = Kind;
	ArrivalCommitted.Broadcast(Info);
	UE_LOG(LogTemp, Log, TEXT("HSR Map ArrivalCommitted Generation=%lld Map=%s Arrival=%s Kind=%d"),
		Info.CommitGeneration, *MapId.ToString(), *ArrivalId.ToString(), static_cast<int32>(Kind));
}
