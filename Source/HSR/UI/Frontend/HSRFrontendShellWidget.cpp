#include "HSRFrontendShellWidget.h"
#include "../HSRUIManagerSubsystem.h"

void UHSRFrontendShellWidget::PresentRoute(const FHSRFrontendRouteSnapshot& Snapshot)
{
	OnRouteChanged(Snapshot);
}

bool UHSRFrontendShellWidget::RequestOpenModule(const EHSRFrontendModule Module)
{
	return GetOwningUIManager() && GetOwningUIManager()->OpenFrontendModule(Module) == EHSRUIScreenResult::Success;
}

bool UHSRFrontendShellWidget::RequestCloseToRoot()
{
	return GetOwningUIManager() && GetOwningUIManager()->CloseFrontendToRoot() == EHSRUIScreenResult::Success;
}
