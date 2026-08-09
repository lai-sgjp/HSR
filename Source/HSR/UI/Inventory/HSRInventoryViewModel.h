#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "../../Equipment/HSREquipmentTypes.h"
#include "HSRInventoryTypes.h"
#include "HSRInventoryViewModel.generated.h"

class UHSRInventoryCatalog;
class UHSREquipmentEnhancementCatalog;
class UHSREquipmentSubsystem;
class UHSRInventorySubsystem;
class UHSRItemEquipmentMappingCatalog;

UCLASS(BlueprintType)
class HSR_API UHSRInventoryViewModel : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	void Initialize(UHSRInventorySubsystem* InInventory, UHSRInventoryCatalog* InCatalog);
	void SetCommandContext(UHSREquipmentSubsystem* InEquipment,
		UHSRItemEquipmentMappingCatalog* InMappingCatalog,
		UHSREquipmentEnhancementCatalog* InEnhancementCatalog, const FGuid& InCharacterId);
	void Shutdown();

	UFUNCTION(BlueprintPure, Category = "HSR|Inventory")
	bool GetSnapshot(FHSRInventoryModuleSnapshot& OutSnapshot) const;

	UFUNCTION(BlueprintCallable, Category = "HSR|Inventory")
	EHSRInventoryViewModelResult SelectCategory(EHSRInventoryCategory InCategory);

	UFUNCTION(BlueprintCallable, Category = "HSR|Inventory")
	EHSRInventoryViewModelResult SetFilterText(const FString& InFilterText);

	UFUNCTION(BlueprintCallable, Category = "HSR|Inventory")
	EHSRInventoryViewModelResult SetSortMode(EHSRInventorySortMode InSortMode);

	UFUNCTION(BlueprintCallable, Category = "HSR|Inventory")
	EHSRInventoryViewModelResult SelectEntry(const FHSRInventoryEntryKey& InKey);

	UFUNCTION(BlueprintCallable, Category = "HSR|Inventory")
	EHSRInventoryViewModelResult SubmitAction(EHSRInventoryAction Action, int32 TargetLevel = -1);

	UFUNCTION(BlueprintPure, Category = "HSR|Inventory")
	bool GetEntry(int32 Index, FHSRInventoryEntryRow& OutEntry) const;

	UFUNCTION(BlueprintPure, Category = "HSR|Inventory")
	bool GetActionState(int32 Index, FHSRInventoryActionState& OutAction) const;

	FHSRInventoryModuleChanged& OnChanged() { return Changed; }

	UPROPERTY(BlueprintAssignable, Category = "HSR|Inventory")
	FHSRInventoryModuleBlueprintChanged OnSnapshotChanged;

private:
	void HandleInventoryChanged(int64 Revision);
	void HandleEquipmentChanged(const FGuid& ChangedCharacterId, int32 Revision);
	void Rebuild();
	void PublishFailure(EHSRInventoryViewModelResult Result) const;
	EHSRInventoryViewModelResult SubmitEquip();
	EHSRInventoryViewModelResult SubmitEnhancement(int32 TargetLevel);
	void BuildActionStates(FHSRInventoryModuleSnapshot& InOutSnapshot,
		const FHSRInventorySnapshot& InventorySnapshot) const;
	void BuildEnhancementOptions(FHSRInventoryModuleSnapshot& InOutSnapshot,
		const FHSRInventorySnapshot& InventorySnapshot) const;
	static EHSRInventoryViewModelResult MapMovementResult(EHSREquipmentMovementResultCode Code);
	static EHSRInventoryViewModelResult MapEnhancementResult(EHSREquipmentEnhancementResultCode Code);
	static int32 FindStackQuantity(const FHSRInventorySnapshot& InventorySnapshot, FName ItemId);
	EHSRInventoryViewModelResult ApplyPresentationState(EHSRInventoryCategory InCategory,
		const FString& InFilterText, EHSRInventorySortMode InSortMode,
		const FHSRInventoryEntryKey& InSelectedKey);
	EHSRInventoryViewModelResult BuildSnapshot(FHSRInventoryModuleSnapshot& OutSnapshot,
		EHSRInventoryCategory InCategory, const FString& InFilterText,
		EHSRInventorySortMode InSortMode, const FHSRInventoryEntryKey& InSelectedKey) const;
	bool IsInitialized() const;
	static bool IsValidCategory(EHSRInventoryCategory InCategory);
	static bool IsValidSortMode(EHSRInventorySortMode InSortMode);
	static bool AreKeysEqual(const FHSRInventoryEntryKey& A, const FHSRInventoryEntryKey& B);

	TWeakObjectPtr<UHSRInventorySubsystem> Inventory;
	TWeakObjectPtr<UHSRInventoryCatalog> Catalog;
	TWeakObjectPtr<UHSREquipmentSubsystem> Equipment;
	TWeakObjectPtr<UHSRItemEquipmentMappingCatalog> MappingCatalog;
	TWeakObjectPtr<UHSREquipmentEnhancementCatalog> EnhancementCatalog;
	FDelegateHandle InventoryHandle;
	FDelegateHandle EquipmentHandle;
	FGuid CharacterId;
	EHSRInventoryCategory Category = EHSRInventoryCategory::Other;
	FString FilterText;
	EHSRInventorySortMode SortMode = EHSRInventorySortMode::CatalogOrder;
	FHSRInventoryModuleSnapshot Snapshot;
	bool bHasSnapshot = false;
	FHSRInventoryModuleChanged Changed;
};
