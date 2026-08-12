#include "HSRUserWidget.h"
#include "HSRAttributeViewModel.h"
#include "HSRInteractionViewModel.h"

// 静态实例计数器：每次创建 UHSRUserWidget 都自增分配一个全局唯一实例号，
// 用于日志中区分同一类 Widget 的不同实例（配合类名形成可读的身份标识）。
static int32 NextWidgetInstanceId = 1;

UHSRUserWidget::UHSRUserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 构造函数：分配实例号，并把“提示接收计数”清零。
	// 该计数用于观察 Widget 从 VM 实际收到了多少次提示回调，属于诊断用数据。
	WidgetInstanceId = NextWidgetInstanceId++;
	PromptReceiveCount = 0;
}

void UHSRUserWidget::NativeConstruct()
{
	// 构造完成回调：目前仅透传给父类，绑定逻辑由外部（如 HUD）通过
	// SetInteractionViewModel / SetAttributeViewModel 注入 ViewModel 触发。
	Super::NativeConstruct();
}

void UHSRUserWidget::NativeDestruct()
{
	// 销毁回调：必须先摘掉对 InteractionViewModel 的订阅并 Teardown VM，
	// 再清空属性 VM 引用，最后才调用父类。顺序很重要——父类析构阶段
	// 不能再触发任何依赖本 Widget 的回调。
	int32 VmId = -1;
	if (InteractionViewModel)
	{
		VmId = InteractionViewModel->GetInstanceId();
		// 摘除动态委托：Widget 销毁后不能再接收 VM 的提示变化回调。
		InteractionViewModel->OnPromptChanged.RemoveDynamic(this, &UHSRUserWidget::OnInternalPromptChanged);
		// 让 VM 解除对交互组件的观察，避免悬垂引用。
		InteractionViewModel->Teardown();
	}
	UE_LOG(LogTemp, Log, TEXT("UHSRUserWidget[%d]::NativeDestruct - VM=%d, totalReceived=%d"),
		WidgetInstanceId, VmId, PromptReceiveCount);
	InteractionViewModel = nullptr;

	Super::NativeDestruct();
	AttributeViewModel = nullptr;
}

// 属性 VM 是纯绑定的：只是把传入的 VM 指针存下来，供子类/蓝图在显示时读取
// 当前属性值。它没有动态订阅，因为属性刷新由 HUD 统一驱动。
void UHSRUserWidget::SetAttributeViewModel(UHSRAttributeViewModel* InViewModel)
{
	AttributeViewModel = InViewModel;
}

// 设置交互 VM。与属性 VM 不同，交互 VM 需要动态订阅它的提示变化事件，
// 所以这里要处理“解绑旧 VM / 绑定新 VM / 强制刷新一次”的完整生命周期。
void UHSRUserWidget::SetInteractionViewModel(UHSRInteractionViewModel* InViewModel)
{
	// 同一个 VM 指针：不做任何事，避免重复绑定导致回调重复。
	if (InteractionViewModel == InViewModel)
	{
		return;
	}

	// 先解绑旧的 VM（若存在）：移除动态委托，避免旧 VM 继续向本 Widget 推送。
	if (InteractionViewModel)
	{
		const int32 OldVmId = InteractionViewModel->GetInstanceId();
		InteractionViewModel->OnPromptChanged.RemoveDynamic(this, &UHSRUserWidget::OnInternalPromptChanged);
		UE_LOG(LogTemp, Log, TEXT("UHSRUserWidget[%d]::SetInteractionViewModel - Unbound from VM[%d]"), WidgetInstanceId, OldVmId);
	}

	InteractionViewModel = InViewModel;

	// 再绑定新的 VM 并强制推送一次当前快照：
	// ForceCurrentSnapshot 让新绑定立刻同步一次当前提示状态，而不是等下次变化才显示。
	if (InteractionViewModel)
	{
		const int32 NewVmId = InteractionViewModel->GetInstanceId();
		InteractionViewModel->OnPromptChanged.AddUniqueDynamic(this, &UHSRUserWidget::OnInternalPromptChanged);
		InteractionViewModel->ForceCurrentSnapshot();
		UE_LOG(LogTemp, Log, TEXT("UHSRUserWidget[%d]::SetInteractionViewModel - Bound to VM[%d] + ForceCurrentSnapshot"),
			WidgetInstanceId, NewVmId);
	}
}

// 内部提示变化入口：VM 广播提示状态时先经过这里（负责计数与日志），
// 再把真正的显示逻辑转发给蓝图可覆写的 OnInteractionPromptChanged。
void UHSRUserWidget::OnInternalPromptChanged(bool bVisible, const FText& PromptText)
{
	PromptReceiveCount++;
	OnInteractionPromptChanged(bVisible, PromptText);
	UE_LOG(LogTemp, Log, TEXT("UHSRUserWidget[%d]::OnInternalPromptChanged - visible=%d prompt=%s (totalReceived=%d)"),
		WidgetInstanceId, bVisible, *PromptText.ToString(), PromptReceiveCount);
}