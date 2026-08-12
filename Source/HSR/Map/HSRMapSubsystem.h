#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HSRMapTypes.h"
#include "HSRMapSubsystem.generated.h"

class UHSRMapDefinition;
class UHSRTeleportDefinition;
class APawn;

UCLASS()
class HSR_API UHSRMapSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	EHSRMapOperationResult RegisterMapDefinition(const UHSRMapDefinition& Definition);
	EHSRMapOperationResult RegisterTeleportDefinition(const UHSRTeleportDefinition& Definition);
	UFUNCTION(BlueprintCallable, Category="HSR|Map")
	EHSRMapOperationResult RegisterMapAsset(UHSRMapDefinition* Definition);
	UFUNCTION(BlueprintCallable, Category="HSR|Map")
	EHSRMapOperationResult RegisterTeleportAsset(UHSRTeleportDefinition* Definition);
	UFUNCTION(BlueprintCallable, Category="HSR|Map")
	EHSRMapOperationResult SetCurrentLocation(FName MapId, FName ArrivalId = NAME_None);
	UFUNCTION(BlueprintCallable, Category="HSR|Map")
	EHSRMapOperationResult UnlockRegion(FName RegionId);
	UFUNCTION(BlueprintCallable, Category="HSR|Map")
	EHSRMapOperationResult UnlockTeleport(FName TeleportId);
	EHSRMapOperationResult SetExplorationFlag(FName FlagId);
	EHSRMapOperationResult BuildTeleportRequest(FName TeleportId, FHSRTeleportRequest& OutRequest) const;
	UFUNCTION(BlueprintCallable, Category="HSR|Map")
	EHSRMapOperationResult RequestTeleportTravel(FName TeleportId);
	EHSRMapOperationResult RequestRestoreTravel();
	EHSRMapOperationResult RequestRestoreTravel(const FHSRMapRuntimeSnapshot& RestoreTarget);
	EHSRMapOperationResult ApplyRestoreLocation(const FHSRMapRuntimeSnapshot& RestoreTarget);
	EHSRMapOperationResult CommitPendingArrival(FName DestinationMapId, FName ArrivalId, APawn* Pawn, const FTransform& ArrivalTransform);
	EHSRMapOperationResult CommitPendingRestoreArrival(FName DestinationMapId, APawn* Pawn, const FTransform& SavedTransform);
	EHSRMapOperationResult ValidatePendingArrivalContext(FName DestinationMapId, FName ArrivalId, const FString& LoadedWorldPackage, int32 MatchingArrivalCount) const;
	EHSRMapOperationResult CommitBattleReturnLocation(FName MapId, APawn* Pawn, const FTransform& ReturnTransform);
	bool ResolveMapIdByPackage(FName MapPackage, FName& OutMapId) const;
	EHSRMapOperationResult CancelPendingTravel(const FGuid& RequestId);
	bool GetPendingRequest(FHSRTeleportRequest& OutRequest) const;
	UFUNCTION(BlueprintPure, Category="HSR|Map")
	bool HasPendingTravel() const { return bTravelPending; }
	void HandleTravelFailure(UWorld* InWorld, ETravelFailure::Type FailureType, const FString& ErrorString);

	bool IsRegionUnlocked(FName RegionId) const;
	bool IsTeleportUnlocked(FName TeleportId) const;
	bool HasMapDefinition(FName MapId) const { return Maps.Contains(MapId); }
	bool HasRegionDefinition(FName RegionId) const;
	bool HasTeleportDefinition(FName TeleportId) const { return Teleports.Contains(TeleportId); }
	UFUNCTION(BlueprintPure, Category = "HSR|Map")
	FText GetMapDisplayName(FName MapId) const;
	UFUNCTION(BlueprintPure, Category = "HSR|Map")
	void GetAvailableTeleports(TArray<FHSRTeleportProjection>& OutTeleports) const;
	UFUNCTION(BlueprintPure, Category = "HSR|Map")
	int32 GetReachableTeleportCount() const;
	UFUNCTION(BlueprintPure, Category = "HSR|Map")
	bool GetReachableTeleport(int32 Index, FHSRTeleportProjection& OutTeleport) const;
	const FHSRMapRuntimeSnapshot& GetSnapshot() const { return Snapshot; }
	FHSRMapStateChanged& OnMapStateChanged() { return MapStateChanged; }
	FHSRMapArrivalCommitted& OnArrivalCommitted() { return ArrivalCommitted; }
	FHSRMapRestoreTravelFailed& OnRestoreTravelFailed() { return RestoreTravelFailed; }
	int64 GetArrivalCommitGeneration() const { return ArrivalCommitGeneration; }
	const FGuid& GetLastCommittedRequestId() const { return LastCommittedRequestId; }
	void ExportSaveData(FHSRMapSaveData& OutData) const;
	bool PrepareRestore(const FHSRMapSaveData& Data, FHSRMapRuntimeSnapshot& OutCandidate) const;
	bool IsRestoreDifferent(const FHSRMapRuntimeSnapshot& Candidate) const;
	void CommitRestore(FHSRMapRuntimeSnapshot&& Candidate, bool bNotify);
	static bool CanRestoreState(bool bOrdinaryTravelPending, bool bBattleReturnPending)
	{
		return !bOrdinaryTravelPending && !bBattleReturnPending;
	}

#if WITH_DEV_AUTOMATION_TESTS
	EHSRMapOperationResult StageTeleportForAutomation(FName TeleportId);
	bool DoesRegisteredMapPackageExistForAutomation(FName MapId) const;
	void PublishArrivalCommittedForAutomation(FName MapId, FName ArrivalId, EHSRMapArrivalCommitKind Kind)
	{
		PublishArrivalCommitted(MapId, ArrivalId, Kind);
	}
#endif

private:
	struct FRegisteredMap
	{
		TSoftObjectPtr<UWorld> World;
		FName RegionId = NAME_None;
		FName DefaultArrivalId = NAME_None;
		FText DisplayName;
	};

	struct FRegisteredTeleport
	{
		FName SourceMapId = NAME_None;
		FName DestinationMapId = NAME_None;
		FName DestinationArrivalId = NAME_None;
		bool bInitiallyUnlocked = false;
	};

	void CommitStateChange();
	EHSRMapOperationResult CommitPendingArrivalValidated(FName DestinationMapId, FName ArrivalId, APawn* Pawn, const FTransform& ArrivalTransform);
	void PublishArrivalCommitted(FName MapId, FName ArrivalId, EHSRMapArrivalCommitKind Kind);
	void HandleTravelTimeout();

	TMap<FName, FRegisteredMap> Maps;
	TMap<FName, FRegisteredTeleport> Teleports;

	UPROPERTY(Transient)
	FHSRMapRuntimeSnapshot Snapshot;

	FHSRMapStateChanged MapStateChanged;
	FHSRMapArrivalCommitted ArrivalCommitted;
	FHSRMapRestoreTravelFailed RestoreTravelFailed;
	int64 ArrivalCommitGeneration = 0;
	FGuid LastCommittedRequestId;
	FHSRTeleportRequest PendingRequest;
	bool bTravelPending = false;
	FTimerHandle TravelTimeoutTimer;
};
