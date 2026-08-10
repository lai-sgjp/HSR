#include "HSRRelicEquipmentWidget.h"

#include "HSRRelicEquipmentViewModel.h"
#include "../../Data/Definitions/HSREquipmentEnhancementCatalog.h"
#include "../../Data/Definitions/HSRItemEquipmentMappingCatalog.h"
#include "../../Equipment/HSREquipmentSubsystem.h"
#include "../../Equipment/HSREquipmentTypes.h"
#include "../../Inventory/HSRInventorySubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UHSRRelicEquipmentWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ViewModel = NewObject<UHSRRelicEquipmentViewModel>(this);
	UE_LOG(LogTemp, Log, TEXT("HSRRelic[%p] Construct VM=%d CharacterId=%s valid=%d"),
		this, ViewModel != nullptr, *CharacterId.ToString(), CharacterId.IsValid());
	if (ViewModel)
	{
		SnapshotHandle = ViewModel->OnChanged().AddUObject(this, &ThisClass::HandleSnapshot);
		if (CharacterId.IsValid()) InitializeRuntimeContext();
		else OnRelicUnavailable(EHSRRelicEquipmentResult::NotInitialized);
	}
	else
	{
		OnRelicUnavailable(EHSRRelicEquipmentResult::NotInitialized);
	}
}

void UHSRRelicEquipmentWidget::NativeDestruct()
{
	if (ViewModel)
	{
		if (SnapshotHandle.IsValid()) ViewModel->OnChanged().Remove(SnapshotHandle);
		SnapshotHandle.Reset();
		ViewModel->Shutdown();
		ViewModel = nullptr;
	}
	bHasSnapshot = false;
	Super::NativeDestruct();
}

void UHSRRelicEquipmentWidget::InitializeForCharacter(const FGuid& InCharacterId,
	UHSRItemEquipmentMappingCatalog* InMappingCatalog,
	UHSREquipmentEnhancementCatalog* InEnhancementCatalog)
{
	CharacterId = InCharacterId;
	if (InMappingCatalog) MappingCatalog = InMappingCatalog;
	if (InEnhancementCatalog) EnhancementCatalog = InEnhancementCatalog;
	if (IsConstructed()) InitializeRuntimeContext();
}

void UHSRRelicEquipmentWidget::InitializeForCharacterProfile(const FName CharacterProfileId)
{
	InitializeForCharacter(HSRCharacterGuidFromProfileName(CharacterProfileId));
}

void UHSRRelicEquipmentWidget::InitializeRuntimeContext()
{
	if (!ViewModel) return;
	UGameInstance* GameInstance = GetGameInstance();
	UHSREquipmentSubsystem* Equipment = GameInstance
		? GameInstance->GetSubsystem<UHSREquipmentSubsystem>() : nullptr;
	UHSRInventorySubsystem* Inventory = GameInstance
		? GameInstance->GetSubsystem<UHSRInventorySubsystem>() : nullptr;
	UE_LOG(LogTemp, Log, TEXT("HSRRelic[%p] InitRuntime equip=%d inv=%d mapCat=%d enhCat=%d char=%d"),
		this, Equipment != nullptr, Inventory != nullptr, MappingCatalog != nullptr,
		EnhancementCatalog != nullptr, CharacterId.IsValid());
	ViewModel->Initialize(Equipment, Inventory, MappingCatalog, EnhancementCatalog, CharacterId);
}

EHSRRelicEquipmentResult UHSRRelicEquipmentWidget::SelectSlot(const EHSRRelicSlot InSlot)
{
	const EHSRRelicEquipmentResult Result = ViewModel
		? ViewModel->SelectSlot(InSlot) : EHSRRelicEquipmentResult::NotInitialized;
	UE_LOG(LogTemp, Log, TEXT("HSRRelic[%p] SelectSlot slot=%d result=%d"),
		this, static_cast<int32>(InSlot), static_cast<int32>(Result));
	return Result;
}

EHSRRelicEquipmentResult UHSRRelicEquipmentWidget::SelectCandidate(const FGuid& InInstanceId)
{
	const EHSRRelicEquipmentResult Result = ViewModel
		? ViewModel->SelectCandidate(InInstanceId) : EHSRRelicEquipmentResult::NotInitialized;
	UE_LOG(LogTemp, Log, TEXT("HSRRelic[%p] SelectCandidate id=%s result=%d"),
		this, *InInstanceId.ToString(), static_cast<int32>(Result));
	return Result;
}

