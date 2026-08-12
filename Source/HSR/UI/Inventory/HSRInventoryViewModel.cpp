#include "HSRInventoryViewModel.h"

#include "../../Data/Definitions/HSREquipmentEnhancementCatalog.h"
#include "../../Data/Definitions/HSRInventoryCatalog.h"
#include "../../Data/Definitions/HSRItemEquipmentMappingCatalog.h"
#include "../../Equipment/HSREquipmentSubsystem.h"
#include "../../Inventory/HSRInventorySubsystem.h"

// 本文件匿名命名空间：背包 ViewModel 的私有过滤/排序/槽位工具函数。
// 它们都是纯函数，不接触任何 UObject，便于单元测试与快照重构时复用。
namespace
{
// 过滤匹配：空过滤词匹配所有行；否则按显示名或物品 ID 做忽略大小写的包含匹配
bool HasFilterMatch(const FHSRInventoryEntryRow& Row, const FString& Filter)
{
	if (Filter.IsEmpty())
	{
		return true;
	}
	return Row.DisplayName.ToString().Contains(Filter, ESearchCase::IgnoreCase)
		|| Row.ItemId.ToString().Contains(Filter, ESearchCase::IgnoreCase);
}

// 显示名字典序比较（大小写不敏感，兼容中文按代码单元比较）
int32 CompareDisplayNames(const FHSRInventoryEntryRow& A, const FHSRInventoryEntryRow& B)
{
	const FString Left = A.DisplayName.ToString();
	const FString Right = B.DisplayName.ToString();
	return FCString::Stricmp(*Left, *Right);
}

// 稳定排序判定：返回 A 是否应排在 B 之前。
// 排序优先级：数量（仅数量降序时）→ 显示名 → 目录 SortOrder → 显示名 → ItemId → 实例 ID。
// 之所以在 SortOrder 前后各做一次显示名比较，是为了在"目录顺序"模式下仍然保证同名物品
// 之间有一个确定、可复现的次序。
bool IsBeforeStable(const FHSRInventoryEntryRow& A, const FHSRInventoryEntryRow& B,
	EHSRInventorySortMode SortMode)
{
	// 数量降序模式：数量不等时按数量排
	if (SortMode == EHSRInventorySortMode::QuantityDescending && A.Quantity != B.Quantity)
	{
		return A.Quantity > B.Quantity;
	}
	// 显示名升序模式：直接按显示名排
	if (SortMode == EHSRInventorySortMode::DisplayNameAscending)
	{
		const int32 NameComparison = CompareDisplayNames(A, B);
		if (NameComparison != 0)
		{
			return NameComparison < 0;
		}
	}
	// 目录顺序（默认）：先按目录里的 SortOrder，再按显示名兜底
	if (A.SortOrder != B.SortOrder)
	{
		return A.SortOrder < B.SortOrder;
	}
	if (SortMode != EHSRInventorySortMode::DisplayNameAscending)
	{
		const int32 NameComparison = CompareDisplayNames(A, B);
		if (NameComparison != 0)
		{
			return NameComparison < 0;
		}
	}
	// 最终兜底：ItemId 字典序，再按实例 ID 保证完全有序
	if (A.ItemId != B.ItemId)
	{
		return A.ItemId.LexicalLess(B.ItemId);
	}
	return A.Key.InstanceId < B.Key.InstanceId;
}

// 槽位占用判定：装备类型查装备槽，圣遗物类型查圣遗物槽
bool IsSlotOccupied(const FHSREquipmentLoadout& Loadout, const EHSREquipmentKind Kind,
	const int32 Slot)
{
	return Kind == EHSREquipmentKind::Equipment
		? Loadout.Equipment.Contains(static_cast<EHSREquipmentSlot>(Slot))
		: Loadout.Relics.Contains(static_cast<EHSRRelicSlot>(Slot));
}
}

// 销毁前必须 Shutdown，确保事件订阅在对象销毁前被移除
void UHSRInventoryViewModel::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

// 初始化：绑定背包子系统的事件（物品变化时重建快照），并立即重建一次快照。
// 这里只建立"数据源"，命令上下文（装备/映射目录/强化目录/角色）由 SetCommandContext 另行注入。
void UHSRInventoryViewModel::Initialize(UHSRInventorySubsystem* InInventory,
	UHSRInventoryCatalog* InCatalog)
{
	// 先清理旧状态，保证可重复初始化
	Shutdown();
	Inventory = InInventory;
	Catalog = InCatalog;
	Category = EHSRInventoryCategory::Other;
	FilterText.Reset();
	SortMode = EHSRInventorySortMode::CatalogOrder;

	// 订阅背包变化：任何物品堆叠/唯一物品变化都会触发重建
	if (Inventory.IsValid())
	{
		InventoryHandle = Inventory->OnInventoryChanged().AddUObject(
			this, &ThisClass::HandleInventoryChanged);
	}
	Rebuild();
}

