#include "HSRMapViewModel.h"

#include "../Map/HSRMapSubsystem.h"

// 地图 ViewModel：把地图子系统的运行时快照缓存成本地副本，
// 并广播给 UI（MVVM 的 VM 层），UI 不直接触碰地图子系统。
void UHSRMapViewModel::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

// 初始化：绑定地图子系统状态变化事件，并立即拉取一次快照。
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

// 关闭：解绑事件、清空缓存快照。
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

// 取缓存快照；尚未收到任何快照时返回 false。
bool UHSRMapViewModel::GetSnapshot(FHSRMapRuntimeSnapshot& OutSnapshot) const
{
	if (!bHasSnapshot)
	{
		return false;
	}
	OutSnapshot = Snapshot;
	return true;
}

// 请求传送（转交地图子系统）。
EHSRMapOperationResult UHSRMapViewModel::RequestTeleport(const FName TeleportId)
{
	return Maps.IsValid() ? Maps->RequestTeleportTravel(TeleportId) : EHSRMapOperationResult::InvalidWorld;
}

// 取地图显示名。
FText UHSRMapViewModel::GetMapDisplayName(const FName MapId) const
{
	return Maps.IsValid() ? Maps->GetMapDisplayName(MapId) : FText::FromName(MapId);
}

// 取全部可用传送点投影。
void UHSRMapViewModel::GetAvailableTeleports(TArray<FHSRTeleportProjection>& OutTeleports) const
{
	OutTeleports.Reset();
	if (Maps.IsValid())
	{
		Maps->GetAvailableTeleports(OutTeleports);
	}
}

// 可达传送点数量。
int32 UHSRMapViewModel::GetReachableTeleportCount() const
{
	return Maps.IsValid() ? Maps->GetReachableTeleportCount() : 0;
}

// 取第 Index 个可达传送点。
bool UHSRMapViewModel::GetReachableTeleport(const int32 Index, FHSRTeleportProjection& OutTeleport) const
{
	return Maps.IsValid() && Maps->GetReachableTeleport(Index, OutTeleport);
}

// 地图状态变化回调：版本没变则忽略（避免重复广播），否则更新缓存并广播两个事件。
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
