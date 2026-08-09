#pragma once

#include "CoreMinimal.h"
#include "../../Inventory/HSRItemTypes.h"
#include "HSRInventoryTypes.generated.h"

UENUM(BlueprintType)
enum class EHSRInventoryCategory : uint8
{
	Weapon,
	Relic,
	Consumable,
	Material,
	Other
};

UENUM(BlueprintType)
enum class EHSRInventorySortMode : uint8
{
	CatalogOrder,
	DisplayNameAscending,
	QuantityDescending
};

UENUM(BlueprintType)
enum class EHSRInventoryAction : uint8
{
	Use,
	Equip,
	Enhance,
	Disassemble
};

UENUM(BlueprintType)
enum class EHSRInventoryViewModelResult : uint8
{
	Success,
	NotInitialized,
	CatalogUnavailable,
	InvalidCatalog,
	InvalidCategory,
	InvalidSortMode,
	EntryUnavailable,
	AuthorityUnavailable,
	AuthorityRejected,
	StaleSnapshot,
	InvalidTargetLevel,
	NoEnhancementOption
};

USTRUCT(BlueprintType)
struct HSR_API FHSRInventoryCatalogEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	EHSRInventoryCategory Category = EHSRInventoryCategory::Other;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	int32 SortOrder = 0;
};

USTRUCT(BlueprintType)
struct HSR_API FHSRInventoryEntryKey
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FGuid InstanceId;

	bool IsValid() const { return !ItemId.IsNone(); }
	bool IsUnique() const { return InstanceId.IsValid(); }

	bool operator==(const FHSRInventoryEntryKey& Other) const
	{
		return ItemId == Other.ItemId && InstanceId == Other.InstanceId;
	}
};

USTRUCT(BlueprintType)
struct HSR_API FHSRInventoryEntryRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FHSRInventoryEntryKey Key;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FName DefinitionId;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	EHSRInventoryCategory Category = EHSRInventoryCategory::Other;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 Quantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 MaxStack = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 SortOrder = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	bool bIsUnique = false;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FHSRItemInstance UniqueInstance;
};

USTRUCT(BlueprintType)
struct HSR_API FHSRInventoryDetailSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	bool bHasSelection = false;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FHSRInventoryEntryRow Entry;
};

USTRUCT(BlueprintType)
struct HSR_API FHSRInventoryActionState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	EHSRInventoryAction Action = EHSRInventoryAction::Use;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	bool bIsAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	EHSRInventoryViewModelResult UnavailableReason =
		EHSRInventoryViewModelResult::AuthorityUnavailable;
};

USTRUCT(BlueprintType)
struct HSR_API FHSRInventoryEnhancementOption
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 TargetLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FName MaterialItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 MaterialCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	bool bAffordable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	bool bAvailable = false;
};

USTRUCT(BlueprintType)
struct HSR_API FHSRInventoryModuleSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	EHSRInventoryCategory Category = EHSRInventoryCategory::Other;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FString FilterText;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	EHSRInventorySortMode SortMode = EHSRInventorySortMode::CatalogOrder;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FHSRInventoryEntryKey SelectedKey;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int64 InventoryRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 EquipmentRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TArray<FHSRInventoryEntryRow> Entries;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FHSRInventoryDetailSnapshot Detail;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TArray<FHSRInventoryActionState> Actions;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TArray<FHSRInventoryEnhancementOption> EnhancementOptions;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	bool bIsValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	EHSRInventoryViewModelResult FailureReason = EHSRInventoryViewModelResult::NotInitialized;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FHSRInventoryModuleChanged,
	const FHSRInventoryModuleSnapshot& /* Snapshot */);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSRInventoryModuleBlueprintChanged,
	const FHSRInventoryModuleSnapshot&, Snapshot);