// 设置命令上下文：注入装备子系统、映射目录、强化目录与目标角色 GUID，
// 并订阅该角色的装备配装变化。上下文就绪后才具备执行"装备/强化"等操作的能力。
void UHSRInventoryViewModel::SetCommandContext(UHSREquipmentSubsystem* InEquipment,
	UHSRItemEquipmentMappingCatalog* InMappingCatalog,
	UHSREquipmentEnhancementCatalog* InEnhancementCatalog, const FGuid& InCharacterId)
{
	// 先解绑旧的装备订阅（若已绑定）
	if (Equipment.IsValid() && EquipmentHandle.IsValid())
	{
		Equipment->OnLoadoutChanged().Remove(EquipmentHandle);
	}
	EquipmentHandle.Reset();
	Equipment = InEquipment;
	MappingCatalog = InMappingCatalog;
	EnhancementCatalog = InEnhancementCatalog;
	CharacterId = InCharacterId;

	// 仅在装备子系统与角色 ID 都有效时才订阅配装变化
	if (Equipment.IsValid() && CharacterId.IsValid())
	{
		EquipmentHandle = Equipment->OnLoadoutChanged().AddUObject(
			this, &ThisClass::HandleEquipmentChanged);
	}
	Rebuild();
}

// 关闭：解绑全部订阅并复位所有状态（可安全重复调用）
void UHSRInventoryViewModel::Shutdown()
{
	if (Inventory.IsValid() && InventoryHandle.IsValid())
	{
		Inventory->OnInventoryChanged().Remove(InventoryHandle);
	}
	InventoryHandle.Reset();
	if (Equipment.IsValid() && EquipmentHandle.IsValid())
	{
		Equipment->OnLoadoutChanged().Remove(EquipmentHandle);
	}
	EquipmentHandle.Reset();
	Inventory.Reset();
	Catalog.Reset();
	Equipment.Reset();
	MappingCatalog.Reset();
	EnhancementCatalog.Reset();
	CharacterId.Invalidate();
	Category = EHSRInventoryCategory::Other;
	FilterText.Reset();
	SortMode = EHSRInventorySortMode::CatalogOrder;
	Snapshot = FHSRInventoryModuleSnapshot();
	bHasSnapshot = false;
}

// 取出当前快照的纯值副本：没有快照时返回 false（UI 层据此走"不可用"分支）
bool UHSRInventoryViewModel::GetSnapshot(FHSRInventoryModuleSnapshot& OutSnapshot) const
{
	if (!bHasSnapshot)
	{
		return false;
	}
	OutSnapshot = Snapshot;
	return true;
}

// 切换分类：校验分类合法后，以当前过滤/排序/选中项重建快照并广播
EHSRInventoryViewModelResult UHSRInventoryViewModel::SelectCategory(
	const EHSRInventoryCategory InCategory)
{
	if (!IsInitialized())
	{
		PublishFailure(EHSRInventoryViewModelResult::NotInitialized);
		return EHSRInventoryViewModelResult::NotInitialized;
	}
	if (!IsValidCategory(InCategory))
	{
		PublishFailure(EHSRInventoryViewModelResult::InvalidCategory);
		return EHSRInventoryViewModelResult::InvalidCategory;
	}
	return ApplyPresentationState(InCategory, FilterText, SortMode, Snapshot.SelectedKey);
}

// 设置过滤文本：去除首尾空白后重建快照
EHSRInventoryViewModelResult UHSRInventoryViewModel::SetFilterText(const FString& InFilterText)
{
	if (!IsInitialized())
	{
		PublishFailure(EHSRInventoryViewModelResult::NotInitialized);
		return EHSRInventoryViewModelResult::NotInitialized;
	}
	FString Normalized = InFilterText;
	Normalized.TrimStartAndEndInline();
	return ApplyPresentationState(Category, Normalized, SortMode, Snapshot.SelectedKey);
}

// 设置排序模式：校验合法后重建快照
EHSRInventoryViewModelResult UHSRInventoryViewModel::SetSortMode(
	const EHSRInventorySortMode InSortMode)
{
	if (!IsInitialized())
	{
		PublishFailure(EHSRInventoryViewModelResult::NotInitialized);
		return EHSRInventoryViewModelResult::NotInitialized;
	}
	if (!IsValidSortMode(InSortMode))
	{
		PublishFailure(EHSRInventoryViewModelResult::InvalidSortMode);
		return EHSRInventoryViewModelResult::InvalidSortMode;
	}
	return ApplyPresentationState(Category, FilterText, InSortMode, Snapshot.SelectedKey);
}

// 选中某条目：要求该条目确实存在于当前快照条目列表中，否则按"条目不可用"拒绝
EHSRInventoryViewModelResult UHSRInventoryViewModel::SelectEntry(
	const FHSRInventoryEntryKey& InKey)
{
	if (!IsInitialized())
	{
		PublishFailure(EHSRInventoryViewModelResult::NotInitialized);
		return EHSRInventoryViewModelResult::NotInitialized;
	}
	if (!bHasSnapshot || !Snapshot.Entries.ContainsByPredicate(
		[&InKey](const FHSRInventoryEntryRow& Row) { return Row.Key == InKey; }))
	{
		PublishFailure(EHSRInventoryViewModelResult::EntryUnavailable);
		return EHSRInventoryViewModelResult::EntryUnavailable;
	}
	return ApplyPresentationState(Category, FilterText, SortMode, InKey);
}

