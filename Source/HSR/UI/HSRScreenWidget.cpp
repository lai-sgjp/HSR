#include "HSRScreenWidget.h"
#include "HSRUIManagerSubsystem.h"
#include "InputCoreTypes.h"

// 屏幕 Widget 基类：持有所属 UI 管理器，提供“返回/关闭到根”的快捷键路由。
void UHSRScreenWidget::SetOwningUIManager(UHSRUIManagerSubsystem* Manager)
{
	OwningUIManager = Manager;
}

// 请求返回上一屏（转交 UI 管理器）。
bool UHSRScreenWidget::RequestBack()
{
	if (UHSRUIManagerSubsystem* Manager = OwningUIManager.Get())
	{
		Manager->RequestBack();
		return true;
	}
	return false;
}

// 请求关闭到根界面；仅当 UI 管理器确认成功时返回 true。
bool UHSRScreenWidget::SubmitCloseToRoot()
{
	if (UHSRUIManagerSubsystem* Manager = OwningUIManager.Get())
	{
		return Manager->CloseFrontendToRoot() == EHSRUIScreenResult::Success;
	}
	return false;
}

// 按键处理：先试“关闭到根”（X 键），再试“返回”（Tab/Esc/手柄右键）。
FReply UHSRScreenWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (ShouldConsumeCloseToRootKey(InKeyEvent.GetKey()))
	{
		SubmitCloseToRoot();
		return FReply::Handled();
	}
	if (ShouldConsumeBackKey(InKeyEvent.GetKey()))
	{
		RequestBack();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// “关闭到根”快捷键判定：X 键。
bool UHSRScreenWidget::ShouldConsumeCloseToRootKey(const FKey& Key) const
{
	return OwningUIManager.IsValid() && Key == EKeys::X;
}

// “返回”快捷键判定：Tab、Esc 或手柄特殊右键。
bool UHSRScreenWidget::ShouldConsumeBackKey(const FKey& Key) const
{
	return OwningUIManager.IsValid()
		&& (Key == EKeys::Tab || Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right);
}

#if WITH_DEV_AUTOMATION_TESTS
// 自动化测试用：暴露“返回”按键判定。
bool UHSRScreenWidget::ShouldConsumeBackKeyForAutomation(const FKey& Key) const
{
	return ShouldConsumeBackKey(Key);
}

// 自动化测试用：暴露“关闭到根”按键判定。
bool UHSRScreenWidget::ShouldConsumeCloseToRootKeyForAutomation(const FKey& Key) const
{
	return ShouldConsumeCloseToRootKey(Key);
}

// 自动化测试用：直接路由一次“关闭到根”。
bool UHSRScreenWidget::RouteCloseToRootKeyForAutomation(const FKey& Key)
{
	return ShouldConsumeCloseToRootKey(Key) && SubmitCloseToRoot();
}
#endif
