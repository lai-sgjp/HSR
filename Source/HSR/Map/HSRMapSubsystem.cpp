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
// 匿名命名空间内的纯工具函数：只做数据校验与集合比较，不依赖任何外部状态。
// 限定在本编译单元内，避免污染全局符号表，也方便无状态地单测。

// 校验一个地图变换是否可用于存档恢复 / 落点摆放。
// 位置、缩放、旋转四元数中只要有一个分量为 NaN / 无穷，就判定为非法，
// 防止把损坏的存档坐标写进 Actor 变换导致物理 / 渲染层异常。
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

// 把 Source 中的名字逐个加入 Target。
// 若遇到空名字或重复名字立即返回 false，保证恢复出来的快照名字集合是唯一且非空的。
// 调用方据此可在“恢复存档”前拒绝掉损坏的保存数据。
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

// 判断两个名字集合是否内容相同（与顺序无关）。
// 用于“恢复存档后是否真的需要提交变更”：集合类型本身无序，所以先比数量再比成员。
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

// 子系统初始化：订阅引擎级“地图旅行失败”事件。
// 挂在 GEngine 而非 World 上，是因为旅行失败事件在跨地图切换时仍由引擎全局广播，
// 这样即使目标地图没有成功加载，本子系统也能捕获失败并及时清掉挂起的传送请求。
void UHSRMapSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (GEngine)
	{
		GEngine->OnTravelFailure().AddUObject(this, &UHSRMapSubsystem::HandleTravelFailure);
	}
}

// 子系统销毁：取消订阅并清空挂起状态。
// 若此时还有未完成的传送请求，直接丢弃即可——游戏实例正在销毁，不会再有人消费它。
void UHSRMapSubsystem::Deinitialize()
{
	if (GEngine)
	{
		GEngine->OnTravelFailure().RemoveAll(this);
	}
	// 清空挂起请求，避免残留的 PendingRequest 在下次初始化时被误用。
	PendingRequest = FHSRTeleportRequest();
	bTravelPending = false;
	Super::Deinitialize();
}

// 注册一份地图定义。MapId / World / RegionId / DefaultArrivalId 缺一不可，
// 否则后续传送无法解析目标包名或默认到达点。
// 幂等：已注册且内容完全一致时返回 NoOp，只换了一部分字段则视为冲突。
EHSRMapOperationResult UHSRMapSubsystem::RegisterMapDefinition(const UHSRMapDefinition& Definition)
{
	if (Definition.MapId.IsNone() || Definition.World.IsNull() || Definition.RegionId.IsNone() || Definition.DefaultArrivalId.IsNone())
	{
		return EHSRMapOperationResult::InvalidDefinition;
	}

	// 先构造候选记录再做比较，避免直接在容器上原地修改。
	const FRegisteredMap Candidate{Definition.World, Definition.RegionId, Definition.DefaultArrivalId, Definition.DisplayName};
	if (const FRegisteredMap* Existing = Maps.Find(Definition.MapId))
	{
		// 逐字段比较注册字段（显示名用文本相等，不区分大小写）。
		const bool bSame = Existing->World == Candidate.World
			&& Existing->RegionId == Candidate.RegionId
			&& Existing->DefaultArrivalId == Candidate.DefaultArrivalId
			&& Existing->DisplayName.EqualTo(Candidate.DisplayName);
		return bSame ? EHSRMapOperationResult::NoOp : EHSRMapOperationResult::DuplicateId;
	}

	Maps.Add(Definition.MapId, Candidate);
	return EHSRMapOperationResult::Success;
}

// 注册一份传送点定义。
// "Save.Restore" 是系统保留的“读档恢复”传送 ID，不允许被内容作者占用。
// 传送要求源/目标地图都已注册，否则无法确定目的地图包名。
EHSRMapOperationResult UHSRMapSubsystem::RegisterTeleportDefinition(const UHSRTeleportDefinition& Definition)
{
	// 保留 ID 检查放在最前：即使是合法字段也不允许注册。
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

	// 源/目标都指向已注册地图后才构造候选记录。
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
	// 出生即解锁的传送点直接写入运行时快照，保证存档/UI 查询与注册保持一致。
	if (Definition.bInitiallyUnlocked)
	{
		Snapshot.UnlockedTeleportIds.Add(Definition.TeleportId);
		CommitStateChange();
	}
	return EHSRMapOperationResult::Success;
}