// 提交条目操作：把 UI 层的动作请求分派到对应的 Authority（装备子系统）。
// Use/Disassemble 目前没有 Authority 支持，直接拒绝。
EHSRInventoryViewModelResult UHSRInventoryViewModel::SubmitAction(
	const EHSRInventoryAction Action, const int32 TargetLevel)
{
	if (!IsInitialized())
	{
		PublishFailure(EHSRInventoryViewModelResult::NotInitialized);
		return EHSRInventoryViewModelResult::NotInitialized;
	}
	if (!bHasSnapshot || !Snapshot.Detail.bHasSelection)
	{
		PublishFailure(EHSRInventoryViewModelResult::EntryUnavailable);
		return EHSRInventoryViewModelResult::EntryUnavailable;
	}
	switch (Action)
	{
	case EHSRInventoryAction::Use:
	case EHSRInventoryAction::Disassemble:
		// 这两个动作目前缺少 Authority 实现，记录日志后按"不可用"返回
		UE_LOG(LogTemp, Verbose,
			TEXT("HSR.Inventory action=%d unavailable: no supporting Authority"),
			static_cast<int32>(Action));
		return EHSRInventoryViewModelResult::AuthorityUnavailable;
	case EHSRInventoryAction::Equip:
		return SubmitEquip();
	case EHSRInventoryAction::Enhance:
		return SubmitEnhancement(TargetLevel);
	default:
		PublishFailure(EHSRInventoryViewModelResult::AuthorityUnavailable);
		return EHSRInventoryViewModelResult::AuthorityUnavailable;
	}
}

// 提交"装备"操作：把当前选中的唯一物品装备到目标角色。
// 通过装备子系统执行移动，要求物品可唯一解析到装备实例（掉落的奖励物品会在执行时现场铸造实例）。
EHSRInventoryViewModelResult UHSRInventoryViewModel::SubmitEquip()
{
	// 命令上下文必须齐全（装备子系统/映射目录/角色）
	if (!Equipment.IsValid() || !MappingCatalog.IsValid() || !CharacterId.IsValid())
	{
		PublishFailure(EHSRInventoryViewModelResult::AuthorityUnavailable);
		return EHSRInventoryViewModelResult::AuthorityUnavailable;
	}
	const FHSRInventoryEntryRow& Row = Snapshot.Detail.Entry;
	// 只有唯一物品（有实例 ID）才能装备
	if (!Row.bIsUnique || !Row.Key.InstanceId.IsValid())
	{
		PublishFailure(EHSRInventoryViewModelResult::EntryUnavailable);
		return EHSRInventoryViewModelResult::EntryUnavailable;
	}

	// 通过映射目录把物品 ID 解析成装备映射（种类 + 槽位）
	FHSRItemEquipmentMappingEntry Mapping;
	if (!MappingCatalog->Resolve(Row.ItemId, Mapping))
	{
		PublishFailure(EHSRInventoryViewModelResult::CatalogUnavailable);
		return EHSRInventoryViewModelResult::CatalogUnavailable;
	}

	// 读取目标角色当前配装与装备修订号：若目标槽位已被占用，意图为 Replace（替换），否则 Equip
	FHSREquipmentLoadout Loadout;
	int32 EquipmentRevision = 0;
	const bool bHasLoadout = Equipment->GetLoadout(CharacterId, Loadout, EquipmentRevision);
	const EHSREquipmentMovementIntent Intent = bHasLoadout
		&& IsSlotOccupied(Loadout, Mapping.Kind, Mapping.Slot)
		? EHSREquipmentMovementIntent::Replace : EHSREquipmentMovementIntent::Equip;

	// 构造移动请求：携带期望修订号供 Authority 做乐观并发校验
	FHSREquipmentMovementRequest Request;
	Request.OperationId = FGuid::NewGuid();
	Request.CharacterId = CharacterId;
	Request.InstanceId = Row.Key.InstanceId;
	Request.Intent = Intent;
	Request.Kind = Mapping.Kind;
	Request.Slot = Mapping.Slot;
	Request.ExpectedInventoryRevision = Snapshot.InventoryRevision;
	Request.ExpectedEquipmentRevision = EquipmentRevision;

	// 执行移动，并把 Authority 的结果码映射成 ViewModel 结果
	const FHSREquipmentMovementResult AuthorityResult = Equipment->ExecuteMovement(
		Request, *Inventory, *MappingCatalog);
	const EHSRInventoryViewModelResult Result = MapMovementResult(AuthorityResult.Code);
	if (Result != EHSRInventoryViewModelResult::Success)
	{
		// 失败：记录操作号与修订号以便排查，并广播失败（快照保留，等待刷新）
		UE_LOG(LogTemp, Warning,
			TEXT("HSR.Inventory Equip rejected op=%s authorityCode=%d mapped=%d invRev=%lld equipRev=%d"),
			*Request.OperationId.ToString(), static_cast<int32>(AuthorityResult.Code),
			static_cast<int32>(Result), Request.ExpectedInventoryRevision,
			Request.ExpectedEquipmentRevision);
		PublishFailure(Result);
	}
	return Result;
}

