#include "HSRPreBattleCandidateWidget.h"

#include "../Party/HSRPartySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Data/Definitions/HSRStageBuffDefinition.h"
#include "Engine/Texture2D.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Containers/Ticker.h"
#include "HSRUIManagerSubsystem.h"

// InitializeCandidate：用遭遇模板初始化战前编队界面。
// 绑定队伍子系统（队伍变化会影响候选默认值），并把模板交给 ViewModel 建起草稿。
void UHSRPreBattleCandidateWidget::InitializeCandidate(const FHSREncounterRequest& Template)
{
	SelectedSlot = 0;
	bSubmitting = bSubmitted = false;
	UGameInstance* GameInstance = GetGameInstance();
	UHSRPartySubsystem* Party = GameInstance ? GameInstance->GetSubsystem<UHSRPartySubsystem>() : nullptr;
	UHSRCharacterProfileSubsystem* Profiles = GameInstance ? GameInstance->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
	if (Party && PartyChangedHandle.IsValid())
	{
		Party->OnPartyChanged().Remove(PartyChangedHandle);
		PartyChangedHandle.Reset();
	}
	if (!ViewModel)
	{
		ViewModel = NewObject<UHSRPreBattleCandidateViewModel>(this);
	}
	ViewModel->Initialize(Party, Profiles, Template);
	if (Party)
	{
		PartyChangedHandle = Party->OnPartyChanged().AddUObject(this, &ThisClass::HandlePartyChanged);
	}
	RefreshSnapshot();
}

// SetCandidateSlot：把某角色放入指定槽位，随后刷新显示。
EHSRPreBattleCandidateResult UHSRPreBattleCandidateWidget::SetCandidateSlot(int32 SlotIndex, FName CharacterId)
{
	const EHSRPreBattleCandidateResult Result = ViewModel && !bSubmitting && !bSubmitted
		? ViewModel->SetCandidateSlot(SlotIndex, CharacterId) : EHSRPreBattleCandidateResult::InvalidCandidate;
	return PresentResult(Result);
}

// SetBuff：添加出战 Buff，随后刷新显示。
EHSRPreBattleCandidateResult UHSRPreBattleCandidateWidget::SetBuff(FName BuffId)
{
	const EHSRPreBattleCandidateResult Result = ViewModel && !bSubmitting && !bSubmitted && IsAvailableBuff(BuffId)
		? ViewModel->SetBuff(BuffId) : EHSRPreBattleCandidateResult::InvalidCandidate;
	return PresentResult(Result);
}

EHSRPreBattleCandidateResult UHSRPreBattleCandidateWidget::ToggleBuff(FName BuffId)
{
	if (!ViewModel || bSubmitting || bSubmitted) return PresentResult(EHSRPreBattleCandidateResult::InvalidCandidate);
	return ViewModel->GetSnapshot().BuffIds.Contains(BuffId) ? PresentResult(ViewModel->RemoveBuff(BuffId)) : SetBuff(BuffId);
}

void UHSRPreBattleCandidateWidget::SelectReplacementSlot(int32 SlotIndex)
{
	if (ViewModel && ViewModel->GetSnapshot().CandidateCharacterIds.IsValidIndex(SlotIndex) && !bSubmitting && !bSubmitted)
	{
		SelectedSlot = SlotIndex;
		RefreshSnapshot();
	}
}

// ConfirmCandidate：校验当前候选并输出遭遇请求（不改 UI，由调用方决定下一步）。
EHSRPreBattleCandidateResult UHSRPreBattleCandidateWidget::ConfirmCandidate(FHSREncounterRequest& OutRequest)
{
	return ViewModel ? ViewModel->ConfirmCandidate(OutRequest) : EHSRPreBattleCandidateResult::InvalidCandidate;
}

