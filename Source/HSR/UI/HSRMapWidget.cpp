#include "HSRMapWidget.h"

#include "HSRMapViewModel.h"
#include "../Map/HSRMapSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"

// SetViewModel：外部注入一个已有的地图 ViewModel（外部拥有其生命周期）。
void UHSRMapWidget::SetViewModel(UHSRMapViewModel* InViewModel)
{
	Unbind();
	ViewModel = InViewModel;
	// 外部注入的 VM 不由本控件负责 Shutdown。
	bOwnsViewModel = false;
	BindAndRefresh();
}

// GetCurrentSnapshot：输出控件缓存的最近一次地图快照；尚无快照时返回 false。
bool UHSRMapWidget::GetCurrentSnapshot(FHSRMapRuntimeSnapshot& OutSnapshot) const
{
	if (!bHasSnapshot)
	{
		return false;
	}
	OutSnapshot = Current;
	return true;
}

// RequestTeleport：请求传送到指定传送点（转发给 ViewModel，无 VM 时返回失败）。
EHSRMapOperationResult UHSRMapWidget::RequestTeleport(const FName TeleportId)
{
	return ViewModel ? ViewModel->RequestTeleport(TeleportId) : EHSRMapOperationResult::InvalidWorld;
}

// GetMapDisplayName：获取地图的显示名（转发给 ViewModel，无 VM 时退回用 MapId 当名字）。
FText UHSRMapWidget::GetMapDisplayName(const FName MapId) const
{
	return ViewModel ? ViewModel->GetMapDisplayName(MapId) : FText::FromName(MapId);
}

// GetAvailableTeleports：输出所有可用传送点的投影信息，供蓝图构建传送列表。
void UHSRMapWidget::GetAvailableTeleports(TArray<FHSRTeleportProjection>& OutTeleports) const
{
	OutTeleports.Reset();
	if (ViewModel)
	{
		ViewModel->GetAvailableTeleports(OutTeleports);
	}
}

// GetReachableTeleportCount：当前可达传送点数量（用于面板只显示可达项）。
int32 UHSRMapWidget::GetReachableTeleportCount() const
{
	return ViewModel ? ViewModel->GetReachableTeleportCount() : 0;
}

// GetReachableTeleport：按下标读取某个可达传送点的投影信息。
bool UHSRMapWidget::GetReachableTeleport(const int32 Index, FHSRTeleportProjection& OutTeleport) const
{
	return ViewModel && ViewModel->GetReachableTeleport(Index, OutTeleport);
}

// RefreshReachableTeleportPanel：手动刷新"当前地图标签 + 两个传送按钮"面板。
// 一般由 HandleSnapshot 驱动；也可由蓝图在需要时手动调用。
void UHSRMapWidget::RefreshReachableTeleportPanel()
{
	// Current-map label.
	// 更新"当前地图"标签：取缓存快照中的当前地图 ID 转成显示名写入文本控件。
	if (UWidgetTree* Tree = WidgetTree)
	{
		if (UTextBlock* CurrentMapText = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_CurrentMap"))))
		{
			const FName CurrentMapId = Current.CurrentLocation.MapId;
			CurrentMapText->SetText(GetMapDisplayName(CurrentMapId));
		}
	}

	// Two named teleport buttons driven by the reachable-teleport projection.
	// 两个命名传送按钮由"可达传送点投影"驱动：有可达项时填充标签并启用，否则清空标签。
	const TCHAR* TextNames[2] = { TEXT("TXT_TeleportAB"), TEXT("TXT_TeleportBA") };
	const TCHAR* ButtonNames[2] = { TEXT("BTN_TeleportAB"), TEXT("BTN_TeleportBA") };
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FHSRTeleportProjection Reachable;
		const bool bFound = GetReachableTeleport(Index, Reachable);
		if (UWidgetTree* Tree = WidgetTree)
		{
			if (UTextBlock* Label = Cast<UTextBlock>(Tree->FindWidget(TextNames[Index])))
			{
				Label->SetText(bFound ? Reachable.DestinationDisplayName : FText::GetEmpty());
			}
			if (UButton* Button = Cast<UButton>(Tree->FindWidget(ButtonNames[Index])))
			{
				Button->SetIsEnabled(bFound && Reachable.bUsable);
			}
		}
	}
}

// RequestReachableTeleport：请求传送到面板上第 Index 个可达传送点。
EHSRMapOperationResult UHSRMapWidget::RequestReachableTeleport(const int32 Index)
{
	FHSRTeleportProjection Reachable;
	if (!GetReachableTeleport(Index, Reachable))
	{
		return EHSRMapOperationResult::UnknownTeleport;
	}
	return RequestTeleport(Reachable.TeleportId);
}

// NativeConstruct：控件入树时若无外部注入的 VM，则自建一个并绑定到地图子系统。
void UHSRMapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!ViewModel)
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UHSRMapSubsystem* Maps = GameInstance->GetSubsystem<UHSRMapSubsystem>())
			{
				ViewModel = NewObject<UHSRMapViewModel>(this);
				ViewModel->Initialize(Maps);
				bOwnsViewModel = true;
			}
		}
	}
	BindAndRefresh();
}

// NativeDestruct：控件出树时解绑订阅并清理自建 VM。
void UHSRMapWidget::NativeDestruct()
{
	Unbind();
	if (bOwnsViewModel && ViewModel)
	{
		ViewModel->Shutdown();
	}
	ViewModel = nullptr;
	bOwnsViewModel = false;
	Super::NativeDestruct();
}

// BindAndRefresh：订阅 ViewModel 的 Changed 事件，并立即拉取一次快照完成初次显示。
void UHSRMapWidget::BindAndRefresh()
{
	if (!ViewModel || Subscription.IsValid())
	{
		return;
	}
	Subscription = ViewModel->OnChanged().AddUObject(this, &ThisClass::HandleSnapshot);
	FHSRMapRuntimeSnapshot Snapshot;
	if (ViewModel->GetSnapshot(Snapshot))
	{
		HandleSnapshot(Snapshot);
	}
}

// Unbind：解除订阅并复位订阅句柄，保证控件可被安全复用。
void UHSRMapWidget::Unbind()
{
	if (ViewModel && Subscription.IsValid())
	{
		ViewModel->OnChanged().Remove(Subscription);
	}
	Subscription.Reset();
}

// HandleSnapshot：ViewModel 广播新快照时的回调——缓存快照、推送蓝图事件并刷新面板。
void UHSRMapWidget::HandleSnapshot(const FHSRMapRuntimeSnapshot& InSnapshot)
{
	Current = InSnapshot;
	bHasSnapshot = true;
	OnMapSnapshotChanged(Current);
	RefreshReachableTeleportPanel();
}
