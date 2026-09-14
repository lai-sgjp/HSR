#include "HSRRelicEquipmentWidget.h"

#include "HSRRelicEquipmentViewModel.h"
#include "../../Data/Definitions/HSREquipmentEnhancementCatalog.h"
#include "../../Data/Definitions/HSRInventoryCatalog.h"
#include "../../Data/Definitions/HSRItemEquipmentMappingCatalog.h"
#include "../../Equipment/HSREquipmentSubsystem.h"
#include "../../Equipment/HSREquipmentTypes.h"
#include "../../Inventory/HSRInventorySubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
FText RelicSlotLabel(EHSRRelicSlot RelicSlot)
{
	switch (RelicSlot)
	{
	case EHSRRelicSlot::Head: return NSLOCTEXT("HSRRelic", "Head", "头部");
	case EHSRRelicSlot::Hands: return NSLOCTEXT("HSRRelic", "Hands", "手部");
	case EHSRRelicSlot::Body: return NSLOCTEXT("HSRRelic", "Body", "躯干");
	case EHSRRelicSlot::Feet: return NSLOCTEXT("HSRRelic", "Feet", "脚部");
	case EHSRRelicSlot::PlanarSphere: return NSLOCTEXT("HSRRelic", "Sphere", "位面球");
	case EHSRRelicSlot::LinkRope: return NSLOCTEXT("HSRRelic", "Rope", "连结绳");
	default: return NSLOCTEXT("HSRRelic", "UnknownSlot", "遗器槽位");
	}
}
FText RelicStatLabel(EHSREquipmentStat Stat)
{
	switch (Stat)
	{
	case EHSREquipmentStat::MaxHealth: return NSLOCTEXT("HSRRelic", "Health", "最大生命");
	case EHSREquipmentStat::Attack: return NSLOCTEXT("HSRRelic", "Attack", "攻击力");
	case EHSREquipmentStat::Defense: return NSLOCTEXT("HSRRelic", "Defense", "防御力");
	case EHSREquipmentStat::Speed: return NSLOCTEXT("HSRRelic", "Speed", "速度");
	default: return NSLOCTEXT("HSRRelic", "Attribute", "属性");
	}
}
FText RelicModifierLabel(const TArray<FHSREquipmentModifier>& Modifiers)
{
	TArray<FString> Lines;
	for (const FHSREquipmentModifier& Modifier : Modifiers)
		Lines.Add(FText::Format(NSLOCTEXT("HSRRelic", "Modifier", "{0} {1}{2}"), RelicStatLabel(Modifier.Stat),
			Modifier.Value >= 0.f ? FText::FromString(TEXT("+")) : FText::GetEmpty(), FText::AsNumber(Modifier.Value)).ToString());
	return Lines.IsEmpty() ? NSLOCTEXT("HSRRelic", "NoModifiers", "无属性加成") : FText::FromString(FString::Join(Lines, TEXT("  ·  ")));
}
}

// NativeConstruct：控件入树时自建遗器装备 ViewModel 并订阅其快照事件。
// 若已指定角色 GUID 则立即初始化运行时上下文，否则进入"未初始化"不可用态。
void UHSRRelicEquipmentWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UButton* Confirm = WidgetTree ? WidgetTree->FindWidget<UButton>(TEXT("BTN_ConfirmEnhance")) : nullptr)
	{
		Confirm->OnClicked.Clear();
		Confirm->OnClicked.AddDynamic(this, &ThisClass::HandleConfirmEnhancement);
	}
	if (UButton* Unequip = WidgetTree ? WidgetTree->FindWidget<UButton>(TEXT("BTN_Unequip")) : nullptr)
	{
		Unequip->OnClicked.Clear();
		Unequip->OnClicked.AddDynamic(this, &ThisClass::HandleUnequip);
	}
	if (UButton* Equip = WidgetTree ? WidgetTree->FindWidget<UButton>(TEXT("BTN_Equip")) : nullptr)
	{
		Equip->OnClicked.Clear();
		Equip->OnClicked.AddDynamic(this, &ThisClass::HandleEquip);
	}
	if (UButton* Enhance = WidgetTree ? WidgetTree->FindWidget<UButton>(TEXT("BTN_Enhance")) : nullptr)
	{
		Enhance->OnClicked.Clear();
		Enhance->OnClicked.AddDynamic(this, &ThisClass::HandleOpenEnhancement);
	}
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
	RefreshPresentation();
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

