#include "HSRInventoryRewardWidget.h"

#include "HSRInventoryRewardViewModel.h"
#include "../Data/Definitions/HSRInventoryCatalog.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"

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
	if (auto* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
	{
		CanvasSlot->SetAnchors(FAnchors(.5f, 1.f));
		CanvasSlot->SetAlignment(FVector2D(.5f, 1.f));
		CanvasSlot->SetPosition(FVector2D(0.f, -48.f));
		CanvasSlot->SetAutoSize(true);
	}
	HideNotification();
	BindAndRefresh();
}

TSharedRef<SWidget> UHSRRewardSummaryWidget::RebuildWidget()
{
	if (WidgetTree)
	{
		// Replace the legacy fixed-width text layout with one content-sized card.
		auto* Card = WidgetTree->ConstructWidget<USizeBox>();
		Card->SetWidthOverride(520.f);
		Card->SetMinDesiredHeight(84.f);
		auto* Background = WidgetTree->ConstructWidget<UBorder>();
		Background->SetPadding(FMargin(22.f, 14.f));
		Background->SetBrushColor(FLinearColor(.025f,.04f,.065f,.96f));
		auto* Text = WidgetTree->FindWidget<UTextBlock>(TEXT("TXT_Reward"));
		if (!Text) Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TXT_Reward"));
		Text->RemoveFromParent();
		Text->SetWrapTextAt(476.f);
		Background->AddChild(Text);
		Card->AddChild(Background);
		// HUD adds this widget directly to the viewport, so its own canvas must
		// anchor the card; the user widget has no parent CanvasPanelSlot there.
		auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		auto* CardSlot = Canvas->AddChildToCanvas(Card);
		CardSlot->SetAnchors(FAnchors(.5f, 1.f));
		CardSlot->SetAlignment(FVector2D(.5f, 1.f));
		CardSlot->SetPosition(FVector2D(0.f, -48.f));
		CardSlot->SetAutoSize(true);
		WidgetTree->RootWidget = Canvas;
	}
	return Super::RebuildWidget();
}

// 销毁时移除订阅；这里直接移除（ViewModel 生命周期由外部管理，如 HUD）。
void UHSRRewardSummaryWidget::NativeDestruct()
{
	SetViewModel(nullptr);
	Super::NativeDestruct();
}

// 设置/替换 ViewModel：先移除旧订阅，再设置新 VM，按需重新绑定刷新。
void UHSRRewardSummaryWidget::SetViewModel(UHSRInventoryRewardViewModel* InViewModel)
{
	if (ViewModel == InViewModel && Subscription.IsValid()) return;
	if (ViewModel && Subscription.IsValid())
	{
		ViewModel->OnChanged().Remove(Subscription);
	}
	Subscription.Reset();
	HideNotification();
	ObservedClaims.Reset();
	PendingReceipts.Reset();
	Current.Reset();
	bHasSnapshot = false;
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
	if (!ViewModel || Subscription.IsValid())
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
	TArray<FHSRRewardReceipt> NewReceipts;
	for (const FHSRRewardReceipt& Receipt : InSnapshot.Receipts)
	{
		const FGuid Claim = Receipt.Request.ClaimId;
		if (!Claim.IsValid() || ObservedClaims.Contains(Claim)) continue;
		ObservedClaims.Add(Claim);
		// The first snapshot seeds the history, including save-game receipts.
		if (bHasSnapshot) NewReceipts.Add(Receipt);
	}
	Current = InSnapshot.Receipts;
	bHasSnapshot = true;
	PendingReceipts.Append(NewReceipts);
	if (NotificationText.IsEmpty()) AdvanceNotification();
}

void UHSRRewardSummaryWidget::AdvanceNotification()
{
	HideNotification();
	if (PendingReceipts.IsEmpty()) return;
	const FHSRRewardReceipt Receipt = PendingReceipts[0];
	PendingReceipts.RemoveAt(0);
	ShowNotification({Receipt});
}

void UHSRRewardSummaryWidget::HideNotification()
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(NotificationTimer);
	NotificationTimer.Invalidate();
	VisibleQuantities.Reset();
	NotificationText = FText::GetEmpty();
	if (WidgetTree)
		if (UTextBlock* Text = WidgetTree->FindWidget<UTextBlock>(TEXT("TXT_Reward"))) Text->SetText(NotificationText);
	SetVisibility(ESlateVisibility::Collapsed);
}

FText UHSRRewardSummaryWidget::ResolveItemName(FName ItemId) const
{
	FHSRInventoryCatalogEntry Entry;
	if (Catalog && Catalog->FindEntry(ItemId, Entry) && !Entry.DisplayName.IsEmpty()) return Entry.DisplayName;
	// Scene content owns loaded item definitions; the inventory authority stores only rules.
	for (TObjectIterator<UHSRItemDefinition> It; It; ++It)
		if (!It->HasAnyFlags(RF_ClassDefaultObject) && It->ItemId == ItemId && !It->DisplayName.IsEmpty()) return It->DisplayName;
	return NSLOCTEXT("HSRReward", "UnnamedItem", "奖励物品");
}

void UHSRRewardSummaryWidget::ShowNotification(const TArray<FHSRRewardReceipt>& NewReceipts)
{
	for (const FHSRRewardReceipt& Receipt : NewReceipts)
		for (const FHSRInventoryGrant& Grant : Receipt.Grants)
			if (Grant.Quantity > 0) VisibleQuantities.FindOrAdd(Grant.ItemId) += Grant.Quantity;
	if (VisibleQuantities.IsEmpty()) return;
	TArray<FName> ItemIds;
	VisibleQuantities.GetKeys(ItemIds);
	ItemIds.Sort(FNameLexicalLess());
	TArray<FText> Lines;
	Lines.Add(NSLOCTEXT("HSRReward", "Received", "获得奖励"));
	for (FName ItemId : ItemIds)
		Lines.Add(FText::Format(NSLOCTEXT("HSRReward", "ReceivedItem", "{0} × {1}"),
			ResolveItemName(ItemId), FText::AsNumber(VisibleQuantities[ItemId])));
	NotificationText = FText::Join(FText::FromString(TEXT("\n")), Lines);
	// Preserve the visual hook, then replace the legacy Claims counter with the real grant text.
	OnRewardSnapshotChanged(NewReceipts);
	if (WidgetTree)
	{
		if (UTextBlock* Text = WidgetTree->FindWidget<UTextBlock>(TEXT("TXT_Reward")))
		{
			Text->SetText(NotificationText);
			Text->SetAutoWrapText(true);
			Text->SetColorAndOpacity(FSlateColor(FLinearColor(.88f, .83f, .65f)));
		}
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UWorld* World = GetWorld())
		World->GetTimerManager().SetTimer(NotificationTimer, this, &ThisClass::AdvanceNotification,
			FMath::Clamp(NotificationSeconds, 1.f, 15.f), false);
}
