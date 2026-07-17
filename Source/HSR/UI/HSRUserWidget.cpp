#include "HSRUserWidget.h"
#include "HSRAttributeViewModel.h"

UHSRUserWidget::UHSRUserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UHSRUserWidget::SetAttributeViewModel(UHSRAttributeViewModel* InViewModel)
{
	AttributeViewModel = InViewModel;
}
