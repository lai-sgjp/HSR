#include "HSRMapWidget.h"

#include "HSRMapViewModel.h"
#include "../Map/HSRMapSubsystem.h"
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
}
