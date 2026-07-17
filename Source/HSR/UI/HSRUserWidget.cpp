#include "HSRUserWidget.h"
#include "HSRAttributeViewModel.h"

UHSRUserWidget::UHSRUserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UHSRUserWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// BP WBP_AttributeDebug binds to ViewModel OnValuesUpdated here
}

void UHSRUserWidget::NativeDestruct()
{
	// BP unbinds from ViewModel OnValuesUpdated in its Destruct event
	Super::NativeDestruct();
	AttributeViewModel = nullptr;
}

void UHSRUserWidget::SetAttributeViewModel(UHSRAttributeViewModel* InViewModel)
{
	AttributeViewModel = InViewModel;
}
