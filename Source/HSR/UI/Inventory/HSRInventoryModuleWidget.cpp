#include "HSRInventoryModuleWidget.h"
#include "HSRInventoryViewModel.h"
#include "../../Data/Definitions/HSRCharacterDefinition.h"
#include "../../Data/Definitions/HSREquipmentEnhancementCatalog.h"
#include "../../Data/Definitions/HSRInventoryCatalog.h"
#include "../../Data/Definitions/HSRItemEquipmentMappingCatalog.h"
#include "../../Equipment/HSREquipmentSubsystem.h"
#include "../../Inventory/HSRInventorySubsystem.h"
#include "../../Progression/HSRCharacterProfileSubsystem.h"
#include "../HSRUIManagerSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"

namespace
{
FText CharacterName(UGameInstance* GameInstance, const FGuid& Id)
{
	if (GameInstance)
	{
		if (auto* Profiles = GameInstance->GetSubsystem<UHSRCharacterProfileSubsystem>())
		{
			TArray<FHSRCharacterProfileSnapshot> Rows; Profiles->GetAllProfileSnapshots(Rows);
			for (const auto& Row : Rows)
			{
				if (HSRCharacterGuidFromProfileName(Row.RuntimeState.CharacterId) == Id)
				{
					const UHSRCharacterDefinition* Definition = nullptr;
					if (Profiles->GetDefinition(Row.RuntimeState.CharacterId, Definition) && Definition)
						return Definition->DisplayName;
				}
			}
		}
	}
	return NSLOCTEXT("HSRInventory", "NoTarget", "未选择角色");
}
FString StatsText(const TArray<FHSREquipmentModifier>& Modifiers)
{
	FString Result;
	for (const auto& Modifier : Modifiers)
	{
		const TCHAR* Name = TEXT("攻击");
		switch (Modifier.Stat) {
		case EHSREquipmentStat::MaxHealth: Name = TEXT("生命上限"); break;
		case EHSREquipmentStat::Defense: Name = TEXT("防御"); break;
		case EHSREquipmentStat::Speed: Name = TEXT("速度"); break;
		default: break;
		}
		Result += FString::Printf(TEXT("\n%s  %+.1f"), Name, Modifier.Value);
	}
	return Result;
}
}

void UHSRInventoryRowClickBridge::Initialize(UHSRInventoryModuleWidget* InOwner, const FHSRInventoryEntryKey& InKey)
{
	Owner = InOwner; Key = InKey;
}
void UHSRInventoryRowClickBridge::HandleClicked()
{
	if (auto* Widget = Owner.Get()) Widget->SelectEntry(Key);
}
void UHSRInventoryModuleWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!ViewModel) ViewModel = NewObject<UHSRInventoryViewModel>(this);
	BindAndRefresh();
	if (!bHasSnapshot || !CurrentSnapshot.bIsValid) InitializeRuntimeContext();
#define BIND_BUTTON(Name, Handler) if (auto* Button = FindButtonByName(TEXT(Name))) { Button->OnClicked.Clear(); Button->OnClicked.AddDynamic(this, &ThisClass::Handler); }
	BIND_BUTTON("BTN_Back", HandleBackClicked)
	BIND_BUTTON("BTN_Close", HandleCloseClicked)
	BIND_BUTTON("BTN_ConfirmAction", HandleConfirmClicked)
	BIND_BUTTON("BTN_CancelAction", HandleCancelClicked)
	BIND_BUTTON("BTN_NextEnhancement", HandleNextEnhancementClicked)
	BIND_BUTTON("BTN_NextCharacter", CycleTargetCharacter)
	BIND_BUTTON("BTN_CatAll", HandleCategoryAll)
	BIND_BUTTON("BTN_CatWeapon", HandleCategoryWeapon)
	BIND_BUTTON("BTN_CatRelic", HandleCategoryRelic)
	BIND_BUTTON("BTN_CatConsumable", HandleCategoryConsumable)
	BIND_BUTTON("BTN_CatMaterial", HandleCategoryMaterial)
	BIND_BUTTON("BTN_CatOther", HandleCategoryOther)
	BIND_BUTTON("BTN_CycleSort", CycleSortMode)