// 提交"强化"操作：把当前选中的唯一装备强化到目标等级。
// 需要该装备已注册实例且归属于当前角色，且强化目录能解析出对应规则。
EHSRInventoryViewModelResult UHSRInventoryViewModel::SubmitEnhancement(const int32 TargetLevel)
{
	if (!Equipment.IsValid() || !EnhancementCatalog.IsValid() || !CharacterId.IsValid())
	{
		PublishFailure(EHSRInventoryViewModelResult::AuthorityUnavailable);
		return EHSRInventoryViewModelResult::AuthorityUnavailable;
	}
	if (TargetLevel < 0)
	{
		PublishFailure(EHSRInventoryViewModelResult::InvalidTargetLevel);
		return EHSRInventoryViewModelResult::InvalidTargetLevel;
	}
	const FHSRInventoryEntryRow& Row = Snapshot.Detail.Entry;
	if (!Row.bIsUnique || !Row.Key.InstanceId.IsValid())
	{
		PublishFailure(EHSRInventoryViewModelResult::EntryUnavailable);
		return EHSRInventoryViewModelResult::EntryUnavailable;
	}

	// 装备实例必须已注册
	FHSREquipmentInstance CurrentInstance;
	if (!Equipment->FindRegisteredInstance(Row.Key.InstanceId, CurrentInstance))
	{
		PublishFailure(EHSRInventoryViewModelResult::AuthorityRejected);
		return EHSRInventoryViewModelResult::AuthorityRejected;
	}
	// 实例归属必须就是当前角色（不允许强化别人身上的装备）
	FGuid OwnerCharacterId;
	if (!Equipment->FindInstanceOwner(Row.Key.InstanceId, OwnerCharacterId)
		|| OwnerCharacterId != CharacterId)
	{
		PublishFailure(EHSRInventoryViewModelResult::AuthorityRejected);
		return EHSRInventoryViewModelResult::AuthorityRejected;
	}
	// 读取角色配装与修订号（强化也要乐观并发校验）
	FHSREquipmentLoadout Loadout;
	int32 EquipmentRevision = 0;
	if (!Equipment->GetLoadout(CharacterId, Loadout, EquipmentRevision))
	{
		PublishFailure(EHSRInventoryViewModelResult::AuthorityRejected);
		return EHSRInventoryViewModelResult::AuthorityRejected;
	}

	// 解析强化规则（材料与消耗）
	FHSREquipmentEnhancementRule Rule;
	if (!EnhancementCatalog->ResolveRule(CurrentInstance.DefinitionId, CurrentInstance.Kind,
		TargetLevel, Rule))
	{
		PublishFailure(EHSRInventoryViewModelResult::NoEnhancementOption);
		return EHSRInventoryViewModelResult::NoEnhancementOption;
	}

	// 构造强化请求并执行
	FHSREquipmentEnhancementRequest Request;
	Request.OperationId = FGuid::NewGuid();
	Request.CharacterId = CharacterId;
	Request.InstanceId = Row.Key.InstanceId;
	Request.Kind = CurrentInstance.Kind;
	Request.ExpectedInventoryRevision = Snapshot.InventoryRevision;
	Request.ExpectedEquipmentRevision = EquipmentRevision;
	Request.ExpectedEnhancementLevel = CurrentInstance.EnhancementLevel;
	Request.TargetLevel = TargetLevel;
	const FHSREquipmentEnhancementResult AuthorityResult = Equipment->ExecuteEnhancement(
		Request, *Inventory, *EnhancementCatalog);
	const EHSRInventoryViewModelResult Result = MapEnhancementResult(AuthorityResult.Code);
	if (Result != EHSRInventoryViewModelResult::Success)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("HSR.Inventory Enhance rejected op=%s authorityCode=%d mapped=%d invRev=%lld equipRev=%d target=%d"),
			*Request.OperationId.ToString(), static_cast<int32>(AuthorityResult.Code),
			static_cast<int32>(Result), Request.ExpectedInventoryRevision,
			Request.ExpectedEquipmentRevision, TargetLevel);
		PublishFailure(Result);
	}
	return Result;
}

// 按索引取条目（快照访问器，UI 列表逐行读取用）；越界或无快照返回 false
bool UHSRInventoryViewModel::GetEntry(const int32 Index, FHSRInventoryEntryRow& OutEntry) const
{
	if (!bHasSnapshot || !Snapshot.Entries.IsValidIndex(Index))
	{
		return false;
	}
	OutEntry = Snapshot.Entries[Index];
	return true;
}

// 按索引取动作状态（UI 动作按钮读取用）；越界或无快照返回 false
bool UHSRInventoryViewModel::GetActionState(const int32 Index,
	FHSRInventoryActionState& OutAction) const
{
	if (!bHasSnapshot || !Snapshot.Actions.IsValidIndex(Index))
	{
		return false;
	}
	OutAction = Snapshot.Actions[Index];
	return true;
}