EHSRRelicEquipmentResult UHSRRelicEquipmentWidget::OpenEnhancement()
{
	const EHSRRelicEquipmentResult Result = ViewModel
		? ViewModel->OpenEnhancement() : EHSRRelicEquipmentResult::NotInitialized;
	UE_LOG(LogTemp, Log, TEXT("HSRRelic[%p] OpenEnhancement result=%d"), this, static_cast<int32>(Result));
	return Result;
}

EHSRRelicEquipmentResult UHSRRelicEquipmentWidget::CommitSelectedMovement()
{
	const EHSRRelicEquipmentResult Result = ViewModel
		? ViewModel->CommitSelectedMovement() : EHSRRelicEquipmentResult::NotInitialized;
	UE_LOG(LogTemp, Log, TEXT("HSRRelic[%p] CommitMovement result=%d"), this, static_cast<int32>(Result));
	ShowOperationResult(Result);
	return Result;
}

EHSRRelicEquipmentResult UHSRRelicEquipmentWidget::CommitEnhancement(const int32 TargetLevel)
{
	const EHSRRelicEquipmentResult Result = ViewModel
		? ViewModel->CommitEnhancement(TargetLevel) : EHSRRelicEquipmentResult::NotInitialized;
	UE_LOG(LogTemp, Log, TEXT("HSRRelic[%p] CommitEnhancement target=%d result=%d"),
		this, TargetLevel, static_cast<int32>(Result));
	ShowOperationResult(Result);
	return Result;
}

EHSRRelicEquipmentResult UHSRRelicEquipmentWidget::Back()
{
	return ViewModel ? ViewModel->Back() : EHSRRelicEquipmentResult::NotInitialized;
}

bool UHSRRelicEquipmentWidget::GetCurrentSnapshot(FHSRRelicEquipmentSnapshot& OutSnapshot) const
{
	if (!bHasSnapshot) return false;
	OutSnapshot = CurrentSnapshot;
	return true;
}

bool UHSRRelicEquipmentWidget::GetEnhancementOption(const int32 Index,
	FHSRRelicEnhancementOption& OutOption) const
{
	if (!bHasSnapshot || !CurrentSnapshot.EnhancementOptions.IsValidIndex(Index)) return false;
	OutOption = CurrentSnapshot.EnhancementOptions[Index];
	return true;
}

int32 UHSRRelicEquipmentWidget::GetEnhancementOptionCount() const
{
	return bHasSnapshot ? CurrentSnapshot.EnhancementOptions.Num() : 0;
}

bool UHSRRelicEquipmentWidget::HasEnhancementOptions() const
{
	return GetEnhancementOptionCount() > 0;
}

void UHSRRelicEquipmentWidget::HandleSnapshot(const FHSRRelicEquipmentSnapshot& InSnapshot)
{
	CurrentSnapshot = InSnapshot;
	bHasSnapshot = true;
	UE_LOG(LogTemp, Log, TEXT("HSRRelic[%p] Snapshot stage=%d valid=%d reason=%d options=%d slots=%d cand=%d curInst=%d"),
		this, static_cast<int32>(InSnapshot.Stage), InSnapshot.bIsValid,
		static_cast<int32>(InSnapshot.FailureReason), InSnapshot.EnhancementOptions.Num(),
		InSnapshot.Slots.Num(), InSnapshot.Candidates.Num(), InSnapshot.CurrentInstanceId.IsValid());
	UpdateStatusText(InSnapshot);
	if (UButton* EnhanceButton = WidgetTree ? WidgetTree->FindWidget<UButton>(TEXT("BTN_Enhance")) : nullptr)
	{
		EnhanceButton->SetIsEnabled(InSnapshot.CurrentInstanceId.IsValid());
	}
	if (UButton* ConfirmButton = WidgetTree ? WidgetTree->FindWidget<UButton>(TEXT("BTN_ConfirmEnhance")) : nullptr)
	{
		const bool bHasAffordableOption = InSnapshot.Stage == EHSRRelicEquipmentStage::Enhancement
			&& InSnapshot.EnhancementOptions.ContainsByPredicate(
				[](const FHSRRelicEnhancementOption& Option) { return Option.bAvailable && Option.bAffordable; });
		ConfirmButton->SetIsEnabled(bHasAffordableOption);
	}
	OnRelicSnapshotChanged(InSnapshot);
	if (!InSnapshot.bIsValid) OnRelicUnavailable(InSnapshot.FailureReason);
	// Apply once immediately after the Blueprint callback so the user never sees the
	// previous stage's panel/list for a frame. The deferred pass below handles Blueprint
	// graphs that schedule their own updates after this callback returns.
	ApplyStageVisibility();
	PopulateCandidates();
	PopulateEnhancementOptions();
	// The Blueprint event also drives visibility + repopulates the lists through graph loops.
	// Run the same C++ pass on the next tick so it remains the final word after any deferred BP work.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick([this]()
		{
			if (!IsValid(this) || !bHasSnapshot) return;
			ApplyStageVisibility();
			PopulateCandidates();
			PopulateEnhancementOptions();
		});
	}
}

