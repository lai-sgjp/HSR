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
	const FHSRMapRuntimeSnapshot& GetSnapshot() const { return Snapshot; }
	FHSRMapStateChanged& OnMapStateChanged() { return MapStateChanged; }

#if WITH_DEV_AUTOMATION_TESTS
	EHSRMapOperationResult StageTeleportForAutomation(FName TeleportId);
	bool DoesRegisteredMapPackageExistForAutomation(FName MapId) const;
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
	void HandleTravelTimeout();

	TMap<FName, FRegisteredMap> Maps;
	TMap<FName, FRegisteredTeleport> Teleports;

	UPROPERTY(Transient)
	FHSRMapRuntimeSnapshot Snapshot;

	FHSRMapStateChanged MapStateChanged;
	FHSRTeleportRequest PendingRequest;
	bool bTravelPending = false;
	FTimerHandle TravelTimeoutTimer;
};
