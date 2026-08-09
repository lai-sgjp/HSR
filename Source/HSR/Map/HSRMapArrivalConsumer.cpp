#include "HSRMapArrivalConsumer.h"

#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HSRMapArrivalPoint.h"
#include "HSRMapSubsystem.h"

AHSRMapArrivalConsumer::AHSRMapArrivalConsumer()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AHSRMapArrivalConsumer::BeginPlay()
{
	Super::BeginPlay();
	GetWorldTimerManager().SetTimer(RetryTimer, this, &AHSRMapArrivalConsumer::TryCommitArrival, RetryInterval, false);
}

void AHSRMapArrivalConsumer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(RetryTimer);
	Super::EndPlay(EndPlayReason);
}

void AHSRMapArrivalConsumer::TryCommitArrival()
{
	UHSRMapSubsystem* Maps = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRMapSubsystem>() : nullptr;
	FHSRTeleportRequest Pending;
	if (!Maps || !Maps->GetPendingRequest(Pending))
	{
		return;
	}
	if (MapId.IsNone() || MapId != Pending.Destination.MapId)
	{
		RetryOrCancel(TEXT("destination map mismatch"));
		return;
	}

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
	const EHSRMapOperationResult ContextResult = Maps->ValidatePendingArrivalContext(MapId,
		Pending.Destination.ArrivalId, LoadedPackage, MatchCount);
	if (MatchCount <= 1 && Pending.TeleportId == TEXT("Save.Restore"))
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		const EHSRMapOperationResult RestoreResult = Maps->CommitPendingRestoreArrival(MapId, Pawn,
			Pending.Destination.WorldTransform);
		if (RestoreResult == EHSRMapOperationResult::Success)
		{
			UE_LOG(LogTemp, Log, TEXT("HSR Map restore arrival used saved transform RequestId=%s Map=%s Arrival=%s"),
				*Pending.RequestId.ToString(), *MapId.ToString(), *Pending.Destination.ArrivalId.ToString());
			return;
		}
		RetryOrCancel(TEXT("saved transform placement not ready"));
		return;
	}
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

void AHSRMapArrivalConsumer::RetryOrCancel(const TCHAR* Reason)
{
	UHSRMapSubsystem* Maps = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRMapSubsystem>() : nullptr;
	FHSRTeleportRequest Pending;
	if (!Maps || !Maps->GetPendingRequest(Pending))
	{
		return;
	}
	if (++RetryCount <= MaxRetries)
	{
		UE_LOG(LogTemp, Log, TEXT("HSR Map arrival waiting RequestId=%s Attempt=%d/%d Reason=%s"),
			*Pending.RequestId.ToString(), RetryCount, MaxRetries, Reason);
		GetWorldTimerManager().SetTimer(RetryTimer, this, &AHSRMapArrivalConsumer::TryCommitArrival, RetryInterval, false);
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("HSR Map arrival canceled RequestId=%s after retries Reason=%s"),
		*Pending.RequestId.ToString(), Reason);
	Maps->CancelPendingTravel(Pending.RequestId);
}
