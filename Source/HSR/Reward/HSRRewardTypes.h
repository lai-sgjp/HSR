#pragma once

#include "CoreMinimal.h"
#include "../Inventory/HSRItemTypes.h"
#include "HSRRewardTypes.generated.h"

USTRUCT(BlueprintType)
struct FHSRRewardItemEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward", meta = (ClampMin = "1"))
	int32 Quantity = 1;
};

USTRUCT(BlueprintType)
struct FHSRDropTableEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop", meta = (ClampMin = "1"))
	int32 MinQuantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop", meta = (ClampMin = "1"))
	int32 MaxQuantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop", meta = (ClampMin = "1"))
	int32 Weight = 1;
};

USTRUCT(BlueprintType)
struct FHSRRewardRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	FGuid ClaimId;

	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	FName RewardDefinitionId;

	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	int32 Seed = 0;
};

USTRUCT(BlueprintType)
struct FHSRRewardReceipt
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	FHSRRewardRequest Request;

	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	TArray<FHSRInventoryGrant> Grants;

	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	int64 Revision = 0;
};

USTRUCT(BlueprintType)
struct FHSRRewardSaveData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Reward|Save")
	TArray<FHSRRewardReceipt> Receipts;

	UPROPERTY(BlueprintReadWrite, Category = "Reward|Save")
	int64 Revision = 0;
};

struct FHSRRewardRestoreState
{
	TMap<FGuid, FHSRRewardReceipt> Receipts;
	int64 Revision = 0;
};

UENUM(BlueprintType)
enum class EHSRRewardOperationResult : uint8
{
	Success,
	NoOp,
	InvalidClaimId,
	InvalidDefinitionId,
	DuplicateDefinitionId,
	UnknownRewardDefinition,
	UnknownDropTable,
	InvalidDefinition,
	ClaimConflict,
	ResolveFailed,
	InventoryRejected,
	InjectedFailure
};

struct FHSRRewardDefinitionRule
{
	FName RewardDefinitionId;
	TArray<FHSRRewardItemEntry> FixedItems;
	FName DropTableId;
	int32 DropRolls = 0;
};

struct FHSRDropTableRule
{
	FName DropTableId;
	TArray<FHSRDropTableEntry> Entries;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FHSRRewardCommitted, const FHSRRewardReceipt&);
DECLARE_MULTICAST_DELEGATE_OneParam(FHSRRewardRestored, int64);
