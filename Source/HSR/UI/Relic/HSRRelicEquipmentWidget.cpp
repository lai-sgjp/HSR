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

// NativeConstruct：控件入树时自建遗器装备 ViewModel 并订阅其快照事件。
// 若已指定角色 GUID 则立即初始化运行时上下文，否则进入"未初始化"不可用态。
void UHSRRelicEquipmentWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ViewModel = NewObject<UHSRRelicEquipmentViewModel>(this);
	UE_LOG(LogTemp, Log, TEXT("HSRRelic[%p] Construct VM=%d CharacterId=%s valid=%d"),
		this, ViewModel != nullptr, *CharacterId.ToString(), CharacterId.IsValid());
	if (ViewModel)
	{
		SnapshotHandle = ViewModel->OnChanged().AddUObject(this, &ThisClass::HandleSnapshot);
		if (CharacterId.IsValid())
		{
			InitializeRuntimeContext();
		}
		else
		{
			OnRelicUnavailable(EHSRRelicEquipmentResult::NotInitialized);
		}
	}
	else
	{
		OnRelicUnavailable(EHSRRelicEquipmentResult::NotInitialized);
	}
}

// NativeDestruct：控件出树时解绑订阅、关闭 ViewModel 并清空快照缓存。
void UHSRRelicEquipmentWidget::NativeDestruct()
{
	if (ViewModel)
	{
		if (SnapshotHandle.IsValid())
		{
			ViewModel->OnChanged().Remove(SnapshotHandle);
		}
		SnapshotHandle.Reset();
		ViewModel->Shutdown();
		ViewModel = nullptr;
	}
	bHasSnapshot = false;
	Super::NativeDestruct();
}

// InitializeForCharacter：为指定角色 GUID 初始化本控件，并（可选）注入映射/强化目录。
// 若控件已入树则立即刷新运行时上下文；未入树时等 NativeConstruct 后再初始化。
void UHSRRelicEquipmentWidget::InitializeForCharacter(const FGuid& InCharacterId,
	UHSRItemEquipmentMappingCatalog* InMappingCatalog,
	UHSREquipmentEnhancementCatalog* InEnhancementCatalog)
{
	CharacterId = InCharacterId;
	if (InMappingCatalog)
	{
		MappingCatalog = InMappingCatalog;
	}
	if (InEnhancementCatalog)
	{
		EnhancementCatalog = InEnhancementCatalog;
	}
	if (IsConstructed())
	{
		InitializeRuntimeContext();
	}
}

// InitializeForCharacterProfile：按角色档案名（FName）定位 GUID 后再初始化。
void UHSRRelicEquipmentWidget::InitializeForCharacterProfile(const FName CharacterProfileId)
{
	InitializeForCharacter(HSRCharacterGuidFromProfileName(CharacterProfileId));
}

// InitializeRuntimeContext：从游戏实例取装备/背包子系统，连同目录与角色 GUID 一起注入 ViewModel。
void UHSRRelicEquipmentWidget::InitializeRuntimeContext()
{
	if (!ViewModel)
	{
		return;
	}
	UGameInstance* GameInstance = GetGameInstance();
	UHSREquipmentSubsystem* Equipment = GameInstance
		? GameInstance->GetSubsystem<UHSREquipmentSubsystem>()
		: nullptr;
	UHSRInventorySubsystem* Inventory = GameInstance
		? GameInstance->GetSubsystem<UHSRInventorySubsystem>()
		: nullptr;
	UE_LOG(LogTemp, Log, TEXT("HSRRelic[%p] InitRuntime equip=%d inv=%d mapCat=%d enhCat=%d char=%d"),
		this, Equipment != nullptr, Inventory != nullptr, MappingCatalog != nullptr,
		EnhancementCatalog != nullptr, CharacterId.IsValid());
	ViewModel->Initialize(Equipment, Inventory, MappingCatalog, EnhancementCatalog, CharacterId);
}

// SelectSlot：选择要装备的遗器槽位（转发给 ViewModel）。
EHSRRelicEquipmentResult UHSRRelicEquipmentWidget::SelectSlot(const EHSRRelicSlot InSlot)
{
	const EHSRRelicEquipmentResult Result = ViewModel
		? ViewModel->SelectSlot(InSlot)
		: EHSRRelicEquipmentResult::NotInitialized;
	UE_LOG(LogTemp, Log, TEXT("HSRRelic[%p] SelectSlot slot=%d result=%d"),
		this, static_cast<int32>(InSlot), static_cast<int32>(Result));
	return Result;
}

