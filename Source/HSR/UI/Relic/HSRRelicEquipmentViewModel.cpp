#include "HSRRelicEquipmentViewModel.h"

#include "../../Data/Definitions/HSREquipmentEnhancementCatalog.h"
#include "../../Data/Definitions/HSRItemEquipmentMappingCatalog.h"
#include "../../Equipment/HSREquipmentStatAggregator.h"
#include "../../Equipment/HSREquipmentSubsystem.h"
#include "../../Inventory/HSRInventorySubsystem.h"

// 销毁前必须 Shutdown，确保事件订阅在对象销毁前被移除
void UHSRRelicEquipmentViewModel::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

// 初始化：注入装备/背包子系统、映射与强化目录、目标角色 GUID。
// 同时订阅装备配装变化与背包变化；初始化后立即重建一次快照。
// 阶段机初始停在"槽位选择"，未选中任何候选。
void UHSRRelicEquipmentViewModel::Initialize(UHSREquipmentSubsystem* InEquipment,
	UHSRInventorySubsystem* InInventory, UHSRItemEquipmentMappingCatalog* InMappingCatalog,
	UHSREquipmentEnhancementCatalog* InEnhancementCatalog, const FGuid& InCharacterId)
{
	// 先清理旧状态，保证可重复初始化
	Shutdown();
	Equipment = InEquipment;
	Inventory = InInventory;
	MappingCatalog = InMappingCatalog;
	EnhancementCatalog = InEnhancementCatalog;
	CharacterId = InCharacterId;
	Stage = EHSRRelicEquipmentStage::SlotSelection;
	SelectedSlot = EHSRRelicSlot::Head;
	SelectedCandidateId.Invalidate();

	// 订阅装备配装变化（任一角色配装变化都会回调，回调内按角色过滤）
	if (Equipment.IsValid())
	{
		EquipmentHandle = Equipment->OnLoadoutChanged().AddUObject(this, &ThisClass::HandleEquipmentChanged);
	}
	// 订阅背包变化（圣遗物候选来自背包唯一物品）
	if (Inventory.IsValid())
	{
		InventoryHandle = Inventory->OnInventoryChanged().AddUObject(this, &ThisClass::HandleInventoryChanged);
	}
	Rebuild();
}

// 关闭：解绑全部订阅并复位所有状态（可安全重复调用）
void UHSRRelicEquipmentViewModel::Shutdown()
{
	if (Equipment.IsValid() && EquipmentHandle.IsValid())
	{
		Equipment->OnLoadoutChanged().Remove(EquipmentHandle);
	}
	if (Inventory.IsValid() && InventoryHandle.IsValid())
	{
		Inventory->OnInventoryChanged().Remove(InventoryHandle);
	}
	EquipmentHandle.Reset();
	InventoryHandle.Reset();
	Equipment.Reset();
	Inventory.Reset();
	MappingCatalog.Reset();
	EnhancementCatalog.Reset();
	CharacterId.Invalidate();
	Stage = EHSRRelicEquipmentStage::SlotSelection;
	SelectedSlot = EHSRRelicSlot::Head;
	SelectedCandidateId.Invalidate();
	Snapshot = FHSRRelicEquipmentSnapshot();
	bHasSnapshot = false;
}

// 选择圣遗物槽位：校验槽位合法后进入"候选选择"阶段并重建快照
EHSRRelicEquipmentResult UHSRRelicEquipmentViewModel::SelectSlot(const EHSRRelicSlot InSlot)
{
	if (!IsInitialized())
	{
		PublishFailure(EHSRRelicEquipmentResult::NotInitialized);
		return EHSRRelicEquipmentResult::NotInitialized;
	}
	if (!IsValidRelicSlot(InSlot))
	{
		PublishFailure(EHSRRelicEquipmentResult::InvalidSlot);
		return EHSRRelicEquipmentResult::InvalidSlot;
	}
	// 换槽位后清空候选选择，进入候选选择阶段
	SelectedSlot = InSlot;
	SelectedCandidateId.Invalidate();
	Stage = EHSRRelicEquipmentStage::CandidateSelection;
	Rebuild();
	// 重建后以快照有效性作为成功判定；失败则透传失败原因
	return Snapshot.bIsValid ? EHSRRelicEquipmentResult::Success : Snapshot.FailureReason;
}

