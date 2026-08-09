#include "HSRFrontendModuleRootWidget.h"
#include "../HSRUIManagerSubsystem.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"

void UHSRFrontendModuleRootWidget::PresentModule(const EHSRFrontendModule InModule)
{
	PresentedModule = InModule;
	OnModuleChanged(InModule);
}

bool UHSRFrontendModuleRootWidget::SetModuleContent(UWidget* const InContent)
{
	if (!ModuleContentHost || !InContent)
	{
		return false;
	}

	// A dynamically created module must not inherit a hidden state left by its
	// former pre-placed frontend instance.
	InContent->SetVisibility(ESlateVisibility::Visible);
	ModuleContentHost->ClearChildren();
	UPanelSlot* MountedSlot = ModuleContentHost->AddChild(InContent);
	if (!MountedSlot)
	{
		return false;
	}

	if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(MountedSlot))
	{
		OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		OverlaySlot->SetVerticalAlignment(VAlign_Fill);
		OverlaySlot->SetPadding(FMargin(0.f));
	}
	else if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(MountedSlot))
	{
		FAnchors FullAnchors;
		FullAnchors.Minimum = FVector2D::ZeroVector;
		FullAnchors.Maximum = FVector2D::UnitVector;
		CanvasSlot->SetAnchors(FullAnchors);
		CanvasSlot->SetOffsets(FMargin(0.f));
		CanvasSlot->SetAlignment(FVector2D::ZeroVector);
		CanvasSlot->SetAutoSize(false);
	}

	return true;
}

void UHSRFrontendModuleRootWidget::ClearModuleContent()
{
	if (ModuleContentHost)
	{
		ModuleContentHost->ClearChildren();
	}
}

bool UHSRFrontendModuleRootWidget::RequestOpenModule(const EHSRFrontendModule Module)
{
	return GetOwningUIManager()
		&& GetOwningUIManager()->OpenFrontendModule(Module) == EHSRUIScreenResult::Success;
}

bool UHSRFrontendModuleRootWidget::RequestCloseToRoot()
{
	return GetOwningUIManager()
		&& GetOwningUIManager()->CloseFrontendToRoot() == EHSRUIScreenResult::Success;
}
