#include "HSRInteractionViewModel.h"
#include "../Interaction/HSRInteractionComponent.h"
#include "../Interaction/HSRInteractableInterface.h"

// 静态实例计数器：每次创建交互 VM 都会拿到一个递增的唯一实例号，
// 配合日志区分同一类 VM 的不同实例（实例号贯穿整个生命周期，含 Teardown 后的日志）。
static int32 NextViewModelInstanceId = 1;

UHSRInteractionViewModel::UHSRInteractionViewModel()
{
	InstanceId = NextViewModelInstanceId++;
	bLastVisible = false;
	LastPrompt = FText::GetEmpty();
	BroadcastCount = 0;
	SkippedDedupCount = 0;
	TeardownCount = 0;
}

// 让 VM 观察指定的交互组件。数据来源是组件的“当前候选交互目标”事件：
// 组件每次候选变化都会广播 OnCandidateChanged，VM 收到后转成“可见性 + 提示文本”。
// 同组件重复调用是纯空操作；换组件或传空则先 Teardown 再重新绑定。
void UHSRInteractionViewModel::Observe(UHSRInteractionComponent* InComponent)
{
	// Same component: pure no-op — candidate changes handled by OnCandidateChanged event
	// 组件相同：纯空操作——候选变化已由 OnCandidateChanged 事件处理，无需重建绑定。
	if (ObservedComponent.Get() == InComponent && InComponent)
	{
		UE_LOG(LogTemp, Verbose, TEXT("UHSRInteractionViewModel[%d]::Observe - Same component, pure no-op"), InstanceId);
		return;
	}

	// Different or null component: full teardown + bind
	// 组件不同或为空：先完整解绑旧组件，再按需绑定新组件。
	Teardown();

	if (InComponent)
	{
		ObservedComponent = InComponent;
		// 订阅候选变化事件，并立即推送一次当前候选，让界面马上有数据。
		InComponent->OnCandidateChanged.AddUniqueDynamic(this, &UHSRInteractionViewModel::OnComponentCandidateChanged);
		BroadcastCurrentState(InComponent->GetCurrentCandidate());
		UE_LOG(LogTemp, Log, TEXT("UHSRInteractionViewModel[%d]::Observe - Bound to Component=%s"),
			InstanceId, *InComponent->GetName());
	}
}

// 解除与当前组件的绑定：移除事件订阅、清空引用、复位上次状态。
// 因为 VM 可能被多次 Teardown，这里用计数跟踪解绑次数以便诊断。
void UHSRInteractionViewModel::Teardown()
{
	UHSRInteractionComponent* OldComp = ObservedComponent.Get();
	if (!OldComp)
	{
		// 已经解绑过：记录次数后直接返回，避免对空组件重复操作。
		TeardownCount++;
		UE_LOG(LogTemp, Verbose, TEXT("UHSRInteractionViewModel[%d]::Teardown - Already torn down (teardown#%d)"),
			InstanceId, TeardownCount);
		return;
	}

	// 移除动态委托并复位状态（隐藏、空提示）。
	OldComp->OnCandidateChanged.RemoveDynamic(this, &UHSRInteractionViewModel::OnComponentCandidateChanged);
	ObservedComponent.Reset();
	bLastVisible = false;
	LastPrompt = FText::GetEmpty();
	TeardownCount++;
	UE_LOG(LogTemp, Log, TEXT("UHSRInteractionViewModel[%d]::Teardown - Unbound from Component=%s, totalBroadcast=%d, skippedDedup=%d, teardown#%d"),
		InstanceId, *OldComp->GetName(), BroadcastCount, SkippedDedupCount, TeardownCount);
}

// 强制推送一次当前快照：忽略去重，无条件广播一次当前候选的提示状态。
// 用于“Widget 刚绑定 VM”时让界面立即同步，而不是等下一次候选变化。
void UHSRInteractionViewModel::ForceCurrentSnapshot()
{
	AActor* Candidate = ObservedComponent.IsValid() ? ObservedComponent.Get()->GetCurrentCandidate() : nullptr;
	FText Prompt;
	bool bVisible = false;
	// 候选目标必须有效且实现交互接口，才把接口返回的提示文本作为显示内容。
	if (IsValid(Candidate) && Candidate->Implements<UHSRInteractableInterface>())
	{
		Prompt = IHSRInteractableInterface::Execute_GetInteractionPrompt(Candidate);
		bVisible = true;
	}

	bLastVisible = bVisible;
	LastPrompt = Prompt;
	OnPromptChanged.Broadcast(bVisible, Prompt);
	BroadcastCount++;
	UE_LOG(LogTemp, Log, TEXT("UHSRInteractionViewModel[%d]::ForceCurrentSnapshot - visible=%d prompt=%s (totalBroadcast=%d)"),
		InstanceId, bVisible, *Prompt.ToString(), BroadcastCount);
}

// 组件候选变化事件：转发给统一的广播入口处理。
void UHSRInteractionViewModel::OnComponentCandidateChanged(AActor* NewCandidate)
{
	BroadcastCurrentState(NewCandidate);
}

// 计算并广播当前候选的提示状态。带去重：只有“可见性或文本”真正变化才广播，
// 否则静默跳过。这样避免组件候选未变时 Widget 被无效刷新刷屏。
void UHSRInteractionViewModel::BroadcastCurrentState(AActor* Candidate)
{
	FText Prompt;
	bool bVisible = false;

	// 与 ForceCurrentSnapshot 相同的“候选 -> 提示文本”映射规则。
	if (IsValid(Candidate) && Candidate->Implements<UHSRInteractableInterface>())
	{
		Prompt = IHSRInteractableInterface::Execute_GetInteractionPrompt(Candidate);
		bVisible = true;
	}

	// 与上次状态完全一致：跳过广播（去重命中）。
	if (bVisible == bLastVisible && Prompt.EqualTo(LastPrompt))
	{
		SkippedDedupCount++;
		UE_LOG(LogTemp, Verbose, TEXT("UHSRInteractionViewModel[%d]::BroadcastCurrentState - skipped (skip#%d)"),
			InstanceId, SkippedDedupCount);
		return;
	}

	// 状态确有变化：记录新状态并广播。
	bLastVisible = bVisible;
	LastPrompt = Prompt;
	OnPromptChanged.Broadcast(bVisible, Prompt);
	BroadcastCount++;
	UE_LOG(LogTemp, Log, TEXT("UHSRInteractionViewModel[%d]::BroadcastCurrentState - visible=%d prompt=%s (totalBroadcast=%d)"),
		InstanceId, bVisible, *Prompt.ToString(), BroadcastCount);
}