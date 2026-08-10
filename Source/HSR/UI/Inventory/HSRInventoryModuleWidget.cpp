#include "HSRInventoryModuleWidget.h"

#include "HSRInventoryViewModel.h"
#include "../../Data/Definitions/HSREquipmentEnhancementCatalog.h"
#include "../../Data/Definitions/HSRInventoryCatalog.h"
#include "../../Data/Definitions/HSRItemEquipmentMappingCatalog.h"
#include "../../Equipment/HSREquipmentSubsystem.h"
#include "../../Inventory/HSRInventorySubsystem.h"
#include "../../Party/HSRPartySubsystem.h"
#include "../../Party/HSRPartyTypes.h"
#include "../HSRUIManagerSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"

void UHSRInventoryRowClickBridge::Initialize(UHSRInventoryModuleWidget* const InOwner,
	const int32 InRowIndex)
{
	Owner = InOwner;
	RowIndex = InRowIndex;
}

void UHSRInventoryRowClickBridge::HandleClicked()
{
	if (UHSRInventoryModuleWidget* Widget = Owner.Get())
	{
		Widget->SelectEntryByIndex(RowIndex);
	}
}

void UHSRInventoryModuleWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!ViewModel)
	{
		ViewModel = NewObject<UHSRInventoryViewModel>(this);
	}
	BindAndRefresh();
	InitializeRuntimeContext();

	// Back and Close must always be reachable, even before a valid snapshot exists.
	if (UButton* BackButton = FindButtonByName(TEXT("BTN_Back")))
	{
		BackButton->OnClicked.Clear();
		BackButton->OnClicked.AddDynamic(this, &UHSRInventoryModuleWidget::HandleBackClicked);
	}
	if (UButton* CloseButton = FindButtonByName(TEXT("BTN_Close")))
	{
		CloseButton->OnClicked.Clear();
		CloseButton->OnClicked.AddDynamic(this, &UHSRInventoryModuleWidget::HandleCloseClicked);
	}
}

void UHSRInventoryModuleWidget::NativeDestruct()
{
	if (ViewModel) ViewModel->Shutdown();
	SetViewModel(nullptr);
	bHasSnapshot = false;
	CurrentSnapshot = FHSRInventoryModuleSnapshot();
	Super::NativeDestruct();
}

void UHSRInventoryModuleWidget::InitializeForInventory(UHSRInventoryCatalog* InCatalog)
{
	if (InCatalog) Catalog = InCatalog;
	if (IsConstructed()) InitializeRuntimeContext();
}

void UHSRInventoryModuleWidget::InitializeCommandContext(const FGuid& InCharacterId,
	UHSRItemEquipmentMappingCatalog* InMappingCatalog,
	UHSREquipmentEnhancementCatalog* InEnhancementCatalog)
{
	CharacterId = InCharacterId;
	if (InMappingCatalog) MappingCatalog = InMappingCatalog;
	if (InEnhancementCatalog) EnhancementCatalog = InEnhancementCatalog;
	if (IsConstructed()) InitializeRuntimeContext();

	// The UIManager validates the snapshot immediately after InitializeCommandContext,
	// which can run before this widget is constructed (CreateWidget does not call
	// NativeConstruct). Ensure the ViewModel exists, is initialized, and has bound the
	// snapshot so GetCurrentSnapshot reflects the committed state right away. NativeConstruct
	// reuses the same ViewModel and re-binding is idempotent.
	if (!ViewModel)
	{
		ViewModel = NewObject<UHSRInventoryViewModel>(this);
	}
	InitializeRuntimeContext();
	BindAndRefresh();
}

bool UHSRInventoryModuleWidget::RequestCloseToRoot()
{
	return GetOwningUIManager()
		&& GetOwningUIManager()->CloseFrontendToRoot() == EHSRUIScreenResult::Success;
}

void UHSRInventoryModuleWidget::SetViewModel(UHSRInventoryViewModel* InViewModel)
{
	if (ViewModel && SnapshotHandle.IsValid())
	{
		ViewModel->OnChanged().Remove(SnapshotHandle);
		SnapshotHandle.Reset();
#if WITH_DEV_AUTOMATION_TESTS
		++UnbindCount;
#endif
	}
	ViewModel = InViewModel;
	if (IsConstructed()) BindAndRefresh();
}

