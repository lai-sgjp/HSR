#include "HSRMapArrivalConsumer.h"

#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HSRMapArrivalPoint.h"
#include "HSRMapSubsystem.h"

// 到达消费者 Actor：负责在新关卡加载完成后，把玩家摆到正确的到达点并提交传送。
// 采用“延迟重试”策略，是因为关卡刚打开时 Pawn / 到达点可能尚未就绪，
// 定时器每隔 RetryInterval 重试一次，直到成功或超过 MaxRetries 后取消。
AHSRMapArrivalConsumer::AHSRMapArrivalConsumer()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AHSRMapArrivalConsumer::BeginPlay()
{
	Super::BeginPlay();
	// 关卡拉起后立即安排第一次尝试；首次失败后由 RetryOrCancel 决定是否续期。
	GetWorldTimerManager().SetTimer(RetryTimer, this, &AHSRMapArrivalConsumer::TryCommitArrival, RetryInterval, false);
}

void AHSRMapArrivalConsumer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 离开关卡前必须清掉定时器，避免回调作用在已销毁的 Actor 上。
	GetWorldTimerManager().ClearTimer(RetryTimer);
	Super::EndPlay(EndPlayReason);
}

// 尝试提交一次到达。核心流程：
// 1. 取回挂起的传送请求，确认目的地是当前关卡；
// 2. 扫描关卡里的到达点，统计目标 ArrivalId 匹配个数；
// 3. 对 "Save.Restore" 恢复旅行优先用存档坐标摆位；
// 4. 普通传送则要求到达点唯一匹配，然后用到达点的世界坐标提交。
void AHSRMapArrivalConsumer::TryCommitArrival()
{
	UHSRMapSubsystem* Maps = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRMapSubsystem>() : nullptr;
	FHSRTeleportRequest Pending;
	// 没有挂起请求（可能已被取消或超时）就不需要做任何事。
	if (!Maps || !Maps->GetPendingRequest(Pending))
	{
		return;
	}
	// 这个 Consumer 只处理它配置的那张地图；目的地不匹配说明挂错了关卡。
	if (MapId.IsNone() || MapId != Pending.Destination.MapId)
	{
		RetryOrCancel(TEXT("destination map mismatch"));
		return;
	}

	// 收集目标到达点在关卡里的匹配情况：唯一匹配才能摆位，0 个或 1 个以上都异常。
	AHSRMapArrivalPoint* Match = nullptr;
	int32 MatchCount = 0;
	for (TActorIterator<AHSRMapArrivalPoint> It(GetWorld()); It; ++It)
	{
		if (It->ArrivalId == Pending.Destination.ArrivalId)
		{
			Match = *It;
			++MatchCount;
		}
	}
	const FString LoadedPackage = GetWorld()->GetOutermost()
		? UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetPathName()) : FString();
	// 让地图子系统校验“到达上下文”（地图包 / 到达点 / 唯一性）。
	const EHSRMapOperationResult ContextResult = Maps->ValidatePendingArrivalContext(MapId,
		Pending.Destination.ArrivalId, LoadedPackage, MatchCount);
	// 恢复旅行：即使关卡里没有对应的到达点，也可以直接用存档坐标摆位。
	if (MatchCount <= 1 && Pending.TeleportId == TEXT("Save.Restore"))
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		const EHSRMapOperationResult RestoreResult = Maps->CommitPendingRestoreArrival(MapId, Pawn,
			Pending.Destination.WorldTransform);
		if (RestoreResult == EHSRMapOperationResult::Success)
		{
			UE_LOG(LogTemp, Log, TEXT("HSR Map restore arrival used saved transform RequestId=%s Map=%s Arrival=%s"),
				*Pending.RequestId.ToString(),
				*MapId.ToString(),
				*Pending.Destination.ArrivalId.ToString());
			return;
		}
		// 摆位失败（例如 Pawn 还没生成），稍后重试。
		RetryOrCancel(TEXT("saved transform placement not ready"));
		return;
	}
	// 普通传送：上下文必须通过且到达点唯一存在，否则说明环境不对，重试或放弃。
	if (ContextResult != EHSRMapOperationResult::Success || !Match)
	{
		RetryOrCancel(ContextResult == EHSRMapOperationResult::InvalidWorld ? TEXT("destination world mismatch")
			: (MatchCount == 0 ? TEXT("arrival missing") : TEXT("arrival ambiguous")));
		return;
	}
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const EHSRMapOperationResult Result = Maps->CommitPendingArrival(MapId, Match->ArrivalId, Pawn, Match->GetActorTransform());
	if (Result != EHSRMapOperationResult::Success)
	{
		RetryOrCancel(TEXT("pawn placement not ready"));
	}
}

// 重试或取消：根据剩余重试次数决定是再次排定 TryCommitArrival，还是放弃并取消挂起旅行。
void AHSRMapArrivalConsumer::RetryOrCancel(const TCHAR* Reason)
{
	UHSRMapSubsystem* Maps = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRMapSubsystem>() : nullptr;
	FHSRTeleportRequest Pending;
	// 请求已被外部取消，无需再重试。
	if (!Maps || !Maps->GetPendingRequest(Pending))
	{
		return;
	}
	if (++RetryCount <= MaxRetries)
	{
		UE_LOG(LogTemp, Log, TEXT("HSR Map arrival waiting RequestId=%s Attempt=%d/%d Reason=%s"),
			*Pending.RequestId.ToString(),
			RetryCount,
			MaxRetries,
			Reason);
		GetWorldTimerManager().SetTimer(RetryTimer, this, &AHSRMapArrivalConsumer::TryCommitArrival, RetryInterval, false);
		return;
	}
	// 超过最大重试次数：不再等待，直接取消这次旅行，避免挂起请求永远无法清理。
	UE_LOG(LogTemp, Warning, TEXT("HSR Map arrival canceled RequestId=%s after retries Reason=%s"),
		*Pending.RequestId.ToString(),
		Reason);
	Maps->CancelPendingTravel(Pending.RequestId);
}
