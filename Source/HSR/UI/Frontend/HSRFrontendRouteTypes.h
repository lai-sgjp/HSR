#pragma once

#include "CoreMinimal.h"
#include "HSRFrontendRouteTypes.generated.h"

UENUM(BlueprintType)
enum class EHSRFrontendModule : uint8
{
	None,
	PauseHub,
	Character,
	Inventory,
	Party,
	Map,
	Challenge,
	Save
};

UENUM(BlueprintType)
enum class EHSRFrontendRouteOperation : uint8
{
	OpenModule,
	Back,
	CloseToRoot
};

UENUM(BlueprintType)
enum class EHSRFrontendRouteResult : uint8
{
	Success,
	NoOp,
	AlreadyProcessed,
	InvalidRequest,
	InvalidModule,
	NothingOpen,
	StaleRequest
};

USTRUCT(BlueprintType)
struct FHSRFrontendRoute
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	EHSRFrontendModule Module = EHSRFrontendModule::None;

	UPROPERTY(BlueprintReadOnly)
	FName PageId = NAME_None;

	bool operator==(const FHSRFrontendRoute& Other) const
	{
		return Module == Other.Module && PageId == Other.PageId;
	}
};

USTRUCT(BlueprintType)
struct FHSRFrontendRouteRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int64 RequestToken = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EHSRFrontendRouteOperation Operation = EHSRFrontendRouteOperation::OpenModule;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FHSRFrontendRoute Route;
};

USTRUCT(BlueprintType)
struct FHSRFrontendRouteSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TArray<FHSRFrontendRoute> History;

	UPROPERTY(BlueprintReadOnly)
	int64 LastProcessedRequestToken = 0;

	bool IsOpen() const { return !History.IsEmpty(); }

	FHSRFrontendRoute GetActiveRoute() const
	{
		return History.IsEmpty() ? FHSRFrontendRoute{} : History.Last();
	}
};