EHSRInventoryViewModelResult UHSRInventoryModuleWidget::SelectCategory(
	const EHSRInventoryCategory InCategory)
{
	return ViewModel ? ViewModel->SelectCategory(InCategory)
		: EHSRInventoryViewModelResult::NotInitialized;
}

EHSRInventoryViewModelResult UHSRInventoryModuleWidget::SetFilterText(const FString& InFilterText)
{
	return ViewModel ? ViewModel->SetFilterText(InFilterText)
		: EHSRInventoryViewModelResult::NotInitialized;
}

EHSRInventoryViewModelResult UHSRInventoryModuleWidget::SetSortMode(
	const EHSRInventorySortMode InSortMode)
{
	return ViewModel ? ViewModel->SetSortMode(InSortMode)
		: EHSRInventoryViewModelResult::NotInitialized;
}

EHSRInventoryViewModelResult UHSRInventoryModuleWidget::SelectEntry(
	const FHSRInventoryEntryKey& InKey)
{
	return ViewModel ? ViewModel->SelectEntry(InKey)
		: EHSRInventoryViewModelResult::NotInitialized;
}

EHSRInventoryViewModelResult UHSRInventoryModuleWidget::SubmitAction(
	const EHSRInventoryAction Action, const int32 TargetLevel)
{
	return ViewModel ? ViewModel->SubmitAction(Action, TargetLevel)
		: EHSRInventoryViewModelResult::NotInitialized;
}

bool UHSRInventoryModuleWidget::GetCurrentSnapshot(
	FHSRInventoryModuleSnapshot& OutSnapshot) const
{
	if (!bHasSnapshot) return false;
	OutSnapshot = CurrentSnapshot;
	return true;
}

bool UHSRInventoryModuleWidget::GetEntry(const int32 Index,
	FHSRInventoryEntryRow& OutEntry) const
{
	if (!bHasSnapshot || !CurrentSnapshot.Entries.IsValidIndex(Index)) return false;
	OutEntry = CurrentSnapshot.Entries[Index];
	return true;
}

bool UHSRInventoryModuleWidget::GetActionState(const int32 Index,
	FHSRInventoryActionState& OutAction) const
{
	if (!bHasSnapshot || !CurrentSnapshot.Actions.IsValidIndex(Index)) return false;
	OutAction = CurrentSnapshot.Actions[Index];
	return true;
}

int32 UHSRInventoryModuleWidget::GetEntryCount() const
{
	return bHasSnapshot ? CurrentSnapshot.Entries.Num() : 0;
}

bool UHSRInventoryModuleWidget::GetEntryDisplay(const int32 Index, FString& OutDisplayName,
	int32& OutQuantity, bool& bOutIsUnique) const
{
	if (!bHasSnapshot || !CurrentSnapshot.Entries.IsValidIndex(Index)) return false;
	const FHSRInventoryEntryRow& Row = CurrentSnapshot.Entries[Index];
	OutDisplayName = Row.DisplayName.ToString();
	OutQuantity = Row.Quantity;
	bOutIsUnique = Row.bIsUnique;
	return true;
}

bool UHSRInventoryModuleWidget::GetSelectedDetail(FString& OutName, int32& OutQuantity,
	bool& bOutHasSelection) const
{
	OutName = TEXT("");
	OutQuantity = 0;
	bOutHasSelection = false;
	if (!bHasSnapshot) return false;
	bOutHasSelection = CurrentSnapshot.Detail.bHasSelection;
	if (bOutHasSelection)
	{
		OutName = CurrentSnapshot.Detail.Entry.DisplayName.ToString();
		OutQuantity = CurrentSnapshot.Detail.Entry.Quantity;
	}
	return true;
}

EHSRInventoryViewModelResult UHSRInventoryModuleWidget::SelectEntryByIndex(const int32 Index)
{
	if (!ViewModel || !bHasSnapshot || !CurrentSnapshot.Entries.IsValidIndex(Index))
	{
		return EHSRInventoryViewModelResult::EntryUnavailable;
	}
	return ViewModel->SelectEntry(CurrentSnapshot.Entries[Index].Key);
}

