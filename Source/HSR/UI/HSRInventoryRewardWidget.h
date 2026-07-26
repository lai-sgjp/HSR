#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HSRInventoryRewardTypes.h"
#include "HSRInventoryRewardWidget.generated.h"

class UHSRInventoryRewardViewModel;

UCLASS(Abstract, Blueprintable)
class HSR_API UHSRInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "HSR|Inventory")
	void SetViewModel(UHSRInventoryRewardViewModel* InViewModel);
	UFUNCTION(BlueprintPure, Category = "HSR|Inventory")
	bool GetCurrentSnapshot(FHSRInventorySnapshot& OutSnapshot) const;
	UFUNCTION(BlueprintImplementableEvent, Category = "HSR|Inventory")
	void OnInventorySnapshotChanged(const FHSRInventorySnapshot& Snapshot);

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

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void HandleSnapshot(const FHSRInventoryRewardSnapshot& InSnapshot);
	void BindAndRefresh();
	UPROPERTY(Transient)
	TObjectPtr<UHSRInventoryRewardViewModel> ViewModel;
	FDelegateHandle Subscription;
	TArray<FHSRRewardReceipt> Current;
	bool bHasSnapshot = false;
};
