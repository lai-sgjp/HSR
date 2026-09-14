#include "HSRPartyWidget.h"

#include "../Party/HSRPartySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "Engine/GameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

// SetViewModel：外部注入已有 ViewModel（外部拥有其生命周期）。
// 先解绑旧订阅并 Shutdown 自建 VM，再接管新 VM 并立即绑定 + 全量刷新。
void UHSRPartyWidget::SetViewModel(UHSRPartyViewModel* InViewModel)
{
	Unbind();
	if (bOwnsViewModel && ViewModel)
	{
		ViewModel->Shutdown();
	}
	ViewModel = InViewModel;
	// 外部注入的 VM 不由本控件负责 Shutdown。
	bOwnsViewModel = false;
	BindAndRefresh();
}

// GetCurrentSnapshot：输出控件缓存的最近一次前端快照；尚无快照时返回 false。
bool UHSRPartyWidget::GetCurrentSnapshot(FHSRPartyFrontendSnapshot& OutSnapshot) const
{
	if (!bHasSnapshot)
	{
		return false;
	}
	OutSnapshot = Current;
	return true;
}

// GetSlotAt：按下标读取某个槽位的视图数据；越界时返回 false。
bool UHSRPartyWidget::GetSlotAt(int32 SlotIndex, FHSRPartySlotViewData& OutSlot) const
{
	if (!Current.Slots.IsValidIndex(SlotIndex))
	{
		return false;
	}
	OutSlot = Current.Slots[SlotIndex];
	return true;
}

// IsSlotOccupied：槽位是否已被角色占用（供蓝图快速判断）。
bool UHSRPartyWidget::IsSlotOccupied(int32 SlotIndex) const
{
	FHSRPartySlotViewData ViewSlot;
	return GetSlotAt(SlotIndex, ViewSlot) && ViewSlot.bOccupied;
}

// GetSlotCharacterId：读取槽位内角色的 ID；空槽位/越界返回 NAME_None。
FName UHSRPartyWidget::GetSlotCharacterId(int32 SlotIndex) const
{
	FHSRPartySlotViewData ViewSlot;
	return GetSlotAt(SlotIndex, ViewSlot) ? ViewSlot.CharacterId : NAME_None;
}

// Authority remains in the ViewModel; the widget only translates the result for the player.
EHSRPartyResult UHSRPartyWidget::SetCandidateSlot(int32 SlotIndex, FName CharacterId)
{
	return PresentActionResult(ViewModel ? ViewModel->SetCandidateSlot(SlotIndex, CharacterId) : EHSRPartyResult::InvalidCandidate, NSLOCTEXT("HSRParty", "DraftUpdated", "队伍已调整，确认后生效"));
}

EHSRPartyResult UHSRPartyWidget::ClearCandidateSlot(int32 SlotIndex)
{
	return PresentActionResult(ViewModel ? ViewModel->ClearCandidateSlot(SlotIndex) : EHSRPartyResult::InvalidCandidate, NSLOCTEXT("HSRParty", "DraftCleared", "槽位已清空，确认后生效"));
}

EHSRPartyResult UHSRPartyWidget::SwapCandidateSlots(int32 FirstSlot, int32 SecondSlot)
{
	return PresentActionResult(ViewModel ? ViewModel->SwapCandidateSlots(FirstSlot, SecondSlot) : EHSRPartyResult::InvalidCandidate, NSLOCTEXT("HSRParty", "DraftSwapped", "角色位置已交换，确认后生效"));
}

EHSRPartyResult UHSRPartyWidget::ConfirmCandidate()
{
	return PresentActionResult(ViewModel ? ViewModel->ConfirmCandidate() : EHSRPartyResult::InvalidCandidate, NSLOCTEXT("HSRParty", "Confirmed", "队伍已保存"));
}

EHSRPartyResult UHSRPartyWidget::CancelCandidate()
{
	return PresentActionResult(ViewModel ? ViewModel->CancelCandidate() : EHSRPartyResult::InvalidCandidate, NSLOCTEXT("HSRParty", "Cancelled", "已撤销修改，恢复原队伍"));
}