// ConfirmAndSubmitEncounter：校验候选并直接把遭遇请求提交给战斗切换子系统。
// 这是"战前编队 -> 进入战斗"的 UI 入口：先本地校验，再交给子系统发起传送。
FHSREncounterResult UHSRPreBattleCandidateWidget::ConfirmAndSubmitEncounter(FHSREncounterRequest& OutRequest)
{
	if (!ViewModel || bSubmitting || bSubmitted)
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidRequest,
			NSLOCTEXT("HSRPreBattle", "CannotSubmit", "准备页尚未就绪，或出战请求已提交。"));
	}
	TGuardValue<bool> SubmittingGuard(bSubmitting, true);

	if (ConfirmCandidate(OutRequest) != EHSRPreBattleCandidateResult::Success)
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidRequest,
			NSLOCTEXT("HSRPreBattle", "InvalidCandidate", "请为队长选择有效角色，并检查队伍是否重复。"));
	}

	UHSRBattleTransitionSubsystem* Transition = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UHSRBattleTransitionSubsystem>()
		: nullptr;
	FHSREncounterResult Result = Transition
		? Transition->SubmitEncounterRequestFromUI(OutRequest)
		: FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidRequest,
			NSLOCTEXT("HSRPreBattle", "TransitionUnavailable", "暂时无法进入战斗。"));
	bSubmitted = Result.ResultType == EHSREncounterResultType::Success;
	if (!bSubmitted) Result.Message = NSLOCTEXT("HSRPreBattle", "AdmissionFailed", "无法进入战斗，请检查队伍与挑战条件后重试。");
	return Result;
}

// CancelCandidate：放弃编辑并重置为队伍权威数据，随后刷新显示。
EHSRPreBattleCandidateResult UHSRPreBattleCandidateWidget::CancelCandidate()
{
	const EHSRPreBattleCandidateResult Result = ViewModel && !bSubmitting && !bSubmitted
		? ViewModel->CancelCandidate() : EHSRPreBattleCandidateResult::InvalidCandidate;
	SelectedSlot = 0;
	if (Result == EHSRPreBattleCandidateResult::Success && GetWorld())
	{
		ULocalPlayer* Player = GetOwningLocalPlayer();
		if (!Player && GetGameInstance()) Player = GetGameInstance()->GetFirstGamePlayer();
		const TWeakObjectPtr<UHSRUIManagerSubsystem> Manager = Player ? Player->GetSubsystem<UHSRUIManagerSubsystem>() : nullptr;
		// The existing Blueprint removes this modal after CancelCandidate returns.
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Manager](float)
		{
			if (Manager.IsValid()) Manager->RestoreActiveFrontendFocus();
			return false;
		}));
	}
	return PresentResult(Result);
}

// GetCandidateSnapshot：暴露当前候选快照给蓝图读取。
FHSRPreBattleCandidateSnapshot UHSRPreBattleCandidateWidget::GetCandidateSnapshot() const
{
	return ViewModel ? ViewModel->GetSnapshot() : FHSRPreBattleCandidateSnapshot();
}

// HandlePartyChanged：队伍变化时刷新显示（例如其它界面改了队伍，候选面板需同步）。
void UHSRPreBattleCandidateWidget::HandlePartyChanged(int64)
{
	RefreshSnapshot();
}

// RefreshSnapshot：拉取最新快照，更新槽位文本并推送蓝图事件。
void UHSRPreBattleCandidateWidget::RefreshSnapshot()
{
	if (ViewModel)
	{
		const FHSRPreBattleCandidateSnapshot Snapshot = ViewModel->GetSnapshot();
		OnCandidateSnapshotChanged(Snapshot);
		UpdateSlotTextBlocks(Snapshot);
		RefreshChoices(Snapshot);
	}
}