// 空指针封装：把“传 UObject 指针”统一转成“传引用”的注册接口。
EHSRMapOperationResult UHSRMapSubsystem::RegisterMapAsset(UHSRMapDefinition* Definition)
{
	return Definition ? RegisterMapDefinition(*Definition) : EHSRMapOperationResult::InvalidDefinition;
}

// 空指针封装：同上，针对传送点定义。
EHSRMapOperationResult UHSRMapSubsystem::RegisterTeleportAsset(UHSRTeleportDefinition* Definition)
{
	return Definition ? RegisterTeleportDefinition(*Definition) : EHSRMapOperationResult::InvalidDefinition;
}

// 设置“当前所在地图”。ArrivalId 可缺省：缺省时回落到该地图的默认到达点。
// 若默认到达点也没有配置，则该地图定义不完整，拒绝写入。
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

// 解锁一个区域。区域必须已被某张地图引用，否则没有意义（没人会查询它）。
EHSRMapOperationResult UHSRMapSubsystem::UnlockRegion(const FName RegionId)
{
	if (RegionId.IsNone())
	{
		return EHSRMapOperationResult::InvalidDefinition;
	}
	// 扫描所有注册地图，确认该区域确实存在。
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

// 查询某个区域是否已被任何已注册地图引用。
// 用于存档恢复前的合法性校验：未知的区域 ID 不应出现在存档里。
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

// 查询地图的展示名；未注册或没有显示名时回退为 MapId 本身。
// UI 用这个函数显示地图名，避免空文本。
FText UHSRMapSubsystem::GetMapDisplayName(const FName MapId) const
{
	const FRegisteredMap* Map = Maps.Find(MapId);
	if (!Map || Map->DisplayName.IsEmpty())
	{
		return FText::FromName(MapId);
	}
	return Map->DisplayName;
}

// 收集当前地图上“可展示”的传送点投影。
// 一个传送点要可用必须同时满足：起点是当前地图、传送点已解锁、目的地区域已解锁。
// 注意这里只填投影，不判断当前是否有传送正在挂起——可用性只看数据层面。
void UHSRMapSubsystem::GetAvailableTeleports(TArray<FHSRTeleportProjection>& OutTeleports) const
{
	OutTeleports.Reset();
	const FName CurrentMapId = Snapshot.CurrentLocation.MapId;
	for (const TPair<FName, FRegisteredTeleport>& Pair : Teleports)
	{
		const FRegisteredTeleport& Teleport = Pair.Value;
		// 组装投影：ID、源/目标、目标地图显示名。
		FHSRTeleportProjection Projection;
		Projection.TeleportId = Pair.Key;
		Projection.SourceMapId = Teleport.SourceMapId;
		Projection.DestinationMapId = Teleport.DestinationMapId;
		Projection.DestinationDisplayName = GetMapDisplayName(Teleport.DestinationMapId);

		// 目的地图可能未注册（数据异常），此时区域解锁判定保守地视为不可用。
		const FRegisteredMap* DestinationMap = Maps.Find(Teleport.DestinationMapId);
		const bool bFromCurrentMap = (CurrentMapId == Teleport.SourceMapId);
		const bool bTeleportUnlocked = Snapshot.UnlockedTeleportIds.Contains(Pair.Key);
		const bool bRegionUnlocked = DestinationMap && Snapshot.UnlockedRegionIds.Contains(DestinationMap->RegionId);
		Projection.bUsable = bFromCurrentMap && bTeleportUnlocked && bRegionUnlocked;

		OutTeleports.Add(Projection);
	}
}

// 统计当前可用的传送点数量（供 HUD / UI 角标使用）。
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

// 按“可用集合中的序号”取第 Index 个可用传送点。
// 返回 false 表示没有第 Index 个可用项（越界）；不做排序，顺序取决于容器遍历顺序。
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

// 解锁某个传送点（只解锁，不触发旅行）。
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

// 设置一个探索标记位。标志集合用于保存探索进度（如剧情旗标），不存在可覆盖语义。
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

// 根据传送点 ID 构建一个“旅行请求”。只做校验与填充，不实际执行旅行。
// 校验顺序有讲究：先查传送点存在性，再查解锁，再查区域解锁，最后查源地图匹配，
// 这样调用方拿到错误码时能直接定位是哪一层被拒绝。
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

	// 用新 GUID 标识这次请求，供后续提交 / 取消 / 超时流程做配对。
	FHSRTeleportRequest Candidate;
	Candidate.RequestId = FGuid::NewGuid();
	Candidate.TeleportId = TeleportId;
	Candidate.Source = Snapshot.CurrentLocation;
	Candidate.Destination = {Teleport->DestinationMapId, Teleport->DestinationArrivalId};
	OutRequest = MoveTemp(Candidate);
	return EHSRMapOperationResult::Success;
}