EHSRRelicEquipmentResult UHSRRelicEquipmentWidget::UnequipSelectedSlot()
{
	const auto Result = ViewModel ? ViewModel->UnequipSelectedSlot() : EHSRRelicEquipmentResult::NotInitialized;
	ShowOperationResult(Result);
	return Result;
}

void UHSRRelicEquipmentWidget::HandleUnequip() { UnequipSelectedSlot(); }
void UHSRRelicEquipmentWidget::HandleEquip() { CommitSelectedMovement(); }
void UHSRRelicEquipmentWidget::HandleOpenEnhancement() { OpenEnhancement(); }

void UHSRRelicEquipmentWidget::SelectEnhancementLevel(int32 TargetLevel)
{
	SelectedEnhancementLevel = TargetLevel;
	HandleSnapshot(CurrentSnapshot);
}

void UHSRRelicEquipmentWidget::HandleConfirmEnhancement()
{
	if (SelectedEnhancementLevel != INDEX_NONE) CommitEnhancement(SelectedEnhancementLevel);
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
	if (CurrentSnapshot.EnhancementInstanceId != InSnapshot.EnhancementInstanceId
		|| !InSnapshot.EnhancementOptions.ContainsByPredicate([this](const FHSRRelicEnhancementOption& Option)
			{ return Option.TargetLevel == SelectedEnhancementLevel; })) SelectedEnhancementLevel = INDEX_NONE;
	OperationMessage = FText::GetEmpty();
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
		EnhanceButton->SetIsEnabled(InSnapshot.CurrentInstanceId.IsValid() || InSnapshot.SelectedCandidateId.IsValid());
	}
	// "确认强化"按钮只在强化阶段存在"可用且负担得起"的选项时可用。
	if (UButton* ConfirmButton = WidgetTree ? WidgetTree->FindWidget<UButton>(TEXT("BTN_ConfirmEnhance")) : nullptr)
	{
		const bool bHasAffordableOption = InSnapshot.Stage == EHSRRelicEquipmentStage::Enhancement
			&& InSnapshot.EnhancementOptions.ContainsByPredicate(
				[this](const FHSRRelicEnhancementOption& Option) { return Option.TargetLevel == SelectedEnhancementLevel && Option.bAvailable && Option.bAffordable; });
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
	PopulateSlots();
	PopulateCandidates();
	PopulateEnhancementOptions();
	RefreshPresentation();
	// The Blueprint event also drives visibility + repopulates the lists through graph loops.
	// Run the same C++ pass on the next tick so it remains the final word after any deferred BP work.
	// 蓝图事件也会通过图里的循环驱动可见性与列表重填；下一帧再执行一次同样的 C++ 处理，
	// 保证在蓝图的延迟更新之后 C++ 仍是最终裁决。
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (!IsValid(this) || !bHasSnapshot)
			{
				return;
			}
			ApplyStageVisibility();
			PopulateSlots();
			PopulateCandidates();
			PopulateEnhancementOptions();
			RefreshPresentation();
		}));
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
	if (UWidget* Column = WidgetTree->FindWidget(TEXT("RightColumn")))
		Column->SetVisibility(bShowSlots ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	if (UWidget* ListFrame = WidgetTree->FindWidget(TEXT("ListRowSizeBox")))
		ListFrame->SetVisibility(bShowSlots ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
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
			NSLOCTEXT("HSRRelic", "SelectedRelic", "已选择：{0} +{1}，确认前可对比属性"),
			ItemLabel(Selected->ItemId), FText::AsNumber(Selected->Instance.EnhancementLevel)));
	}
	else if (InSnapshot.Stage == EHSRRelicEquipmentStage::Enhancement)
	{
		Status->SetText(FText::Format(
			NSLOCTEXT("HSRRelic", "EnhancementTargetExplicit", "强化目标：当前选中的遗器 +{0}　{1}"),
			FText::AsNumber(InSnapshot.EnhancementInstance.EnhancementLevel),
			InSnapshot.EnhancementOptions.IsEmpty()
				? NSLOCTEXT("HSRRelic", "NoFurtherLevels", "已无可用强化等级")
				: NSLOCTEXT("HSRRelic", "ChooseThenConfirm", "选择目标等级并确认消耗")));
	}
	else if (InSnapshot.Stage == EHSRRelicEquipmentStage::CandidateSelection)
	{
		Status->SetText(InSnapshot.Candidates.IsEmpty()
			? NSLOCTEXT("HSRRelic", "NoCandidates", "背包中没有适用于此槽位的遗器")
			: NSLOCTEXT("HSRRelic", "PickCandidate", "选择遗器后对比属性，再确认装备"));
	}
	else
	{
		Status->SetText(NSLOCTEXT("HSRRelic", "PickSlot", "选择遗器槽位"));
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
	case EHSRRelicEquipmentResult::Success: OperationMessage = NSLOCTEXT("HSRRelic", "OperationOk", "操作已完成"); break;
	case EHSRRelicEquipmentResult::InsufficientMaterial: OperationMessage = NSLOCTEXT("HSRRelic", "NoMaterial", "强化材料不足，请选择其他等级或收集材料"); break;
	case EHSRRelicEquipmentResult::StaleSnapshot: OperationMessage = NSLOCTEXT("HSRRelic", "Stale", "遗器或材料已发生变化，请重新选择后确认"); break;
	case EHSRRelicEquipmentResult::CatalogUnavailable: OperationMessage = NSLOCTEXT("HSRRelic", "NoCatalog", "遗器配置暂不可用，请返回后重试"); break;
	case EHSRRelicEquipmentResult::NoEnhancementOption: OperationMessage = NSLOCTEXT("HSRRelic", "NoOption", "此遗器没有可用的强化等级"); break;
	case EHSRRelicEquipmentResult::CandidateUnavailable:
	case EHSRRelicEquipmentResult::ComparisonUnavailable: OperationMessage = NSLOCTEXT("HSRRelic", "ChooseAgain", "请重新选择要操作的遗器"); break;
	case EHSRRelicEquipmentResult::NotInitialized:
	case EHSRRelicEquipmentResult::InvalidCharacterId: OperationMessage = NSLOCTEXT("HSRRelic", "NotReady", "角色装备数据尚未就绪，请返回后重试"); break;
	default: OperationMessage = NSLOCTEXT("HSRRelic", "OperationFailed", "操作未完成，请检查遗器和材料后重试"); break;
	}
	Status->SetText(OperationMessage);
	RefreshPresentation();
}

