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

// UHSRInventoryRowClickBridge 的职责：
// UMG 的 Button::OnClicked 只能绑定到 UObject 的动态委托，且不会携带参数；
// 列表里每一行按钮都要把“点击事件”翻译成“选中第几行”。
// 因此每个行按钮都配一个本桥接对象，保存宿主 Widget 与行号，点击时转调
// 宿主 Widget 的 SelectEntryByIndex 完成选中。
void UHSRInventoryRowClickBridge::Initialize(UHSRInventoryModuleWidget* const InOwner,
	const int32 InRowIndex)
{
	// 记录宿主 Widget（弱引用持有，避免阻止其销毁）与当前行索引。
	Owner = InOwner;
	RowIndex = InRowIndex;
}

// 行按钮点击回调：仅当宿主 Widget 仍存活时才转发选中请求，否则静默丢弃。
void UHSRInventoryRowClickBridge::HandleClicked()
{
	if (UHSRInventoryModuleWidget* Widget = Owner.Get())
	{
		Widget->SelectEntryByIndex(RowIndex);
	}
}

// NativeConstruct：UMG 控件被创建并进入可视树后调用。
// 这里完成三件事：
//   1. 确保 ViewModel 存在（延迟创建，便于手动初始化场景复用同一个 ViewModel）；
//   2. 绑定 ViewModel 的 OnChanged 并立即拉取一次快照；
//   3. 把 ViewModel 与运行时子系统（Inventory/Equipment 等）接好。
// 最后单独绑定“返回/关闭”两个常驻按钮——它们在还没有有效快照之前也必须可用。
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
	// 返回/关闭按钮始终可达：即便 Inventory 快照尚未就绪，也必须允许玩家退出面板。
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

// NativeDestruct：控件从可视树移除/销毁时调用。
// 需要释放 ViewModel（Shutdown 清空其内部状态并解绑子系统委托）、解绑快照订阅，
// 并把本地缓存快照复位，避免析构后还有回调引用已销毁的控件。
void UHSRInventoryModuleWidget::NativeDestruct()
{
	// 通知 ViewModel 停机：清空快照、复位 LastResult 等权威状态。
	if (ViewModel)
	{
		ViewModel->Shutdown();
	}
	// 解绑 OnChanged 订阅并置空，防止残留回调。
	SetViewModel(nullptr);
	// 本地缓存快照作废，后续 GetSnapshot 类访问都会返回失败。
	bHasSnapshot = false;
	CurrentSnapshot = FHSRInventoryModuleSnapshot();
	Super::NativeDestruct();
}

// InitializeForInventory：由外部（通常是 UIManager）在打开背包面板前注入物品目录。
// 目录用于把物品 ID 展开成显示名/数量/唯一性等展示信息；
// 若控件已经构造，则立即重新初始化运行时上下文以让新目录生效。
void UHSRInventoryModuleWidget::InitializeForInventory(UHSRInventoryCatalog* InCatalog)
{
	// 仅当外部确实传入非空目录时才覆盖，避免误清空已有配置。
	if (InCatalog)
	{
		Catalog = InCatalog;
	}
	// 已经进入可视树时同步刷新 ViewModel 的上下文。
	if (IsConstructed())
	{
		InitializeRuntimeContext();
	}
}

// InitializeCommandContext：注入“命令上下文”——当前背包所属角色、物品→装备映射目录、
// 强化目录。ViewModel 后续做 Equip/Enhance 等操作时靠这些数据定位到具体装备与强化选项。
void UHSRInventoryModuleWidget::InitializeCommandContext(const FGuid& InCharacterId,
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

	// The UIManager validates the snapshot immediately after InitializeCommandContext,
	// which can run before this widget is constructed (CreateWidget does not call
	// NativeConstruct). Ensure the ViewModel exists, is initialized, and has bound the
	// snapshot so GetCurrentSnapshot reflects the committed state right away. NativeConstruct
	// reuses the same ViewModel and re-binding is idempotent.
	// 关键时序问题：UIManager 在 InitializeCommandContext 返回后立刻校验快照，
	// 而这一步可能发生在控件尚未构造时（CreateWidget 不会触发 NativeConstruct）。
	// 因此这里必须主动确保 ViewModel 存在、完成初始化并绑定快照，保证调用方立即读到
	// 已提交的状态。NativeConstruct 之后会复用同一个 ViewModel，重复绑定是幂等的。
	if (!ViewModel)
	{
		ViewModel = NewObject<UHSRInventoryViewModel>(this);
	}
	InitializeRuntimeContext();
	BindAndRefresh();
}

