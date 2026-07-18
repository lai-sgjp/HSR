#include "HSRUserWidget.h"
#include "HSRAttributeViewModel.h"
#include "HSRInteractionViewModel.h"

static int32 NextWidgetInstanceId = 1;

UHSRUserWidget::UHSRUserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WidgetInstanceId = NextWidgetInstanceId++;
	PromptReceiveCount = 0;
}

void UHSRUserWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UHSRUserWidget::NativeDestruct()
{
	int32 VmId = -1;
	if (InteractionViewModel)
	{
		VmId = InteractionViewModel->GetInstanceId();
		InteractionViewModel->OnPromptChanged.RemoveDynamic(this, &UHSRUserWidget::OnInternalPromptChanged);
		InteractionViewModel->Teardown();
	}
	UE_LOG(LogTemp, Log, TEXT("UHSRUserWidget[%d]::NativeDestruct - VM=%d, totalReceived=%d"),
		WidgetInstanceId, VmId, PromptReceiveCount);
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
	// Same VM pointer: no-op
	if (InteractionViewModel == InViewModel)
	{
		return;
	}

	// Unbind old
	if (InteractionViewModel)
	{
		const int32 OldVmId = InteractionViewModel->GetInstanceId();
		InteractionViewModel->OnPromptChanged.RemoveDynamic(this, &UHSRUserWidget::OnInternalPromptChanged);
		UE_LOG(LogTemp, Log, TEXT("UHSRUserWidget[%d]::SetInteractionViewModel - Unbound from VM[%d]"), WidgetInstanceId, OldVmId);
	}

	InteractionViewModel = InViewModel;

	// Bind new and force once
	if (InteractionViewModel)
	{
		const int32 NewVmId = InteractionViewModel->GetInstanceId();
		InteractionViewModel->OnPromptChanged.AddUniqueDynamic(this, &UHSRUserWidget::OnInternalPromptChanged);
		InteractionViewModel->ForceCurrentSnapshot();
		UE_LOG(LogTemp, Log, TEXT("UHSRUserWidget[%d]::SetInteractionViewModel - Bound to VM[%d] + ForceCurrentSnapshot"),
			WidgetInstanceId, NewVmId);
	}
}

void UHSRUserWidget::OnInternalPromptChanged(bool bVisible, const FText& PromptText)
{
	PromptReceiveCount++;
	OnInteractionPromptChanged(bVisible, PromptText);
	UE_LOG(LogTemp, Log, TEXT("UHSRUserWidget[%d]::OnInternalPromptChanged - visible=%d prompt=%s (totalReceived=%d)"),
		WidgetInstanceId, bVisible, *PromptText.ToString(), PromptReceiveCount);
}