// SelectCandidate：选择候选遗器（转发给 ViewModel）。
EHSRRelicEquipmentResult UHSRRelicEquipmentWidget::SelectCandidate(const FGuid& InInstanceId)
{
	const EHSRRelicEquipmentResult Result = ViewModel
		? ViewModel->SelectCandidate(InInstanceId)
		: EHSRRelicEquipmentResult::NotInitialized;
	UE_LOG(LogTemp, Log, TEXT("HSRRelic[%p] SelectCandidate id=%s result=%d"),
		this, *InInstanceId.ToString(), static_cast<int32>(Result));
	return Result;
}

// OpenEnhancement：进入遗器强化流程（转发给 ViewModel）。
EHSRRelicEquipmentResult UHSRRelicEquipmentWidget::OpenEnhancement()
{
	const EHSRRelicEquipmentResult Result = ViewModel
		? ViewModel->OpenEnhancement()
		: EHSRRelicEquipmentResult::NotInitialized;
	UE_LOG(LogTemp, Log, TEXT("HSRRelic[%p] OpenEnhancement result=%d"), this, static_cast<int32>(Result));
	return Result;
}

// CommitSelectedMovement：把当前选中的遗器装到所选槽位（转发给 ViewModel 并显示操作结果）。
EHSRRelicEquipmentResult UHSRRelicEquipmentWidget::CommitSelectedMovement()
{
	const EHSRRelicEquipmentResult Result = ViewModel
		? ViewModel->CommitSelectedMovement()
		: EHSRRelicEquipmentResult::NotInitialized;
	UE_LOG(LogTemp, Log, TEXT("HSRRelic[%p] CommitMovement result=%d"), this, static_cast<int32>(Result));
	ShowOperationResult(Result);
	return Result;
}

// CommitEnhancement：把当前遗器强化到指定等级（转发给 ViewModel 并显示操作结果）。
EHSRRelicEquipmentResult UHSRRelicEquipmentWidget::CommitEnhancement(const int32 TargetLevel)
{
	const EHSRRelicEquipmentResult Result = ViewModel
		? ViewModel->CommitEnhancement(TargetLevel)
		: EHSRRelicEquipmentResult::NotInitialized;
	UE_LOG(LogTemp, Log, TEXT("HSRRelic[%p] CommitEnhancement target=%d result=%d"),
		this, TargetLevel, static_cast<int32>(Result));
	ShowOperationResult(Result);
	return Result;
}

// Back：返回上一级界面（转发给 ViewModel）。
EHSRRelicEquipmentResult UHSRRelicEquipmentWidget::Back()
{
	return ViewModel ? ViewModel->Back() : EHSRRelicEquipmentResult::NotInitialized;
}

// GetCurrentSnapshot：输出控件缓存的最近一次快照；尚无快照时返回 false。
bool UHSRRelicEquipmentWidget::GetCurrentSnapshot(FHSRRelicEquipmentSnapshot& OutSnapshot) const
{
	if (!bHasSnapshot)
	{
		return false;
	}
	OutSnapshot = CurrentSnapshot;
	return true;
}

// GetEnhancementOption：按下标读取强化选项；无快照或越界时返回 false。
bool UHSRRelicEquipmentWidget::GetEnhancementOption(const int32 Index,
	FHSRRelicEnhancementOption& OutOption) const
{
	if (!bHasSnapshot || !CurrentSnapshot.EnhancementOptions.IsValidIndex(Index))
	{
		return false;
	}
	OutOption = CurrentSnapshot.EnhancementOptions[Index];
	return true;
}

// GetEnhancementOptionCount：可用强化选项数量。
int32 UHSRRelicEquipmentWidget::GetEnhancementOptionCount() const
{
	return bHasSnapshot ? CurrentSnapshot.EnhancementOptions.Num() : 0;
}

// HasEnhancementOptions：是否存在强化选项（供蓝图快速判断）。
bool UHSRRelicEquipmentWidget::HasEnhancementOptions() const
{
	return GetEnhancementOptionCount() > 0;
}

