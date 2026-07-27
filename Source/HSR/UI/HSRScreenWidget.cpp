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

FReply UHSRScreenWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (ShouldConsumeBackKey(InKeyEvent.GetKey()))
	{
		RequestBack();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
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
#endif