// UpdateSlotTextBlocks：把候选角色 ID 写入四个命名的文本控件。
// The panel renders up to four candidate slots.  Resolving by name keeps the C++ side
// independent of how many slot widgets the Blueprint actually places, and lets an authored
// panel show all committed members without hardcoding slot indices in the graph.
// 面板最多渲染四个候选槽位。按名字解析文本控件使 C++ 侧与蓝图实际摆放的槽位数量解耦，
// 也避免在图里硬编码槽位下标。空槽位显示 "Empty"。
void UHSRPreBattleCandidateWidget::UpdateSlotTextBlocks(const FHSRPreBattleCandidateSnapshot& Snapshot)
{
	if (!WidgetTree)
	{
		return;
	}
	const FName SlotNames[] = { TEXT("Text_Slot0_Character"), TEXT("Text_Slot1_Character"),
		TEXT("Text_Slot2_Character"), TEXT("Text_Slot3_Character") };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(SlotNames); ++Index)
	{
		UTextBlock* TextBlock = WidgetTree->FindWidget<UTextBlock>(SlotNames[Index]);
		if (!TextBlock)
		{
			continue;
		}
		if (Snapshot.CandidateCharacterIds.IsValidIndex(Index) && !Snapshot.CandidateCharacterIds[Index].IsNone())
		{
			const UHSRCharacterDefinition* Definition = nullptr;
			auto* Profiles = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
			if (Profiles) Profiles->GetDefinition(Snapshot.CandidateCharacterIds[Index], Definition);
			TextBlock->SetText(Definition && !Definition->DisplayName.IsEmpty() ? Definition->DisplayName
				: NSLOCTEXT("HSRPreBattle", "UnavailableCharacter", "角色资料不可用"));
		}
		else
		{
			TextBlock->SetText(NSLOCTEXT("HSRPreBattle", "Empty", "空位"));
		}
	}
}

// NativeDestruct：控件出树时解除队伍监听并清空 ViewModel 引用。
void UHSRPreBattleCandidateWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ULocalPlayer* Player = GetOwningLocalPlayer();
	if (!Player && GetGameInstance()) Player = GetGameInstance()->GetFirstGamePlayer();
	if (Player)
		if (auto* Manager = Player->GetSubsystem<UHSRUIManagerSubsystem>()) Manager->RegisterPreBattlePopup(this);
}

void UHSRPreBattleCandidateWidget::NativeDestruct()
{
	ULocalPlayer* Player = GetOwningLocalPlayer();
	if (!Player && GetGameInstance()) Player = GetGameInstance()->GetFirstGamePlayer();
	if (Player)
		if (auto* Manager = Player->GetSubsystem<UHSRUIManagerSubsystem>()) Manager->UnregisterPreBattlePopup(this);
	if (ViewModel)
	{
		if (UHSRPartySubsystem* Party = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRPartySubsystem>() : nullptr)
		{
			Party->OnPartyChanged().Remove(PartyChangedHandle);
		}
	}
	PartyChangedHandle.Reset();
	ViewModel = nullptr;
	ChoiceBindings.Reset();
	Super::NativeDestruct();
}

void UHSRPreBattleChoiceBinding::Initialize(UHSRPreBattleCandidateWidget* InOwner, int32 InSlot, FName InId, bool bInBuff)
{
	Owner = InOwner; Slot = InSlot; Id = InId; bBuff = bInBuff;
}

void UHSRPreBattleChoiceBinding::HandleClicked()
{
	if (!Owner.IsValid()) return;
	if (bBuff) Owner->ToggleBuff(Id);
	else if (Slot >= 0) Owner->SelectReplacementSlot(Slot);
	else if (Slot <= -2) Owner->SetCandidateSlot(-Slot - 2, NAME_None);
	else Owner->SetCandidateSlot(Owner->GetSelectedReplacementSlot(), Id);
}

void UHSRPreBattleCandidateWidget::SetLabel(FName Name, const FText& Text)
{
	if (auto* Label = WidgetTree ? WidgetTree->FindWidget<UTextBlock>(Name) : nullptr) Label->SetText(Text);
}