// 选择候选圣遗物：要求候选确实存在于当前候选列表中，随后进入"对比"阶段并重建
EHSRRelicEquipmentResult UHSRRelicEquipmentViewModel::SelectCandidate(const FGuid& InInstanceId)
{
	if (!IsInitialized())
	{
		PublishFailure(EHSRRelicEquipmentResult::NotInitialized);
		return EHSRRelicEquipmentResult::NotInitialized;
	}
	const bool bFound = Snapshot.Candidates.ContainsByPredicate([&InInstanceId](const FHSRRelicCandidateRow& Row)
	{
		return Row.InstanceId == InInstanceId;
	});
	if (!bFound)
	{
		PublishFailure(EHSRRelicEquipmentResult::CandidateUnavailable);
		return EHSRRelicEquipmentResult::CandidateUnavailable;
	}
	SelectedCandidateId = InInstanceId;
	Stage = EHSRRelicEquipmentStage::Comparison;
	Rebuild();
	return Snapshot.bIsValid ? EHSRRelicEquipmentResult::Success : Snapshot.FailureReason;
}

// 进入强化阶段：切换阶段后重建；若该装备没有任何可强化选项，则拒绝并回退
EHSRRelicEquipmentResult UHSRRelicEquipmentViewModel::OpenEnhancement()
{
	if (!IsInitialized())
	{
		PublishFailure(EHSRRelicEquipmentResult::NotInitialized);
		return EHSRRelicEquipmentResult::NotInitialized;
	}
	Stage = EHSRRelicEquipmentStage::Enhancement;
	Rebuild();
	if (Snapshot.EnhancementOptions.IsEmpty())
	{
		PublishFailure(EHSRRelicEquipmentResult::NoEnhancementOption);
		return EHSRRelicEquipmentResult::NoEnhancementOption;
	}
	return Snapshot.bIsValid ? EHSRRelicEquipmentResult::Success : Snapshot.FailureReason;
}

// 提交候选圣遗物的装备/替换移动：仅当处于"对比"阶段且候选有效时才允许。
// 当前槽位已装备则意图为 Replace，否则为 Equip；执行成功后回到候选选择阶段。
EHSRRelicEquipmentResult UHSRRelicEquipmentViewModel::CommitSelectedMovement()
{
	if (!IsInitialized())
	{
		PublishFailure(EHSRRelicEquipmentResult::NotInitialized);
		return EHSRRelicEquipmentResult::NotInitialized;
	}
	if (Stage != EHSRRelicEquipmentStage::Comparison || !SelectedCandidateId.IsValid())
	{
		PublishFailure(EHSRRelicEquipmentResult::ComparisonUnavailable);
		return EHSRRelicEquipmentResult::ComparisonUnavailable;
	}
	if (!MappingCatalog.IsValid())
	{
		PublishFailure(EHSRRelicEquipmentResult::CatalogUnavailable);
		return EHSRRelicEquipmentResult::CatalogUnavailable;
	}
	// 从快照候选里找回所选候选（防止快照过期导致引用悬空）
	const FHSRRelicCandidateRow* Candidate = Snapshot.Candidates.FindByPredicate(
		[this](const FHSRRelicCandidateRow& Row) { return Row.InstanceId == SelectedCandidateId; });
	if (Candidate == nullptr)
	{
		PublishFailure(EHSRRelicEquipmentResult::CandidateUnavailable);
		return EHSRRelicEquipmentResult::CandidateUnavailable;
	}

	// 构造移动请求：意图由当前槽位是否已装备决定，携带期望修订号做并发校验
	FHSREquipmentMovementRequest Request;
	Request.OperationId = FGuid::NewGuid();
	Request.CharacterId = CharacterId;
	Request.InstanceId = Candidate->InstanceId;
	Request.Intent = Snapshot.CurrentInstanceId.IsValid()
		? EHSREquipmentMovementIntent::Replace : EHSREquipmentMovementIntent::Equip;
	Request.Kind = EHSREquipmentKind::Relic;
	Request.Slot = static_cast<int32>(SelectedSlot);
	Request.ExpectedInventoryRevision = Snapshot.InventoryRevision;
	Request.ExpectedEquipmentRevision = Snapshot.EquipmentRevision;

	// 交给装备子系统执行，并把结果码映射为 ViewModel 结果
	const FHSREquipmentMovementResult Result = Equipment->ExecuteMovement(Request, *Inventory, *MappingCatalog);
	const EHSRRelicEquipmentResult MappedResult = MapMovementResult(Result.Code);
	if (MappedResult != EHSRRelicEquipmentResult::Success)
	{
		PublishFailure(MappedResult);
		return MappedResult;
	}
	// 成功：回到候选选择阶段，清空选择并重建
	Stage = EHSRRelicEquipmentStage::CandidateSelection;
	SelectedCandidateId.Invalidate();
	Rebuild();
	return EHSRRelicEquipmentResult::Success;
}

