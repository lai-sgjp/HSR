#include "HSRMapViewModel.h"

#include "../Map/HSRMapSubsystem.h"

void UHSRMapViewModel::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

void UHSRMapViewModel::Initialize(UHSRMapSubsystem* InMaps)
{
	Shutdown();
	if (!InMaps)
	{
		return;
	}
	Maps = InMaps;
	MapStateChangedHandle = InMaps->OnMapStateChanged().AddUObject(this, &ThisClass::HandleMapStateChanged);
	HandleMapStateChanged(InMaps->GetSnapshot());
}

void UHSRMapViewModel::Shutdown()
{
	if (Maps.IsValid())
	{
		Maps->OnMapStateChanged().Remove(MapStateChangedHandle);
	}
	Maps.Reset();
	MapStateChangedHandle.Reset();
	bHasSnapshot = false;
	Snapshot = FHSRMapRuntimeSnapshot();
}

bool UHSRMapViewModel::GetSnapshot(FHSRMapRuntimeSnapshot& OutSnapshot) const
{
	if (!bHasSnapshot)
	{
		return false;
	}
	OutSnapshot = Snapshot;
	return true;
}

EHSRMapOperationResult UHSRMapViewModel::RequestTeleport(const FName TeleportId)
{
	return Maps.IsValid() ? Maps->RequestTeleportTravel(TeleportId) : EHSRMapOperationResult::InvalidWorld;
}

FText UHSRMapViewModel::GetMapDisplayName(const FName MapId) const
{
	return Maps.IsValid() ? Maps->GetMapDisplayName(MapId) : FText::FromName(MapId);
}

void UHSRMapViewModel::GetAvailableTeleports(TArray<FHSRTeleportProjection>& OutTeleports) const
{
	OutTeleports.Reset();
	if (Maps.IsValid())
	{
		Maps->GetAvailableTeleports(OutTeleports);
	}
}

int32 UHSRMapViewModel::GetReachableTeleportCount() const
{
	return Maps.IsValid() ? Maps->GetReachableTeleportCount() : 0;
}

bool UHSRMapViewModel::GetReachableTeleport(const int32 Index, FHSRTeleportProjection& OutTeleport) const
{
	return Maps.IsValid() && Maps->GetReachableTeleport(Index, OutTeleport);
}

void UHSRMapViewModel::HandleMapStateChanged(const FHSRMapRuntimeSnapshot& InSnapshot)
{
	if (bHasSnapshot && Snapshot.Revision == InSnapshot.Revision)
	{
		return;
	}
	Snapshot = InSnapshot;
	bHasSnapshot = true;
	Changed.Broadcast(Snapshot);
	OnSnapshotChanged.Broadcast(Snapshot);
}