// NativeConstruct：控件入树时若无外部注入的 VM，则自建一个并绑定到两个子系统。
void UHSRPartyWidget::NativeConstruct()
{
	BindCharacterSelectors();
	if (!ViewModel)
	{
		UGameInstance* GameInstance = GetGameInstance();
		UHSRPartySubsystem* Party = GameInstance ? GameInstance->GetSubsystem<UHSRPartySubsystem>() : nullptr;
		UHSRCharacterProfileSubsystem* Profiles = GameInstance ? GameInstance->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
		ViewModel = NewObject<UHSRPartyViewModel>(this);
		ViewModel->Initialize(Party, Profiles);
		// 自建 VM 标记 bOwnsViewModel，析构时由本控件负责 Shutdown。
		bOwnsViewModel = true;
	}
	BindAndRefresh();
	// Initialize before Super so the Blueprint Construct event can read a valid snapshot.
	// 必须在调用 Super（触发蓝图 Construct 事件）之前先完成数据初始化，
	// 这样蓝图端在 Construct 事件里就能读到有效的队伍快照。
	{
		TGuardValue<bool> Guard(bUpdatingSelectors, true);
		Super::NativeConstruct();
	}
	BindCharacterSelectors();
	RefreshCharacterSelectors();
}

// NativeDestruct：控件出树时解绑订阅并清理自建 VM。
void UHSRPartyWidget::NativeDestruct()
{
	Unbind();
	if (bOwnsViewModel && ViewModel)
	{
		ViewModel->Shutdown();
		ViewModel = nullptr;
		bOwnsViewModel = false;
	}
	Super::NativeDestruct();
}

// BindAndRefresh：订阅 ViewModel 的 Changed 事件，并立即拉取一次快照完成初次显示。
void UHSRPartyWidget::BindAndRefresh()
{
	if (!ViewModel || Subscription.IsValid())
	{
		return;
	}
	Subscription = ViewModel->OnChanged().AddUObject(this, &ThisClass::HandleSnapshot);
#if WITH_DEV_AUTOMATION_TESTS
	++BindCount;
#endif
	FHSRPartyFrontendSnapshot Snapshot;
	if (ViewModel->GetSnapshot(Snapshot))
	{
		HandleSnapshot(Snapshot);
	}
}

// Unbind：解除订阅并复位缓存快照，保证控件可被安全复用。
void UHSRPartyWidget::Unbind()
{
	if (ViewModel && Subscription.IsValid())
	{
		ViewModel->OnChanged().Remove(Subscription);
#if WITH_DEV_AUTOMATION_TESTS
		++UnbindCount;
#endif
	}
	Subscription.Reset();
	bHasSnapshot = false;
	Current = FHSRPartyFrontendSnapshot();
	ActionMessage = FText::GetEmpty();
}

// HandleSnapshot：ViewModel 广播新快照时的回调——缓存快照并推送给蓝图事件。
void UHSRPartyWidget::HandleSnapshot(const FHSRPartyFrontendSnapshot& InSnapshot)
{
	Current = InSnapshot;
	bHasSnapshot = true;
	{
		TGuardValue<bool> Guard(bUpdatingSelectors, true);
		OnPartySnapshotChanged(Current);
	}
	RefreshCharacterSelectors();
}

void UHSRPartyWidget::BindCharacterSelectors()
{
	if (!WidgetTree) return;
#define HSR_BIND_PARTY_BUTTON(Name, Handler) \
	if (UButton* Button = WidgetTree->FindWidget<UButton>(TEXT(Name))) { Button->OnClicked.Clear(); Button->OnClicked.AddUniqueDynamic(this, &ThisClass::Handler); }
	HSR_BIND_PARTY_BUTTON("Button_ClearSlot0", HandleClear0);
	HSR_BIND_PARTY_BUTTON("Button_ClearSlot1", HandleClear1);
	HSR_BIND_PARTY_BUTTON("Button_ClearSlot2", HandleClear2);
	HSR_BIND_PARTY_BUTTON("Button_ClearSlot3", HandleClear3);
	HSR_BIND_PARTY_BUTTON("Button_Confirm", HandleConfirm);
	HSR_BIND_PARTY_BUTTON("Button_CancelChanges", HandleCancel);
#undef HSR_BIND_PARTY_BUTTON
	// These four selectors now map visible labels to stable IDs in C++.
	// Remove legacy BP callbacks that interpreted the display string as an ID.
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UComboBoxString* Combo = WidgetTree->FindWidget<UComboBoxString>(FName(*FString::Printf(TEXT("ComboBoxString_Slot%d"), Index)));
		if (!Combo) continue;
		Combo->OnSelectionChanged.Clear();
		switch (Index)
		{
		case 0: Combo->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleSlot0); break;
		case 1: Combo->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleSlot1); break;
		case 2: Combo->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleSlot2); break;
		case 3: Combo->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleSlot3); break;
		}
	}
}