// 提交强化：以目标等级在快照强化选项里查找规则。
// 预检失败时，材料不足单独上报（玩家可行动），其余合并为通用拒绝。
EHSRRelicEquipmentResult UHSRRelicEquipmentViewModel::CommitEnhancement(const int32 TargetLevel)
{
	if (!IsInitialized())
	{
		PublishFailure(EHSRRelicEquipmentResult::NotInitialized);
		return EHSRRelicEquipmentResult::NotInitialized;
	}
	const FHSRRelicEnhancementOption* Option = Snapshot.EnhancementOptions.FindByPredicate(
		[TargetLevel](const FHSRRelicEnhancementOption& Row) { return Row.TargetLevel == TargetLevel; });
	if (Option == nullptr)
	{
		PublishFailure(EHSRRelicEquipmentResult::InvalidTargetLevel);
		return EHSRRelicEquipmentResult::InvalidTargetLevel;
	}
	if (!Option->bAffordable || !Option->bAvailable || !Snapshot.CurrentInstanceId.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("HSR.Relic CommitEnhancement PreflightRejected Target=%d Affordable=%d Available=%d CurInst=%d ")
			TEXT("Material=%s Cost=%d Held=%d CurLevel=%d"),
			TargetLevel, Option->bAffordable ? 1 : 0, Option->bAvailable ? 1 : 0,
			Snapshot.CurrentInstanceId.IsValid() ? 1 : 0, *Option->MaterialItemId.ToString(),
			Option->MaterialCost, GetHeldMaterialQuantity(Option->MaterialItemId),
			Snapshot.CurrentEnhancementLevel);
		// A shortfall is the one preflight failure the player can actually act on, so say so
		// instead of collapsing it into a generic rejection.
		// 材料不足是玩家唯一能补救的预检失败，因此单独上报，而不是合并成通用拒绝
		const EHSRRelicEquipmentResult Reason = !Option->bAffordable
			? EHSRRelicEquipmentResult::InsufficientMaterial
			: EHSRRelicEquipmentResult::AuthorityRejected;
		PublishFailure(Reason);
		return Reason;
	}
	if (!EnhancementCatalog.IsValid())
	{
		PublishFailure(EHSRRelicEquipmentResult::CatalogUnavailable);
		return EHSRRelicEquipmentResult::CatalogUnavailable;
	}

	// 构造强化请求并执行
	FHSREquipmentEnhancementRequest Request;
	Request.OperationId = FGuid::NewGuid();
	Request.CharacterId = CharacterId;
	Request.InstanceId = Snapshot.CurrentInstanceId;
	Request.Kind = EHSREquipmentKind::Relic;
	Request.ExpectedInventoryRevision = Snapshot.InventoryRevision;
	Request.ExpectedEquipmentRevision = Snapshot.EquipmentRevision;
	Request.ExpectedEnhancementLevel = Snapshot.CurrentEnhancementLevel;
	Request.TargetLevel = TargetLevel;
	const FHSREquipmentEnhancementResult Result = Equipment->ExecuteEnhancement(
		Request, *Inventory, *EnhancementCatalog);
	const EHSRRelicEquipmentResult MappedResult = MapEnhancementResult(Result.Code);
	if (MappedResult != EHSRRelicEquipmentResult::Success)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("HSR.Relic CommitEnhancement SubsystemRejected Target=%d SubsystemCode=%d Mapped=%d ")
			TEXT("InvRev=%lld EquipRev=%d ExpectedLevel=%d"),
			TargetLevel, static_cast<int32>(Result.Code), static_cast<int32>(MappedResult),
			Request.ExpectedInventoryRevision, Request.ExpectedEquipmentRevision,
			Request.ExpectedEnhancementLevel);
		PublishFailure(MappedResult);
		return MappedResult;
	}
	// 成功：留在强化阶段并重建（显示强化后的新状态）
	Stage = EHSRRelicEquipmentStage::Enhancement;
	Rebuild();
	return EHSRRelicEquipmentResult::Success;
}

