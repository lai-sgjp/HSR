#include "HSRFrontendShellWidget.h"
#include "../HSRUIManagerSubsystem.h"

// 展示一条路由快照：把最新路由广播给监听方（蓝图事件 OnRouteChanged）。
void UHSRFrontendShellWidget::PresentRoute(const FHSRFrontendRouteSnapshot& Snapshot)
{
	OnRouteChanged(Snapshot);
}

// 请求打开一个模块：转发给 UI 管理器，成功返回 true。
bool UHSRFrontendShellWidget::RequestOpenModule(const EHSRFrontendModule Module)
{
	return GetOwningUIManager() && GetOwningUIManager()->OpenFrontendModule(Module) == EHSRUIScreenResult::Success;
}

// 请求关闭到根界面：转发给 UI 管理器，成功返回 true。
bool UHSRFrontendShellWidget::RequestCloseToRoot()
{
	return GetOwningUIManager() && GetOwningUIManager()->CloseFrontendToRoot() == EHSRUIScreenResult::Success;
}
