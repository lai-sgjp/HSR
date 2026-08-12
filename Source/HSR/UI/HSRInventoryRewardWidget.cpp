#include "HSRInventoryRewardWidget.h"

#include "HSRInventoryRewardViewModel.h"

// UHSRInventoryWidget：背包面板 Widget。
// 它从 UHSRInventoryRewardViewModel 读取快照（背包部分），订阅 VM 的 OnChanged 事件，
// 收到新快照后转成当前背包快照并触发蓝图事件 OnInventorySnapshotChanged 刷新显示。
void UHSRInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindAndRefresh();
}

// 销毁时先解除 ViewModel 绑定，避免销毁后仍收到 VM 回调。
void UHSRInventoryWidget::NativeDestruct()
{
	SetViewModel(nullptr);
	Super::NativeDestruct();
}

// 设置/替换 ViewModel：替换前先移除旧 VM 的订阅，替换后再按需重新绑定并刷新。
void UHSRInventoryWidget::SetViewModel(UHSRInventoryRewardViewModel* InViewModel)
{
	// 旧 VM 存在且有订阅时先移除订阅，并复位订阅句柄。
	if (ViewModel && Subscription.IsValid())
	{
		ViewModel->OnChanged().Remove(Subscription);
		Subscription.Reset();
#if WITH_DEV_AUTOMATION_TESTS
		++UnbindCount;
#endif
	}
	ViewModel = InViewModel;
	// 仅当 Widget 已构造完成时才绑定并刷新（构造前绑定没有意义）。
	if (IsConstructed())
	{
		BindAndRefresh();
	}
}

// 取当前背包快照：未就绪时返回 false，就绪时写入出参。
bool UHSRInventoryWidget::GetCurrentSnapshot(FHSRInventorySnapshot& OutSnapshot) const
{
	if (!bHasSnapshot)
	{
		return false;
	}
	OutSnapshot = Current;
	return true;
}

// 建立订阅并立即刷新一次：订阅 VM 的 OnChanged，然后拉取当前快照同步显示。
void UHSRInventoryWidget::BindAndRefresh()
{
	if (!ViewModel)
	{
		return;
	}
	// 若已有旧订阅，先移除再重建（防止重复订阅导致回调翻倍）。
	if (Subscription.IsValid())
	{
		ViewModel->OnChanged().Remove(Subscription);
		Subscription.Reset();
#if WITH_DEV_AUTOMATION_TESTS
		++UnbindCount;
#endif
	}
	Subscription = ViewModel->OnChanged().AddUObject(this, &ThisClass::HandleSnapshot);
#if WITH_DEV_AUTOMATION_TESTS
	++BindCount;
#endif
	// 立即用当前快照初始化一次，避免界面等待下一次变化才显示。
	FHSRInventoryRewardSnapshot Initial;
	if (ViewModel->GetSnapshot(Initial))
	{
		HandleSnapshot(Initial);
	}
}

// VM 快照回调：只取背包部分存为 Current，然后通知蓝图事件刷新显示。
void UHSRInventoryWidget::HandleSnapshot(const FHSRInventoryRewardSnapshot& InSnapshot)
{
	Current = InSnapshot.Inventory;
	bHasSnapshot = true;
	OnInventorySnapshotChanged(Current);
}

// UHSRRewardSummaryWidget：奖励摘要 Widget。
// 与背包面板同源，但只消费快照中的“奖励凭证列表”部分，
// 变化时触发 OnRewardSnapshotChanged 供蓝图刷新奖励展示。
void UHSRRewardSummaryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindAndRefresh();
}

// 销毁时移除订阅；这里直接移除（ViewModel 生命周期由外部管理，如 HUD）。
void UHSRRewardSummaryWidget::NativeDestruct()
{
	if (ViewModel)
	{
		ViewModel->OnChanged().Remove(Subscription);
	}
	Super::NativeDestruct();
}

// 设置/替换 ViewModel：先移除旧订阅，再设置新 VM，按需重新绑定刷新。
void UHSRRewardSummaryWidget::SetViewModel(UHSRInventoryRewardViewModel* InViewModel)
{
	if (ViewModel)
	{
		ViewModel->OnChanged().Remove(Subscription);
	}
	ViewModel = InViewModel;
	if (IsConstructed())
	{
		BindAndRefresh();
	}
}

// 取当前奖励凭证列表：未就绪返回 false，就绪时写入出参。
bool UHSRRewardSummaryWidget::GetCurrentReceipts(TArray<FHSRRewardReceipt>& OutReceipts) const
{
	if (!bHasSnapshot)
	{
		return false;
	}
	OutReceipts = Current;
	return true;
}

// 建立订阅并立即刷新一次（与背包面板的 BindAndRefresh 同理）。
void UHSRRewardSummaryWidget::BindAndRefresh()
{
	if (!ViewModel)
	{
		return;
	}
	Subscription = ViewModel->OnChanged().AddUObject(this, &ThisClass::HandleSnapshot);
	FHSRInventoryRewardSnapshot Initial;
	if (ViewModel->GetSnapshot(Initial))
	{
		HandleSnapshot(Initial);
	}
}

// VM 快照回调：只取奖励凭证部分，并通知蓝图事件。
void UHSRRewardSummaryWidget::HandleSnapshot(const FHSRInventoryRewardSnapshot& InSnapshot)
{
	Current = InSnapshot.Receipts;
	bHasSnapshot = true;
	OnRewardSnapshotChanged(Current);
}