bool UHSRInventoryModuleWidget::GetActionAvailable(const EHSRInventoryAction Action) const
{
	for (const FHSRInventoryActionState& ActionState : CurrentSnapshot.Actions)
	{
		if (ActionState.Action == Action) return ActionState.bIsAvailable;
	}
	return false;
}

int32 UHSRInventoryModuleWidget::GetSelectedEnhancementTargetLevel() const
{
	if (!bHasSnapshot || !CurrentSnapshot.Detail.bHasSelection) return -1;
	for (const FHSRInventoryEnhancementOption& Option : CurrentSnapshot.EnhancementOptions)
	{
		if (Option.bAvailable) return Option.TargetLevel;
	}
	return -1;
}

void UHSRInventoryModuleWidget::RefreshListAndDetail()
{
	if (!bHasSnapshot) return;
	OnInventorySnapshotChanged(CurrentSnapshot);
}

void UHSRInventoryModuleWidget::InitializeRuntimeContext()
{
	if (!ViewModel) return;
	UGameInstance* GameInstance = GetGameInstance();
	UHSRInventorySubsystem* Inventory = GameInstance
		? GameInstance->GetSubsystem<UHSRInventorySubsystem>() : nullptr;
	UHSREquipmentSubsystem* Equipment = GameInstance
		? GameInstance->GetSubsystem<UHSREquipmentSubsystem>() : nullptr;
	ViewModel->Initialize(Inventory, Catalog);
	ViewModel->SetCommandContext(Equipment, MappingCatalog, EnhancementCatalog, CharacterId);
}

void UHSRInventoryModuleWidget::BindAndRefresh()
{
	if (!ViewModel) return;
	if (SnapshotHandle.IsValid())
	{
		ViewModel->OnChanged().Remove(SnapshotHandle);
		SnapshotHandle.Reset();
#if WITH_DEV_AUTOMATION_TESTS
		++UnbindCount;
#endif
	}
	SnapshotHandle = ViewModel->OnChanged().AddUObject(this, &ThisClass::HandleSnapshot);
#if WITH_DEV_AUTOMATION_TESTS
	++BindCount;
#endif
	FHSRInventoryModuleSnapshot InitialSnapshot;
	if (ViewModel->GetSnapshot(InitialSnapshot)) HandleSnapshot(InitialSnapshot);
}

void UHSRInventoryModuleWidget::HandleSnapshot(
	const FHSRInventoryModuleSnapshot& InSnapshot)
{
	CurrentSnapshot = InSnapshot;
	bHasSnapshot = true;
	UpdateTargetCharacterText();
	OnInventorySnapshotChanged(InSnapshot);
	if (InSnapshot.bIsValid) PopulateListAndDetail();
	if (!InSnapshot.bIsValid) OnInventoryUnavailable(InSnapshot.FailureReason);
}

void UHSRInventoryModuleWidget::UpdateTargetCharacterText()
{
	if (!WidgetTree) return;
	UTextBlock* TextBlock = FindTextByName(TEXT("TXT_TargetCharacter"));
	if (!TextBlock) return;
	FName CharacterIdName = NAME_None;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UHSRPartySubsystem* Party = GameInstance->GetSubsystem<UHSRPartySubsystem>())
		{
			FHSRPartySnapshot PartySnapshot;
			if (Party->GetSnapshot(PartySnapshot) && !PartySnapshot.Slots.IsEmpty())
			{
				// Mirror ResolveInventoryCharacterGuid: the actively-controlled member, falling
				// back to the leader when the active slot is unset or empty.
				int32 TargetSlot = PartySnapshot.ActiveSlot;
				if (TargetSlot < 0 || TargetSlot >= PartySnapshot.Slots.Num()
					|| PartySnapshot.Slots[TargetSlot].IsEmpty())
				{
					TargetSlot = 0;
				}
				if (TargetSlot >= 0 && TargetSlot < PartySnapshot.Slots.Num()
					&& !PartySnapshot.Slots[TargetSlot].IsEmpty())
				{
					CharacterIdName = PartySnapshot.Slots[TargetSlot].CharacterId;
				}
			}
		}
	}
	TextBlock->SetText(CharacterIdName.IsNone()
		? NSLOCTEXT("HSRInventory", "NoTarget", "Target: -")
		: FText::Format(NSLOCTEXT("HSRInventory", "TargetChar", "Target: {0}"), FText::FromName(CharacterIdName)));
}

