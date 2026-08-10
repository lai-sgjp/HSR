#pragma once

#include "CoreMinimal.h"
#include "HSRItemTypes.generated.h"

UENUM(BlueprintType)
enum class EHSRItemStorageKind : uint8
{
	Stackable,
	Unique
};

UENUM(BlueprintType)
enum class EHSRInventoryOperationResult : uint8
{
	Success,
	NoOp,
	InvalidDefinitionId,
	DuplicateDefinitionId,
	UnknownDefinition,
	InvalidDefinition,
	InvalidQuantity,
	QuantityOverflow,
	StackLimitExceeded,
	CapacityExceeded,
	InvalidInstanceId,
	DuplicateInstanceId,
	InstanceNotFound,
	InsufficientQuantity,
	StorageKindMismatch,
	RevisionConflict
};

USTRUCT(BlueprintType)
struct FHSRItemInstance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	FGuid InstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	FName DefinitionId;
};

USTRUCT(BlueprintType)
struct FHSRInventoryGrant
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 Quantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TArray<FGuid> InstanceIds;
};

USTRUCT(BlueprintType)
struct FHSRItemStackSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 Quantity = 0;
};

USTRUCT(BlueprintType)
struct FHSRInventorySnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TArray<FHSRItemStackSnapshot> Stacks;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TArray<FHSRItemInstance> UniqueItems;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 Capacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 UsedSlots = 0;

	/**
	 * How many of ItemId this snapshot holds; 0 when absent. Single definition of the affordability
	 * question, which battle preflight, the battle re-check, and the relic UI all ask.
	 */
	int32 GetStackQuantity(FName ItemId) const
	{
		const FHSRItemStackSnapshot* Stack = Stacks.FindByPredicate(
			[ItemId](const FHSRItemStackSnapshot& Candidate) { return Candidate.ItemId == ItemId; });
		return Stack ? Stack->Quantity : 0;
	}

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int64 Revision = 0;
};

USTRUCT(BlueprintType)
struct FHSRInventorySaveData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Save")
	TArray<FHSRItemStackSnapshot> Stacks;

	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Save")
	TArray<FHSRItemInstance> UniqueItems;

	UPROPERTY(BlueprintReadWrite, Category = "Inventory|Save")
	int64 Revision = 0;
};

struct FHSRInventoryRestoreState
{
	TMap<FName, int32> Stacks;
	TMap<FGuid, FHSRItemInstance> UniqueItems;
	int64 Revision = 0;
};

struct FHSRInventoryMovementCandidate
{
	TMap<FName, int32> Stacks;
	TMap<FGuid, FHSRItemInstance> UniqueItems;
	int64 NextRevision = 0;
};

struct FHSRInventoryEnhancementCandidate
{
	TMap<FName, int32> Stacks;
	TMap<FGuid, FHSRItemInstance> UniqueItems;
	int64 NextRevision = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FHSRInventoryChanged, int64 /* Revision */);