// HandleSnapshot：ViewModel 广播新快照时的回调——更新状态文本、按钮可用性、
// 各阶段面板可见性、候选/强化列表，并推送蓝图事件。这是整个遗器装备界面的刷新核心。
void UHSRRelicEquipmentWidget::HandleSnapshot(const FHSRRelicEquipmentSnapshot& InSnapshot)
{
	CurrentSnapshot = InSnapshot;
	bHasSnapshot = true;
	UE_LOG(LogTemp, Log, TEXT("HSRRelic[%p] Snapshot stage=%d valid=%d reason=%d options=%d slots=%d cand=%d curInst=%d"),
		this, static_cast<int32>(InSnapshot.Stage), InSnapshot.bIsValid,
		static_cast<int32>(InSnapshot.FailureReason), InSnapshot.EnhancementOptions.Num(),
		InSnapshot.Slots.Num(), InSnapshot.Candidates.Num(), InSnapshot.CurrentInstanceId.IsValid());
	UpdateStatusText(InSnapshot);
	// "强化"按钮只在存在已选遗器时可用。
	if (UButton* EnhanceButton = WidgetTree ? WidgetTree->FindWidget<UButton>(TEXT("BTN_Enhance")) : nullptr)
	{
		EnhanceButton->SetIsEnabled(InSnapshot.CurrentInstanceId.IsValid());
	}
	// "确认强化"按钮只在强化阶段存在"可用且负担得起"的选项时可用。
	if (UButton* ConfirmButton = WidgetTree ? WidgetTree->FindWidget<UButton>(TEXT("BTN_ConfirmEnhance")) : nullptr)
	{
		const bool bHasAffordableOption = InSnapshot.Stage == EHSRRelicEquipmentStage::Enhancement
			&& InSnapshot.EnhancementOptions.ContainsByPredicate(
				[](const FHSRRelicEnhancementOption& Option) { return Option.bAvailable && Option.bAffordable; });
		ConfirmButton->SetIsEnabled(bHasAffordableOption);
	}
	OnRelicSnapshotChanged(InSnapshot);
	if (!InSnapshot.bIsValid)
	{
		OnRelicUnavailable(InSnapshot.FailureReason);
	}
	// Apply once immediately after the Blueprint callback so the user never sees the
	// previous stage's panel/list for a frame. The deferred pass below handles Blueprint
	// graphs that schedule their own updates after this callback returns.
	// 蓝图回调之后立即应用一次，确保用户不会看到旧阶段的面板/列表残留一帧；
	// 下面的延迟一帧再跑一次，是为了兜住蓝图端在自己回调返回后才调度更新的图。
	ApplyStageVisibility();
	PopulateCandidates();
	PopulateEnhancementOptions();
	// The Blueprint event also drives visibility + repopulates the lists through graph loops.
	// Run the same C++ pass on the next tick so it remains the final word after any deferred BP work.
	// 蓝图事件也会通过图里的循环驱动可见性与列表重填；下一帧再执行一次同样的 C++ 处理，
	// 保证在蓝图的延迟更新之后 C++ 仍是最终裁决。
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick([this]()
		{
			if (!IsValid(this) || !bHasSnapshot)
			{
				return;
			}
			ApplyStageVisibility();
			PopulateCandidates();
			PopulateEnhancementOptions();
		});
	}
}