void UHSRInventoryModuleWidget::PopulateListAndDetail()
{
	UWidgetTree* Tree = WidgetTree;
	if (!Tree) return;

	PopulateListRows();

	for (UTextBlock* Text : {FindTextByName(TEXT("TXT_DetailName")),
		FindTextByName(TEXT("TXT_DetailQuantity")),
		FindTextByName(TEXT("TXT_DetailDescription"))})
	{
		if (!Text) continue;
		if (CurrentSnapshot.Detail.bHasSelection)
		{
			const FHSRInventoryEntryRow& Row = CurrentSnapshot.Detail.Entry;
			if (Text == FindTextByName(TEXT("TXT_DetailName")))
			{
				Text->SetText(Row.DisplayName);
			}
			else if (Text == FindTextByName(TEXT("TXT_DetailQuantity")))
			{
				Text->SetText(FText::Format(NSLOCTEXT("HSRInventory", "Quantity", "x{0}"),
					FText::AsNumber(Row.Quantity)));
			}
			else
			{
				Text->SetText(FText::Format(
					NSLOCTEXT("HSRInventory", "DetailDesc", "{0} (分类已就绪)"),
					Row.DisplayName));
			}
		}
		else
		{
			if (Text == FindTextByName(TEXT("TXT_DetailName")))
			{
				Text->SetText(NSLOCTEXT("HSRInventory", "NoSelection", "选择物品"));
			}
			else if (Text == FindTextByName(TEXT("TXT_DetailQuantity")))
			{
				Text->SetText(FText::GetEmpty());
			}
			else
			{
				Text->SetText(NSLOCTEXT("HSRInventory", "NoSelectionDesc", "选择左侧物品以查看详情"));
			}
		}
	}

	SetActionButton(TEXT("BTN_ActionUse"), EHSRInventoryAction::Use);
	SetActionButton(TEXT("BTN_ActionEquip"), EHSRInventoryAction::Equip);
	SetActionButton(TEXT("BTN_ActionEnhance"), EHSRInventoryAction::Enhance);
	SetActionButton(TEXT("BTN_ActionDisassemble"), EHSRInventoryAction::Disassemble);
}

void UHSRInventoryModuleWidget::SetActionButton(const FName ButtonName,
	const EHSRInventoryAction Action)
{
	UButton* Button = FindButtonByName(ButtonName);
	if (!Button) return;
	Button->OnClicked.Clear();
	const bool bAvailable = GetActionAvailable(Action);
	Button->SetIsEnabled(bAvailable);
	if (!bAvailable) return;
	switch (Action)
	{
	case EHSRInventoryAction::Equip:
		Button->OnClicked.AddDynamic(this, &UHSRInventoryModuleWidget::HandleEquipClicked);
		break;
	case EHSRInventoryAction::Enhance:
		Button->OnClicked.AddDynamic(this, &UHSRInventoryModuleWidget::HandleEnhanceClicked);
		break;
	case EHSRInventoryAction::Use:
		Button->OnClicked.AddDynamic(this, &UHSRInventoryModuleWidget::HandleUseClicked);
		break;
	case EHSRInventoryAction::Disassemble:
		Button->OnClicked.AddDynamic(this, &UHSRInventoryModuleWidget::HandleDisassembleClicked);
		break;
	default:
		break;
	}
}

UButton* UHSRInventoryModuleWidget::FindButtonByName(const FName Name) const
{
	if (!WidgetTree) return nullptr;
	return WidgetTree->FindWidget<UButton>(Name);
}