// 返回/后退：按阶段机逐级回退（强化→对比→候选选择→槽位选择），到根时返回 AtRoot
EHSRRelicEquipmentResult UHSRRelicEquipmentViewModel::Back()
{
	if (!bHasSnapshot)
	{
		return EHSRRelicEquipmentResult::NotInitialized;
	}
	switch (Stage)
	{
	case EHSRRelicEquipmentStage::Enhancement:
		Stage = EHSRRelicEquipmentStage::Comparison;
		Rebuild();
		return EHSRRelicEquipmentResult::Success;
	case EHSRRelicEquipmentStage::Comparison:
		Stage = EHSRRelicEquipmentStage::CandidateSelection;
		Rebuild();
		return EHSRRelicEquipmentResult::Success;
	case EHSRRelicEquipmentStage::CandidateSelection:
		SelectedCandidateId.Invalidate();
		Stage = EHSRRelicEquipmentStage::SlotSelection;
		Rebuild();
		return EHSRRelicEquipmentResult::Success;
	case EHSRRelicEquipmentStage::SlotSelection:
		return EHSRRelicEquipmentResult::AtRoot;
	}
	return EHSRRelicEquipmentResult::AtRoot;
}

// 重建快照：按当前阶段机状态重新构建全部展示数据。
// 流程：写阶段头 → 构建槽位行 → 取库存修订号 → 构建候选行 → （对比/强化阶段）构建对比 →
// 强化阶段构建强化选项。任一环节失败都发布带失败原因的快照；成功则广播。
void UHSRRelicEquipmentViewModel::Rebuild()
{
	// 先复位快照并写阶段头
	Snapshot = FHSRRelicEquipmentSnapshot();
	Snapshot.CharacterId = CharacterId;
	Snapshot.Stage = Stage;
	Snapshot.SelectedSlot = SelectedSlot;
	Snapshot.SelectedCandidateId = SelectedCandidateId;
	if (!IsInitialized())
	{
		PublishFailure(EHSRRelicEquipmentResult::NotInitialized);
		return;
	}

	// 构建槽位行（同时填充当前装备信息与装备修订号）
	FHSREquipmentLoadout Loadout;
	BuildSlotRows(Loadout);

	// 记录库存修订号（用于操作并发校验）
	FHSRInventorySnapshot InventorySnapshot;
	Inventory->GetSnapshot(InventorySnapshot);
	Snapshot.InventoryRevision = InventorySnapshot.Revision;

	// 构建候选行需要映射目录
	if (!MappingCatalog.IsValid())
	{
		PublishFailure(EHSRRelicEquipmentResult::CatalogUnavailable);
		return;
	}
	BuildCandidateRows(Loadout);

	// 对比阶段（或强化阶段仍持有选中候选）需要构建对比数据
	if (Stage == EHSRRelicEquipmentStage::Comparison
		|| (Stage == EHSRRelicEquipmentStage::Enhancement && SelectedCandidateId.IsValid()))
	{
		if (!BuildComparison())
		{
			// 对比构建失败：回退到候选选择阶段并发布失败
			Stage = EHSRRelicEquipmentStage::CandidateSelection;
			Snapshot.Stage = Stage;
			Snapshot.SelectedCandidateId = SelectedCandidateId;
			PublishFailure(EHSRRelicEquipmentResult::ComparisonUnavailable);
			return;
		}
	}

	// 强化阶段必须能构建出强化选项，否则回退（绝不让 UI 渲染一个不存在的选项列表）
	if (Stage == EHSRRelicEquipmentStage::Enhancement && !BuildEnhancementOptions())
	{
		// Match the comparison failure above: never publish Enhancement stage with no options,
		// or a view can render an option list that does not exist.
		// 与上面对比失败的处理保持一致：绝不发布"无选项的强化阶段"
		Stage = EHSRRelicEquipmentStage::CandidateSelection;
		Snapshot.Stage = Stage;
		Snapshot.EnhancementOptions.Reset();
		PublishFailure(EHSRRelicEquipmentResult::NoEnhancementOption);
		return;
	}

	// 全部成功：标记有效并广播
	Snapshot.bIsValid = true;
	Snapshot.FailureReason = EHSRRelicEquipmentResult::Success;
	bHasSnapshot = true;
	Broadcast();
}

