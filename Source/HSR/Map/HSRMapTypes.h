#pragma once

#include "CoreMinimal.h"
#include "HSRMapTypes.generated.h"

UENUM(BlueprintType)
enum class EHSRMapOperationResult : uint8
{
	Success,
	NoOp,
	InvalidDefinition,
	DuplicateId,
	UnknownMap,
	UnknownTeleport,
	Locked,
	InvalidSource,
	AlreadyPending,
	InvalidMapPackage,
	InvalidWorld,
	WrongDestination,
	ArrivalNotFound,
	ArrivalAmbiguous,
	PawnUnavailable,
	PlacementFailed,
	NothingPending,
	RequestMismatch
};

USTRUCT(BlueprintType)
struct FHSRMapLocation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName MapId = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	FName ArrivalId = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	FTransform WorldTransform = FTransform::Identity;

	bool operator==(const FHSRMapLocation& Other) const
	{
		return MapId == Other.MapId && ArrivalId == Other.ArrivalId && WorldTransform.Equals(Other.WorldTransform);
	}
};

USTRUCT(BlueprintType)
struct FHSRTeleportRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly)
	FName TeleportId = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	FHSRMapLocation Source;

	UPROPERTY(BlueprintReadOnly)
	FHSRMapLocation Destination;
};

USTRUCT(BlueprintType)
struct FHSRMapRuntimeSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FHSRMapLocation CurrentLocation;

	UPROPERTY(BlueprintReadOnly)
	TSet<FName> UnlockedRegionIds;

	UPROPERTY(BlueprintReadOnly)
	TSet<FName> UnlockedTeleportIds;

	UPROPERTY(BlueprintReadOnly)
	TSet<FName> ExplorationFlags;

	UPROPERTY(BlueprintReadOnly)
	int64 Revision = 0;
};

USTRUCT(BlueprintType)
struct FHSRMapSaveData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FHSRMapLocation CurrentLocation;

	UPROPERTY(BlueprintReadWrite)
	TArray<FName> UnlockedRegionIds;

	UPROPERTY(BlueprintReadWrite)
	TArray<FName> UnlockedTeleportIds;

	UPROPERTY(BlueprintReadWrite)
	TArray<FName> ExplorationFlags;

	UPROPERTY(BlueprintReadWrite)
	int64 Revision = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FHSRMapStateChanged, const FHSRMapRuntimeSnapshot&);
