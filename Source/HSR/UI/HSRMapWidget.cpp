#include "HSRMapWidget.h"

#include "HSRMapViewModel.h"
#include "../Map/HSRMapSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"

void UHSRMapWidget::SetViewModel(UHSRMapViewModel* InViewModel)
{
	Unbind();
	ViewModel = InViewModel;
	bOwnsViewModel = false;
	BindAndRefresh();
}

bool UHSRMapWidget::GetCurrentSnapshot(FHSRMapRuntimeSnapshot& OutSnapshot) const
{
	if (!bHasSnapshot)
	{
		return false;
	}
	OutSnapshot = Current;
	return true;
}

EHSRMapOperationResult UHSRMapWidget::RequestTeleport(const FName TeleportId)
{
	return ViewModel ? ViewModel->RequestTeleport(TeleportId) : EHSRMapOperationResult::InvalidWorld;
}

FText UHSRMapWidget::GetMapDisplayName(const FName MapId) const
{
	return ViewModel ? ViewModel->GetMapDisplayName(MapId) : FText::FromName(MapId);
}

void UHSRMapWidget::GetAvailableTeleports(TArray<FHSRTeleportProjection>& OutTeleports) const
{
	OutTeleports.Reset();
	if (ViewModel)
	{
		ViewModel->GetAvailableTeleports(OutTeleports);
	}
}

int32 UHSRMapWidget::GetReachableTeleportCount() const
{
	return ViewModel ? ViewModel->GetReachableTeleportCount() : 0;
}

bool UHSRMapWidget::GetReachableTeleport(const int32 Index, FHSRTeleportProjection& OutTeleport) const
{
	return ViewModel && ViewModel->GetReachableTeleport(Index, OutTeleport);
}

void UHSRMapWidget::RefreshReachableTeleportPanel()
{
	// Current-map label.
	if (UWidgetTree* Tree = WidgetTree)
	{
		if (UTextBlock* CurrentMapText = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_CurrentMap"))))
		{
			const FName CurrentMapId = Current.CurrentLocation.MapId;
			CurrentMapText->SetText(GetMapDisplayName(CurrentMapId));
		}
	}

	// Two named teleport buttons driven by the reachable-teleport projection.
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

EHSRMapOperationResult UHSRMapWidget::RequestReachableTeleport(const int32 Index)
{
	FHSRTeleportProjection Reachable;
	if (!GetReachableTeleport(Index, Reachable))
	{
		return EHSRMapOperationResult::UnknownTeleport;
	}
	return RequestTeleport(Reachable.TeleportId);
}

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

void UHSRMapWidget::Unbind()
{
	if (ViewModel && Subscription.IsValid())
	{
		ViewModel->OnChanged().Remove(Subscription);
	}
	Subscription.Reset();
}

void UHSRMapWidget::HandleSnapshot(const FHSRMapRuntimeSnapshot& InSnapshot)
{
	Current = InSnapshot;
	bHasSnapshot = true;
	OnMapSnapshotChanged(Current);
	RefreshReachableTeleportPanel();
}