// RequestCloseToRoot：请求 UI 管理器把整个前端界面一路关闭到根界面。
// 返回是否成功；失败时由调用方决定是否给出提示。
bool UHSRInventoryModuleWidget::RequestCloseToRoot()
{
	return GetOwningUIManager()
		&& GetOwningUIManager()->CloseFrontendToRoot() == EHSRUIScreenResult::Success;
}

// SetViewModel：替换当前 ViewModel。
// 换绑时先移除旧 ViewModel 上的快照订阅（保证旧数据源不再驱动本控件），
// 再保存新指针；若控件已构造则立即重新绑定并拉取一次快照。
void UHSRInventoryModuleWidget::SetViewModel(UHSRInventoryViewModel* InViewModel)
{
	if (ViewModel && SnapshotHandle.IsValid())
	{
		// 移除旧 ViewModel 的订阅，防止旧数据流继续刷新本控件。
		ViewModel->OnChanged().Remove(SnapshotHandle);
		SnapshotHandle.Reset();
#if WITH_DEV_AUTOMATION_TESTS
		++UnbindCount;
#endif
	}
	ViewModel = InViewModel;
	// 已进入可视树才需要立刻重绑；否则等待 NativeConstruct 统一处理。
	if (IsConstructed())
	{
		BindAndRefresh();
	}
}

// SelectCategory：把用户选择物品分类的动作转发给 ViewModel。
// 返回执行结果；ViewModel 缺失时返回 NotInitialized 提示调用方尚未就绪。
EHSRInventoryViewModelResult UHSRInventoryModuleWidget::SelectCategory(
	const EHSRInventoryCategory InCategory)
{
	return ViewModel ? ViewModel->SelectCategory(InCategory)
		: EHSRInventoryViewModelResult::NotInitialized;
}

// SetFilterText：把搜索框文本转发给 ViewModel 进行过滤。
// 过滤逻辑完全由 ViewModel 完成，控件只负责转达与展示结果。
EHSRInventoryViewModelResult UHSRInventoryModuleWidget::SetFilterText(const FString& InFilterText)
{
	return ViewModel ? ViewModel->SetFilterText(InFilterText)
		: EHSRInventoryViewModelResult::NotInitialized;
}

// SetSortMode：把排序方式（按数量/名称等）转发给 ViewModel 重排列表。
EHSRInventoryViewModelResult UHSRInventoryModuleWidget::SetSortMode(
	const EHSRInventorySortMode InSortMode)
{
	return ViewModel ? ViewModel->SetSortMode(InSortMode)
		: EHSRInventoryViewModelResult::NotInitialized;
}

// SelectEntry：按物品键请求选中某条目（高亮并刷新详情区）。
EHSRInventoryViewModelResult UHSRInventoryModuleWidget::SelectEntry(
	const FHSRInventoryEntryKey& InKey)
{
	return ViewModel ? ViewModel->SelectEntry(InKey)
		: EHSRInventoryViewModelResult::NotInitialized;
}

// SubmitAction：把“使用/装备/强化/分解”等操作命令转发给 ViewModel 执行。
// TargetLevel 供强化操作指定目标等级，其余操作通常传 -1 表示不适用。
EHSRInventoryViewModelResult UHSRInventoryModuleWidget::SubmitAction(
	const EHSRInventoryAction Action, const int32 TargetLevel)
{
	return ViewModel ? ViewModel->SubmitAction(Action, TargetLevel)
		: EHSRInventoryViewModelResult::NotInitialized;
}

// GetCurrentSnapshot：向调用方导出当前缓存的模块快照（纯值 DTO）。
// 快照尚未就绪（bHasSnapshot 为假）时返回 false，避免用空数据做展示决策。
bool UHSRInventoryModuleWidget::GetCurrentSnapshot(
	FHSRInventoryModuleSnapshot& OutSnapshot) const
{
	// 无有效快照直接返回失败，保持调用方行为可预期。
	if (!bHasSnapshot)
	{
		return false;
	}
	OutSnapshot = CurrentSnapshot;
	return true;
}

