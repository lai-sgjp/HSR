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
	EHSRMapOperationResult CommitPendingArrival(FName DestinationMapId, FName ArrivalId, APawn* Pawn, const FTransform& ArrivalTransform);
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
	const FHSRMapRuntimeSnapshot& GetSnapshot() const { return Snapshot; }
	FHSRMapStateChanged& OnMapStateChanged() { return MapStateChanged; }
	FHSRMapArrivalCommitted& OnArrivalCommitted() { return ArrivalCommitted; }
	int64 GetArrivalCommitGeneration() const { return ArrivalCommitGeneration; }
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
	};

	struct FRegisteredTeleport
	{
		FName SourceMapId = NAME_None;
		FName DestinationMapId = NAME_None;
		FName DestinationArrivalId = NAME_None;
		bool bInitiallyUnlocked = false;
	};

	void CommitStateChange();
	void PublishArrivalCommitted(FName MapId, FName ArrivalId, EHSRMapArrivalCommitKind Kind);
	void HandleTravelTimeout();

	TMap<FName, FRegisteredMap> Maps;
	TMap<FName, FRegisteredTeleport> Teleports;

	UPROPERTY(Transient)
	FHSRMapRuntimeSnapshot Snapshot;

	FHSRMapStateChanged MapStateChanged;
	FHSRMapArrivalCommitted ArrivalCommitted;
	int64 ArrivalCommitGeneration = 0;
	FHSRTeleportRequest PendingRequest;
	bool bTravelPending = false;
	FTimerHandle TravelTimeoutTimer;
};
