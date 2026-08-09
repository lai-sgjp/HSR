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