// GetEntry：按行索引取出该条目（名称/数量/唯一性等）给列表行使用。
// 越界或快照未就绪时返回 false。
bool UHSRInventoryModuleWidget::GetEntry(const int32 Index,
	FHSRInventoryEntryRow& OutEntry) const
{
	if (!bHasSnapshot || !CurrentSnapshot.Entries.IsValidIndex(Index))
	{
		return false;
	}
	OutEntry = CurrentSnapshot.Entries[Index];
	return true;
}

// GetActionState：按行索引取出该条目的操作可用状态（使用/装备/强化/分解是否可点）。
bool UHSRInventoryModuleWidget::GetActionState(const int32 Index,
	FHSRInventoryActionState& OutAction) const
{
	if (!bHasSnapshot || !CurrentSnapshot.Actions.IsValidIndex(Index))
	{
		return false;
	}
	OutAction = CurrentSnapshot.Actions[Index];
	return true;
}

// GetEntryCount：返回当前列表条目数；快照未就绪时按 0 处理（空列表）。
int32 UHSRInventoryModuleWidget::GetEntryCount() const
{
	return bHasSnapshot ? CurrentSnapshot.Entries.Num() : 0;
}

// GetEntryDisplay：取出某条目在列表行上需要展示的三个字段——
// 显示名（转成 FString 便于 Slate 直接使用）、数量、是否唯一物品。
bool UHSRInventoryModuleWidget::GetEntryDisplay(const int32 Index, FString& OutDisplayName,
	int32& OutQuantity, bool& bOutIsUnique) const
{
	if (!bHasSnapshot || !CurrentSnapshot.Entries.IsValidIndex(Index))
	{
		return false;
	}
	const FHSRInventoryEntryRow& Row = CurrentSnapshot.Entries[Index];
	OutDisplayName = Row.DisplayName.ToString();
	OutQuantity = Row.Quantity;
	bOutIsUnique = Row.bIsUnique;
	return true;
}

// GetSelectedDetail：导出详情区数据（选中物品名/数量/是否有选中项）。
// 这里先以“无选中”为默认值再依据快照覆盖，保证返回 false 时输出参数也是确定值。
bool UHSRInventoryModuleWidget::GetSelectedDetail(FString& OutName, int32& OutQuantity,
	bool& bOutHasSelection) const
{
	OutName = TEXT("");
	OutQuantity = 0;
	bOutHasSelection = false;
	// 快照未就绪时返回 false，并保持输出参数为已初始化的默认值。
	if (!bHasSnapshot)
	{
		return false;
	}
	bOutHasSelection = CurrentSnapshot.Detail.bHasSelection;
	if (bOutHasSelection)
	{
		OutName = CurrentSnapshot.Detail.Entry.DisplayName.ToString();
		OutQuantity = CurrentSnapshot.Detail.Entry.Quantity;
	}
	return true;
}

// SelectEntryByIndex：行点击桥接对象转调到的入口——把“第几行”翻译成
// “快照里该行对应的物品键”，再交给 ViewModel 真正执行选中。
// 使用快照行而不是直接取 ViewModel 内部数据，是为了让控件只消费纯值快照。
EHSRInventoryViewModelResult UHSRInventoryModuleWidget::SelectEntryByIndex(const int32 Index)
{
	// 前置校验：ViewModel 就绪、快照有效、且行索引未越界。
	if (!ViewModel || !bHasSnapshot || !CurrentSnapshot.Entries.IsValidIndex(Index))
	{
		return EHSRInventoryViewModelResult::EntryUnavailable;
	}
	return ViewModel->SelectEntry(CurrentSnapshot.Entries[Index].Key);
}

// GetActionAvailable：遍历快照中的操作状态，查询某个操作当前是否可用（用于按钮置灰）。
bool UHSRInventoryModuleWidget::GetActionAvailable(const EHSRInventoryAction Action) const
{
	for (const FHSRInventoryActionState& ActionState : CurrentSnapshot.Actions)
	{
		if (ActionState.Action == Action)
		{
			return ActionState.bIsAvailable;
		}
	}
	// 快照里没有该操作记录时按不可用处理。
	return false;
}