// 背包变化回调：直接重建快照并广播
void UHSRInventoryViewModel::HandleInventoryChanged(const int64)
{
	Rebuild();
}

// 装备配装变化回调：仅当变化发生在当前命令上下文的角色身上时才重建
void UHSRInventoryViewModel::HandleEquipmentChanged(const FGuid& ChangedCharacterId,
	const int32)
{
	if (ChangedCharacterId == CharacterId)
	{
		Rebuild();
	}
}

// 重建快照：用当前展示状态重新 BuildSnapshot。
// 若重建失败且从未发布过快照，则发布一个"无效快照"（bIsValid=false）作为初始占位；
// 否则只发布失败日志、保留旧快照（避免 UI 抖动）。
void UHSRInventoryViewModel::Rebuild()
{
	FHSRInventoryModuleSnapshot Candidate;
	const EHSRInventoryViewModelResult Result = BuildSnapshot(
		Candidate, Category, FilterText, SortMode, bHasSnapshot ? Snapshot.SelectedKey : FHSRInventoryEntryKey());
	if (Result != EHSRInventoryViewModelResult::Success)
	{
		if (!bHasSnapshot)
		{
			// 首次失败：发布一个带失败原因的无效快照，让 UI 有东西可显示
			Snapshot = Candidate;
			Snapshot.Category = Category;
			Snapshot.FilterText = FilterText;
			Snapshot.SortMode = SortMode;
			Snapshot.bIsValid = false;
			Snapshot.FailureReason = Result;
			bHasSnapshot = true;
			Changed.Broadcast(Snapshot);
			OnSnapshotChanged.Broadcast(Snapshot);
		}
		else
		{
			// 已有有效快照：只记录失败，保留旧快照等下次数据修复
			PublishFailure(Result);
		}
		return;
	}

	// 成功：提交新快照并双通道广播（Changed + OnSnapshotChanged）
	Snapshot = MoveTemp(Candidate);
	bHasSnapshot = true;
	Changed.Broadcast(Snapshot);
	OnSnapshotChanged.Broadcast(Snapshot);
}

// 发布失败：记录日志并广播失败，但保留已提交的快照不变
void UHSRInventoryViewModel::PublishFailure(const EHSRInventoryViewModelResult Result) const
{
	UE_LOG(LogTemp, Warning, TEXT("HSR.Inventory ViewModel rejected result=%d; committed snapshot retained"),
		static_cast<int32>(Result));
}

// 应用展示状态（分类/过滤/排序/选中）：重建候选快照，成功后提交并广播
EHSRInventoryViewModelResult UHSRInventoryViewModel::ApplyPresentationState(
	const EHSRInventoryCategory InCategory, const FString& InFilterText,
	const EHSRInventorySortMode InSortMode, const FHSRInventoryEntryKey& InSelectedKey)
{
	FHSRInventoryModuleSnapshot Candidate;
	const EHSRInventoryViewModelResult Result = BuildSnapshot(
		Candidate, InCategory, InFilterText, InSortMode, InSelectedKey);
	if (Result != EHSRInventoryViewModelResult::Success)
	{
		PublishFailure(Result);
		return Result;
	}

	// 成功：写入展示状态、提交快照并广播
	Category = InCategory;
	FilterText = InFilterText;
	SortMode = InSortMode;
	Snapshot = MoveTemp(Candidate);
	bHasSnapshot = true;
	Changed.Broadcast(Snapshot);
	OnSnapshotChanged.Broadcast(Snapshot);
	return EHSRInventoryViewModelResult::Success;
}