void UHSRRelicEquipmentWidget::ApplyStageVisibility()
{
	if (!WidgetTree) return;
	const auto RevealWidgetChain = [](UWidget* Leaf)
	{
		for (UWidget* Current = Leaf; Current; Current = Current->GetParent())
		{
			Current->SetVisibility(ESlateVisibility::Visible);
		}
	};
	UWidget* SlotBox = WidgetTree->FindWidget(TEXT("SlotBox"));
	UWidget* CandidateBox = WidgetTree->FindWidget(TEXT("CandidateBox"));
	UWidget* ComparisonBox = WidgetTree->FindWidget(TEXT("ComparisonBox"));
	UWidget* EnhanceBox = WidgetTree->FindWidget(TEXT("EnhanceBox"));
	UWidget* SlotListHost = WidgetTree->FindWidget(TEXT("SlotListHost"));
	UWidget* CandidateListHost = WidgetTree->FindWidget(TEXT("CandidateListHost"));
	UWidget* EnhancementOptionsHost = WidgetTree->FindWidget(TEXT("EnhancementOptionsHost"));
	const EHSRRelicEquipmentStage Stage = CurrentSnapshot.Stage;
	const bool bShowSlots = ShouldShowSlotAndCandidateLists(Stage);
	if (SlotBox) SlotBox->SetVisibility(bShowSlots ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (CandidateBox) CandidateBox->SetVisibility(bShowSlots ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (ComparisonBox) ComparisonBox->SetVisibility(Stage == EHSRRelicEquipmentStage::Comparison ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (EnhanceBox) EnhanceBox->SetVisibility(ShouldShowEnhancementOptions(Stage) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bShowSlots)
	{
		RevealWidgetChain(SlotListHost);
		RevealWidgetChain(CandidateListHost);
	}
	if (ShouldShowEnhancementOptions(Stage))
	{
		RevealWidgetChain(EnhancementOptionsHost);
	}
	UE_LOG(LogTemp, Log, TEXT("HSRRelic[%p] ApplyStageVisibility Stage=%d Slot=%s Candidate=%s Comparison=%s Enhance=%s"),
		this, static_cast<int32>(Stage),
		SlotBox ? *UEnum::GetValueAsString(SlotBox->GetVisibility()) : TEXT("Missing"),
		CandidateBox ? *UEnum::GetValueAsString(CandidateBox->GetVisibility()) : TEXT("Missing"),
		ComparisonBox ? *UEnum::GetValueAsString(ComparisonBox->GetVisibility()) : TEXT("Missing"),
		EnhanceBox ? *UEnum::GetValueAsString(EnhanceBox->GetVisibility()) : TEXT("Missing"));
}

void UHSRRelicEquipmentWidget::UpdateStatusText(const FHSRRelicEquipmentSnapshot& InSnapshot)
{
	if (!WidgetTree) return;
	UTextBlock* Status = WidgetTree->FindWidget<UTextBlock>(TEXT("TXT_Status"));
	if (!Status) return;

	// Show which candidate relic is selected, so equipping has a visible target.
	const FHSRRelicCandidateRow* Selected = InSnapshot.Candidates.FindByPredicate(
		[&InSnapshot](const FHSRRelicCandidateRow& Row) { return Row.InstanceId == InSnapshot.SelectedCandidateId; });
	if (InSnapshot.Stage == EHSRRelicEquipmentStage::Comparison && Selected)
	{
		Status->SetText(FText::Format(
			NSLOCTEXT("HSRRelic", "SelectedRelic", "Selected: {0} ({1})"),
			FText::FromName(Selected->DefinitionId), FText::FromName(Selected->ItemId)));
	}
	else if (InSnapshot.Stage == EHSRRelicEquipmentStage::Enhancement)
	{
		const FHSRRelicSlotRow* Current = InSnapshot.Slots.FindByPredicate(
			[&InSnapshot](const FHSRRelicSlotRow& Row) { return Row.Slot == InSnapshot.SelectedSlot; });
		if (Current && Current->bHasEquipped)
		{
			const FString InstanceText = Current->EquippedInstanceId.ToString(EGuidFormats::Digits).Left(8);
			Status->SetText(FText::Format(
				NSLOCTEXT("HSRRelic", "EnhanceTarget", "Enhancing {0} ({1}) Lv{2} - choose target level"),
				FText::FromName(Current->EquippedInstance.DefinitionId),
				FText::FromString(InstanceText),
				FText::AsNumber(Current->EquippedInstance.EnhancementLevel)));
		}
		else
		{
			Status->SetText(NSLOCTEXT("HSRRelic", "EnhanceNoTarget", "No equipped relic selected"));
		}
	}
	else if (InSnapshot.Stage == EHSRRelicEquipmentStage::CandidateSelection)
	{
		Status->SetText(InSnapshot.Candidates.IsEmpty()
			? NSLOCTEXT("HSRRelic", "NoCandidates", "No relics available for this slot")
			: NSLOCTEXT("HSRRelic", "PickCandidate", "Select a relic from the list to equip"));
	}
	else
	{
		Status->SetText(NSLOCTEXT("HSRRelic", "PickSlot", "Select a slot to equip"));
	}
}

void UHSRRelicEquipmentWidget::ShowOperationResult(EHSRRelicEquipmentResult Result)
{
	if (!WidgetTree) return;
	UTextBlock* Status = WidgetTree->FindWidget<UTextBlock>(TEXT("TXT_Status"));
	if (!Status) return;
	switch (Result)
	{
	case EHSRRelicEquipmentResult::Success:
		Status->SetText(NSLOCTEXT("HSRRelic", "EquipOk", "Equipped successfully"));
		break;
	case EHSRRelicEquipmentResult::InsufficientMaterial:
		Status->SetText(NSLOCTEXT("HSRRelic", "NoMaterial", "Not enough material"));
		break;
	case EHSRRelicEquipmentResult::ComparisonUnavailable:
		Status->SetText(NSLOCTEXT("HSRRelic", "NoSelection", "Select a relic first"));
		break;
	default:
		Status->SetText(FText::Format(NSLOCTEXT("HSRRelic", "Failed", "Failed ({0})"),
			StaticEnum<EHSRRelicEquipmentResult>()->GetDisplayNameTextByValue(static_cast<int64>(Result))));
		break;
	}
}

UButton* UHSRRelicEquipmentWidget::MakeListButton(const FText& Label, const FLinearColor& Color)
{
	if (!WidgetTree) return nullptr;
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	if (!Button) return nullptr;
	Button->SetVisibility(ESlateVisibility::Visible);
	Button->SetBackgroundColor(Color);
	UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (RowBox) Button->SetContent(RowBox);
	UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelText->SetText(Label);
	LabelText->SetColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.92f, 0.96f, 1.0f)));
	FSlateFontInfo Font = LabelText->GetFont();
	Font.Size = 14;
	LabelText->SetFont(Font);
	if (RowBox) RowBox->AddChild(LabelText);
	return Button;
}