// 广播快照到两条通道：Changed（事件委托）与 OnSnapshotChanged（多播动态委托）
void UHSRRelicEquipmentViewModel::Broadcast()
{
	Changed.Broadcast(Snapshot);
	OnSnapshotChanged.Broadcast(Snapshot);
}

// 发布失败快照：保留阶段头，标记无效并携带失败原因，然后广播
void UHSRRelicEquipmentViewModel::PublishFailure(const EHSRRelicEquipmentResult Result)
{
	Snapshot.CharacterId = CharacterId;
	Snapshot.Stage = Stage;
	Snapshot.SelectedSlot = SelectedSlot;
	Snapshot.SelectedCandidateId = SelectedCandidateId;
	Snapshot.bIsValid = false;
	Snapshot.FailureReason = Result;
	bHasSnapshot = true;
	Broadcast();
}

// 是否已初始化：装备/背包子系统与角色 GUID 三者都必须有效
bool UHSRRelicEquipmentViewModel::IsInitialized() const
{
	return Equipment.IsValid() && Inventory.IsValid() && CharacterId.IsValid();
}

// 圣遗物槽位合法性：枚举值不超过 LinkRope（最后一个合法槽位）
bool UHSRRelicEquipmentViewModel::IsValidRelicSlot(const EHSRRelicSlot InSlot) const
{
	return static_cast<uint8>(InSlot) <= static_cast<uint8>(EHSRRelicSlot::LinkRope);
}

// 构建槽位行：遍历全部圣遗物槽位，标记选中态与已装备实例；
// 同时把当前选中槽的已装备实例记录为快照的"当前实例"（用于对比与强化）。
bool UHSRRelicEquipmentViewModel::BuildSlotRows(FHSREquipmentLoadout& OutLoadout)
{
	// 读取角色配装，记录装备修订号
	int32 Revision = 0;
	const bool bHasLoadout = Equipment->GetLoadout(CharacterId, OutLoadout, Revision);
	Snapshot.EquipmentRevision = bHasLoadout ? Revision : 0;
	Snapshot.Slots.Reset();
	Snapshot.CurrentInstanceId.Invalidate();
	Snapshot.CurrentEnhancementLevel = 0;

	for (uint8 SlotIndex = 0; SlotIndex <= static_cast<uint8>(EHSRRelicSlot::LinkRope); ++SlotIndex)
	{
		const EHSRRelicSlot Slot = static_cast<EHSRRelicSlot>(SlotIndex);
		FHSRRelicSlotRow Row;
		Row.Slot = Slot;
		Row.bIsSelected = Slot == SelectedSlot;
		if (bHasLoadout)
		{
			if (const FHSREquipmentInstance* Instance = OutLoadout.Relics.Find(Slot))
			{
				Row.bHasEquipped = true;
				Row.EquippedInstanceId = Instance->InstanceId;
				Row.EquippedInstance = *Instance;
				// 当前选中槽的已装备实例作为"当前实例"快照字段
				if (Slot == SelectedSlot)
				{
					Snapshot.CurrentInstanceId = Instance->InstanceId;
					Snapshot.CurrentEnhancementLevel = Instance->EnhancementLevel;
				}
			}
		}
		Snapshot.Slots.Add(MoveTemp(Row));
	}
	return true;
}

