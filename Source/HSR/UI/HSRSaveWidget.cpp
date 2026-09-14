#include "HSRSaveWidget.h"

#include "../Save/HSRSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

// SetViewModel：外部注入一个已有 ViewModel（而非本控件自建）。
// 先解绑旧监听，接管新 VM 后立即做一次全量刷新，保证挂接瞬间 UI 就呈现最新状态。
void UHSRSaveWidget::SetViewModel(UHSRSaveViewModel* InViewModel)
{
	UnbindViewModelChanged();
	if (bOwnsViewModel && ViewModel && ViewModel != InViewModel)
	{
		ViewModel->Shutdown();
	}
	const bool bRetainOwnership = bOwnsViewModel && ViewModel == InViewModel;
	ViewModel = InViewModel;
	// 外部注入的 VM 生命周期归调用方所有，本控件不负责 Shutdown。
	bOwnsViewModel = bRetainOwnership;
	bHasResult = false;
	bLastActionWasSave = false;
	bOverwriteCancelled = false;
	Current = FHSRSaveFrontendResult();
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
			if (!WidgetTree) continue;
			const bool bFirst = Summary.SlotName == TEXT("p17_slot_01");
			const bool bLoadable = Summary.State == EHSRSaveSlotState::Ready || Summary.State == EHSRSaveSlotState::Recoverable;
			if (UButton* Load = WidgetTree->FindWidget<UButton>(bFirst ? TEXT("BTN_LoadSlot1") : TEXT("btn_LoadSlot2")))
				Load->SetIsEnabled(bLoadable && !Current.bPending && !Current.bAwaitingOverwrite);
			if (UButton* Save = WidgetTree->FindWidget<UButton>(bFirst ? TEXT("BTN_SaveSlot1") : TEXT("BTN_SaveSlot2")))
				Save->SetIsEnabled(!Current.bPending && !Current.bAwaitingOverwrite);
			if (UTextBlock* Text = WidgetTree->FindWidget<UTextBlock>(bFirst ? TEXT("TXT_Slot1Summary") : TEXT("TXT_Slot2Summary")))
			{
				FText Label = NSLOCTEXT("HSRSave", "EmptySlot", "空存档位");
				if (bLoadable)
				{
					const FDateTime SavedAt = FDateTime::FromUnixTimestamp(Summary.UtcUnixMilliseconds / 1000);
					Label = FText::Format(NSLOCTEXT("HSRSave", "SlotSummary", "{0}  ·  队伍 {1} 人{2}"),
						FText::AsDateTime(SavedAt), FText::AsNumber(Summary.PartyMemberCount),
						Summary.State == EHSRSaveSlotState::Recoverable ? NSLOCTEXT("HSRSave", "Backup", " · 将恢复备份") : FText::GetEmpty());
				}
				else if (Summary.State == EHSRSaveSlotState::Unavailable)
					Label = NSLOCTEXT("HSRSave", "UnreadableSlot", "存档无法读取");
				Text->SetText(Label);
				Text->SetAutoWrapText(true);
			}
		}
	}
	RefreshPresentation();
}

// RequestSave：转发保存请求，并在操作后刷新结果与槽位摘要，让 UI 立即反映写盘效果。
EHSRSaveFrontendActionResult UHSRSaveWidget::RequestSave(const FString& SlotName)
{
	bLastActionWasSave = true;
	bOverwriteCancelled = false;
	const EHSRSaveFrontendActionResult Result = ViewModel ? ViewModel->RequestSave(SlotName) : EHSRSaveFrontendActionResult::InvalidArgument;
	Refresh();
	RefreshSlotSummaries();
	return Result;
}

// ConfirmOverwrite：转发覆盖确认，成功后同样刷新结果与槽位摘要。
EHSRSaveFrontendActionResult UHSRSaveWidget::ConfirmOverwrite()
{
	bLastActionWasSave = true;
	bOverwriteCancelled = false;
	const EHSRSaveFrontendActionResult Result = ViewModel ? ViewModel->ConfirmOverwrite() : EHSRSaveFrontendActionResult::InvalidArgument;
	Refresh();
	RefreshSlotSummaries();
	return Result;
}

// CancelOverwrite：转发覆盖取消，并刷新（撤销待确认态后 UI 需回到可操作状态）。
void UHSRSaveWidget::CancelOverwrite()
{
	bOverwriteCancelled = true;
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
	bLastActionWasSave = false;
	bOverwriteCancelled = false;
	const EHSRSaveResult Result = ViewModel ? ViewModel->RequestLoad(SlotName) : EHSRSaveResult::InvalidArgument;
	Refresh();
	RefreshSlotSummaries();
	return Result;
}

// NativeConstruct：控件入树时若无外部注入的 VM，则自建一个并绑定到存档子系统。
void UHSRSaveWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindActionButtons();
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
	RefreshPresentation();
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

void UHSRSaveWidget::BindActionButtons()
{
	if (!WidgetTree) return;
	// Own the complete click path; old Blueprint handlers wrote raw enum strings after the call returned.
#define HSR_BIND_SAVE_BUTTON(Name, Handler) \
	if (UButton* Button = WidgetTree->FindWidget<UButton>(TEXT(Name))) { Button->OnClicked.Clear(); Button->OnClicked.AddUniqueDynamic(this, &ThisClass::Handler); }
	HSR_BIND_SAVE_BUTTON("BTN_SaveSlot1", HandleSave1);
	HSR_BIND_SAVE_BUTTON("BTN_SaveSlot2", HandleSave2);
	HSR_BIND_SAVE_BUTTON("BTN_LoadSlot1", HandleLoad1);
	HSR_BIND_SAVE_BUTTON("btn_LoadSlot2", HandleLoad2);
	HSR_BIND_SAVE_BUTTON("BTN_ConfirmOverwrite", HandleConfirmOverwrite);
	HSR_BIND_SAVE_BUTTON("BTN_CancelOverwrite", HandleCancelOverwrite);
#undef HSR_BIND_SAVE_BUTTON
}

