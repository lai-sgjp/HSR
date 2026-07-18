#include "HSRUserWidget.h"
#include "HSRAttributeViewModel.h"
#include "HSRInteractionViewModel.h"

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
	// Unbind from ViewModel
	if (InteractionViewModel)
	{
		InteractionViewModel->OnPromptChanged.RemoveDynamic(this, &UHSRUserWidget::OnInternalPromptChanged);
	}
	InteractionViewModel = nullptr;

	Super::NativeDestruct();
	AttributeViewModel = nullptr;
}

void UHSRUserWidget::SetAttributeViewModel(UHSRAttributeViewModel* InViewModel)
{
	AttributeViewModel = InViewModel;
}

void UHSRUserWidget::SetInteractionViewModel(UHSRInteractionViewModel* InViewModel)
{
	// Unbind old
	if (InteractionViewModel)
	{
		InteractionViewModel->OnPromptChanged.RemoveDynamic(this, &UHSRUserWidget::OnInternalPromptChanged);
	}

	InteractionViewModel = InViewModel;

	// Bind new
	if (InteractionViewModel)
	{
		InteractionViewModel->OnPromptChanged.AddUniqueDynamic(this, &UHSRUserWidget::OnInternalPromptChanged);
	}
}

void UHSRUserWidget::OnInternalPromptChanged(bool bVisible, const FText& PromptText)
{
	OnInteractionPromptChanged(bVisible, PromptText);
}
