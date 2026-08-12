#include "HSRFrontendModuleRootWidget.h"
#include "../HSRUIManagerSubsystem.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"

// 展示一个模块：记录当前模块并广播给监听方（蓝图事件 OnModuleChanged）。
void UHSRFrontendModuleRootWidget::PresentModule(const EHSRFrontendModule InModule)
{
	PresentedModule = InModule;
	OnModuleChanged(InModule);
}

// 把模块内容挂到内容宿主上。返回 false 表示宿主或内容无效。
bool UHSRFrontendModuleRootWidget::SetModuleContent(UWidget* const InContent)
{
	if (!ModuleContentHost || !InContent)
	{
		return false;
	}

	// A dynamically created module must not inherit a hidden state left by its
	// former pre-placed frontend instance.
	// （中文说明）动态创建的模块不能继承之前预置实例遗留的“隐藏”状态，先强制设为可见。
	InContent->SetVisibility(ESlateVisibility::Visible);
	ModuleContentHost->ClearChildren();
	UPanelSlot* MountedSlot = ModuleContentHost->AddChild(InContent);
	if (!MountedSlot)
	{
		return false;
	}

	// 根据宿主类型填充子项：Overlay 用 Fill，Canvas 用全锚点。
	if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(MountedSlot))
	{
		OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		OverlaySlot->SetVerticalAlignment(VAlign_Fill);
		OverlaySlot->SetPadding(FMargin(0.f));
	}
	else if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(MountedSlot))
	{
		// Canvas 槽位需要显式铺满：锚点(0,0)-(1,1)、无偏移、无对齐、不自动尺寸。
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

// 清空内容宿主（切换模块时调用）。
void UHSRFrontendModuleRootWidget::ClearModuleContent()
{
	if (ModuleContentHost)
	{
		ModuleContentHost->ClearChildren();
	}
}

// 请求打开一个模块：转发给 UI 管理器，成功返回 true。
bool UHSRFrontendModuleRootWidget::RequestOpenModule(const EHSRFrontendModule Module)
{
	return GetOwningUIManager()
		&& GetOwningUIManager()->OpenFrontendModule(Module) == EHSRUIScreenResult::Success;
}

// 请求关闭到根界面：转发给 UI 管理器，成功返回 true。
bool UHSRFrontendModuleRootWidget::RequestCloseToRoot()
{
	return GetOwningUIManager()
		&& GetOwningUIManager()->CloseFrontendToRoot() == EHSRUIScreenResult::Success;
}
