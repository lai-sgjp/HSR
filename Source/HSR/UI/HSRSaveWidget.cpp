#include "HSRSaveWidget.h"

#include "../Save/HSRSaveSubsystem.h"
#include "Engine/GameInstance.h"

// SetViewModel：外部注入一个已有 ViewModel（而非本控件自建）。
// 先解绑旧监听，接管新 VM 后立即做一次全量刷新，保证挂接瞬间 UI 就呈现最新状态。
void UHSRSaveWidget::SetViewModel(UHSRSaveViewModel* InViewModel)
{
	UnbindViewModelChanged();
	ViewModel = InViewModel;
	// 外部注入的 VM 生命周期归调用方所有，本控件不负责 Shutdown。
	bOwnsViewModel = false;
	BindViewModelChanged();
	Refresh();
	RefreshSlotSummaries();
}

// GetCurrentResult：输出控件缓存的最近一次前端结果；尚未取得任何结果时返回 false。
bool UHSRSaveWidget::GetCurrentResult(FHSRSaveFrontendResult& OutResult) const
{
	if (!bHasResult)
	{
		return false;
	}
	OutResult = Current;
	return true;
}

// GetSlotSummary：把槽位摘要查询转发给 ViewModel（无 VM 时短路返回 false）。
bool UHSRSaveWidget::GetSlotSummary(const FString& SlotName, FHSRSaveSlotSummary& OutSummary) const
{
	return ViewModel && ViewModel->GetSlotSummary(SlotName, OutSummary);
}

// RefreshSlotSummaries：遍历固定的槽位列表，把每个槽位的摘要变化推送给蓝图事件。
// 槽位名是固定写死的（P17 阶段固定两个槽位），因此无需把槽位清单做成数据资产。
void UHSRSaveWidget::RefreshSlotSummaries()
{
	if (!ViewModel)
	{
		return;
	}
	static const TCHAR* SlotNames[] = { TEXT("p17_slot_01"), TEXT("p17_slot_02") };
	for (const TCHAR* SlotName : SlotNames)
	{
		FHSRSaveSlotSummary Summary;
		if (ViewModel->GetSlotSummary(SlotName, Summary))
		{
			OnSaveSlotSummaryChanged(Summary);
		}
	}
}

// RequestSave：转发保存请求，并在操作后刷新结果与槽位摘要，让 UI 立即反映写盘效果。
EHSRSaveFrontendActionResult UHSRSaveWidget::RequestSave(const FString& SlotName)
{
	const EHSRSaveFrontendActionResult Result = ViewModel ? ViewModel->RequestSave(SlotName) : EHSRSaveFrontendActionResult::InvalidArgument;
	Refresh();
	RefreshSlotSummaries();
	return Result;
}

// ConfirmOverwrite：转发覆盖确认，成功后同样刷新结果与槽位摘要。
EHSRSaveFrontendActionResult UHSRSaveWidget::ConfirmOverwrite()
{
	const EHSRSaveFrontendActionResult Result = ViewModel ? ViewModel->ConfirmOverwrite() : EHSRSaveFrontendActionResult::InvalidArgument;
	Refresh();
	RefreshSlotSummaries();
	return Result;
}

// CancelOverwrite：转发覆盖取消，并刷新（撤销待确认态后 UI 需回到可操作状态）。
void UHSRSaveWidget::CancelOverwrite()
{
	if (ViewModel)
	{
		ViewModel->CancelOverwrite();
	}
	Refresh();
	RefreshSlotSummaries();
}

// RequestLoad：转发加载请求，并刷新结果与槽位摘要。
EHSRSaveResult UHSRSaveWidget::RequestLoad(const FString& SlotName)
{
	const EHSRSaveResult Result = ViewModel ? ViewModel->RequestLoad(SlotName) : EHSRSaveResult::InvalidArgument;
	Refresh();
	RefreshSlotSummaries();
	return Result;
}

// NativeConstruct：控件入树时若无外部注入的 VM，则自建一个并绑定到存档子系统。
void UHSRSaveWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!ViewModel)
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UHSRSaveSubsystem* Save = GameInstance->GetSubsystem<UHSRSaveSubsystem>())
			{
				ViewModel = NewObject<UHSRSaveViewModel>(this);
				ViewModel->Initialize(Save);
				// 自建 VM 标记 bOwnsViewModel，析构时由本控件负责 Shutdown。
				bOwnsViewModel = true;
			}
		}
	}
	BindViewModelChanged();
	Refresh();
	RefreshSlotSummaries();
}

// NativeDestruct：控件出树时解绑并清理自建 VM。
void UHSRSaveWidget::NativeDestruct()
{
	UnbindViewModelChanged();
	if (bOwnsViewModel && ViewModel)
	{
		ViewModel->Shutdown();
	}
	ViewModel = nullptr;
	bOwnsViewModel = false;
	Super::NativeDestruct();
}

// Refresh：从 ViewModel 拉取最新前端结果并缓存，再通过蓝图事件推送显示。
// 若尚无结果则静默跳过——由 ViewModel 的异步加载回调稍后驱动刷新。
void UHSRSaveWidget::Refresh()
{
	FHSRSaveFrontendResult Result;
	if (ViewModel && ViewModel->GetFrontendResult(Result))
	{
		Current = Result;
		bHasResult = true;
		OnSaveResultChanged(Current);
	}
}

// HandleViewModelChanged：ViewModel 广播 Changed 时的回调——View 层只管重新拉取刷新。
void UHSRSaveWidget::HandleViewModelChanged()
{
	Refresh();
	RefreshSlotSummaries();
}

// BindViewModelChanged：订阅 ViewModel 的 Changed 事件；已绑定则跳过，防止重复订阅。
void UHSRSaveWidget::BindViewModelChanged()
{
	if (ViewModel && !ViewModelChangedHandle.IsValid())
	{
		ViewModelChangedHandle = ViewModel->OnChanged().AddUObject(this, &UHSRSaveWidget::HandleViewModelChanged);
	}
}

// UnbindViewModelChanged：解除订阅并复位句柄。无论句柄是否有效都执行 Reset，保证可重复调用。
void UHSRSaveWidget::UnbindViewModelChanged()
{
	if (ViewModel && ViewModelChangedHandle.IsValid())
	{
		ViewModel->OnChanged().Remove(ViewModelChangedHandle);
	}
	ViewModelChangedHandle.Reset();
}