UTextBlock* UHSRInventoryModuleWidget::FindTextByName(const FName Name) const
{
	if (!WidgetTree) return nullptr;
	return WidgetTree->FindWidget<UTextBlock>(Name);
}

void UHSRInventoryModuleWidget::HandleBackClicked()
{
	RequestBack();
}

void UHSRInventoryModuleWidget::HandleCloseClicked()
{
	RequestCloseToRoot();
}

void UHSRInventoryModuleWidget::HandleUseClicked()
{
	SubmitAction(EHSRInventoryAction::Use, -1);
}

void UHSRInventoryModuleWidget::HandleEquipClicked()
{
	SubmitAction(EHSRInventoryAction::Equip, -1);
}

void UHSRInventoryModuleWidget::HandleEnhanceClicked()
{
	SubmitAction(EHSRInventoryAction::Enhance, GetSelectedEnhancementTargetLevel());
}

void UHSRInventoryModuleWidget::HandleDisassembleClicked()
{
	SubmitAction(EHSRInventoryAction::Disassemble, -1);
}

void UHSRInventoryModuleWidget::PopulateListRows()
{
	UScrollBox* ScrollBox = WidgetTree ? WidgetTree->FindWidget<UScrollBox>(TEXT("ListScrollBox")) : nullptr;
	UVerticalBox* Host = WidgetTree ? WidgetTree->FindWidget<UVerticalBox>(TEXT("ListHost")) : nullptr;
	if (!ScrollBox || !Host)
	{
		return;
	}
	Host->ClearChildren();
	RowBridges.Reset();
	for (int32 Index = 0; Index < CurrentSnapshot.Entries.Num(); ++Index)
	{
		const FHSRInventoryEntryRow& Row = CurrentSnapshot.Entries[Index];
		UButton* RowButton = NewObject<UButton>(this);
		if (!RowButton) continue;
		RowButton->SetVisibility(ESlateVisibility::Visible);
		const bool bSelected = Row.Key == CurrentSnapshot.SelectedKey;
		// Selected row: gold-tinted fill + bright text; otherwise subtle dark fill + muted text.
		RowButton->SetBackgroundColor(bSelected
			? FLinearColor(0.78f, 0.61f, 0.24f, 0.35f)
			: FLinearColor(1.0f, 1.0f, 1.0f, 0.05f));
		if (UVerticalBoxSlot* RowSlot = Cast<UVerticalBoxSlot>(Host->AddChild(RowButton)))
		{
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
			RowSlot->SetPadding(FMargin(4.0f, 3.0f, 4.0f, 3.0f));
		}
		UHorizontalBox* RowBox = NewObject<UHorizontalBox>(RowButton);
		if (!RowBox) continue;
		RowButton->SetContent(RowBox);
		UTextBlock* NameText = NewObject<UTextBlock>(RowButton);
		NameText->SetText(Row.DisplayName);
		NameText->SetColorAndOpacity(bSelected
			? FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f))
			: FSlateColor(FLinearColor(0.60f, 0.64f, 0.71f, 1.0f)));
		FSlateFontInfo NameFont = NameText->GetFont();
		NameFont.Size = 16;
		NameText->SetFont(NameFont);
		RowBox->AddChild(NameText);
		UTextBlock* QtyText = NewObject<UTextBlock>(RowButton);
		QtyText->SetText(FText::Format(NSLOCTEXT("HSRInventory", "RowQty", "x{0}"),
			FText::AsNumber(Row.Quantity)));
		QtyText->SetColorAndOpacity(bSelected
			? FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f))
			: FSlateColor(FLinearColor(0.60f, 0.64f, 0.71f, 1.0f)));
		FSlateFontInfo QtyFont = QtyText->GetFont();
		QtyFont.Size = 14;
		QtyText->SetFont(QtyFont);
		RowBox->AddChild(QtyText);
		UHSRInventoryRowClickBridge* Bridge = NewObject<UHSRInventoryRowClickBridge>(this);
		Bridge->Initialize(this, Index);
		RowButton->OnClicked.AddDynamic(Bridge, &UHSRInventoryRowClickBridge::HandleClicked);
		RowBridges.Add(Bridge);
	}
}
