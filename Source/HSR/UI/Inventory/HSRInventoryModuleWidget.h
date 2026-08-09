#pragma once

#include "CoreMinimal.h"
#include "../HSRScreenWidget.h"
#include "HSRInventoryTypes.h"
#include "HSRInventoryModuleWidget.generated.h"

class UHSRInventoryCatalog;
class UHSREquipmentEnhancementCatalog;
class UHSRItemEquipmentMappingCatalog;
class UHSRInventoryViewModel;
class UHSRInventoryModuleWidget;
class UButton;
class UTextBlock;

/**
 * Small click bridge so dynamically-created list row buttons can forward their
 * stable row identity back to the module widget without capturing indices in
 * dynamic delegates (which cannot bind lambdas with captures).
 */
UCLASS()
class HSR_API UHSRInventoryRowClickBridge : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UHSRInventoryModuleWidget* InOwner, int32 InRowIndex);

	UFUNCTION()
	void HandleClicked();

private:
	TWeakObjectPtr<UHSRInventoryModuleWidget> Owner;
	int32 RowIndex = -1;
};

UCLASS(Blueprintable)
class HSR_API UHSRInventoryModuleWidget : public UHSRScreenWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "HSR|Inventory")
	void InitializeForInventory(UHSRInventoryCatalog* InCatalog = nullptr);

	UFUNCTION(BlueprintCallable, Category = "HSR|Inventory")
	void SetViewModel(UHSRInventoryViewModel* InViewModel);

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

	UFUNCTION(BlueprintCallable, Category = "HSR|Inventory")
	void InitializeCommandContext(const FGuid& InCharacterId,
		UHSRItemEquipmentMappingCatalog* InMappingCatalog = nullptr,
		UHSREquipmentEnhancementCatalog* InEnhancementCatalog = nullptr);

	UFUNCTION(BlueprintCallable, Category = "HSR|Inventory|Navigation")
	bool RequestCloseToRoot();

	UFUNCTION(BlueprintPure, Category = "HSR|Inventory")
	bool GetCurrentSnapshot(FHSRInventoryModuleSnapshot& OutSnapshot) const;

	UFUNCTION(BlueprintPure, Category = "HSR|Inventory")
	bool GetEntry(int32 Index, FHSRInventoryEntryRow& OutEntry) const;

	UFUNCTION(BlueprintPure, Category = "HSR|Inventory")
	bool GetActionState(int32 Index, FHSRInventoryActionState& OutAction) const;

	UFUNCTION(BlueprintPure, Category = "HSR|Inventory")
	int32 GetEntryCount() const;

	UFUNCTION(BlueprintPure, Category = "HSR|Inventory")
	bool GetEntryDisplay(int32 Index, FString& OutDisplayName, int32& OutQuantity,
		bool& bOutIsUnique) const;

	UFUNCTION(BlueprintPure, Category = "HSR|Inventory")
	bool GetSelectedDetail(FString& OutName, int32& OutQuantity, bool& bOutHasSelection) const;

	UFUNCTION(BlueprintCallable, Category = "HSR|Inventory")
	EHSRInventoryViewModelResult SelectEntryByIndex(int32 Index);

	UFUNCTION(BlueprintPure, Category = "HSR|Inventory|Action")
	bool GetActionAvailable(EHSRInventoryAction Action) const;

	UFUNCTION(BlueprintPure, Category = "HSR|Inventory|Enhance")
	int32 GetSelectedEnhancementTargetLevel() const;

	/** Rebuilds the list rows, detail text, and action button enablement from the current snapshot. */
	UFUNCTION(BlueprintCallable, Category = "HSR|Inventory")
	void RefreshListAndDetail();

	UFUNCTION(BlueprintImplementableEvent, Category = "HSR|Inventory")
	void OnInventorySnapshotChanged(const FHSRInventoryModuleSnapshot& Snapshot);

	UFUNCTION(BlueprintImplementableEvent, Category = "HSR|Inventory")
	void OnInventoryUnavailable(EHSRInventoryViewModelResult Result);

#if WITH_DEV_AUTOMATION_TESTS
	void AttachForAutomation() { BindAndRefresh(); }
	int32 GetBindCountForAutomation() const { return BindCount; }
	int32 GetUnbindCountForAutomation() const { return UnbindCount; }
#endif

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void InitializeRuntimeContext();
	void BindAndRefresh();
	void HandleSnapshot(const FHSRInventoryModuleSnapshot& InSnapshot);
	void PopulateListAndDetail();
	void PopulateListRows();
	UButton* FindButtonByName(const FName Name) const;
	UTextBlock* FindTextByName(const FName Name) const;
	void SetActionButton(const FName ButtonName, const EHSRInventoryAction Action);
	UFUNCTION()
	void HandleBackClicked();
	UFUNCTION()
	void HandleCloseClicked();
	UFUNCTION()
	void HandleUseClicked();
	UFUNCTION()
	void HandleEquipClicked();
	UFUNCTION()
	void HandleEnhanceClicked();
	UFUNCTION()
	void HandleDisassembleClicked();
	UPROPERTY(Transient)
	TArray<TObjectPtr<UHSRInventoryRowClickBridge>> RowBridges;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HSR|Inventory",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHSRInventoryCatalog> Catalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HSR|Inventory",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHSRItemEquipmentMappingCatalog> MappingCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HSR|Inventory",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHSREquipmentEnhancementCatalog> EnhancementCatalog;

	UPROPERTY(Transient)
	TObjectPtr<UHSRInventoryViewModel> ViewModel;

	FGuid CharacterId;
	FDelegateHandle SnapshotHandle;
	FHSRInventoryModuleSnapshot CurrentSnapshot;
	bool bHasSnapshot = false;
#if WITH_DEV_AUTOMATION_TESTS
	int32 BindCount = 0;
	int32 UnbindCount = 0;
#endif
};