void UHSRSaveWidget::RefreshPresentation()
{
	if (!WidgetTree) return;
	const auto SetText = [this](const TCHAR* Name, const FText& Label)
	{
		if (UTextBlock* Text = WidgetTree->FindWidget<UTextBlock>(Name))
		{
			Text->SetText(Label);
			Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.95f, 1.f)));
			Text->SetAutoWrapText(true);
		}
	};
	SetText(TEXT("TXT_Title"), NSLOCTEXT("HSRSave", "Title", "旅程存档"));
	SetText(TEXT("TXT_Back"), NSLOCTEXT("HSRSave", "Back", "返回"));
	SetText(TEXT("TXT_Close"), NSLOCTEXT("HSRSave", "Close", "关闭"));
	SetText(TEXT("TXT_SaveSlot1"), NSLOCTEXT("HSRSave", "Save1", "保存至存档 1"));
	SetText(TEXT("TXT_SaveSlot2"), NSLOCTEXT("HSRSave", "Save2", "保存至存档 2"));
	SetText(TEXT("TXT_LoadSlot1"), NSLOCTEXT("HSRSave", "Load1", "读取存档 1"));
	SetText(TEXT("TXT_LoadSlot2"), NSLOCTEXT("HSRSave", "Load2", "读取存档 2"));
	SetText(TEXT("TXT_ConfirmOverwrite"), NSLOCTEXT("HSRSave", "Confirm", "确认覆盖"));
	SetText(TEXT("TXT_CancelOverwrite"), NSLOCTEXT("HSRSave", "Cancel", "保留原存档"));
	const FText Overwrite = NSLOCTEXT("HSRSave", "OverwriteWarning", "此位置已有存档。确认后将用当前进度覆盖。");
	SetText(TEXT("TXT_OverwriteMessage"), Current.bAwaitingOverwrite ? Overwrite : FText::GetEmpty());
	if (UWidget* Confirm = WidgetTree->FindWidget(TEXT("Border_OverwriteConfirm")))
		Confirm->SetVisibility(Current.bAwaitingOverwrite ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	for (const TCHAR* Name : { TEXT("BTN_ConfirmOverwrite"), TEXT("BTN_CancelOverwrite") })
		if (UButton* Button = WidgetTree->FindWidget<UButton>(Name))
			Button->SetIsEnabled(Current.bAwaitingOverwrite && !Current.bPending);
	// Generation is an internal recovery version, not player-facing progress.
	if (UWidget* Generation = WidgetTree->FindWidget(TEXT("TXT_SaveGeneration")))
		Generation->SetVisibility(ESlateVisibility::Collapsed);
	SetText(TEXT("TXT_SaveRecovery"), Current.bRecoveredFromBackup
		? NSLOCTEXT("HSRSave", "BackupRecovered", "已从备份恢复进度") : FText::GetEmpty());
	FText Message;
	if (!ViewModel) Message = NSLOCTEXT("HSRSave", "Unavailable", "存档功能暂不可用，请返回后重试");
	else if (Current.bAwaitingOverwrite) Message = NSLOCTEXT("HSRSave", "Awaiting", "请确认是否覆盖所选存档");
	else if (Current.bPending) Message = NSLOCTEXT("HSRSave", "Loading", "正在恢复进度…");
	else if (bOverwriteCancelled) Message = NSLOCTEXT("HSRSave", "Cancelled", "已取消覆盖，原存档保留");
	else if (!bHasResult || Current.SlotName.IsEmpty()) Message = NSLOCTEXT("HSRSave", "ChooseSlot", "选择存档位保存或继续旅程");
	else if (Current.Result == EHSRSaveResult::Success) Message = bLastActionWasSave
		? NSLOCTEXT("HSRSave", "Saved", "当前进度已保存") : NSLOCTEXT("HSRSave", "Loaded", "旅程进度已恢复");
	else if (Current.Result == EHSRSaveResult::SlotNotFound) Message = NSLOCTEXT("HSRSave", "Missing", "此存档位尚无进度");
	else if (Current.Result == EHSRSaveResult::SaveFailed || Current.Result == EHSRSaveResult::CreateFailed)
		Message = NSLOCTEXT("HSRSave", "WriteFailed", "进度未能保存，请检查可用磁盘空间后重试");
	else if (Current.Result == EHSRSaveResult::UnsupportedSchema)
		Message = NSLOCTEXT("HSRSave", "VersionMismatch", "此存档版本不兼容，请选择其他存档");
	else Message = NSLOCTEXT("HSRSave", "Failed", "存档操作未完成，请重试或选择其他存档位");
	SetText(TEXT("TXT_SaveResult"), Message);
}

void UHSRSaveWidget::HandleSave1() { RequestSave(TEXT("p17_slot_01")); }
void UHSRSaveWidget::HandleSave2() { RequestSave(TEXT("p17_slot_02")); }
void UHSRSaveWidget::HandleLoad1() { RequestLoad(TEXT("p17_slot_01")); }
void UHSRSaveWidget::HandleLoad2() { RequestLoad(TEXT("p17_slot_02")); }
void UHSRSaveWidget::HandleConfirmOverwrite() { ConfirmOverwrite(); }
void UHSRSaveWidget::HandleCancelOverwrite() { CancelOverwrite(); }
