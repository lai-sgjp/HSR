#include "HSRInventoryRewardWidget.h"

#include "HSRInventoryRewardViewModel.h"

void UHSRInventoryWidget::NativeConstruct() { Super::NativeConstruct(); BindAndRefresh(); }
void UHSRInventoryWidget::NativeDestruct() { if (ViewModel) ViewModel->OnChanged().Remove(Subscription); Super::NativeDestruct(); }
void UHSRInventoryWidget::SetViewModel(UHSRInventoryRewardViewModel* InViewModel) { if (ViewModel) ViewModel->OnChanged().Remove(Subscription); ViewModel = InViewModel; if (IsConstructed()) BindAndRefresh(); }
bool UHSRInventoryWidget::GetCurrentSnapshot(FHSRInventorySnapshot& OutSnapshot) const { if (!bHasSnapshot) return false; OutSnapshot = Current; return true; }
void UHSRInventoryWidget::BindAndRefresh() { if (!ViewModel) return; Subscription = ViewModel->OnChanged().AddUObject(this, &ThisClass::HandleSnapshot); FHSRInventoryRewardSnapshot Initial; if (ViewModel->GetSnapshot(Initial)) HandleSnapshot(Initial); }
void UHSRInventoryWidget::HandleSnapshot(const FHSRInventoryRewardSnapshot& InSnapshot) { Current = InSnapshot.Inventory; bHasSnapshot = true; OnInventorySnapshotChanged(Current); }

void UHSRRewardSummaryWidget::NativeConstruct() { Super::NativeConstruct(); BindAndRefresh(); }
void UHSRRewardSummaryWidget::NativeDestruct() { if (ViewModel) ViewModel->OnChanged().Remove(Subscription); Super::NativeDestruct(); }
void UHSRRewardSummaryWidget::SetViewModel(UHSRInventoryRewardViewModel* InViewModel) { if (ViewModel) ViewModel->OnChanged().Remove(Subscription); ViewModel = InViewModel; if (IsConstructed()) BindAndRefresh(); }
bool UHSRRewardSummaryWidget::GetCurrentReceipts(TArray<FHSRRewardReceipt>& OutReceipts) const { if (!bHasSnapshot) return false; OutReceipts = Current; return true; }
void UHSRRewardSummaryWidget::BindAndRefresh() { if (!ViewModel) return; Subscription = ViewModel->OnChanged().AddUObject(this, &ThisClass::HandleSnapshot); FHSRInventoryRewardSnapshot Initial; if (ViewModel->GetSnapshot(Initial)) HandleSnapshot(Initial); }
void UHSRRewardSummaryWidget::HandleSnapshot(const FHSRInventoryRewardSnapshot& InSnapshot) { Current = InSnapshot.Receipts; bHasSnapshot = true; OnRewardSnapshotChanged(Current); }