// 构建候选行：从背包唯一物品里筛出"属于当前槽位且未被装备"的圣遗物。
// 掉落圣遗物在背包里是唯一物品但尚未注册装备实例，这里先确保注册再入候选。
void UHSRRelicEquipmentViewModel::BuildCandidateRows(const FHSREquipmentLoadout&)
{
	Snapshot.Candidates.Reset();
	if (!MappingCatalog.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("HSRRelic BuildCandidates NoMappingCatalog"));
		return;
	}
	FHSRInventorySnapshot InventorySnapshot;
	Inventory->GetSnapshot(InventorySnapshot);
	UE_LOG(LogTemp, Log, TEXT("HSRRelic BuildCandidates Slot=%d UniqueItems=%d"),
		static_cast<int32>(SelectedSlot), InventorySnapshot.UniqueItems.Num());

	for (const FHSRItemInstance& Item : InventorySnapshot.UniqueItems)
	{
		// 物品必须能映射成装备
		FHSRItemEquipmentMappingEntry Mapping;
		if (!MappingCatalog->Resolve(Item.DefinitionId, Mapping))
		{
			UE_LOG(LogTemp, Log, TEXT("HSRRelic BuildCandidates Skip ResolveFailed ItemId=%s"), *Item.DefinitionId.ToString());
			continue;
		}
		// 必须是圣遗物种类
		if (Mapping.Kind != EHSREquipmentKind::Relic)
		{
			UE_LOG(LogTemp, Log, TEXT("HSRRelic BuildCandidates Skip NotRelic ItemId=%s Kind=%d"), *Item.DefinitionId.ToString(), static_cast<int32>(Mapping.Kind));
			continue;
		}
		// 槽位必须匹配当前选中槽
		if (Mapping.Slot != static_cast<int32>(SelectedSlot))
		{
			UE_LOG(LogTemp, Log, TEXT("HSRRelic BuildCandidates Skip SlotMismatch ItemId=%s MapSlot=%d SelSlot=%d"), *Item.DefinitionId.ToString(), Mapping.Slot, static_cast<int32>(SelectedSlot));
			continue;
		}
		// A dropped relic has an inventory unique item but no equipment instance until one is
		// minted; ensure it here so the candidate list shows every relic in the bag for this slot.
		// 掉落圣遗物在背包里是唯一物品，但装备实例要到铸造时才存在；
		// 这里先确保实例注册，候选列表才能展示该槽位的全部背包圣遗物
		Equipment->EnsureRegisteredFromItem(Item.DefinitionId, Item.InstanceId, *MappingCatalog);

		// 实例必须已注册且与映射的设备定义一致
		FHSREquipmentInstance Instance;
		if (!Equipment->FindRegisteredInstance(Item.InstanceId, Instance)
			|| Instance.Kind != EHSREquipmentKind::Relic
			|| Instance.DefinitionId != Mapping.EquipmentDefinitionId)
		{
			UE_LOG(LogTemp, Log, TEXT("HSRRelic BuildCandidates Skip NoInstance ItemId=%s Inst=%s"), *Item.DefinitionId.ToString(), *Item.InstanceId.ToString());
			continue;
		}
		// 已装备的（有归属）物品不进入候选
		FGuid Owner;
		if (Equipment->FindInstanceOwner(Item.InstanceId, Owner))
		{
			UE_LOG(LogTemp, Log, TEXT("HSRRelic BuildCandidates Skip Equipped ItemId=%s Inst=%s Owner=%s"), *Item.DefinitionId.ToString(), *Item.InstanceId.ToString(), *Owner.ToString());
			continue;
		}

		// 生成候选行
		FHSRRelicCandidateRow Row;
		Row.InstanceId = Item.InstanceId;
		Row.ItemId = Item.DefinitionId;
		Row.DefinitionId = Instance.DefinitionId;
		Row.Slot = SelectedSlot;
		Row.Instance = Instance;
		Row.bIsSelected = Item.InstanceId == SelectedCandidateId;
		Snapshot.Candidates.Add(MoveTemp(Row));
		UE_LOG(LogTemp, Log, TEXT("HSRRelic BuildCandidates Added ItemId=%s Inst=%s"), *Item.DefinitionId.ToString(), *Item.InstanceId.ToString());
	}

	// 候选按实例 ID 稳定排序，保证 UI 顺序可复现
	Snapshot.Candidates.Sort([](const FHSRRelicCandidateRow& A, const FHSRRelicCandidateRow& B)
	{
		return A.InstanceId < B.InstanceId;
	});
	UE_LOG(LogTemp, Log, TEXT("HSRRelic BuildCandidates Result=%d"), Snapshot.Candidates.Num());
}

