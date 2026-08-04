#include "HSRPartyWidget.h"

#include "../Party/HSRPartySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "Engine/GameInstance.h"

void UHSRPartyWidget::SetViewModel(UHSRPartyViewModel* InViewModel)
{
	Unbind();
	if (bOwnsViewModel && ViewModel) ViewModel->Shutdown();
	ViewModel = InViewModel;
	bOwnsViewModel = false;
	BindAndRefresh();
}

bool UHSRPartyWidget::GetCurrentSnapshot(FHSRPartyFrontendSnapshot& OutSnapshot) const
{
	if (!bHasSnapshot) return false;
	OutSnapshot = Current;
	return true;
}

void UHSRPartyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!ViewModel)
	{
		UGameInstance* GameInstance = GetGameInstance();
		UHSRPartySubsystem* Party = GameInstance ? GameInstance->GetSubsystem<UHSRPartySubsystem>() : nullptr;
		UHSRCharacterProfileSubsystem* Profiles = GameInstance ? GameInstance->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
		ViewModel = NewObject<UHSRPartyViewModel>(this);
		ViewModel->Initialize(Party, Profiles);
		bOwnsViewModel = true;
	}
	BindAndRefresh();
}

void UHSRPartyWidget::NativeDestruct()
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

void UHSRPartyWidget::BindAndRefresh()
{
	if (!ViewModel || Subscription.IsValid()) return;
	Subscription = ViewModel->OnChanged().AddUObject(this, &ThisClass::HandleSnapshot);
#if WITH_DEV_AUTOMATION_TESTS
	++BindCount;
#endif
	FHSRPartyFrontendSnapshot Snapshot;
	if (ViewModel->GetSnapshot(Snapshot)) HandleSnapshot(Snapshot);
}

void UHSRPartyWidget::Unbind()
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
	Current = FHSRPartyFrontendSnapshot();
}

void UHSRPartyWidget::HandleSnapshot(const FHSRPartyFrontendSnapshot& InSnapshot)
{
	Current = InSnapshot;
	bHasSnapshot = true;
	OnPartySnapshotChanged(Current);
}