// GetSelectedEnhancementTargetLevel：查询当前选中的强化目标等级。
// 强化按钮需要知道“升到多少级”才能构造命令；快照中第一个可用选项即为目标等级。
int32 UHSRInventoryModuleWidget::GetSelectedEnhancementTargetLevel() const
{
	// 无有效快照或没有选中项时返回 -1（无效等级）。
	if (!bHasSnapshot || !CurrentSnapshot.Detail.bHasSelection)
	{
		return -1;
	}
	for (const FHSRInventoryEnhancementOption& Option : CurrentSnapshot.EnhancementOptions)
	{
		if (Option.bAvailable)
		{
			return Option.TargetLevel;
		}
	}
	return -1;
}

// RefreshListAndDetail：强制用当前缓存快照重刷一遍列表与详情区。
// 用于外部数据（如角色切换后）已变化但快照尚未更新的场景，保持展示与最新状态一致。
void UHSRInventoryModuleWidget::RefreshListAndDetail()
{
	if (!bHasSnapshot)
	{
		return;
	}
	OnInventorySnapshotChanged(CurrentSnapshot);
}

// InitializeRuntimeContext：把 ViewModel 与运行时子系统接好。
// 从 GameInstance 取 Inventory/Equipment 子系统，连同此前注入的目录、角色 ID
// 一起交给 ViewModel，之后 ViewModel 才能从这些数据源构建快照。
void UHSRInventoryModuleWidget::InitializeRuntimeContext()
{
	if (!ViewModel)
	{
		return;
	}
	UGameInstance* GameInstance = GetGameInstance();
	UHSRInventorySubsystem* Inventory = GameInstance
		? GameInstance->GetSubsystem<UHSRInventorySubsystem>() : nullptr;
	UHSREquipmentSubsystem* Equipment = GameInstance
		? GameInstance->GetSubsystem<UHSREquipmentSubsystem>() : nullptr;
	ViewModel->Initialize(Inventory, Catalog);
	ViewModel->SetCommandContext(Equipment, MappingCatalog, EnhancementCatalog, CharacterId);
}

// BindAndRefresh：核心订阅逻辑。
// 先解绑旧订阅（防重复），再订阅 ViewModel 的 OnChanged 广播，
// 最后立刻主动拉取一次初始快照，让控件在打开瞬间就呈现当前数据。
void UHSRInventoryModuleWidget::BindAndRefresh()
{
	if (!ViewModel)
	{
		return;
	}
	if (SnapshotHandle.IsValid())
	{
		// 已有订阅则先解绑再重绑，保证不会重复收到广播。
		ViewModel->OnChanged().Remove(SnapshotHandle);
		SnapshotHandle.Reset();
#if WITH_DEV_AUTOMATION_TESTS
		++UnbindCount;
#endif
	}
	// 绑定快照更新回调：ViewModel 每次广播新快照都从这里进入本控件。
	SnapshotHandle = ViewModel->OnChanged().AddUObject(this, &ThisClass::HandleSnapshot);
#if WITH_DEV_AUTOMATION_TESTS
	++BindCount;
#endif
	// 立即拉取一次初始快照，使控件创建后无需等待下一次数据变化即可显示。
	FHSRInventoryModuleSnapshot InitialSnapshot;
	if (ViewModel->GetSnapshot(InitialSnapshot))
	{
		HandleSnapshot(InitialSnapshot);
	}
}

// HandleSnapshot：ViewModel 每次广播新快照时触发的统一入口。
// 职责是把快照缓存到本地，再据此刷新界面：
//   有效快照 -> 重建列表与详情；无效快照 -> 通知上层（如 UIManager）原因。
void UHSRInventoryModuleWidget::HandleSnapshot(
	const FHSRInventoryModuleSnapshot& InSnapshot)
{
	// 缓存最新快照，供后续各类 GetXxx 查询使用。
	CurrentSnapshot = InSnapshot;
	bHasSnapshot = true;
	// 目标角色文本来自 Party 数据而非快照，因此每次快照到达都要独立刷新一次。
	UpdateTargetCharacterText();
	// 先广播事件，让订阅者（如 UIManager/自动化测试）拿到快照数据。
	OnInventorySnapshotChanged(InSnapshot);
	if (InSnapshot.bIsValid)
	{
		PopulateListAndDetail();
	}
	if (!InSnapshot.bIsValid)
	{
		// 快照无效（例如背包尚未加载成功）时，把失败原因转达给上层处理。
		OnInventoryUnavailable(InSnapshot.FailureReason);
	}
}

