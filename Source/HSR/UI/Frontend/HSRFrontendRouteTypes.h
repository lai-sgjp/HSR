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
	Quest,
	Save
};

/**
 * Module classification helpers.
 *
 * These used to be open-coded as `Module >= Party && Module <= Save` range checks, which made the
 * declaration order above a load-bearing contract: inserting a module in the middle, or reordering
 * for readability, silently changed which modules were considered valid and which were considered
 * generically hosted. Both questions are now asked by name, so declaration order is free again.
 */
namespace HSRFrontendModule
{
	/** Any addressable module -- everything except the None sentinel. */
	inline bool IsAddressable(const EHSRFrontendModule Module)
	{
		return Module != EHSRFrontendModule::None;
	}

	/**
	 * True when the module is presented by the shared ModuleRoot host rather than its own bespoke
	 * widget. PauseHub is the shell's own root, and Character/Inventory predate ModuleRoot and still
	 * own dedicated widgets plus dedicated input/focus policy.
	 */
	inline bool UsesSharedModuleRoot(const EHSRFrontendModule Module)
	{
		switch (Module)
		{
		case EHSRFrontendModule::Party:
		case EHSRFrontendModule::Map:
		case EHSRFrontendModule::Challenge:
		case EHSRFrontendModule::Quest:
		case EHSRFrontendModule::Save:
			return true;
		default:
			return false;
		}
	}
}

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