FText UHSRRelicEquipmentWidget::ItemLabel(FName ItemId) const
{
	FHSRInventoryCatalogEntry Entry;
	if (PresentationCatalog && PresentationCatalog->FindEntry(ItemId, Entry) && !Entry.DisplayName.IsEmpty())
		return Entry.DisplayName;
	return NSLOCTEXT("HSRRelic", "UnnamedItem", "物品");
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
	Button->SetColorAndOpacity(FLinearColor::White);
	UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (RowBox)
	{
		Button->SetContent(RowBox);
		if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(RowBox->Slot))
		{
			ContentSlot->SetHorizontalAlignment(HAlign_Fill);
			ContentSlot->SetPadding(FMargin(12.f, 8.f));
		}
	}
	UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelText->SetText(Label);
	// 浅色文字在深色 HSR 风格下保证可读性。
	LabelText->SetColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.92f, 0.96f, 1.0f)));
	// 默认字体偏小，放大到 14 让列表行文字清晰可点。
	FSlateFontInfo Font = LabelText->GetFont();
	Font.Size = 18;
	LabelText->SetAutoWrapText(true);
	LabelText->SetFont(Font);
	if (RowBox)
	{
		if (auto* TextSlot = RowBox->AddChildToHorizontalBox(LabelText))
		{
			TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
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
		const FText Label = FText::Format(NSLOCTEXT("HSRRelic", "CandidateDetails", "{0} +{1}\n{2}"),
			ItemLabel(Row.ItemId), FText::AsNumber(Row.Instance.EnhancementLevel), RelicModifierLabel(Row.Instance.Modifiers));
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
		const FText Label = FText::Format(NSLOCTEXT("HSRRelic", "EnhanceDetails", "强化至 +{0}　消耗 {2} ×{1}\n{3}\n{4}"),
			FText::AsNumber(Option.TargetLevel), FText::AsNumber(Option.MaterialCost),
			ItemLabel(Option.MaterialItemId), RelicModifierLabel(Option.TargetModifiers),
			!Option.bAvailable ? NSLOCTEXT("HSRRelic", "OptionUnavailable", "此等级不可用") : (!Option.bAffordable ? NSLOCTEXT("HSRRelic", "OptionUnaffordable", "材料不足") : NSLOCTEXT("HSRRelic", "OptionAvailable", "材料充足，选择后确认")));
		UButton* Button = MakeListButton(Label,
			Option.TargetLevel == SelectedEnhancementLevel ? FLinearColor(0.78f, 0.61f, 0.24f, 0.4f) : FLinearColor(0.08f, 0.12f, 0.18f, 0.8f));
		if (!Button)
		{
			continue;
		}
		Button->SetIsEnabled(CurrentSnapshot.Stage == EHSRRelicEquipmentStage::Enhancement && Option.bAvailable && Option.bAffordable);
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
	if (SlotIndex != INDEX_NONE)
	{
		Owner->SelectSlot(static_cast<EHSRRelicSlot>(SlotIndex));
	}
	else if (TargetLevel >= 0)
	{
		Owner->SelectEnhancementLevel(TargetLevel);
	}
	else
	{
		Owner->SelectCandidate(InstanceId);
	}
}

void UHSRRelicListClickBridge::InitializeSlot(UHSRRelicEquipmentWidget* InOwner, EHSRRelicSlot InSlot)
{
	Owner = InOwner;
	SlotIndex = static_cast<int32>(InSlot);
}

FText UHSRRelicEquipmentWidget::InstanceLabel(const FHSREquipmentInstance& Instance) const
{
	FHSRItemEquipmentMappingEntry Mapping;
	if (MappingCatalog && MappingCatalog->ResolveEquipmentDefinition(Instance.DefinitionId, Mapping))
		return ItemLabel(Mapping.ItemId);
	return NSLOCTEXT("HSRRelic", "Relic", "遗器");
}

void UHSRRelicEquipmentWidget::PopulateSlots()
{
	UPanelWidget* Host = WidgetTree ? WidgetTree->FindWidget<UPanelWidget>(TEXT("SlotListHost")) : nullptr;
	if (!Host) return;
	Host->ClearChildren();
	SlotBindings.Reset();
	for (const FHSRRelicSlotRow& Row : CurrentSnapshot.Slots)
	{
		const FText Detail = Row.bHasEquipped
			? FText::Format(NSLOCTEXT("HSRRelic", "EquippedRelic", "{0} +{1}"), InstanceLabel(Row.EquippedInstance), FText::AsNumber(Row.EquippedInstance.EnhancementLevel))
			: NSLOCTEXT("HSRRelic", "EmptyRelic", "未装备");
		UButton* Button = MakeListButton(FText::Format(NSLOCTEXT("HSRRelic", "SlotDetails", "{0}\n{1}"), RelicSlotLabel(Row.Slot), Detail),
			Row.bIsSelected ? FLinearColor(0.78f, 0.61f, 0.24f, 0.4f) : FLinearColor(0.08f, 0.12f, 0.18f, 0.8f));
		if (!Button) continue;
		USizeBox* RowSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		RowSize->SetMinDesiredWidth(260.f);
		RowSize->SetMinDesiredHeight(68.f);
		RowSize->SetContent(Button);
		if (UVerticalBoxSlot* ListSlot = Cast<UVerticalBoxSlot>(Host->AddChild(RowSize)))
		{
			ListSlot->SetHorizontalAlignment(HAlign_Fill);
			ListSlot->SetPadding(FMargin(4.f, 3.f));
		}
		UHSRRelicListClickBridge* Bridge = NewObject<UHSRRelicListClickBridge>(this);
		Bridge->InitializeSlot(this, Row.Slot);
		Button->OnClicked.AddDynamic(Bridge, &UHSRRelicListClickBridge::HandleClicked);
		SlotBindings.Add(Bridge);
	}
}

void UHSRRelicEquipmentWidget::RefreshPresentation()
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
	SetText(TEXT("TXT_Title"), NSLOCTEXT("HSRRelic", "Title", "遗器装备"));
	SetText(TEXT("TXT_SlotsTitle"), NSLOCTEXT("HSRRelic", "SlotsTitle", "装备槽位"));
	SetText(TEXT("TXT_CandidatesTitle"), NSLOCTEXT("HSRRelic", "CandidatesTitle", "背包中的适用遗器"));
	SetText(TEXT("TXT_ComparisonTitle"), NSLOCTEXT("HSRRelic", "ComparisonTitle", "属性对比 · 当前 → 候选"));
	SetText(TEXT("TXT_EnhanceTitle"), NSLOCTEXT("HSRRelic", "EnhanceTitle", "遗器强化"));
	SetText(TEXT("TXT_Equip"), NSLOCTEXT("HSRRelic", "Equip", "确认装备"));
	SetText(TEXT("TXT_Enhance"), NSLOCTEXT("HSRRelic", "Enhance", "强化所选遗器"));
	SetText(TEXT("TXT_ConfirmEnhance"), NSLOCTEXT("HSRRelic", "ConfirmEnhance", "确认强化"));
	SetText(TEXT("PR_Label_BTN_Unequip"), NSLOCTEXT("HSRRelic", "Unequip", "卸下当前遗器"));
	SetText(TEXT("TXT_Unequip"), NSLOCTEXT("HSRRelic", "Unequip", "卸下当前遗器"));
	SetText(TEXT("TXT_Back"), NSLOCTEXT("HSRRelic", "Back", "返回"));
	const bool bEnhancing = CurrentSnapshot.Stage == EHSRRelicEquipmentStage::Enhancement;
	const bool bValid = bHasSnapshot && CurrentSnapshot.bIsValid;
	const bool bCandidateExists = CurrentSnapshot.SelectedCandidateId.IsValid() && CurrentSnapshot.Candidates.ContainsByPredicate(
		[this](const FHSRRelicCandidateRow& Row) { return Row.InstanceId == CurrentSnapshot.SelectedCandidateId; });
	const auto SetEnabled = [this](const TCHAR* Name, bool bEnabled)
	{
		if (UButton* Button = WidgetTree->FindWidget<UButton>(Name)) Button->SetIsEnabled(bEnabled);
	};
	const auto ShowAction = [this](const TCHAR* Name, bool bShow)
	{
		if (UButton* Button = WidgetTree->FindWidget<UButton>(Name))
			Button->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	};
	ShowAction(TEXT("BTN_Equip"), CurrentSnapshot.Stage == EHSRRelicEquipmentStage::Comparison);
	ShowAction(TEXT("BTN_ConfirmEnhance"), bEnhancing);
	ShowAction(TEXT("BTN_Enhance"), !bEnhancing && (bCandidateExists || CurrentSnapshot.CurrentInstanceId.IsValid()));
	ShowAction(TEXT("BTN_Unequip"), !bEnhancing && CurrentSnapshot.CurrentInstanceId.IsValid());
	SetEnabled(TEXT("BTN_Equip"), bValid && CurrentSnapshot.Stage == EHSRRelicEquipmentStage::Comparison
		&& CurrentSnapshot.Comparison.bIsValid && bCandidateExists
		&& CurrentSnapshot.Comparison.CandidateInstanceId == CurrentSnapshot.SelectedCandidateId);
	SetEnabled(TEXT("BTN_Unequip"), bValid && !bEnhancing && CurrentSnapshot.CurrentInstanceId.IsValid());
	SetEnabled(TEXT("BTN_Enhance"), bValid && !bEnhancing && (bCandidateExists || CurrentSnapshot.CurrentInstanceId.IsValid()));
	SetEnabled(TEXT("BTN_ConfirmEnhance"), bValid && bEnhancing && CurrentSnapshot.EnhancementInstanceId.IsValid()
		&& CurrentSnapshot.EnhancementOptions.ContainsByPredicate([this](const FHSRRelicEnhancementOption& Option)
		{ return Option.TargetLevel == SelectedEnhancementLevel && Option.bAvailable && Option.bAffordable; }));
	// Replace legacy Blueprint stat rows, which stringify the internal stat enum.
	if (UPanelWidget* Host = WidgetTree->FindWidget<UPanelWidget>(TEXT("ComparisonBodyHost"))) Host->ClearChildren();
	TArray<FString> Lines;
	if (CurrentSnapshot.Comparison.bIsValid)
	{
		const FText CurrentItem = CurrentSnapshot.Comparison.CurrentInstanceId.IsValid()
			? InstanceLabel(CurrentSnapshot.Comparison.CurrentInstance) : NSLOCTEXT("HSRRelic", "EmptyRelic", "未装备");
		Lines.Add(FText::Format(NSLOCTEXT("HSRRelic", "ComparisonItems", "{0} → {1}"), CurrentItem, InstanceLabel(CurrentSnapshot.Comparison.CandidateInstance)).ToString());
		for (const FHSRRelicStatDeltaRow& Row : CurrentSnapshot.Comparison.StatDeltas)
			Lines.Add(FText::Format(NSLOCTEXT("HSRRelic", "ComparisonStat", "{0}  {1} → {2}  （{3}{4}）"),
				RelicStatLabel(Row.Stat), FText::AsNumber(Row.CurrentValue), FText::AsNumber(Row.CandidateValue),
				Row.Delta > 0.f ? FText::FromString(TEXT("+")) : FText::GetEmpty(), FText::AsNumber(Row.Delta)).ToString());
	}
	SetText(TEXT("TXT_ComparisonBody"), FText::FromString(FString::Join(Lines, TEXT("\n"))));
	SetText(TEXT("TXT_EnhanceBody"), CurrentSnapshot.EnhancementInstanceId.IsValid()
		? FText::Format(NSLOCTEXT("HSRRelic", "EnhancementItem", "{0} +{1}\n当前属性：{2}\n选择下方目标等级查看消耗与强化后属性"),
			InstanceLabel(CurrentSnapshot.EnhancementInstance), FText::AsNumber(CurrentSnapshot.EnhancementInstance.EnhancementLevel), RelicModifierLabel(CurrentSnapshot.EnhancementInstance.Modifiers))
		: FText::GetEmpty());
	if (!OperationMessage.IsEmpty()) SetText(TEXT("TXT_Status"), OperationMessage);
	else if (!bValid) SetText(TEXT("TXT_Status"), NSLOCTEXT("HSRRelic", "UnavailableHint", "遗器数据暂不可用，请重新选择槽位或返回后重试"));
	else UpdateStatusText(CurrentSnapshot);
}