// UpdateTargetCharacterText：刷新标题栏上的“Target: X”文本。
// 该数据不来自 Inventory 快照，而是来自 Party 子系统（当前操控成员），
// 因此这里单独查询 Party 快照并解析出目标角色 GUID。
void UHSRInventoryModuleWidget::UpdateTargetCharacterText()
{
	if (!WidgetTree)
	{
		return;
	}
	UTextBlock* TextBlock = FindTextByName(TEXT("TXT_TargetCharacter"));
	if (!TextBlock)
	{
		return;
	}
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
				// 与 ResolveInventoryCharacterGuid 保持一致：优先取当前操控槽位，
				// 槽位未设置或为空时回退到队长（0 号位）。
				int32 TargetSlot = PartySnapshot.ActiveSlot;
				if (TargetSlot < 0 || TargetSlot >= PartySnapshot.Slots.Num()
					|| PartySnapshot.Slots[TargetSlot].IsEmpty())
				{
					TargetSlot = 0;
				}
				// 兜底校验：槽位合法且非空才真正取出角色 ID。
				if (TargetSlot >= 0 && TargetSlot < PartySnapshot.Slots.Num()
					&& !PartySnapshot.Slots[TargetSlot].IsEmpty())
				{
					CharacterIdName = PartySnapshot.Slots[TargetSlot].CharacterId;
				}
			}
		}
	}
	// 无目标角色时显示“Target: -”，否则显示角色名。
	TextBlock->SetText(CharacterIdName.IsNone()
		? NSLOCTEXT("HSRInventory", "NoTarget", "Target: -")
		: FText::Format(NSLOCTEXT("HSRInventory", "TargetChar", "Target: {0}"), FText::FromName(CharacterIdName)));
}

