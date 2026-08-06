#include "HSRFrontendModuleRootWidget.h"
#include "../HSRUIManagerSubsystem.h"
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

	ModuleContentHost->ClearChildren();
	return ModuleContentHost->AddChild(InContent) != nullptr;
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