// 构建快照：把背包子系统/装备子系统的数据按展示状态转成 UI 可消费的纯值快照。
// 依次：写入展示状态头 → 校验数据源 → 收集堆叠物品与唯一物品 → 排序 → 解析选中项 → 构建动作状态。
EHSRInventoryViewModelResult UHSRInventoryViewModel::BuildSnapshot(
	FHSRInventoryModuleSnapshot& OutSnapshot, const EHSRInventoryCategory InCategory,
	const FString& InFilterText, const EHSRInventorySortMode InSortMode,
	const FHSRInventoryEntryKey& InSelectedKey) const
{
	// 先写展示状态头，失败时也能留下可读的半成品
	OutSnapshot = FHSRInventoryModuleSnapshot();
	OutSnapshot.Category = InCategory;
	OutSnapshot.FilterText = InFilterText;
	OutSnapshot.SortMode = InSortMode;
	OutSnapshot.SelectedKey = InSelectedKey;

	// 数据源校验
	if (!Inventory.IsValid())
	{
		return EHSRInventoryViewModelResult::NotInitialized;
	}
	if (!Catalog.IsValid())
	{
		return EHSRInventoryViewModelResult::CatalogUnavailable;
	}
	// 目录本身必须合法（防止配置错误导致运行期崩溃）
	FString CatalogError;
	if (!Catalog->Validate(&CatalogError))
	{
		UE_LOG(LogTemp, Warning, TEXT("HSR.Inventory invalid catalog: %s"), *CatalogError);
		return EHSRInventoryViewModelResult::InvalidCatalog;
	}
	if (!IsValidCategory(InCategory))
	{
		return EHSRInventoryViewModelResult::InvalidCategory;
	}
	if (!IsValidSortMode(InSortMode))
	{
		return EHSRInventoryViewModelResult::InvalidSortMode;
	}

	// 取背包快照，记录库存修订号
	FHSRInventorySnapshot InventorySnapshot;
	Inventory->GetSnapshot(InventorySnapshot);
	OutSnapshot.InventoryRevision = InventorySnapshot.Revision;

	// 命令上下文就绪时，同时记录该角色装备配装的修订号（用于操作并发校验）
	if (Equipment.IsValid() && CharacterId.IsValid())
	{
		FHSREquipmentLoadout Loadout;
		int32 EquipmentRevision = 0;
		if (Equipment->GetLoadout(CharacterId, Loadout, EquipmentRevision))
		{
			OutSnapshot.EquipmentRevision = EquipmentRevision;
		}
	}
	// 归一化过滤词（去除首尾空白）
	FString NormalizedFilter = InFilterText;
	NormalizedFilter.TrimStartAndEndInline();

	// AddRow：把一条物品数据转成展示行；分类不匹配时返回 true（跳过该行但不报错），
	// 目录缺失时返回 false（数据不一致，整体失败）
	auto AddRow = [&OutSnapshot, this, InCategory, &NormalizedFilter](
		const FName ItemId, const FName DefinitionId, const int32 Quantity,
		const int32 MaxStack, const bool bIsUnique, const FHSRItemInstance* UniqueInstance)
	{
		FHSRInventoryCatalogEntry CatalogEntry;
		if (!Catalog->FindEntry(ItemId, CatalogEntry))
		{
			return false;
		}
		if (CatalogEntry.Category != InCategory)
		{
			return true;
		}

		FHSRInventoryEntryRow Row;
		Row.ItemId = ItemId;
		Row.DefinitionId = DefinitionId;
		Row.Category = CatalogEntry.Category;
		Row.DisplayName = CatalogEntry.DisplayName;
		Row.Quantity = Quantity;
		Row.MaxStack = MaxStack;
		Row.bIsUnique = bIsUnique;
		Row.SortOrder = CatalogEntry.SortOrder;
		Row.Key.ItemId = ItemId;
		Row.Key.InstanceId = bIsUnique && UniqueInstance ? UniqueInstance->InstanceId : FGuid();
		if (UniqueInstance)
		{
			Row.UniqueInstance = *UniqueInstance;
		}
		// 命中过滤词才进列表
		if (HasFilterMatch(Row, NormalizedFilter))
		{
			OutSnapshot.Entries.Add(MoveTemp(Row));
		}
		return true;
	};

	// 收集堆叠物品：必须可解析为 Stackable 存储类型，否则数据不一致
	for (const FHSRItemStackSnapshot& Stack : InventorySnapshot.Stacks)
	{
		EHSRItemStorageKind StorageKind = EHSRItemStorageKind::Unique;
		int32 MaxStack = 0;
		if (!Inventory->GetDefinitionInfo(Stack.ItemId, StorageKind, MaxStack)
			|| StorageKind != EHSRItemStorageKind::Stackable
			|| !AddRow(Stack.ItemId, Stack.ItemId, Stack.Quantity, MaxStack, false, nullptr))
		{
			return EHSRInventoryViewModelResult::EntryUnavailable;
		}
	}
	// 收集唯一物品：必须可解析为 Unique 存储类型且实例 ID 有效
	for (const FHSRItemInstance& Instance : InventorySnapshot.UniqueItems)
	{
		EHSRItemStorageKind StorageKind = EHSRItemStorageKind::Stackable;
		int32 MaxStack = 0;
		if (!Inventory->GetDefinitionInfo(Instance.DefinitionId, StorageKind, MaxStack)
			|| StorageKind != EHSRItemStorageKind::Unique
			|| !Instance.InstanceId.IsValid()
			|| !AddRow(Instance.DefinitionId, Instance.DefinitionId, 1, 1, true, &Instance))
		{
			return EHSRInventoryViewModelResult::EntryUnavailable;
		}
	}

	// 按排序模式稳定排序
	OutSnapshot.Entries.Sort([InSortMode](const FHSRInventoryEntryRow& A, const FHSRInventoryEntryRow& B)
	{
		return IsBeforeStable(A, B, InSortMode);
	});

	// 解析选中项：请求的选中键必须存在于排序后的列表里，否则清空选中
	const FHSRInventoryEntryRow* Selected = OutSnapshot.Entries.FindByPredicate(
		[&InSelectedKey](const FHSRInventoryEntryRow& Row) { return Row.Key == InSelectedKey; });
	if (Selected)
	{
		OutSnapshot.SelectedKey = Selected->Key;
		OutSnapshot.Detail.bHasSelection = true;
		OutSnapshot.Detail.Entry = *Selected;
	}
	else
	{
		OutSnapshot.SelectedKey = FHSRInventoryEntryKey();
	}
	BuildActionStates(OutSnapshot, InventorySnapshot);
	OutSnapshot.bIsValid = true;
	OutSnapshot.FailureReason = EHSRInventoryViewModelResult::Success;
	return EHSRInventoryViewModelResult::Success;
}