EHSRPreBattleCandidateResult UHSRPreBattleCandidateWidget::PresentResult(EHSRPreBattleCandidateResult Result)
{
	RefreshSnapshot();
	const FText Message = Result == EHSRPreBattleCandidateResult::Success ? FText::GetEmpty()
		: Result == EHSRPreBattleCandidateResult::EmptyLeader ? NSLOCTEXT("HSRPreBattle", "LeaderRequired", "队长不能留空")
		: Result == EHSRPreBattleCandidateResult::DuplicateCharacter ? NSLOCTEXT("HSRPreBattle", "Duplicate", "该角色已在队伍中，请先移出原位置")
		: NSLOCTEXT("HSRPreBattle", "ChoiceUnavailable", "此选项暂不可用，请重新选择");
	SetLabel(TEXT("Text_ErrorMessage"), Message);
	if (auto* Label = WidgetTree ? WidgetTree->FindWidget(TEXT("Text_ErrorMessage")) : nullptr)
		Label->SetVisibility(Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	return Result;
}

bool UHSRPreBattleCandidateWidget::IsAvailableBuff(FName BuffId) const
{
	const auto* Transition = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRBattleTransitionSubsystem>() : nullptr;
	const FText* Name = BuffDisplayNames.Find(BuffId);
	return ViewModel && Transition && Name && !Name->IsEmpty()
		&& AvailableBuffs.ContainsByPredicate([BuffId](const auto& Definition) { return Definition && Definition->BuffId == BuffId; })
		&& Transition->ValidateStageBuffIds(ViewModel->GetSnapshot().EncounterId, {BuffId});
}

void UHSRPreBattleCandidateWidget::RefreshChoices(const FHSRPreBattleCandidateSnapshot& Snapshot)
{
	if (!WidgetTree) return;
	ChoiceBindings.Reset();
	const auto Bind = [this](UButton* Button, int32 ChoiceSlot, FName Id = NAME_None, bool bBuff = false)
	{
		if (!Button) return;
		Button->OnClicked.Clear();
		auto* Binding = NewObject<UHSRPreBattleChoiceBinding>(this);
		Binding->Initialize(this, ChoiceSlot, Id, bBuff);
		Button->OnClicked.AddDynamic(Binding, &UHSRPreBattleChoiceBinding::HandleClicked);
		ChoiceBindings.Add(Binding);
	};
	SetLabel(TEXT("Text_Title"), NSLOCTEXT("HSRPreBattle", "Title", "出战准备"));
	SetLabel(TEXT("Text_Confirm"), NSLOCTEXT("HSRPreBattle", "Confirm", "确认出战"));
	SetLabel(TEXT("Text_Cancel"), NSLOCTEXT("HSRPreBattle", "Cancel", "取消并返回"));
	SetLabel(TEXT("Text_CandidateSnapshot"), FText::Format(NSLOCTEXT("HSRPreBattle", "ChoicePrompt", "正在选择第 {0} 位角色；本次编队仅用于这场战斗"), FText::AsNumber(SelectedSlot + 1)));
	for (int32 Index = 0; Index < 4; ++Index)
	{
		SetLabel(FName(*FString::Printf(TEXT("Text_Slot%d_Label"), Index)), Index == 0 ? NSLOCTEXT("HSRPreBattle", "Leader", "队长")
			: FText::Format(NSLOCTEXT("HSRPreBattle", "Member", "队员 {0}"), FText::AsNumber(Index + 1)));
		SetLabel(FName(*FString::Printf(TEXT("Text_ReplaceSlot%d"), Index)), NSLOCTEXT("HSRPreBattle", "Choose", "选择角色"));
		SetLabel(FName(*FString::Printf(TEXT("Text_ClearSlot%d"), Index)), NSLOCTEXT("HSRPreBattle", "ClearSlot", "移出"));
		auto* Button = WidgetTree->FindWidget<UButton>(FName(*FString::Printf(TEXT("Button_ReplaceSlot%d"), Index)));
		Bind(Button, Index);
		if (Button)
		{
			Button->SetIsEnabled(Snapshot.CandidateCharacterIds.IsValidIndex(Index) && !bSubmitting && !bSubmitted);
			Button->SetBackgroundColor(Index == SelectedSlot ? FLinearColor(.65f,.47f,.18f) : FLinearColor(.08f,.12f,.2f));
		}
		if (auto* Clear = WidgetTree->FindWidget<UButton>(FName(*FString::Printf(TEXT("Button_ClearSlot%d"), Index))))
		{
			Bind(Clear, -Index - 2);
			Clear->SetVisibility(Index == 0 ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
			Clear->SetIsEnabled(Index > 0 && Snapshot.CandidateCharacterIds.IsValidIndex(Index) && !Snapshot.CandidateCharacterIds[Index].IsNone() && !bSubmitting && !bSubmitted);
		}
	}
	if (auto* Choices = Cast<UPanelWidget>(WidgetTree->FindWidget(TEXT("SelectionPanel"))))
	{
		Choices->ClearChildren();
		Choices->SetVisibility(ESlateVisibility::Visible);
		auto* Profiles = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
		TArray<FHSRCharacterProfileSnapshot> Entries;
		if (Profiles) Profiles->GetAllProfileSnapshots(Entries);
		Entries.Sort([](const auto& A, const auto& B) { return A.RuntimeState.CharacterId.LexicalLess(B.RuntimeState.CharacterId); });
		for (const auto& Entry : Entries)
		{
			const UHSRCharacterDefinition* Definition = nullptr;
			const FName Id = Entry.RuntimeState.CharacterId;
			if (!Profiles->GetDefinition(Id, Definition) || !Definition || Definition->CharacterClass.IsNull() || Definition->DisplayName.IsEmpty()) continue;
			auto* Button = WidgetTree->ConstructWidget<UButton>();
			auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
			Button->SetContent(Row);
			if (auto* Portrait = Definition->Portrait.LoadSynchronous())
			{
				auto* Image = WidgetTree->ConstructWidget<UImage>(); Image->SetBrushFromTexture(Portrait);
				auto* Frame = WidgetTree->ConstructWidget<USizeBox>(); Frame->SetWidthOverride(56); Frame->SetHeightOverride(72); Frame->SetContent(Image);
				Row->AddChildToHorizontalBox(Frame)->SetPadding(FMargin(4));
			}
			auto* Label = WidgetTree->ConstructWidget<UTextBlock>(); Label->SetText(Definition->DisplayName);
			Label->SetColorAndOpacity(FSlateColor(FLinearColor(.94f,.96f,1.f)));
			FSlateFontInfo Font = Label->GetFont(); Font.Size = 18; Label->SetFont(Font);
			Row->AddChildToHorizontalBox(Label)->SetVerticalAlignment(VAlign_Center);
			const int32 Assigned = Snapshot.CandidateCharacterIds.IndexOfByKey(Id);
			Button->SetIsEnabled((Assigned == INDEX_NONE || Assigned == SelectedSlot) && !bSubmitting && !bSubmitted);
			Button->SetBackgroundColor(Assigned == SelectedSlot ? FLinearColor(.65f,.47f,.18f) : FLinearColor(.08f,.12f,.2f));
			Bind(Button, INDEX_NONE, Id);
			Choices->AddChild(Button);
		}
	}
	if (auto* BuffHost = Cast<UPanelWidget>(WidgetTree->FindWidget(TEXT("BuffSelection"))))
	{
		BuffHost->ClearChildren();
		TSet<FName> Seen;
		for (const auto& Definition : AvailableBuffs)
		{
			if (!Definition || Seen.Contains(Definition->BuffId) || !IsAvailableBuff(Definition->BuffId)) continue;
			Seen.Add(Definition->BuffId);
			auto* Button = WidgetTree->ConstructWidget<UButton>();
			auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
			const bool bSelected = Snapshot.BuffIds.Contains(Definition->BuffId);
			Label->SetText(FText::Format(NSLOCTEXT("HSRPreBattle", "BuffChoice", "{0}{1}"),
				bSelected ? NSLOCTEXT("HSRPreBattle", "Selected", "已选 · ") : FText::GetEmpty(), BuffDisplayNames.FindChecked(Definition->BuffId)));
			Button->SetContent(Label);
			Button->SetIsEnabled(!bSubmitting && !bSubmitted);
			Bind(Button, INDEX_NONE, Definition->BuffId, true);
			BuffHost->AddChild(Button);
		}
		BuffHost->SetVisibility(Seen.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (auto* SelectedBuff = WidgetTree->FindWidget(TEXT("Text_SelectedBuff"))) SelectedBuff->SetVisibility(ESlateVisibility::Collapsed);
}
