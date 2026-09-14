#pragma once

#include "CoreMinimal.h"
#include "HSRScreenWidget.h"
#include "HSRInventoryRewardTypes.h"
#include "HSRInventoryRewardWidget.generated.h"

class UHSRInventoryRewardViewModel;
class UHSRInventoryCatalog;

UCLASS(Blueprintable)
class HSR_API UHSRInventoryWidget : public UHSRScreenWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "HSR|Inventory")
	void SetViewModel(UHSRInventoryRewardViewModel* InViewModel);
	UFUNCTION(BlueprintPure, Category = "HSR|Inventory")
	bool GetCurrentSnapshot(FHSRInventorySnapshot& OutSnapshot) const;
	UFUNCTION(BlueprintImplementableEvent, Category = "HSR|Inventory")
	void OnInventorySnapshotChanged(const FHSRInventorySnapshot& Snapshot);

#if WITH_DEV_AUTOMATION_TESTS
	void AttachForAutomation() { BindAndRefresh(); }
	int32 GetBindCountForAutomation() const { return BindCount; }
	int32 GetUnbindCountForAutomation() const { return UnbindCount; }
#endif

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void HandleSnapshot(const FHSRInventoryRewardSnapshot& InSnapshot);
	void BindAndRefresh();
	UPROPERTY(Transient)
	TObjectPtr<UHSRInventoryRewardViewModel> ViewModel;
	FDelegateHandle Subscription;
	FHSRInventorySnapshot Current;
	bool bHasSnapshot = false;
#if WITH_DEV_AUTOMATION_TESTS
	int32 BindCount = 0;
	int32 UnbindCount = 0;
#endif
};

UCLASS(Abstract, Blueprintable)
class HSR_API UHSRRewardSummaryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "HSR|Reward")
	void SetViewModel(UHSRInventoryRewardViewModel* InViewModel);
	UFUNCTION(BlueprintPure, Category = "HSR|Reward")
	bool GetCurrentReceipts(TArray<FHSRRewardReceipt>& OutReceipts) const;
	UFUNCTION(BlueprintImplementableEvent, Category = "HSR|Reward")
	void OnRewardSnapshotChanged(const TArray<FHSRRewardReceipt>& Receipts);

	UFUNCTION(BlueprintPure, Category = "HSR|Reward")
	FText GetRewardNotificationText() const { return NotificationText; }

#if WITH_DEV_AUTOMATION_TESTS
	void AttachForAutomation() { BindAndRefresh(); }
	void ExpireForAutomation() { AdvanceNotification(); }
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void HandleSnapshot(const FHSRInventoryRewardSnapshot& InSnapshot);
	void BindAndRefresh();
	void HideNotification();
	void AdvanceNotification();
	void ShowNotification(const TArray<FHSRRewardReceipt>& NewReceipts);
	FText ResolveItemName(FName ItemId) const;
	UPROPERTY(EditDefaultsOnly, Category = "HSR|Reward")
	TObjectPtr<UHSRInventoryCatalog> Catalog;
	UPROPERTY(EditDefaultsOnly, Category = "HSR|Reward", meta = (ClampMin = "1.0", ClampMax = "15.0"))
	float NotificationSeconds = 5.f;
	TSet<FGuid> ObservedClaims;
	TMap<FName, int64> VisibleQuantities;
	FTimerHandle NotificationTimer;
	FText NotificationText;
	UPROPERTY(Transient)
	TObjectPtr<UHSRInventoryRewardViewModel> ViewModel;
	FDelegateHandle Subscription;
	TArray<FHSRRewardReceipt> Current;
	TArray<FHSRRewardReceipt> PendingReceipts;
	bool bHasSnapshot = false;
};