// 发起一次地图传送。入口要同时处理三件事：校验可传送性、准备 UI、真正打开目标关卡。
// 若战斗世界正在“返回探索”的挂起中，或本系统已有挂起请求，则拒绝新请求（一次只允许一段旅程）。
EHSRMapOperationResult UHSRMapSubsystem::RequestTeleportTravel(const FName TeleportId)
{
	// 战斗返回会使用同一个旅行通道，二者不能同时挂起。
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
	// 关卡包必须真实存在于磁盘，否则 OpenLevel 会静默失败或触发旅行失败回调。
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
	// 传送前先让 UI 收尾（关闭可能遮挡加载画面的面板）。失败则中止传送。
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

	// 一切就绪后才落盘“挂起请求”：记录请求、启动 5 秒超时保护、发起关卡切换。
	PendingRequest = Candidate;
	bTravelPending = true;
	World->GetTimerManager().SetTimer(TravelTimeoutTimer, this, &UHSRMapSubsystem::HandleTravelTimeout, 5.0f, false);
	UE_LOG(LogTemp, Log, TEXT("HSR Map travel issued RequestId=%s Teleport=%s Source=%s Destination=%s Arrival=%s"),
		*Candidate.RequestId.ToString(),
		*Candidate.TeleportId.ToString(),
		*Candidate.Source.MapId.ToString(),
		*Candidate.Destination.MapId.ToString(),
		*Candidate.Destination.ArrivalId.ToString());
	UGameplayStatics::OpenLevel(World, FName(*PackageName), true);
	return EHSRMapOperationResult::Success;
}

// 便捷重载：用当前运行时快照作为恢复目标。
EHSRMapOperationResult UHSRMapSubsystem::RequestRestoreTravel()
{
	return RequestRestoreTravel(Snapshot);
}

// 读档恢复旅行：把玩家传送回存档里记录的地图位置。
// 与普通传送不同，恢复旅行总是使用 "Save.Restore" 这个保留 ID，到达点由存档决定。
EHSRMapOperationResult UHSRMapSubsystem::RequestRestoreTravel(const FHSRMapRuntimeSnapshot& RestoreTarget)
{
	// 存档里没有记录当前位置，无从恢复。
	if (RestoreTarget.CurrentLocation.MapId.IsNone())
	{
		return EHSRMapOperationResult::NoOp;
	}
	// 当前已在目标地图，无需切换关卡。
	if (Snapshot.CurrentLocation.MapId == RestoreTarget.CurrentLocation.MapId)
	{
		return EHSRMapOperationResult::NoOp;
	}
	UWorld* World = GetWorld();
	if (!World || bTravelPending)
	{
		return bTravelPending ? EHSRMapOperationResult::AlreadyPending : EHSRMapOperationResult::InvalidWorld;
	}
	const FRegisteredMap* Destination = Maps.Find(RestoreTarget.CurrentLocation.MapId);
	if (!Destination || Destination->World.IsNull())
	{
		return EHSRMapOperationResult::UnknownMap;
	}
	const FString Package = Destination->World.GetLongPackageName();
	if (Package.IsEmpty() || !FPackageName::DoesPackageExist(Package))
	{
		return EHSRMapOperationResult::InvalidMapPackage;
	}
	// 解析当前已加载关卡的 MapId；若与恢复目标一致，说明其实已经在目标地图（等价于上面的 NoOp 分支）。
	FName Loaded;
	if (World->GetOutermost() && ResolveMapIdByPackage(World->GetOutermost()->GetFName(), Loaded) && Loaded == RestoreTarget.CurrentLocation.MapId)
	{
		return EHSRMapOperationResult::NoOp;
	}
	// 同普通传送：先让 UI 收尾。
	if (ULocalPlayer* LP = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr)
	{
		if (UHSRUIManagerSubsystem* UI = LP->GetSubsystem<UHSRUIManagerSubsystem>())
		{
			if (UI->PrepareExplorationTravel() != EHSRUIScreenResult::Success)
			{
				return EHSRMapOperationResult::UIPreparationFailed;
			}
		}
	}
	// 挂起请求：Source.MapId 记录的是“当前所在地图”，Destination 是存档里的恢复目标。
	PendingRequest.RequestId = FGuid::NewGuid();
	PendingRequest.TeleportId = TEXT("Save.Restore");
	PendingRequest.Source.MapId = Loaded;
	PendingRequest.Destination = RestoreTarget.CurrentLocation;
	bTravelPending = true;
	World->GetTimerManager().SetTimer(TravelTimeoutTimer, this, &UHSRMapSubsystem::HandleTravelTimeout, 5.0f, false);
	UE_LOG(LogTemp, Log, TEXT("HSR Map restore travel issued RequestId=%s Destination=%s"),
		*PendingRequest.RequestId.ToString(),
		*PendingRequest.Destination.MapId.ToString());
	UGameplayStatics::OpenLevel(World, FName(*Package), true);
	return EHSRMapOperationResult::Success;
}

