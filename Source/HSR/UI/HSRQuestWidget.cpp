#include "HSRQuestWidget.h"

#include "../Quest/HSRQuestSubsystem.h"
#include "Engine/GameInstance.h"

void UHSRQuestWidget::SetViewModel(UHSRQuestViewModel* InViewModel)
{
	Unbind();
	if (bOwnsViewModel && ViewModel)
	{
		ViewModel->Shutdown();
	}
	ViewModel = InViewModel;
	bOwnsViewModel = false;
	BindAndRefresh();
}

bool UHSRQuestWidget::GetCurrentSnapshot(FHSRQuestFrontendSnapshot& OutSnapshot) const
{
	if (!bHasSnapshot) return false;
	OutSnapshot = Current;
	return true;
}

void UHSRQuestWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!ViewModel)
	{
		UGameInstance* GameInstance = GetGameInstance();
		UHSRQuestSubsystem* Quest = GameInstance ? GameInstance->GetSubsystem<UHSRQuestSubsystem>() : nullptr;
		ViewModel = NewObject<UHSRQuestViewModel>(this);
		ViewModel->Initialize(Quest);
		bOwnsViewModel = true;
	}
	BindAndRefresh();
}

void UHSRQuestWidget::NativeDestruct()
{
	Unbind();
	if (bOwnsViewModel && ViewModel)
	{
		ViewModel->Shutdown();
		ViewModel = nullptr;
		bOwnsViewModel = false;
	}
	Super::NativeDestruct();
}

void UHSRQuestWidget::BindAndRefresh()
{
	if (!ViewModel || Subscription.IsValid()) return;
	Subscription = ViewModel->OnChanged().AddUObject(this, &ThisClass::HandleSnapshot);
#if WITH_DEV_AUTOMATION_TESTS
	++BindCount;
#endif
	FHSRQuestFrontendSnapshot Snapshot;
	if (ViewModel->GetSnapshot(Snapshot)) HandleSnapshot(Snapshot);
}

void UHSRQuestWidget::Unbind()
{
	if (ViewModel && Subscription.IsValid())
	{
		ViewModel->OnChanged().Remove(Subscription);
#if WITH_DEV_AUTOMATION_TESTS
		++UnbindCount;
#endif
	}
	Subscription.Reset();
	bHasSnapshot = false;
	Current = FHSRQuestFrontendSnapshot();
}

void UHSRQuestWidget::HandleSnapshot(const FHSRQuestFrontendSnapshot& InSnapshot)
{
	Current = InSnapshot;
	bHasSnapshot = true;
	OnQuestSnapshotChanged(Current);
}