void UHSRPartyWidget::RefreshCharacterSelectors()
{
	if (!WidgetTree || bUpdatingSelectors) return;
	TGuardValue<bool> Guard(bUpdatingSelectors, true);
	CharacterOptionLabels.Reset();
	for (int32 Index = 0; Index < Current.AvailableCharacterIds.Num(); ++Index)
	{
		const FText* DisplayName = Current.CharacterDisplayNames.Find(Current.AvailableCharacterIds[Index]);
		// The ordinal disambiguates characters that share a localized display name.
		CharacterOptionLabels.Add(FString::Printf(TEXT("%d  %s"), Index + 1,
			DisplayName ? *DisplayName->ToString() : TEXT("未知角色")));
	}
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UComboBoxString* Combo = WidgetTree->FindWidget<UComboBoxString>(FName(*FString::Printf(TEXT("ComboBoxString_Slot%d"), Index)));
		if (!Combo) continue;
		Combo->ClearOptions();
		for (const FString& Label : CharacterOptionLabels) Combo->AddOption(Label);
		const int32 Selected = Current.Slots.IsValidIndex(Index)
			? Current.AvailableCharacterIds.IndexOfByKey(Current.Slots[Index].CharacterId) : INDEX_NONE;
		if (CharacterOptionLabels.IsValidIndex(Selected)) Combo->SetSelectedOption(CharacterOptionLabels[Selected]);
		Combo->SetIsEnabled(Current.Slots.IsValidIndex(Index) && !CharacterOptionLabels.IsEmpty());
	}
	if (UPanelWidget* SlotList = WidgetTree->FindWidget<UPanelWidget>(TEXT("PartySlotList")))
	{
		for (int32 Index = 0; Index < SlotList->GetChildrenCount() && Current.Slots.IsValidIndex(Index); ++Index)
		{
			UUserWidget* Entry = Cast<UUserWidget>(SlotList->GetChildAt(Index));
			if (Entry && Entry->WidgetTree)
			{
				if (UTextBlock* Title = Entry->WidgetTree->FindWidget<UTextBlock>(TEXT("TXT_SlotTitle")))
				{
					Title->SetText(FText::Format(NSLOCTEXT("HSRParty", "SlotNumber", "队伍位置 {0}"), FText::AsNumber(Index + 1)));
					Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.68f, 0.78f, 0.9f)));
				}
				if (UTextBlock* Hint = Entry->WidgetTree->FindWidget<UTextBlock>(TEXT("TXT_EmptyHint")))
					Hint->SetText(NSLOCTEXT("HSRParty", "SelectMember", "在下方选择角色"));
				if (UTextBlock* Text = Entry->WidgetTree->FindWidget<UTextBlock>(TEXT("TXT_CharacterId")))
				{
					Text->SetText(Current.Slots[Index].bOccupied ? Current.Slots[Index].DisplayName : NSLOCTEXT("HSRParty", "EmptySlot", "空位"));
					Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
				}
			}
		}
	}
	RefreshPresentation();
}

void UHSRPartyWidget::HandleCharacterSelection(int32 SlotIndex, const FString& Label)
{
	if (bUpdatingSelectors) return;
	const int32 Index = CharacterOptionLabels.IndexOfByKey(Label);
	if (!Current.AvailableCharacterIds.IsValidIndex(Index)) return;
	const FName SelectedId = Current.AvailableCharacterIds[Index];
	for (const FHSRPartySlotViewData& PartySlot : Current.Slots)
	{
		if (PartySlot.CharacterId == SelectedId)
		{
			if (PartySlot.SlotIndex != SlotIndex) SwapCandidateSlots(SlotIndex, PartySlot.SlotIndex);
			return;
		}
	}
	SetCandidateSlot(SlotIndex, SelectedId);
}

void UHSRPartyWidget::HandleSlot0(FString Label, ESelectInfo::Type) { HandleCharacterSelection(0, Label); }
void UHSRPartyWidget::HandleSlot1(FString Label, ESelectInfo::Type) { HandleCharacterSelection(1, Label); }
void UHSRPartyWidget::HandleSlot2(FString Label, ESelectInfo::Type) { HandleCharacterSelection(2, Label); }
void UHSRPartyWidget::HandleSlot3(FString Label, ESelectInfo::Type) { HandleCharacterSelection(3, Label); }