// 把“恢复位置”落实到当前关卡的 Pawn 上（无需切换关卡时使用）。
// 与 CommitPendingArrival 不同，它不消费挂起请求，只负责把玩家 Actor 摆到存档坐标。
EHSRMapOperationResult UHSRMapSubsystem::ApplyRestoreLocation(const FHSRMapRuntimeSnapshot& RestoreTarget)
{
	if (RestoreTarget.CurrentLocation.MapId.IsNone())
	{
		return EHSRMapOperationResult::NoOp;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return EHSRMapOperationResult::NoOp;
	}
	FName LoadedMapId;
	// 必须确认当前关卡确实就是存档记录的那张地图，否则坐标无意义。
	if (!World->GetOutermost() || !ResolveMapIdByPackage(World->GetOutermost()->GetFName(), LoadedMapId)
		|| LoadedMapId != RestoreTarget.CurrentLocation.MapId)
	{
		return EHSRMapOperationResult::InvalidWorld;
	}
	APlayerController* Controller = World->GetFirstPlayerController();
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Pawn)
	{
		return EHSRMapOperationResult::PawnUnavailable;
	}
	// 存档坐标含 NaN，或落点摆放失败，都视为放置失败。
	if (RestoreTarget.CurrentLocation.WorldTransform.ContainsNaN()
		|| !Pawn->SetActorTransform(RestoreTarget.CurrentLocation.WorldTransform, false, nullptr, ETeleportType::TeleportPhysics))
	{
		return EHSRMapOperationResult::PlacementFailed;
	}
	return EHSRMapOperationResult::Success;
}

// 到达目标地图后提交挂起请求（普通传送路径）。
// 先做“上下文校验”（地图包、到达点、唯一性），通过后才真正提交并落盘快照。
EHSRMapOperationResult UHSRMapSubsystem::CommitPendingArrival(const FName DestinationMapId, const FName ArrivalId,
	APawn* Pawn, const FTransform& ArrivalTransform)
{
	UWorld* World = GetWorld();
	// 去掉 PIE 前缀，避免打包与编辑器里包名不一致导致校验失败。
	const FString LoadedPackage = World && World->GetOutermost()
		? UWorld::RemovePIEPrefix(World->GetOutermost()->GetPathName()) : FString();
	const EHSRMapOperationResult ContextResult = ValidatePendingArrivalContext(DestinationMapId, ArrivalId, LoadedPackage, 1);
	if (ContextResult != EHSRMapOperationResult::Success)
	{
		return ContextResult;
	}
	return CommitPendingArrivalValidated(DestinationMapId, ArrivalId, Pawn, ArrivalTransform);
}