// 构建动作状态：为固定动作集（使用/装备/强化/分解）逐项判定可用性。
// 预检顺序：Use/Disassemble 无 Authority → 无选中 → 非唯一物品 → 命令上下文缺失 → 各自具体校验。
void UHSRInventoryViewModel::BuildActionStates(FHSRInventoryModuleSnapshot& InOutSnapshot,
	const FHSRInventorySnapshot& InventorySnapshot) const
{
	InOutSnapshot.Actions.Reset();
	// 强化选项先构建（增强可用性依赖其是否为空）
	BuildEnhancementOptions(InOutSnapshot, InventorySnapshot);
	const bool bHasSelection = InOutSnapshot.Detail.bHasSelection;
	const FHSRInventoryEntryRow& Row = InOutSnapshot.Detail.Entry;

	for (const EHSRInventoryAction Action : {EHSRInventoryAction::Use, EHSRInventoryAction::Equip,
		EHSRInventoryAction::Enhance, EHSRInventoryAction::Disassemble})
	{
		FHSRInventoryActionState& ActionState = InOutSnapshot.Actions.AddDefaulted_GetRef();
		ActionState.Action = Action;
		ActionState.bIsAvailable = false;
		ActionState.UnavailableReason = EHSRInventoryViewModelResult::AuthorityUnavailable;
		// Use/Disassemble：当前无 Authority 支持
		if (Action == EHSRInventoryAction::Use || Action == EHSRInventoryAction::Disassemble)
		{
			continue;
		}
		if (!bHasSelection)
		{
			ActionState.UnavailableReason = EHSRInventoryViewModelResult::EntryUnavailable;
			continue;
		}
		if (!Row.bIsUnique || !Row.Key.InstanceId.IsValid())
		{
			ActionState.UnavailableReason = EHSRInventoryViewModelResult::EntryUnavailable;
			continue;
		}
		if (!Equipment.IsValid() || !CharacterId.IsValid())
		{
			continue;
		}

		if (Action == EHSRInventoryAction::Equip)
		{
			// 装备：依赖映射目录能解析物品 → 装备映射
			if (!MappingCatalog.IsValid())
			{
				continue;
			}
			FHSRItemEquipmentMappingEntry Mapping;
			if (!MappingCatalog->Resolve(Row.ItemId, Mapping))
			{
				ActionState.UnavailableReason = EHSRInventoryViewModelResult::CatalogUnavailable;
				continue;
			}
			// A dropped reward item may not have an equipment instance registered yet; ExecuteMovement
			// mints one on the fly.  Treat a resolvable mapping as equippable so the button is not
			// disabled for items that just entered the bag.
			// 掉落奖励物品可能尚未注册装备实例；ExecuteMovement 会在执行时现场铸造。
			// 因此只要映射可解析就视为可装备，避免刚进背包的物品按钮被禁用。
			ActionState.bIsAvailable = true;
			ActionState.UnavailableReason = EHSRInventoryViewModelResult::Success;
		}
		else if (Action == EHSRInventoryAction::Enhance)
		{
			// 强化：依赖强化目录 + 实例已注册 + 归属当前角色 + 存在可选强化项
			if (!EnhancementCatalog.IsValid())
			{
				ActionState.UnavailableReason = EHSRInventoryViewModelResult::CatalogUnavailable;
				continue;
			}
			FHSREquipmentInstance RegisteredInstance;
			FGuid OwnerCharacterId;
			if (!Equipment->FindRegisteredInstance(Row.Key.InstanceId, RegisteredInstance)
				|| !Equipment->FindInstanceOwner(Row.Key.InstanceId, OwnerCharacterId)
				|| OwnerCharacterId != CharacterId)
			{
				ActionState.UnavailableReason = EHSRInventoryViewModelResult::AuthorityRejected;
				continue;
			}
			if (InOutSnapshot.EnhancementOptions.IsEmpty())
			{
				ActionState.UnavailableReason = EHSRInventoryViewModelResult::NoEnhancementOption;
				continue;
			}
			ActionState.bIsAvailable = true;
			ActionState.UnavailableReason = EHSRInventoryViewModelResult::Success;
		}
	}
}

