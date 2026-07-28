#include "HSRScreenWidget.h"
#include "HSRUIManagerSubsystem.h"
#include "InputCoreTypes.h"

void UHSRScreenWidget::SetOwningUIManager(UHSRUIManagerSubsystem* Manager)
{
	OwningUIManager = Manager;
}

bool UHSRScreenWidget::RequestBack()
{
	if (UHSRUIManagerSubsystem* Manager = OwningUIManager.Get())
	{
		Manager->RequestBack();
		return true;
	}
	return false;
}

bool UHSRScreenWidget::SubmitCloseToRoot()
{
	if (UHSRUIManagerSubsystem* Manager = OwningUIManager.Get())
	{
		return Manager->CloseFrontendToRoot() == EHSRUIScreenResult::Success;
	}
	return false;
}

FReply UHSRScreenWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (ShouldConsumeCloseToRootKey(InKeyEvent.GetKey()))
	{
		SubmitCloseToRoot();
		return FReply::Handled();
	}
	if (ShouldConsumeBackKey(InKeyEvent.GetKey()))
	{
		RequestBack();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

bool UHSRScreenWidget::ShouldConsumeCloseToRootKey(const FKey& Key) const
{
	return OwningUIManager.IsValid() && Key == EKeys::X;
}

bool UHSRScreenWidget::ShouldConsumeBackKey(const FKey& Key) const
{
	return OwningUIManager.IsValid() && (Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right);
}

#if WITH_DEV_AUTOMATION_TESTS
bool UHSRScreenWidget::ShouldConsumeBackKeyForAutomation(const FKey& Key) const
{
	return ShouldConsumeBackKey(Key);
}

bool UHSRScreenWidget::ShouldConsumeCloseToRootKeyForAutomation(const FKey& Key) const
{
	return ShouldConsumeCloseToRootKey(Key);
}

bool UHSRScreenWidget::RouteCloseToRootKeyForAutomation(const FKey& Key)
{
	return ShouldConsumeCloseToRootKey(Key) && SubmitCloseToRoot();
}
#endif
