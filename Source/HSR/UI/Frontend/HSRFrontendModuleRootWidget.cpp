#include "HSRFrontendModuleRootWidget.h"
#include "../HSRUIManagerSubsystem.h"

void UHSRFrontendModuleRootWidget::PresentModule(const EHSRFrontendModule InModule)
{
	PresentedModule = InModule;
	OnModuleChanged(InModule);
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