// PopulateListAndDetail：依据当前快照一次性刷新整个面板——
// 重建列表行，并刷新详情区三个文本与四个操作按钮。
void UHSRInventoryModuleWidget::PopulateListAndDetail()
{
	UWidgetTree* Tree = WidgetTree;
	if (!Tree)
	{
		return;
	}

	// 列表行可能增删（过滤/排序后条目数变化），所以总是重建而不是增量更新。
	PopulateListRows();

	// 详情区：逐个刷新名称/数量/描述三个文本，选中与未选中状态展示不同文案。
	for (UTextBlock* Text : {FindTextByName(TEXT("TXT_DetailName")),
		FindTextByName(TEXT("TXT_DetailQuantity")),
		FindTextByName(TEXT("TXT_DetailDescription"))})
	{
		if (!Text)
		{
			continue;
		}
		if (CurrentSnapshot.Detail.bHasSelection)
		{
			// 有选中物品：名称显示物品名，数量显示 xN，描述临时显示物品名。
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
			// 未选中任何物品：显示引导玩家选择的占位文案。
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

	// 四个操作按钮的可用状态与点击行为都取决于当前选中项，统一在此重建。
	SetActionButton(TEXT("BTN_ActionUse"), EHSRInventoryAction::Use);
	SetActionButton(TEXT("BTN_ActionEquip"), EHSRInventoryAction::Equip);
	SetActionButton(TEXT("BTN_ActionEnhance"), EHSRInventoryAction::Enhance);
	SetActionButton(TEXT("BTN_ActionDisassemble"), EHSRInventoryAction::Disassemble);
}

// SetActionButton：配置单个操作按钮的外观与行为。
// 先清空旧点击事件（防止重复绑定），再按操作是否可用置灰按钮；
// 仅当可用时才绑定对应点击处理，避免不可用时仍响应输入。
void UHSRInventoryModuleWidget::SetActionButton(const FName ButtonName,
	const EHSRInventoryAction Action)
{
	UButton* Button = FindButtonByName(ButtonName);
	if (!Button)
	{
		return;
	}
	// 清空旧绑定，确保重复调用不会叠加多个回调。
	Button->OnClicked.Clear();
	const bool bAvailable = GetActionAvailable(Action);
	Button->SetIsEnabled(bAvailable);
	if (!bAvailable)
	{
		return;
	}
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

// FindButtonByName：按名字在控件树中查找按钮。
UButton* UHSRInventoryModuleWidget::FindButtonByName(const FName Name) const
{
	if (!WidgetTree)
	{
		return nullptr;
	}
	return WidgetTree->FindWidget<UButton>(Name);
}

// FindTextByName：按名字在控件树中查找文本块。
UTextBlock* UHSRInventoryModuleWidget::FindTextByName(const FName Name) const
{
	if (!WidgetTree)
	{
		return nullptr;
	}
	return WidgetTree->FindWidget<UTextBlock>(Name);
}

// HandleBackClicked：返回按钮的点击回调——请求后退一层界面。
void UHSRInventoryModuleWidget::HandleBackClicked()
{
	RequestBack();
}

// HandleCloseClicked：关闭按钮的点击回调——请求一路关闭到根界面。
void UHSRInventoryModuleWidget::HandleCloseClicked()
{
	RequestCloseToRoot();
}

// HandleUseClicked：使用按钮回调。TargetLevel 传 -1 表示该操作不需要等级参数。
void UHSRInventoryModuleWidget::HandleUseClicked()
{
	SubmitAction(EHSRInventoryAction::Use, -1);
}

// HandleEquipClicked：装备按钮回调。
void UHSRInventoryModuleWidget::HandleEquipClicked()
{
	SubmitAction(EHSRInventoryAction::Equip, -1);
}

// HandleEnhanceClicked：强化按钮回调。强化需要目标等级，先查出当前可选的强化目标等级。
void UHSRInventoryModuleWidget::HandleEnhanceClicked()
{
	SubmitAction(EHSRInventoryAction::Enhance, GetSelectedEnhancementTargetLevel());
}

// HandleDisassembleClicked：分解按钮回调。
void UHSRInventoryModuleWidget::HandleDisassembleClicked()
{
	SubmitAction(EHSRInventoryAction::Disassemble, -1);
}

// PopulateListRows：重建列表区。
// 每次都会清空列表容器与行桥接对象，然后按当前快照逐行创建
// “整行按钮 + 名称文本 + 数量文本”，并为每行绑定独立的点击桥接。
// 选中行使用金色底 + 亮色文字，未选中行使用深色底 + 灰文字。
void UHSRInventoryModuleWidget::PopulateListRows()
{
	UScrollBox* ScrollBox = WidgetTree ? WidgetTree->FindWidget<UScrollBox>(TEXT("ListScrollBox")) : nullptr;
	UVerticalBox* Host = WidgetTree ? WidgetTree->FindWidget<UVerticalBox>(TEXT("ListHost")) : nullptr;
	if (!ScrollBox || !Host)
	{
		return;
	}
	// 清空旧行与旧桥接对象，避免行数与引用残留。
	Host->ClearChildren();
	RowBridges.Reset();
	for (int32 Index = 0; Index < CurrentSnapshot.Entries.Num(); ++Index)
	{
		const FHSRInventoryEntryRow& Row = CurrentSnapshot.Entries[Index];
		UButton* RowButton = NewObject<UButton>(this);
		if (!RowButton)
		{
			continue;
		}
		RowButton->SetVisibility(ESlateVisibility::Visible);
		const bool bSelected = Row.Key == CurrentSnapshot.SelectedKey;
		// Selected row: gold-tinted fill + bright text; otherwise subtle dark fill + muted text.
		// 选中行用金色半透明底强调，未选中行用极淡的白色底，保证列表有清晰的当前项。
		RowButton->SetBackgroundColor(bSelected
			? FLinearColor(0.78f, 0.61f, 0.24f, 0.35f)
			: FLinearColor(1.0f, 1.0f, 1.0f, 0.05f));
		// 把整行按钮铺满容器宽度并留出上下内边距，形成行间距。
		if (UVerticalBoxSlot* RowSlot = Cast<UVerticalBoxSlot>(Host->AddChild(RowButton)))
		{
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
			RowSlot->SetPadding(FMargin(4.0f, 3.0f, 4.0f, 3.0f));
		}
		// 行按钮内部再用横向盒放“名称 + 数量”。
		UHorizontalBox* RowBox = NewObject<UHorizontalBox>(RowButton);
		if (!RowBox)
		{
			continue;
		}
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
		// 每行一个桥接对象：把“点击”翻译成“选中第 Index 行”，并随本行一起管理生命周期。
		UHSRInventoryRowClickBridge* Bridge = NewObject<UHSRInventoryRowClickBridge>(this);
		Bridge->Initialize(this, Index);
		RowButton->OnClicked.AddDynamic(Bridge, &UHSRInventoryRowClickBridge::HandleClicked);
		RowBridges.Add(Bridge);
	}
}