// ApplyStageVisibility：按当前阶段切换各面板盒子的可见性。
// 阶段变化时"比较"面板与"强化"面板互斥显示；槽位/候选列表共享同一个显示条件。
// RevealWidgetChain 负责把列表宿主到根之间的整条父链全部设为可见，
// 防止父级 Collapsed 把动态子项也一起隐藏。
void UHSRRelicEquipmentWidget::ApplyStageVisibility()
{
	if (!WidgetTree)
	{
		return;
	}
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
	if (SlotBox)
	{
		SlotBox->SetVisibility(bShowSlots ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (CandidateBox)
	{
		CandidateBox->SetVisibility(bShowSlots ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (ComparisonBox)
	{
		ComparisonBox->SetVisibility(Stage == EHSRRelicEquipmentStage::Comparison ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (EnhanceBox)
	{
		EnhanceBox->SetVisibility(ShouldShowEnhancementOptions(Stage) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
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

// UpdateStatusText：根据快照状态更新界面底部的状态提示文本。
// 三种主要状态：比较阶段（显示当前选中的候选遗器）、强化阶段（显示强化目标与材料要求）、
// 候选选择阶段（提示从列表选择遗器）；都不匹配时提示先选择槽位。
void UHSRRelicEquipmentWidget::UpdateStatusText(const FHSRRelicEquipmentSnapshot& InSnapshot)
{
	if (!WidgetTree)
	{
		return;
	}
	UTextBlock* Status = WidgetTree->FindWidget<UTextBlock>(TEXT("TXT_Status"));
	if (!Status)
	{
		return;
	}

	// Show which candidate relic is selected, so equipping has a visible target.
	// 展示当前选中的候选遗器，让"装备"操作有一个可见的目标。
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
		// 强化阶段：显示当前强化目标（已装备遗器），并提示用户选择目标等级。
		const FHSRRelicSlotRow* Current = InSnapshot.Slots.FindByPredicate(
			[&InSnapshot](const FHSRRelicSlotRow& Row) { return Row.Slot == InSnapshot.SelectedSlot; });
		if (Current && Current->bHasEquipped)
		{
			// 只取实例 GUID 前 8 位作为简短展示，避免界面被长串 ID 撑爆。
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

// ShowOperationResult：把一次操作（装备/强化）的结果反馈到状态文本。
// 成功/材料不足/未选中分别给友好提示；其它失败走默认分支显示枚举的显示名。
void UHSRRelicEquipmentWidget::ShowOperationResult(EHSRRelicEquipmentResult Result)
{
	if (!WidgetTree)
	{
		return;
	}
	UTextBlock* Status = WidgetTree->FindWidget<UTextBlock>(TEXT("TXT_Status"));
	if (!Status)
	{
		return;
	}
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

// MakeListButton：动态构造一个列表行按钮（标签 + 背景色）。
// 按钮在运行时动态创建，因此需要显式设置可见性与背景色；可选地在其内部放一个水平盒子，
// 再向盒子里塞入带样式的文本标签。
UButton* UHSRRelicEquipmentWidget::MakeListButton(const FText& Label, const FLinearColor& Color)
{
	if (!WidgetTree)
	{
		return nullptr;
	}
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	if (!Button)
	{
		return nullptr;
	}
	Button->SetVisibility(ESlateVisibility::Visible);
	Button->SetBackgroundColor(Color);
	UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (RowBox)
	{
		Button->SetContent(RowBox);
	}
	UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelText->SetText(Label);
	// 浅色文字在深色 HSR 风格下保证可读性。
	LabelText->SetColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.92f, 0.96f, 1.0f)));
	// 默认字体偏小，放大到 14 让列表行文字清晰可点。
	FSlateFontInfo Font = LabelText->GetFont();
	Font.Size = 14;
	LabelText->SetFont(Font);
	if (RowBox)
	{
		RowBox->AddChild(LabelText);
	}
	return Button;
}

// PopulateCandidates：重建"候选遗器"列表。
// 先清空宿主面板与绑定数组，再为每个候选行动态创建按钮；按钮通过 Bridge 对象转发点击，
// 从而避免 UMG 的 OnClicked 事件签名无法直接携带 FGuid 参数的问题。
void UHSRRelicEquipmentWidget::PopulateCandidates()
{
	if (!WidgetTree)
	{
		return;
	}
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
		if (!Button)
		{
			continue;
		}
		// A dynamically-created Button has no authored slot constraints. Give each row
		// an explicit footprint so ScrollBox/VBox layout cannot collapse it to zero.
		// 动态创建的按钮没有作者布局的槽位约束；给每行一个显式的最小尺寸，
		// 防止 ScrollBox/VBox 布局把它折叠成 0 高度。
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

// PopulateEnhancementOptions：重建"强化选项"列表。
// 每个选项是一行按钮：显示目标等级与材料消耗；负担不起的选项按钮置灰且不可点击。
void UHSRRelicEquipmentWidget::PopulateEnhancementOptions()
{
	if (!WidgetTree)
	{
		return;
	}
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
		if (!Button)
		{
			continue;
		}
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

// Initialize：记录桥接回调所需的宿主控件与参数。
// TargetLevel >= 0 表示这是强化选项按钮（点击走 CommitEnhancement），
// 否则视为候选遗器按钮（点击走 SelectCandidate，使用 InstanceId）。
void UHSRRelicListClickBridge::Initialize(UHSRRelicEquipmentWidget* InOwner, FGuid InInstanceId, int32 InTargetLevel)
{
	Owner = InOwner;
	InstanceId = InInstanceId;
	TargetLevel = InTargetLevel;
}

// HandleClicked：列表行按钮被点击时的回调入口。
void UHSRRelicListClickBridge::HandleClicked()
{
	if (!Owner.IsValid())
	{
		return;
	}
	if (TargetLevel >= 0)
	{
		Owner->CommitEnhancement(TargetLevel);
	}
	else
	{
		Owner->SelectCandidate(InstanceId);
	}
}