void UHSRPartyWidget::RefreshPresentation()
{
	if (!WidgetTree) return;
	if (WidgetTree->FindWidget(TEXT("PR_PartyChoices")))
	{
		for (const TCHAR* LegacyName : {TEXT("PartySlotList"), TEXT("HBox_Columns")})
			if (UWidget* Legacy = WidgetTree->FindWidget(LegacyName)) Legacy->SetVisibility(ESlateVisibility::Collapsed);
	}
	const auto SetText = [this](const TCHAR* Name, const FText& Label)
	{
		if (UTextBlock* Text = WidgetTree->FindWidget<UTextBlock>(Name))
		{
			Text->SetText(Label);
			Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.95f, 1.f)));
		}
	};
	SetText(TEXT("TXT_Title"), NSLOCTEXT("HSRParty", "Title", "队伍编成"));
	SetText(TEXT("TXT_Confirm"), NSLOCTEXT("HSRParty", "Confirm", "确认编队"));
	SetText(TEXT("TXT_Cancel"), NSLOCTEXT("HSRParty", "Cancel", "撤销修改"));
	SetText(TEXT("TXT_Swap"), NSLOCTEXT("HSRParty", "Swap", "交换位置"));
	SetText(TEXT("TXT_Back"), NSLOCTEXT("HSRParty", "Back", "返回"));
	SetText(TEXT("TXT_Close"), NSLOCTEXT("HSRParty", "Close", "关闭"));
	SetText(TEXT("TXT_EmptyTitle"), NSLOCTEXT("HSRParty", "EmptyTitle", "队伍尚未编成"));
	SetText(TEXT("TXT_EmptyHint"), NSLOCTEXT("HSRParty", "EmptyHint", "选择角色加入队伍"));
	SetText(TEXT("TXT_UnavailableTitle"), NSLOCTEXT("HSRParty", "UnavailableTitle", "暂时无法编队"));
	SetText(TEXT("TXT_UnavailableHint"), NSLOCTEXT("HSRParty", "UnavailableHint", "角色数据尚未就绪，请返回后重试"));
	SetText(TEXT("Text_EditingState"), Current.bHasPendingChanges
		? NSLOCTEXT("HSRParty", "Pending", "有未保存的调整 · 确认后生效")
		: NSLOCTEXT("HSRParty", "Saved", "当前出战队伍"));
	SetText(TEXT("Text_Result"), ActionMessage.IsEmpty()
		? NSLOCTEXT("HSRParty", "Hint", "选择已在队伍中的角色可交换位置") : ActionMessage);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		SetText(*FString::Printf(TEXT("TXT_Clear%d"), Index), NSLOCTEXT("HSRParty", "Clear", "移出队伍"));
		if (UButton* Button = WidgetTree->FindWidget<UButton>(FName(*FString::Printf(TEXT("Button_ClearSlot%d"), Index))))
			Button->SetIsEnabled(Current.Slots.IsValidIndex(Index) && Current.Slots[Index].bOccupied);
	}
	for (const TCHAR* Name : { TEXT("Button_Confirm"), TEXT("Button_CancelChanges") })
		if (UButton* Button = WidgetTree->FindWidget<UButton>(Name))
			Button->SetIsEnabled(Current.bHasPendingChanges && Current.Status != EHSRPartyFrontendStatus::Unavailable);
	// An empty draft must remain editable so removing the last member cannot trap the user.
	const bool bCanEdit = Current.Status != EHSRPartyFrontendStatus::Unavailable && !Current.AvailableCharacterIds.IsEmpty();
	if (UWidget* Card = WidgetTree->FindWidget(TEXT("Card_Ready")))
		Card->SetVisibility(bCanEdit ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (UWidget* Card = WidgetTree->FindWidget(TEXT("Card_Empty")))
		Card->SetVisibility(!bCanEdit && Current.Status == EHSRPartyFrontendStatus::Empty ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

EHSRPartyResult UHSRPartyWidget::PresentActionResult(EHSRPartyResult Result, const FText& SuccessMessage)
{
	switch (Result)
	{
	case EHSRPartyResult::Success: ActionMessage = SuccessMessage; break;
	case EHSRPartyResult::RevisionConflict: ActionMessage = NSLOCTEXT("HSRParty", "Conflict", "队伍已发生变化，请撤销修改后重新调整"); break;
	case EHSRPartyResult::DuplicateCharacter: ActionMessage = NSLOCTEXT("HSRParty", "Duplicate", "同一角色不能重复上阵，请交换位置"); break;
	case EHSRPartyResult::ProfileNotFound: ActionMessage = NSLOCTEXT("HSRParty", "Missing", "该角色暂不可用，请选择其他角色"); break;
	case EHSRPartyResult::EmptySlot: ActionMessage = NSLOCTEXT("HSRParty", "AlreadyEmpty", "此位置尚未加入角色"); break;
	default: ActionMessage = NSLOCTEXT("HSRParty", "Failed", "调整未能完成，请检查队伍后重试"); break;
	}
	RefreshPresentation();
	return Result;
}

void UHSRPartyWidget::HandleClear0() { ClearCandidateSlot(0); }
void UHSRPartyWidget::HandleClear1() { ClearCandidateSlot(1); }
void UHSRPartyWidget::HandleClear2() { ClearCandidateSlot(2); }
void UHSRPartyWidget::HandleClear3() { ClearCandidateSlot(3); }
void UHSRPartyWidget::HandleConfirm() { ConfirmCandidate(); }
void UHSRPartyWidget::HandleCancel() { CancelCandidate(); }