// 构建对比数据：把当前已装备实例与选中候选实例逐属性计算差值。
// 当前实例缺失（槽位为空）时只展示候选属性，差值按零基线计算。
bool UHSRRelicEquipmentViewModel::BuildComparison()
{
	// 找回所选候选
	const FHSRRelicCandidateRow* Candidate = Snapshot.Candidates.FindByPredicate(
		[this](const FHSRRelicCandidateRow& Row) { return Row.InstanceId == SelectedCandidateId; });
	if (Candidate == nullptr)
	{
		return false;
	}
	FHSRRelicComparisonSnapshot Comparison;
	Comparison.CandidateInstanceId = Candidate->InstanceId;
	Comparison.CandidateInstance = Candidate->Instance;

	// 当前实例：从槽位行里取当前选中槽的已装备实例
	if (Snapshot.CurrentInstanceId.IsValid())
	{
		const FHSRRelicSlotRow* Current = Snapshot.Slots.FindByPredicate(
			[this](const FHSRRelicSlotRow& Row) { return Row.Slot == SelectedSlot; });
		if (Current != nullptr && Current->bHasEquipped)
		{
			Comparison.CurrentInstanceId = Current->EquippedInstanceId;
			Comparison.CurrentInstance = Current->EquippedInstance;
		}
	}

	// 对每个属性算差值
	for (uint8 StatIndex = 0; StatIndex <= static_cast<uint8>(EHSREquipmentStat::Speed); ++StatIndex)
	{
		FHSRRelicStatDeltaRow Delta;
		Delta.Stat = static_cast<EHSREquipmentStat>(StatIndex);
		Delta.CurrentValue = GetStatValue(Comparison.CurrentInstance, Delta.Stat);
		Delta.CandidateValue = GetStatValue(Comparison.CandidateInstance, Delta.Stat);
		Delta.Delta = Delta.CandidateValue - Delta.CurrentValue;
		Comparison.StatDeltas.Add(MoveTemp(Delta));
	}
	Comparison.bIsValid = true;
	Snapshot.Comparison = MoveTemp(Comparison);
	return true;
}

// 构建强化选项：基于当前已装备圣遗物的强化目录规则，逐项计算可负担/可用性。
// 返回是否有任何选项（无选项时调用方会把阶段回退到候选选择）。
bool UHSRRelicEquipmentViewModel::BuildEnhancementOptions()
{
	Snapshot.EnhancementOptions.Reset();
	if (!EnhancementCatalog.IsValid() || !Snapshot.CurrentInstanceId.IsValid())
	{
		return false;
	}
	const FHSRRelicSlotRow* Current = Snapshot.Slots.FindByPredicate(
		[this](const FHSRRelicSlotRow& Row) { return Row.Slot == SelectedSlot; });
	if (Current == nullptr || !Current->bHasEquipped)
	{
		return false;
	}
	// 取强化目录规则
	TArray<FHSREquipmentEnhancementRule> Rules;
	EnhancementCatalog->GetRulesFor(Current->EquippedInstance.DefinitionId,
		Current->EquippedInstance.Kind, Current->EquippedInstance.EnhancementLevel, Rules);

	// 取库存快照用于材料数量判定
	FHSRInventorySnapshot InventorySnapshot;
	Inventory->GetSnapshot(InventorySnapshot);

	for (const FHSREquipmentEnhancementRule& Rule : Rules)
	{
		FHSRRelicEnhancementOption Option;
		Option.TargetLevel = Rule.TargetLevel;
		Option.MaterialItemId = Rule.MaterialItemId;
		Option.MaterialCost = Rule.MaterialCost;
		Option.TargetModifiers = Rule.TargetModifiers;
		Option.bAffordable = InventorySnapshot.GetStackQuantity(Rule.MaterialItemId) >= Rule.MaterialCost;
		// 可用 = 付得起 且 目标等级高于当前 且 有材料消耗
		Option.bAvailable = Rule.TargetLevel > Current->EquippedInstance.EnhancementLevel
			&& Rule.MaterialCost > 0;
		Snapshot.EnhancementOptions.Add(MoveTemp(Option));
	}
	return !Snapshot.EnhancementOptions.IsEmpty();
}

// 查询当前持有某材料数量（未初始化时返回 -1，用于日志区分）
int32 UHSRRelicEquipmentViewModel::GetHeldMaterialQuantity(const FName ItemId) const
{
	if (!Inventory.IsValid())
	{
		return -1;
	}
	FHSRInventorySnapshot InventorySnapshot;
	Inventory->GetSnapshot(InventorySnapshot);
	return InventorySnapshot.GetStackQuantity(ItemId);
}

// 装备配装变化回调：仅当变化发生在当前角色身上时才重建
void UHSRRelicEquipmentViewModel::HandleEquipmentChanged(const FGuid& ChangedCharacterId, int32)
{
	if (ChangedCharacterId == CharacterId)
	{
		Rebuild();
	}
}

