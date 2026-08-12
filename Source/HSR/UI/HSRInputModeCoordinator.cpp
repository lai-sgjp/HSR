#include "HSRInputModeCoordinator.h"
#include "HSRScreenStack.h"
#include "../Player/HSRPlayerController.h"
#include "Components/Widget.h"

// ResolvePolicy 把屏幕栈中“当前激活屏幕”的纯值状态，翻译成一份输入模式策略
// （FHSRInputModePolicy）。它从 UHSRScreenStack 读取数据：取出激活屏幕条目的
// 输入意图/焦点令牌/屏幕 ID，再推导出“是否显示鼠标光标”。
// 本方法不持有任何 UI 对象引用，只做纯值到纯值的映射，便于测试与重放。
FHSRInputModePolicy UHSRInputModeCoordinator::ResolvePolicy(const UHSRScreenStack* ScreenStack) const
{
	FHSRInputModePolicy Policy;
	// 没有屏幕栈可用时，返回全默认的策略（不改变输入模式）。
	if (!ScreenStack)
	{
		return Policy;
	}

	// 从屏幕栈读取当前激活条目；栈为空（没有任何界面）时同样返回默认策略。
	FHSRScreenStackEntry ActiveEntry;
	if (!ScreenStack->GetActiveEntry(ActiveEntry))
	{
		return Policy;
	}

	// 激活屏幕的输入意图直接决定输入模式：GameOnly 时隐藏鼠标，其余都显示鼠标。
	Policy.InputIntent = ActiveEntry.InputIntent;
	Policy.bShowMouseCursor = ActiveEntry.InputIntent != EHSRUIInputIntent::GameOnly;
	Policy.PreferredFocusToken = ActiveEntry.FocusToken;
	Policy.OwningScreenId = ActiveEntry.ScreenId;
	return Policy;
}

// ApplyPolicy 把解析好的策略实际应用到玩家控制器上，最终由 PlayerController 的
// ApplyUIInputPolicy 落地（例如切换输入模式、设置鼠标光标、释放/抢占输入）。
// 控制器为空时直接失败，避免空指针。
bool UHSRInputModeCoordinator::ApplyPolicy(AHSRPlayerController* PlayerController, const FHSRInputModePolicy& Policy,
	const EHSRPlayerControlMode SemanticMode) const
{
	return PlayerController && PlayerController->ApplyUIInputPolicy(Policy, SemanticMode);
}

// ApplyFocus 负责把键盘/手柄焦点交给合适的控件。
// 它先判断“首选控件”与“备用控件”各自是否可聚焦，再用 ChooseFocusTarget
// 决定实际目标，最后把用户焦点设置到目标控件上。这样焦点路由集中在一处。
EHSRFocusApplyResult UHSRInputModeCoordinator::ApplyFocus(AHSRPlayerController* PlayerController,
	UWidget* PreferredWidget, UWidget* ScreenFallback) const
{
	// 可聚焦判定：控件非空、已启用、且处于可见状态，三者缺一不可。
	const auto IsEligible = [](const UWidget* Widget)
	{
		return Widget && Widget->GetIsEnabled() && Widget->IsVisible();
	};
	// 只有本地玩家控制器才拥有真实用户输入，远端/AI 控制器不应抢占焦点。
	const EHSRFocusApplyResult Choice = ChooseFocusTarget(
		PlayerController && PlayerController->IsLocalPlayerController(), IsEligible(PreferredWidget), IsEligible(ScreenFallback));
	if (Choice == EHSRFocusApplyResult::Preferred)
	{
		PreferredWidget->SetUserFocus(PlayerController);
	}
	else if (Choice == EHSRFocusApplyResult::ScreenFallback)
	{
		ScreenFallback->SetUserFocus(PlayerController);
	}
	return Choice;
}

// 纯决策函数：输入“是否可聚焦”的三个布尔量，输出焦点目标选择结果。
// 与 ApplyFocus 分离是为了让这条优先级规则（本地玩家 > 首选 > 备用 > 不可用）
// 可以被单独测试。
EHSRFocusApplyResult UHSRInputModeCoordinator::ChooseFocusTarget(const bool bOwnerValid,
	const bool bPreferredEligible, const bool bFallbackEligible)
{
	// 控制器不可用（非本地玩家）时，任何焦点操作都没有意义。
	if (!bOwnerValid)
	{
		return EHSRFocusApplyResult::Unavailable;
	}
	// 首选控件可用就优先给它焦点。
	if (bPreferredEligible)
	{
		return EHSRFocusApplyResult::Preferred;
	}
	// 首选不可用时退到备用控件；备用也不可用则判定为不可用。
	if (bFallbackEligible)
	{
		return EHSRFocusApplyResult::ScreenFallback;
	}
	return EHSRFocusApplyResult::Unavailable;
}