void UHSRRelicEquipmentWidget::PopulateCandidates()
{
	if (!WidgetTree) return;
	UPanelWidget* Host = Cast<UPanelWidget>(WidgetTree->FindWidget(TEXT("CandidateListHost")));
	if (!Host)
	{
		UE_LOG(LogTemp, Warning, TEXT("HSRRelic[%p] PopulateCandidates HostMissing Name=CandidateListHost"), this);
		return;
	}
	Host->ClearChildren();
	ListBindings.Reset();
	for (const FHSRRelicCandidateRow& Row : CurrentSnapshot.Candidates)
	{
		const FText Label = FText::Format(NSLOCTEXT("HSRRelic", "CandidateLabel", "{0} (Lv{1})"),
			FText::FromName(Row.DefinitionId), FText::AsNumber(Row.Instance.EnhancementLevel));
		UButton* Button = MakeListButton(Label,
			Row.bIsSelected ? FLinearColor(0.78f, 0.61f, 0.24f, 0.35f) : FLinearColor(1.0f, 1.0f, 1.0f, 0.05f));
		if (!Button) continue;
		// A dynamically-created Button has no authored slot constraints. Give each row
		// an explicit footprint so ScrollBox/VBox layout cannot collapse it to zero.
		USizeBox* RowSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		RowSize->SetMinDesiredWidth(280.0f);
		RowSize->SetMinDesiredHeight(44.0f);
		RowSize->AddChild(Button);
		if (UVerticalBoxSlot* ListSlot = Cast<UVerticalBoxSlot>(Host->AddChild(RowSize)))
		{
			ListSlot->SetHorizontalAlignment(HAlign_Fill);
			ListSlot->SetPadding(FMargin(4.0f, 3.0f, 4.0f, 3.0f));
		}
		UHSRRelicListClickBridge* Bridge = NewObject<UHSRRelicListClickBridge>(this);
		Bridge->Initialize(this, Row.InstanceId, -1);
		Button->OnClicked.AddDynamic(Bridge, &UHSRRelicListClickBridge::HandleClicked);
		ListBindings.Add(Bridge);
	}
	UE_LOG(LogTemp, Log, TEXT("HSRRelic[%p] PopulateCandidates Host=%s Children=%d Rows=%d"),
		this, *Host->GetName(), Host->GetChildrenCount(), CurrentSnapshot.Candidates.Num());
}