#undef BIND_BUTTON
	if (auto* Search = WidgetTree ? WidgetTree->FindWidget<UEditableTextBox>(TEXT("SearchBox")) : nullptr)
	{
		Search->OnTextChanged.Clear();
		Search->OnTextCommitted.Clear();
		Search->SetText(FText::FromString(CurrentSnapshot.FilterText));
		Search->SetHintText(NSLOCTEXT("HSRInventory", "SearchHint", "搜索物品名称"));
		Search->OnTextChanged.AddDynamic(this, &ThisClass::HandleSearchChanged);
	}
	RefreshBrowseControls();
	RefreshActionPreview();
}
void UHSRInventoryModuleWidget::NativeDestruct()
{
	if (auto* Search = WidgetTree ? WidgetTree->FindWidget<UEditableTextBox>(TEXT("SearchBox")) : nullptr)
		Search->OnTextChanged.RemoveDynamic(this, &ThisClass::HandleSearchChanged);
	if (ViewModel) ViewModel->Shutdown();
	SetViewModel(nullptr);
	bHasSnapshot = false; CurrentSnapshot = FHSRInventoryModuleSnapshot();
	bHasPendingAction = false; RowBridges.Reset();
	Super::NativeDestruct();
}
void UHSRInventoryModuleWidget::InitializeForInventory(UHSRInventoryCatalog* InCatalog)
{
	if (InCatalog) Catalog = InCatalog;
	if (IsConstructed()) InitializeRuntimeContext();
}
void UHSRInventoryModuleWidget::InitializeCommandContext(const FGuid& InCharacterId,
	UHSRItemEquipmentMappingCatalog* InMappingCatalog, UHSREquipmentEnhancementCatalog* InEnhancementCatalog)
{
	CharacterId = InCharacterId;
	if (InMappingCatalog) MappingCatalog = InMappingCatalog;
	if (InEnhancementCatalog) EnhancementCatalog = InEnhancementCatalog;
	if (!ViewModel) ViewModel = NewObject<UHSRInventoryViewModel>(this);
	InitializeRuntimeContext(); BindAndRefresh();
}
bool UHSRInventoryModuleWidget::RequestCloseToRoot()
{
	return GetOwningUIManager() && GetOwningUIManager()->CloseFrontendToRoot() == EHSRUIScreenResult::Success;
}
void UHSRInventoryModuleWidget::SetViewModel(UHSRInventoryViewModel* InViewModel)
{
	if (ViewModel && SnapshotHandle.IsValid()) {
		ViewModel->OnChanged().Remove(SnapshotHandle); SnapshotHandle.Reset();
#if WITH_DEV_AUTOMATION_TESTS
		++UnbindCount;
#endif
	}
	ViewModel = InViewModel;
	if (IsConstructed()) BindAndRefresh();
}
EHSRInventoryViewModelResult UHSRInventoryModuleWidget::SelectCategory(EHSRInventoryCategory InCategory)
{
	CancelAction();
	return ViewModel ? ViewModel->SelectCategory(InCategory) : EHSRInventoryViewModelResult::NotInitialized;
}
EHSRInventoryViewModelResult UHSRInventoryModuleWidget::SetFilterText(const FString& InFilterText)
{
	CancelAction();
	return ViewModel ? ViewModel->SetFilterText(InFilterText) : EHSRInventoryViewModelResult::NotInitialized;
}
EHSRInventoryViewModelResult UHSRInventoryModuleWidget::SetSortMode(EHSRInventorySortMode InSortMode)
{
	return ViewModel ? ViewModel->SetSortMode(InSortMode) : EHSRInventoryViewModelResult::NotInitialized;
}
EHSRInventoryViewModelResult UHSRInventoryModuleWidget::SelectEntry(const FHSRInventoryEntryKey& InKey)
{
	CancelAction();
	return ViewModel ? ViewModel->SelectEntry(InKey) : EHSRInventoryViewModelResult::NotInitialized;
}
EHSRInventoryViewModelResult UHSRInventoryModuleWidget::SubmitAction(EHSRInventoryAction Action, int32 TargetLevel)
{
	if (bSubmitting) return EHSRInventoryViewModelResult::AuthorityRejected;
	TGuardValue<bool> Guard(bSubmitting, true);
	const auto Result = ViewModel ? ViewModel->SubmitAction(Action, TargetLevel) : EHSRInventoryViewModelResult::NotInitialized;
	ShowActionResult(Result);
	return Result;
}
bool UHSRInventoryModuleWidget::GetCurrentSnapshot(FHSRInventoryModuleSnapshot& OutSnapshot) const
{
	if (!bHasSnapshot) return false;
	OutSnapshot = CurrentSnapshot; return true;
}
bool UHSRInventoryModuleWidget::GetEntry(int32 Index, FHSRInventoryEntryRow& OutEntry) const
{
	if (!bHasSnapshot || !CurrentSnapshot.Entries.IsValidIndex(Index)) return false;
	OutEntry = CurrentSnapshot.Entries[Index]; return true;
}
bool UHSRInventoryModuleWidget::GetActionState(int32 Index, FHSRInventoryActionState& OutAction) const
{
	if (!bHasSnapshot || !CurrentSnapshot.Actions.IsValidIndex(Index)) return false;
	OutAction = CurrentSnapshot.Actions[Index]; return true;
}
int32 UHSRInventoryModuleWidget::GetEntryCount() const { return bHasSnapshot ? CurrentSnapshot.Entries.Num() : 0; }
bool UHSRInventoryModuleWidget::GetEntryDisplay(int32 Index, FString& OutName, int32& OutQuantity, bool& bOutUnique) const
{
	FHSRInventoryEntryRow Row;
	if (!GetEntry(Index, Row)) return false;
	OutName = Row.DisplayName.ToString(); OutQuantity = Row.Quantity; bOutUnique = Row.bIsUnique; return true;
}
bool UHSRInventoryModuleWidget::GetSelectedDetail(FString& OutName, int32& OutQuantity, bool& bOutSelection) const
{
	OutName.Reset(); OutQuantity = 0; bOutSelection = bHasSnapshot && CurrentSnapshot.Detail.bHasSelection;
	if (bOutSelection) { OutName = CurrentSnapshot.Detail.Entry.DisplayName.ToString(); OutQuantity = CurrentSnapshot.Detail.Entry.Quantity; }
	return bHasSnapshot;
}
EHSRInventoryViewModelResult UHSRInventoryModuleWidget::SelectEntryByIndex(int32 Index)
{
	FHSRInventoryEntryRow Row;
	return GetEntry(Index, Row) ? SelectEntry(Row.Key) : EHSRInventoryViewModelResult::EntryUnavailable;
}
bool UHSRInventoryModuleWidget::GetActionAvailable(EHSRInventoryAction Action) const
{
	if (!bHasSnapshot || !CurrentSnapshot.bIsValid) return false;
	for (const auto& State : CurrentSnapshot.Actions) if (State.Action == Action) return State.bIsAvailable;
	return false;
}
int32 UHSRInventoryModuleWidget::GetSelectedEnhancementTargetLevel() const
{
	return bHasPendingAction && PendingAction == EHSRInventoryAction::Enhance ? PendingTargetLevel : -1;
}
void UHSRInventoryModuleWidget::RefreshListAndDetail()
{
	if (bHasSnapshot) { OnInventorySnapshotChanged(CurrentSnapshot); PopulateListAndDetail(); }
}
void UHSRInventoryModuleWidget::InitializeRuntimeContext()
{
	if (!ViewModel) return;
	auto* GI = GetGameInstance();
	ViewModel->Initialize(GI ? GI->GetSubsystem<UHSRInventorySubsystem>() : nullptr, Catalog);
	ViewModel->SetCommandContext(GI ? GI->GetSubsystem<UHSREquipmentSubsystem>() : nullptr, MappingCatalog, EnhancementCatalog, CharacterId);
}
void UHSRInventoryModuleWidget::BindAndRefresh()
{
	if (!ViewModel) return;
	if (SnapshotHandle.IsValid()) {
		ViewModel->OnChanged().Remove(SnapshotHandle); SnapshotHandle.Reset();
#if WITH_DEV_AUTOMATION_TESTS
		++UnbindCount;
#endif
	}
	SnapshotHandle = ViewModel->OnChanged().AddUObject(this, &ThisClass::HandleSnapshot);
#if WITH_DEV_AUTOMATION_TESTS
	++BindCount;
#endif
	FHSRInventoryModuleSnapshot Initial;
	if (ViewModel->GetSnapshot(Initial)) HandleSnapshot(Initial);
}
void UHSRInventoryModuleWidget::HandleSnapshot(const FHSRInventoryModuleSnapshot& InSnapshot)
{
	if (bHasPendingAction && (InSnapshot.SelectedKey != PendingKey || InSnapshot.InventoryRevision != PendingInventoryRevision
		|| InSnapshot.EquipmentRevision != PendingEquipmentRevision || InSnapshot.TargetCharacterId != PendingCharacterId))
	{
		bHasPendingAction = false;
		ActionMessage = NSLOCTEXT("HSRInventory", "PreviewChanged", "物品状态已更新，请重新预览操作。");
	}
	CurrentSnapshot = InSnapshot; bHasSnapshot = true;
	UpdateTargetCharacterText(); OnInventorySnapshotChanged(InSnapshot);
	PopulateListAndDetail();
	if (!InSnapshot.bIsValid) OnInventoryUnavailable(InSnapshot.FailureReason);
}
void UHSRInventoryModuleWidget::UpdateTargetCharacterText()
{
	if (auto* Text = FindTextByName(TEXT("TXT_TargetCharacter")))
		Text->SetText(FText::Format(NSLOCTEXT("HSRInventory", "Target", "装备给：{0}"), CharacterName(GetGameInstance(), CurrentSnapshot.TargetCharacterId)));
}
void UHSRInventoryModuleWidget::PopulateListAndDetail()
{
	if (!WidgetTree) return;
	RefreshPresentationLabels();
	PopulateListRows();
	RefreshBrowseControls();
	const auto& Row = CurrentSnapshot.Detail.Entry;
	const bool bSelected = CurrentSnapshot.Detail.bHasSelection;
	if (auto* Text = FindTextByName(TEXT("TXT_DetailName"))) Text->SetText(bSelected ? Row.DisplayName : NSLOCTEXT("HSRInventory", "Select", "选择物品"));
	if (auto* Text = FindTextByName(TEXT("TXT_DetailQuantity"))) Text->SetText(bSelected ? FText::Format(NSLOCTEXT("HSRInventory", "Quantity", "数量：{0}"), FText::AsNumber(Row.Quantity)) : FText::GetEmpty());
	FString Detail = Row.Description.ToString();
	if (bSelected && Row.bIsUnique) {
		Detail += FString::Printf(TEXT("\n稀有度 %d  ·  强化 +%d"), Row.Rarity, Row.EnhancementLevel);
		Detail += Row.EquippedCharacterId.IsValid() ? TEXT("\n已装备：") + CharacterName(GetGameInstance(), Row.EquippedCharacterId).ToString() : TEXT("\n未装备");
		Detail += StatsText(Row.Modifiers);
	}
	if (auto* Text = FindTextByName(TEXT("TXT_DetailDescription"))) {
		Text->SetAutoWrapText(true);
		Text->SetText(bSelected ? FText::FromString(Detail) : NSLOCTEXT("HSRInventory", "SelectHint", "选择物品查看详情。装备和强化会先展示操作预览。"));
	}
	if (auto* Icon = WidgetTree->FindWidget<UImage>(TEXT("IMG_DetailIcon"))) {
		Icon->SetBrushFromTexture(bSelected ? Row.Icon.LoadSynchronous() : nullptr);
		Icon->SetVisibility(bSelected && !Row.Icon.IsNull() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (auto* Text = FindTextByName(TEXT("TXT_EmptyState"))) {
		Text->SetText(CurrentSnapshot.FilterText.IsEmpty() ? NSLOCTEXT("HSRInventory", "Empty", "此分类暂无物品") : NSLOCTEXT("HSRInventory", "NoMatches", "没有符合搜索条件的物品"));
		Text->SetVisibility(CurrentSnapshot.Entries.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	SetActionButton(TEXT("BTN_ActionUse"), EHSRInventoryAction::Use);
	SetActionButton(TEXT("BTN_ActionEquip"), EHSRInventoryAction::Equip);
	SetActionButton(TEXT("BTN_ActionEnhance"), EHSRInventoryAction::Enhance);
	SetActionButton(TEXT("BTN_ActionDisassemble"), EHSRInventoryAction::Disassemble);
	RefreshActionPreview();
}
void UHSRInventoryModuleWidget::SetActionButton(FName Name, EHSRInventoryAction Action)
{
	auto* Button = FindButtonByName(Name); if (!Button) return;
	Button->OnClicked.Clear();
	if (Action == EHSRInventoryAction::Use || Action == EHSRInventoryAction::Disassemble) { Button->SetVisibility(ESlateVisibility::Collapsed); return; }
	Button->SetVisibility(ESlateVisibility::Visible);
	Button->SetIsEnabled(GetActionAvailable(Action) && !bHasPendingAction && !bSubmitting);
	Button->SetToolTipText(Action == EHSRInventoryAction::Equip ? NSLOCTEXT("HSRInventory", "EquipTip", "预览当前角色的装备替换") : NSLOCTEXT("HSRInventory", "EnhanceTip", "预览等级、属性和材料消耗"));
	if (Action == EHSRInventoryAction::Equip) Button->OnClicked.AddDynamic(this, &ThisClass::HandleEquipClicked);
	if (Action == EHSRInventoryAction::Enhance) Button->OnClicked.AddDynamic(this, &ThisClass::HandleEnhanceClicked);
}
UButton* UHSRInventoryModuleWidget::FindButtonByName(FName Name) const { return WidgetTree ? WidgetTree->FindWidget<UButton>(Name) : nullptr; }
UTextBlock* UHSRInventoryModuleWidget::FindTextByName(FName Name) const { return WidgetTree ? WidgetTree->FindWidget<UTextBlock>(Name) : nullptr; }
void UHSRInventoryModuleWidget::HandleBackClicked() { if (bHasPendingAction) CancelAction(); else RequestBack(); }
void UHSRInventoryModuleWidget::HandleCloseClicked() { RequestCloseToRoot(); }
void UHSRInventoryModuleWidget::HandleUseClicked() { SubmitAction(EHSRInventoryAction::Use); }
void UHSRInventoryModuleWidget::HandleEquipClicked() { PreviewAction(EHSRInventoryAction::Equip); }
void UHSRInventoryModuleWidget::HandleEnhanceClicked() { PreviewAction(EHSRInventoryAction::Enhance); }
void UHSRInventoryModuleWidget::HandleDisassembleClicked() { SubmitAction(EHSRInventoryAction::Disassemble); }
void UHSRInventoryModuleWidget::HandleConfirmClicked() { ConfirmAction(); }
void UHSRInventoryModuleWidget::HandleCancelClicked() { CancelAction(); }
void UHSRInventoryModuleWidget::HandleNextEnhancementClicked()
{
	if (!bHasPendingAction || PendingAction != EHSRInventoryAction::Enhance || CurrentSnapshot.EnhancementOptions.IsEmpty()) return;
	int32 Index = CurrentSnapshot.EnhancementOptions.IndexOfByPredicate([&](const auto& Option) { return Option.TargetLevel == PendingTargetLevel; });
	PendingTargetLevel = CurrentSnapshot.EnhancementOptions[(Index + 1) % CurrentSnapshot.EnhancementOptions.Num()].TargetLevel;
	RefreshActionPreview();
}
bool UHSRInventoryModuleWidget::PreviewAction(EHSRInventoryAction Action, int32 TargetLevel)
{
	if (bSubmitting || bHasPendingAction || !GetActionAvailable(Action) || (Action != EHSRInventoryAction::Equip && Action != EHSRInventoryAction::Enhance)) return false;
	if (Action == EHSRInventoryAction::Enhance) {
		if (CurrentSnapshot.EnhancementOptions.IsEmpty()) return false;
		if (TargetLevel < 0) TargetLevel = CurrentSnapshot.EnhancementOptions[0].TargetLevel;
		if (!CurrentSnapshot.EnhancementOptions.ContainsByPredicate([&](const auto& Option) { return Option.TargetLevel == TargetLevel; })) return false;
	}
	PendingAction = Action; PendingTargetLevel = TargetLevel; PendingKey = CurrentSnapshot.SelectedKey;
	PendingInventoryRevision = CurrentSnapshot.InventoryRevision; PendingEquipmentRevision = CurrentSnapshot.EquipmentRevision;
	PendingCharacterId = CurrentSnapshot.TargetCharacterId; bHasPendingAction = true; ActionMessage = FText::GetEmpty();
	PopulateListAndDetail(); return true;
}
EHSRInventoryViewModelResult UHSRInventoryModuleWidget::ConfirmAction()
{
	if (!bHasPendingAction || bSubmitting) return EHSRInventoryViewModelResult::EntryUnavailable;
	const auto Action = PendingAction; const int32 Level = PendingTargetLevel;
	if (PendingKey != CurrentSnapshot.SelectedKey || PendingInventoryRevision != CurrentSnapshot.InventoryRevision
		|| PendingEquipmentRevision != CurrentSnapshot.EquipmentRevision || PendingCharacterId != CurrentSnapshot.TargetCharacterId) {
		CancelAction(); ShowActionResult(EHSRInventoryViewModelResult::StaleSnapshot); return EHSRInventoryViewModelResult::StaleSnapshot;
	}
	if (Action == EHSRInventoryAction::Enhance && !CurrentSnapshot.EnhancementOptions.ContainsByPredicate([&](const auto& Option) { return Option.TargetLevel == Level && Option.bAvailable; })) {
		ActionMessage = NSLOCTEXT("HSRInventory", "Insufficient", "强化材料不足"); RefreshActionPreview(); return EHSRInventoryViewModelResult::AuthorityRejected;
	}
	bHasPendingAction = false;
	const auto Result = SubmitAction(Action, Level);
	PopulateListAndDetail(); return Result;
}
void UHSRInventoryModuleWidget::CancelAction() { bHasPendingAction = false; PendingTargetLevel = -1; ActionMessage = FText::GetEmpty(); PopulateListAndDetail(); }
void UHSRInventoryModuleWidget::ShowActionResult(EHSRInventoryViewModelResult Result)
{
	if (Result == EHSRInventoryViewModelResult::Success) ActionMessage = NSLOCTEXT("HSRInventory", "Committed", "操作成功，物品状态已更新。");
	else if (Result == EHSRInventoryViewModelResult::StaleSnapshot) ActionMessage = NSLOCTEXT("HSRInventory", "Stale", "物品状态发生变化，请重新选择并预览。");
	else if (Result == EHSRInventoryViewModelResult::NoEnhancementOption) ActionMessage = NSLOCTEXT("HSRInventory", "AtCap", "没有更高等级的强化方案。");
	else ActionMessage = NSLOCTEXT("HSRInventory", "Rejected", "操作未成功。请检查物品归属、强化材料和装备配置。");
	RefreshActionPreview();
}
void UHSRInventoryModuleWidget::RefreshActionPreview()
{
	FString Preview; bool bCanConfirm = bHasPendingAction;
	if (bHasPendingAction) {
		const auto& Row = CurrentSnapshot.Detail.Entry;
		if (PendingAction == EHSRInventoryAction::Equip) {
			Preview = FString::Printf(TEXT("将「%s」装备给 %s。\n候选装备属性%s"), *Row.DisplayName.ToString(), *CharacterName(GetGameInstance(), PendingCharacterId).ToString(), *StatsText(Row.Modifiers));
			if (CurrentSnapshot.Detail.bReplacesEquipment) Preview += FString::Printf(TEXT("\n当前装备：%s%s\n确认后原装备返回背包。"), *CurrentSnapshot.Detail.ReplacedEquipmentName.ToString(), *StatsText(CurrentSnapshot.Detail.ReplacedModifiers));
			else Preview += TEXT("\n该部位当前为空。");
		} else {
			const auto* Option = CurrentSnapshot.EnhancementOptions.FindByPredicate([&](const auto& Value) { return Value.TargetLevel == PendingTargetLevel; });
			if (Option) {
				Preview = FString::Printf(TEXT("%s  +%d → +%d\n消耗 %s ×%d（持有 %d）\n强化后属性%s%s"), *Row.DisplayName.ToString(), Row.EnhancementLevel, PendingTargetLevel, *Option->MaterialName.ToString(), Option->MaterialCost, Option->OwnedMaterial, *StatsText(Option->TargetModifiers), Option->bAffordable ? TEXT("") : TEXT("\n材料不足"));
				bCanConfirm = Option->bAvailable;
			} else bCanConfirm = false;
		}
	}
	if (auto* Text = FindTextByName(TEXT("TXT_ActionPreview"))) { Text->SetAutoWrapText(true); Text->SetText(FText::FromString(Preview)); }
	if (auto* Text = FindTextByName(TEXT("TXT_ActionResult"))) Text->SetText(ActionMessage);
	for (const FName Name : {FName(TEXT("BTN_ConfirmAction")), FName(TEXT("BTN_CancelAction"))})
		if (auto* Button = FindButtonByName(Name)) Button->SetVisibility(bHasPendingAction ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (auto* Button = FindButtonByName(TEXT("BTN_ConfirmAction"))) Button->SetIsEnabled(bCanConfirm && !bSubmitting);
	if (auto* Button = FindButtonByName(TEXT("BTN_CancelAction"))) Button->SetIsEnabled(!bSubmitting);
	if (auto* Button = FindButtonByName(TEXT("BTN_NextEnhancement"))) {
		Button->SetVisibility(bHasPendingAction && PendingAction == EHSRInventoryAction::Enhance && CurrentSnapshot.EnhancementOptions.Num() > 1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Button->SetIsEnabled(!bSubmitting);
	}
}
void UHSRInventoryModuleWidget::CycleTargetCharacter()
{
	auto* GI = GetGameInstance(); if (!GI || !ViewModel) return;
	auto* Profiles = GI->GetSubsystem<UHSRCharacterProfileSubsystem>(); if (!Profiles) return;
	TArray<FHSRCharacterProfileSnapshot> Rows; Profiles->GetAllProfileSnapshots(Rows);
	Rows.Sort([](const auto& A, const auto& B) { return A.RuntimeState.CharacterId.LexicalLess(B.RuntimeState.CharacterId); });
	if (Rows.IsEmpty()) return;
	int32 Index = Rows.IndexOfByPredicate([&](const auto& Row) { return HSRCharacterGuidFromProfileName(Row.RuntimeState.CharacterId) == CharacterId; });
	CharacterId = HSRCharacterGuidFromProfileName(Rows[(Index + 1) % Rows.Num()].RuntimeState.CharacterId);
	CancelAction();
	ViewModel->SetCommandContext(GI->GetSubsystem<UHSREquipmentSubsystem>(), MappingCatalog, EnhancementCatalog, CharacterId);
}
void UHSRInventoryModuleWidget::PopulateListRows()
{
	auto* Host = WidgetTree ? WidgetTree->FindWidget<UVerticalBox>(TEXT("ListHost")) : nullptr;
	if (!Host) return;
	Host->ClearChildren(); RowBridges.Reset();
	for (const auto& Row : CurrentSnapshot.Entries) {
		auto* Button = NewObject<UButton>(this);
		const bool bSelected = Row.Key == CurrentSnapshot.SelectedKey;
		Button->SetColorAndOpacity(FLinearColor::White);
		Button->SetBackgroundColor(bSelected ? FLinearColor(.78f,.61f,.24f,.6f) : FLinearColor(.08f,.12f,.19f,.9f));
		if (auto* RowSlot = Host->AddChildToVerticalBox(Button)) { RowSlot->SetPadding(FMargin(4,4)); RowSlot->SetHorizontalAlignment(HAlign_Fill); }
		auto* Box = NewObject<UHorizontalBox>(Button); Button->SetContent(Box);
		if (auto* ContentSlot = Cast<UButtonSlot>(Box->Slot)) { ContentSlot->SetHorizontalAlignment(HAlign_Fill); ContentSlot->SetVerticalAlignment(VAlign_Center); }
		if (!Row.Icon.IsNull()) { auto* Icon = NewObject<UImage>(Button); Icon->SetBrushFromTexture(Row.Icon.LoadSynchronous()); Icon->SetDesiredSizeOverride(FVector2D(44,44)); Box->AddChildToHorizontalBox(Icon)->SetPadding(FMargin(8)); }
		auto* Name = NewObject<UTextBlock>(Button);
		Name->SetText(Row.DisplayName); Name->SetAutoWrapText(true);
		Name->SetColorAndOpacity(FSlateColor(FLinearColor(.95f, .97f, 1.f, 1.f)));
		Name->SetJustification(ETextJustify::Left);
		FSlateFontInfo Font = Name->GetFont(); Font.Size = 18; Name->SetFont(Font);
		auto* NameSlot = Box->AddChildToHorizontalBox(Name); NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); NameSlot->SetPadding(FMargin(12)); NameSlot->SetVerticalAlignment(VAlign_Center);
		auto* Info = NewObject<UTextBlock>(Button);
		Info->SetText(FText::FromString(Row.bIsUnique ? FString::Printf(TEXT("+%d  %s"), Row.EnhancementLevel, Row.EquippedCharacterId.IsValid() ? TEXT("已装备") : TEXT("未装备")) : FString::Printf(TEXT("×%d"), Row.Quantity)));
		Info->SetColorAndOpacity(FSlateColor(bSelected ? FLinearColor(.95f, .91f, .73f, 1.f) : FLinearColor(.73f, .79f, .87f, 1.f)));
		FSlateFontInfo InfoFont = Info->GetFont(); InfoFont.Size = 16; Info->SetFont(InfoFont);
		Info->SetJustification(ETextJustify::Right);
		auto* InfoSlot = Box->AddChildToHorizontalBox(Info); InfoSlot->SetPadding(FMargin(12)); InfoSlot->SetVerticalAlignment(VAlign_Center);
		auto* Bridge = NewObject<UHSRInventoryRowClickBridge>(this); Bridge->Initialize(this, Row.Key);
		Button->OnClicked.AddDynamic(Bridge, &UHSRInventoryRowClickBridge::HandleClicked); RowBridges.Add(Bridge);
	}
}


void UHSRInventoryModuleWidget::HandleCategoryAll() { SelectCategory(EHSRInventoryCategory::All); }
void UHSRInventoryModuleWidget::HandleCategoryWeapon() { SelectCategory(EHSRInventoryCategory::Weapon); }
void UHSRInventoryModuleWidget::HandleCategoryRelic() { SelectCategory(EHSRInventoryCategory::Relic); }
void UHSRInventoryModuleWidget::HandleCategoryConsumable() { SelectCategory(EHSRInventoryCategory::Consumable); }
void UHSRInventoryModuleWidget::HandleCategoryMaterial() { SelectCategory(EHSRInventoryCategory::Material); }
void UHSRInventoryModuleWidget::HandleCategoryOther() { SelectCategory(EHSRInventoryCategory::Other); }
void UHSRInventoryModuleWidget::HandleSearchChanged(const FText& Text)
{
	if (bUpdatingSearch) return;
	TGuardValue<bool> Guard(bUpdatingSearch, true);
	SetFilterText(Text.ToString());
}
void UHSRInventoryModuleWidget::CycleSortMode()
{
	const auto Next = CurrentSnapshot.SortMode == EHSRInventorySortMode::CatalogOrder
		? EHSRInventorySortMode::DisplayNameAscending
		: CurrentSnapshot.SortMode == EHSRInventorySortMode::DisplayNameAscending
			? EHSRInventorySortMode::QuantityDescending : EHSRInventorySortMode::CatalogOrder;
	SetSortMode(Next);
}
void UHSRInventoryModuleWidget::RefreshBrowseControls()
{
	if (!WidgetTree) return;
	const TPair<FName, EHSRInventoryCategory> Categories[] = {
		{TEXT("BTN_CatAll"), EHSRInventoryCategory::All},
		{TEXT("BTN_CatWeapon"), EHSRInventoryCategory::Weapon},
		{TEXT("BTN_CatRelic"), EHSRInventoryCategory::Relic},
		{TEXT("BTN_CatConsumable"), EHSRInventoryCategory::Consumable},
		{TEXT("BTN_CatMaterial"), EHSRInventoryCategory::Material},
		{TEXT("BTN_CatOther"), EHSRInventoryCategory::Other}};
	for (const auto& CategoryButton : Categories)
	{
		if (auto* Button = FindButtonByName(CategoryButton.Key))
			Button->SetBackgroundColor(CurrentSnapshot.Category == CategoryButton.Value
				? FLinearColor(.78f, .61f, .24f, .9f) : FLinearColor(.08f, .12f, .19f, .9f));
	}
	if (auto* Text = FindTextByName(TEXT("TXT_SortLabel")))
	{
		Text->SetText(CurrentSnapshot.SortMode == EHSRInventorySortMode::CatalogOrder
			? NSLOCTEXT("HSRInventory", "SortCatalog", "排序：默认")
			: CurrentSnapshot.SortMode == EHSRInventorySortMode::DisplayNameAscending
				? NSLOCTEXT("HSRInventory", "SortName", "排序：名称")
				: NSLOCTEXT("HSRInventory", "SortQuantity", "排序：数量"));
	}
	if (auto* Search = WidgetTree->FindWidget<UEditableTextBox>(TEXT("SearchBox")))
	{
		// Do not rewrite whitespace while typing: this preserves caret position and IME composition.
		FString VisibleFilter = Search->GetText().ToString(); VisibleFilter.TrimStartAndEndInline();
		if (!bUpdatingSearch && VisibleFilter != CurrentSnapshot.FilterText)
		{
			TGuardValue<bool> Guard(bUpdatingSearch, true);
			Search->SetText(FText::FromString(CurrentSnapshot.FilterText));
		}
	}
}


void UHSRInventoryModuleWidget::RefreshPresentationLabels()
{
	// Run after the legacy Blueprint snapshot event so authored English placeholders cannot win.
	const TPair<FName, FText> Labels[] = {
		{TEXT("TXT_Title"), NSLOCTEXT("HSRInventory", "Title", "背包")},
		{TEXT("TXT_Back"), NSLOCTEXT("HSRInventory", "Back", "返回")},
		{TEXT("TXT_Close"), NSLOCTEXT("HSRInventory", "Close", "关闭")},
		{TEXT("TXT_CatWeapon"), NSLOCTEXT("HSRInventory", "Weapon", "武器")},
		{TEXT("TXT_CatRelic"), NSLOCTEXT("HSRInventory", "Relic", "遗器")},
		{TEXT("TXT_CatConsumable"), NSLOCTEXT("HSRInventory", "Consumable", "消耗品")},
		{TEXT("TXT_CatMaterial"), NSLOCTEXT("HSRInventory", "Material", "材料")},
		{TEXT("TXT_CatOther"), NSLOCTEXT("HSRInventory", "Other", "其他")},
		{TEXT("TXT_ActionEquip"), NSLOCTEXT("HSRInventory", "EquipPreview", "装备预览")},
		{TEXT("TXT_ActionEnhance"), NSLOCTEXT("HSRInventory", "EnhancePreview", "强化预览")}};
	for (const auto& Label : Labels)
		if (auto* Text = FindTextByName(Label.Key)) Text->SetText(Label.Value);

	const TPair<FName, FText> ButtonLabels[] = {
		{TEXT("BTN_CatAll"), NSLOCTEXT("HSRInventory", "All", "全部")},
		{TEXT("BTN_CycleSort"), NSLOCTEXT("HSRInventory", "CycleSort", "切换排序")},
		{TEXT("BTN_NextCharacter"), NSLOCTEXT("HSRInventory", "NextCharacter", "切换装备角色")},
		{TEXT("BTN_ConfirmAction"), NSLOCTEXT("HSRInventory", "Confirm", "确认")},
		{TEXT("BTN_CancelAction"), NSLOCTEXT("HSRInventory", "Cancel", "取消")},
		{TEXT("BTN_NextEnhancement"), NSLOCTEXT("HSRInventory", "NextLevel", "选择目标等级")}};
	for (const auto& Label : ButtonLabels)
	{
		if (auto* Button = FindButtonByName(Label.Key))
		{
			if (auto* Text = Cast<UTextBlock>(Button->GetContent())) Text->SetText(Label.Value);
		}
	}

	TArray<UWidget*> Widgets;
	WidgetTree->GetAllWidgets(Widgets);
	for (auto* Widget : Widgets)
	{
		if (auto* Button = Cast<UButton>(Widget)) Button->SetColorAndOpacity(FLinearColor::White);
		if (auto* Border = Cast<UBorder>(Widget)) Border->SetContentColorAndOpacity(FLinearColor::White);
		if (auto* Text = Cast<UTextBlock>(Widget))
		{
			const bool bHeading = Text->GetFName() == TEXT("TXT_Title") || Text->GetFName() == TEXT("TXT_DetailName");
			const bool bSecondary = Text->GetFName() == TEXT("TXT_DetailDescription") || Text->GetFName() == TEXT("TXT_SortLabel");
			Text->SetColorAndOpacity(FSlateColor(bSecondary ? FLinearColor(.76f, .82f, .90f, 1.f) : FLinearColor(.95f, .97f, 1.f, 1.f)));
			FSlateFontInfo Font = Text->GetFont(); Font.Size = bHeading ? 26 : 18; Text->SetFont(Font);
		}
	}
	if (auto* Search = WidgetTree->FindWidget<UEditableTextBox>(TEXT("SearchBox")))
	{
		Search->SetForegroundColor(FLinearColor(.95f, .97f, 1.f, 1.f));
	}
	UpdateTargetCharacterText();
}


bool UHSRInventoryModuleWidget::CancelPreviewForBackKey(const FKey& Key)
{
	if (!bHasPendingAction || (Key != EKeys::Escape && Key != EKeys::Tab && Key != EKeys::Gamepad_Special_Right)) return false;
	CancelAction();
	return true;
}

FReply UHSRInventoryModuleWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	// Preview routing catches Escape before a focused search box or child button consumes it.
	if (CancelPreviewForBackKey(KeyEvent.GetKey())) return FReply::Handled();
	return Super::NativeOnPreviewKeyDown(Geometry, KeyEvent);
}