// 提交“读档恢复”到达（Save.Restore 路径）。
// 恢复到达不要求 ArrivalId 精确匹配，到达点由存档里的 WorldTransform 决定。
EHSRMapOperationResult UHSRMapSubsystem::CommitPendingRestoreArrival(const FName DestinationMapId, APawn* Pawn,
	const FTransform& SavedTransform)
{
	if (!bTravelPending)
	{
		return EHSRMapOperationResult::NothingPending;
	}
	// 只有挂起的确实是一次恢复旅行、且目的地匹配时才能提交。
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

// 提交到达的公共收尾路径：校验通过后摆放 Pawn、落盘快照、广播事件。
// 这是所有到达提交（普通/恢复）都会经过的唯一出口，保证状态一致性。
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

	// 摆放成功后，把挂起请求固化进运行时快照：当前所在地就是目的地，坐标用实际落点。
	Snapshot.CurrentLocation = PendingRequest.Destination;
	Snapshot.CurrentLocation.WorldTransform = ArrivalTransform;
	// 记录已提交的请求 ID，供外部确认“这次旅行确实完成了”。
	const FGuid CompletedRequestId = PendingRequest.RequestId;
	LastCommittedRequestId = CompletedRequestId;
	// 清理挂起状态与超时定时器。
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
		*CompletedRequestId.ToString(),
		*DestinationMapId.ToString(),
		*ArrivalId.ToString(),
		*ArrivalTransform.GetLocation().ToString());
	return EHSRMapOperationResult::Success;
}

// 战斗返回探索时，直接把玩家放回战斗前的坐标。
// 与普通传送不同，这里不消费“传送挂起请求”——战斗返回走的是战斗子系统的通道。
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
	// 校验当前关卡包与地图注册包一致，防止战斗返回时跑到别的地图。
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
	// 战斗返回不设具体到达点，ArrivalId 记为 None，坐标直接用战斗前位置。
	Snapshot.CurrentLocation.MapId = MapId;
	Snapshot.CurrentLocation.ArrivalId = NAME_None;
	Snapshot.CurrentLocation.WorldTransform = ReturnTransform;
	CommitStateChange();
	PublishArrivalCommitted(MapId, NAME_None, EHSRMapArrivalCommitKind::BattleReturn);
	UE_LOG(LogTemp, Log, TEXT("HSR Battle map location committed Map=%s Location=%s"),
		*MapId.ToString(),
		*ReturnTransform.GetLocation().ToString());
	return EHSRMapOperationResult::Success;
}

// 根据关卡包名反向解析出注册的 MapId。
// 反查用“去掉 PIE 前缀后的长包名”做精确匹配，避免编辑器前缀干扰。
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

// 校验“到达上下文”：是否有挂起请求、目的地与到达点是否匹配、关卡包是否正确、
// 到达点在关卡中出现的次数是否唯一。这是防止“摆错位置 / 目标有歧义”的最后防线。
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
	// 关卡里找不到该到达点，或找到多个同名到达点，都无法可靠摆放。
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

// 取消一段挂起的旅行。只有请求 ID 精确匹配时才能取消（防止取消掉别人的旅行）。
// 对 "Save.Restore" 这类恢复旅行，额外广播失败事件，方便存档系统感知。
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
	// 保存副本后再清空，供下面广播使用。
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

// 取回当前挂起的旅行请求。没有挂起请求时返回 false。
bool UHSRMapSubsystem::GetPendingRequest(FHSRTeleportRequest& OutRequest) const
{
	if (!bTravelPending)
	{
		return false;
	}
	OutRequest = PendingRequest;
	return true;
}

// 引擎旅行失败回调：只处理“当前挂起请求对应目标地图”的失败，
// 其他地图的失败与我们无关，直接忽略。
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
	// 失败来自别的地图：清空挂起会误伤正在进行的其他旅行，必须跳过。
	if (InWorld && FailurePackage != ExpectedPackage)
	{
		UE_LOG(LogTemp, Log, TEXT("HSR Map ignored unrelated travel failure World=%s Expected=%s"), *FailurePackage, *ExpectedPackage);
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("HSR Map travel failed RequestId=%s Type=%d Error=%s; pending cleared for retry"),
		*PendingRequest.RequestId.ToString(),
		static_cast<int32>(FailureType),
		*ErrorString);
	CancelPendingTravel(PendingRequest.RequestId);
}