void UHSRRelicEquipmentWidget::PopulateEnhancementOptions()
{
	if (!WidgetTree) return;
	UPanelWidget* Host = Cast<UPanelWidget>(WidgetTree->FindWidget(TEXT("EnhancementOptionsHost")));
	if (!Host)
	{
		UE_LOG(LogTemp, Warning, TEXT("HSRRelic[%p] PopulateEnhancementOptions HostMissing Name=EnhancementOptionsHost"), this);
		return;
	}
	Host->ClearChildren();
	for (const FHSRRelicEnhancementOption& Option : CurrentSnapshot.EnhancementOptions)
	{
		const FText Label = FText::Format(NSLOCTEXT("HSRRelic", "EnhanceLabel", "Lv{0} (-{1} {2})"),
			FText::AsNumber(Option.TargetLevel), FText::AsNumber(Option.MaterialCost),
			FText::FromName(Option.MaterialItemId));
		UButton* Button = MakeListButton(Label,
			Option.bAffordable ? FLinearColor(1.0f, 1.0f, 1.0f, 0.08f) : FLinearColor(0.4f, 0.4f, 0.4f, 0.15f));
		if (!Button) continue;
		Button->SetIsEnabled(Option.bAffordable);
		USizeBox* RowSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		RowSize->SetMinDesiredWidth(280.0f);
		RowSize->SetMinDesiredHeight(44.0f);
		RowSize->AddChild(Button);
		if (UVerticalBoxSlot* ListSlot = Cast<UVerticalBoxSlot>(Host->AddChild(RowSize)))
		{
			ListSlot->SetHorizontalAlignment(HAlign_Fill);
			ListSlot->SetPadding(FMargin(4.0f, 2.0f, 4.0f, 2.0f));
		}
		UHSRRelicListClickBridge* Bridge = NewObject<UHSRRelicListClickBridge>(this);
		Bridge->Initialize(this, FGuid(), Option.TargetLevel);
		Button->OnClicked.AddDynamic(Bridge, &UHSRRelicListClickBridge::HandleClicked);
		ListBindings.Add(Bridge);
	}
	UE_LOG(LogTemp, Log, TEXT("HSRRelic[%p] PopulateEnhancementOptions Host=%s Children=%d Rows=%d"),
		this, *Host->GetName(), Host->GetChildrenCount(), CurrentSnapshot.EnhancementOptions.Num());
}

void UHSRRelicListClickBridge::Initialize(UHSRRelicEquipmentWidget* InOwner, FGuid InInstanceId, int32 InTargetLevel)
{
	Owner = InOwner;
	InstanceId = InInstanceId;
	TargetLevel = InTargetLevel;
}

void UHSRRelicListClickBridge::HandleClicked()
{
	if (!Owner.IsValid()) return;
	if (TargetLevel >= 0) Owner->CommitEnhancement(TargetLevel);
	else Owner->SelectCandidate(InstanceId);
}