// 背包变化回调：直接重建（候选/强化材料都来自背包）
void UHSRRelicEquipmentViewModel::HandleInventoryChanged(int64)
{
	Rebuild();
}

// Sums one instance through the same aggregator the authority uses, so the comparison panel cannot
// show a value Equip would reject. An unaggregatable instance reads as zero across every stat.
// 用与 Authority 相同的聚合器把单个实例求和，保证对比面板不会显示"装备时会被拒绝"的值；
// 无法聚合的实例在每个属性上都读作零。
float UHSRRelicEquipmentViewModel::GetStatValue(const FHSREquipmentInstance& Instance,
	const EHSREquipmentStat Stat)
{
	FHSREquipmentAggregate Aggregate;
	if (!UHSREquipmentStatAggregator::AddInstance(Instance, Aggregate))
	{
		return 0.0f;
	}

	switch (Stat)
	{
	case EHSREquipmentStat::MaxHealth:
		return Aggregate.MaxHealth;
	case EHSREquipmentStat::Attack:
		return Aggregate.Attack;
	case EHSREquipmentStat::Defense:
		return Aggregate.Defense;
	case EHSREquipmentStat::Speed:
		return Aggregate.Speed;
	default:
		return 0.0f;
	}
}

// 把装备移动结果码映射为 ViewModel 结果码
EHSRRelicEquipmentResult UHSRRelicEquipmentViewModel::MapMovementResult(
	const EHSREquipmentMovementResultCode Code)
{
	if (Code == EHSREquipmentMovementResultCode::Success)
	{
		return EHSRRelicEquipmentResult::Success;
	}
	if (Code == EHSREquipmentMovementResultCode::InventoryRevisionConflict
		|| Code == EHSREquipmentMovementResultCode::EquipmentRevisionConflict)
	{
		return EHSRRelicEquipmentResult::StaleSnapshot;
	}
	if (Code == EHSREquipmentMovementResultCode::MappingRejected)
	{
		return EHSRRelicEquipmentResult::CatalogUnavailable;
	}
	if (Code == EHSREquipmentMovementResultCode::InvalidRequest)
	{
		return EHSRRelicEquipmentResult::InvalidRequest;
	}
	// InventoryRejected/EquipmentRejected/ProjectionRejected/OperationIdConflict all mean an
	// authority refused the move; the player cannot act on the distinction.
	// 这些结果码都表示 Authority 拒绝了移动，玩家无法据此采取行动，统一为通用拒绝
	return EHSRRelicEquipmentResult::AuthorityRejected;
}

// 把装备强化结果码映射为 ViewModel 结果码
EHSRRelicEquipmentResult UHSRRelicEquipmentViewModel::MapEnhancementResult(
	const EHSREquipmentEnhancementResultCode Code)
{
	if (Code == EHSREquipmentEnhancementResultCode::Success
		|| Code == EHSREquipmentEnhancementResultCode::NoOp)
	{
		return EHSRRelicEquipmentResult::Success;
	}
	if (Code == EHSREquipmentEnhancementResultCode::InventoryRevisionConflict
		|| Code == EHSREquipmentEnhancementResultCode::EquipmentRevisionConflict
		|| Code == EHSREquipmentEnhancementResultCode::EnhancementLevelConflict)
	{
		return EHSRRelicEquipmentResult::StaleSnapshot;
	}
	if (Code == EHSREquipmentEnhancementResultCode::CatalogRejected)
	{
		return EHSRRelicEquipmentResult::CatalogUnavailable;
	}
	// The only inventory gate in ExecuteEnhancement is PrepareEquipmentEnhancementCandidate,
	// which fails when the player cannot pay the material cost -- an actionable message.
	// ExecuteEnhancement 里唯一的库存关卡是材料准备，失败即玩家付不起材料——这是可行动的消息
	if (Code == EHSREquipmentEnhancementResultCode::InventoryRejected)
	{
		return EHSRRelicEquipmentResult::InsufficientMaterial;
	}
	if (Code == EHSREquipmentEnhancementResultCode::InvalidRequest)
	{
		return EHSRRelicEquipmentResult::InvalidRequest;
	}
	// EquipmentRejected/ProjectionRejected/OperationIdConflict: authority refused, not actionable.
	// 其余结果码均为 Authority 拒绝，不可行动，统一为通用拒绝
	return EHSRRelicEquipmentResult::AuthorityRejected;
}