// 旅行超时保护：关卡迟迟未加载完成时清掉挂起请求，避免永久卡在“传送中”状态。
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
// 自动化测试专用：把传送请求直接挂起，但不真正打开关卡，方便测试后续的提交/取消流程。
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

// 自动化测试专用：检查某地图的关卡包是否真实存在且已注册。
bool UHSRMapSubsystem::DoesRegisteredMapPackageExistForAutomation(const FName MapId) const
{
	const FRegisteredMap* Map = Maps.Find(MapId);
	return Map && !Map->World.IsNull() && FPackageName::DoesPackageExist(Map->World.GetLongPackageName());
}
#endif

// 查询区域是否已解锁（供 UI / 存档校验使用）。
bool UHSRMapSubsystem::IsRegionUnlocked(const FName RegionId) const
{
	return Snapshot.UnlockedRegionIds.Contains(RegionId);
}

// 查询传送点是否已解锁（供 UI / 存档校验使用）。
bool UHSRMapSubsystem::IsTeleportUnlocked(const FName TeleportId) const
{
	return Snapshot.UnlockedTeleportIds.Contains(TeleportId);
}

// 导出存档数据：把运行时快照转成可序列化的保存结构，并做字典序排序，
// 保证多次存档的内容稳定可比较（集合转数组后顺序是随机的）。
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

// 校验并准备一份存档恢复候选快照。
// 规则：恢复时不能有旅行挂起；快照数据必须自洽（名字唯一、区域/传送点已注册、
// 若没有当前位置则其他字段必须全空）；最终返回一份可直接提交的候选。
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
	// 名字必须唯一且非空；有任何重复/空项即拒绝。
	if (!AddUniqueNames(Data.UnlockedRegionIds, Candidate.UnlockedRegionIds)
		|| !AddUniqueNames(Data.UnlockedTeleportIds, Candidate.UnlockedTeleportIds)
		|| !AddUniqueNames(Data.ExplorationFlags, Candidate.ExplorationFlags))
	{
		return false;
	}
	// 存档没记录位置：只有其它字段也全空才接受（等价于“全新进度”）。
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
	// 当前位置必须对应已注册地图，否则恢复后无法解析包名。
	if (!Maps.Contains(Candidate.CurrentLocation.MapId))
	{
		return false;
	}
	// 已解锁区域必须真实存在（防止旧版本存档引用被删掉的区域）。
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
	// 已解锁传送点必须仍被注册。
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

// 判断恢复候选是否与当前运行时快照不同。
// 集合比较无序（用 NameSetsEqual），只有内容确实变化才需要走一次完整的提交广播。
bool UHSRMapSubsystem::IsRestoreDifferent(const FHSRMapRuntimeSnapshot& Candidate) const
{
	return !(Snapshot.CurrentLocation == Candidate.CurrentLocation)
		|| !NameSetsEqual(Snapshot.UnlockedRegionIds, Candidate.UnlockedRegionIds)
		|| !NameSetsEqual(Snapshot.UnlockedTeleportIds, Candidate.UnlockedTeleportIds)
		|| !NameSetsEqual(Snapshot.ExplorationFlags, Candidate.ExplorationFlags)
		|| Snapshot.Revision != Candidate.Revision;
}

// 提交恢复：用候选快照整体替换当前快照，可选广播“地图状态变更”。
void UHSRMapSubsystem::CommitRestore(FHSRMapRuntimeSnapshot&& Candidate, const bool bNotify)
{
	Snapshot = MoveTemp(Candidate);
	if (bNotify)
	{
		MapStateChanged.Broadcast(Snapshot);
	}
}

// 提交一次状态变更：递增修订号并广播。所有写操作都走这里，保证监听方能拿到最新快照。
void UHSRMapSubsystem::CommitStateChange()
{
	++Snapshot.Revision;
	MapStateChanged.Broadcast(Snapshot);
}

// 广播“到达已提交”事件，并把提交世代号递增（每次到达都生成一个新世代号）。
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
		Info.CommitGeneration,
		*MapId.ToString(),
		*ArrivalId.ToString(),
		static_cast<int32>(Kind));
}