// 构建强化选项：基于当前选中装备实例的当前等级，列出目录里所有可达到的目标等级及其材料。
// 每个选项计算"是否付得起"（材料库存）与"是否可用"（付得起且目标等级高于当前）。
void UHSRInventoryViewModel::BuildEnhancementOptions(
	FHSRInventoryModuleSnapshot& InOutSnapshot,
	const FHSRInventorySnapshot& InventorySnapshot) const
{
	InOutSnapshot.EnhancementOptions.Reset();
	if (!InOutSnapshot.Detail.bHasSelection || !EnhancementCatalog.IsValid()
		|| !Equipment.IsValid() || !CharacterId.IsValid())
	{
		return;
	}
	const FHSRInventoryEntryRow& Row = InOutSnapshot.Detail.Entry;
	if (!Row.bIsUnique || !Row.Key.InstanceId.IsValid())
	{
		return;
	}
	// 实例必须已注册且归属当前角色
	FHSREquipmentInstance CurrentInstance;
	FGuid OwnerCharacterId;
	if (!Equipment->FindRegisteredInstance(Row.Key.InstanceId, CurrentInstance)
		|| !Equipment->FindInstanceOwner(Row.Key.InstanceId, OwnerCharacterId)
		|| OwnerCharacterId != CharacterId)
	{
		return;
	}

	TArray<FHSREquipmentEnhancementRule> Rules;
	EnhancementCatalog->GetRulesFor(CurrentInstance.DefinitionId, CurrentInstance.Kind,
		CurrentInstance.EnhancementLevel, Rules);
	for (const FHSREquipmentEnhancementRule& Rule : Rules)
	{
		FHSRInventoryEnhancementOption& Option = InOutSnapshot.EnhancementOptions.AddDefaulted_GetRef();
		Option.TargetLevel = Rule.TargetLevel;
		Option.MaterialItemId = Rule.MaterialItemId;
		Option.MaterialCost = Rule.MaterialCost;
		Option.bAffordable = FindStackQuantity(InventorySnapshot, Rule.MaterialItemId) >= Rule.MaterialCost;
		Option.bAvailable = Option.bAffordable && Rule.TargetLevel > CurrentInstance.EnhancementLevel;
	}
}

// 把装备移动结果码映射为 ViewModel 结果码（并发冲突→StaleSnapshot，映射拒绝→目录不可用）
EHSRInventoryViewModelResult UHSRInventoryViewModel::MapMovementResult(
	const EHSREquipmentMovementResultCode Code)
{
	if (Code == EHSREquipmentMovementResultCode::Success)
	{
		return EHSRInventoryViewModelResult::Success;
	}
	if (Code == EHSREquipmentMovementResultCode::InventoryRevisionConflict
		|| Code == EHSREquipmentMovementResultCode::EquipmentRevisionConflict)
	{
		return EHSRInventoryViewModelResult::StaleSnapshot;
	}
	if (Code == EHSREquipmentMovementResultCode::MappingRejected)
	{
		return EHSRInventoryViewModelResult::CatalogUnavailable;
	}
	return EHSRInventoryViewModelResult::AuthorityRejected;
}

// 把装备强化结果码映射为 ViewModel 结果码（NoOp 视为成功；等级冲突也归为 StaleSnapshot）
EHSRInventoryViewModelResult UHSRInventoryViewModel::MapEnhancementResult(
	const EHSREquipmentEnhancementResultCode Code)
{
	if (Code == EHSREquipmentEnhancementResultCode::Success
		|| Code == EHSREquipmentEnhancementResultCode::NoOp)
	{
		return EHSRInventoryViewModelResult::Success;
	}
	if (Code == EHSREquipmentEnhancementResultCode::InventoryRevisionConflict
		|| Code == EHSREquipmentEnhancementResultCode::EquipmentRevisionConflict
		|| Code == EHSREquipmentEnhancementResultCode::EnhancementLevelConflict)
	{
		return EHSRInventoryViewModelResult::StaleSnapshot;
	}
	if (Code == EHSREquipmentEnhancementResultCode::CatalogRejected)
	{
		return EHSRInventoryViewModelResult::CatalogUnavailable;
	}
	return EHSRInventoryViewModelResult::AuthorityRejected;
}

// 在库存快照中查找指定物品的堆叠数量（找不到返回 0）
int32 UHSRInventoryViewModel::FindStackQuantity(const FHSRInventorySnapshot& InventorySnapshot,
	const FName ItemId)
{
	for (const FHSRItemStackSnapshot& Stack : InventorySnapshot.Stacks)
	{
		if (Stack.ItemId == ItemId)
		{
			return Stack.Quantity;
		}
	}
	return 0;
}

// 是否已初始化：以背包子系统数据源是否有效为准
bool UHSRInventoryViewModel::IsInitialized() const
{
	return Inventory.IsValid();
}

// 分类合法性：枚举区间检查
bool UHSRInventoryViewModel::IsValidCategory(const EHSRInventoryCategory InCategory)
{
	return InCategory >= EHSRInventoryCategory::Weapon && InCategory <= EHSRInventoryCategory::Other;
}

// 排序模式合法性：枚举区间检查
bool UHSRInventoryViewModel::IsValidSortMode(const EHSRInventorySortMode InSortMode)
{
	return InSortMode >= EHSRInventorySortMode::CatalogOrder
		&& InSortMode <= EHSRInventorySortMode::QuantityDescending;
}

// 条目键相等判定（包装运算符，供 UI 层比较选中项）
bool UHSRInventoryViewModel::AreKeysEqual(const FHSRInventoryEntryKey& A,
	const FHSRInventoryEntryKey& B)
{
	return A == B;
}
