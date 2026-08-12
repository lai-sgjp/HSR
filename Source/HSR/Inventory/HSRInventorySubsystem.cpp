#include "HSRInventorySubsystem.h"

#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Reward/HSRSettlementTypes.h"

// RegisterDefinition：注册一个物品定义。先做校验（CanRegisterDefinition），
// 通过后把「存储类型 + 最大堆叠」写进 Definitions 表。
EHSRInventoryOperationResult UHSRInventorySubsystem::RegisterDefinition(const UHSRItemDefinition& Definition)
{
	const EHSRInventoryOperationResult Validation = CanRegisterDefinition(Definition);
	if (Validation != EHSRInventoryOperationResult::Success)
	{
		return Validation;
	}
	Definitions.Add(Definition.ItemId, {Definition.StorageKind, Definition.MaxStack});
	return EHSRInventoryOperationResult::Success;
}

// CanRegisterDefinition：校验一个物品定义能否注册。规则：
//   - ItemId 非空；
//   - MaxStack >= 1，且唯一物品（Unique）必须 MaxStack == 1；
//   - 若该 ID 已注册：属性完全一致则 NoOp，不一致则 DuplicateDefinitionId。
EHSRInventoryOperationResult UHSRInventorySubsystem::CanRegisterDefinition(const UHSRItemDefinition& Definition) const
{
	if (Definition.ItemId.IsNone())
	{
		return EHSRInventoryOperationResult::InvalidDefinitionId;
	}
	if (Definition.MaxStack < 1 || (Definition.StorageKind == EHSRItemStorageKind::Unique && Definition.MaxStack != 1))
	{
		return EHSRInventoryOperationResult::InvalidDefinition;
	}
	if (const FDefinitionRule* Existing = Definitions.Find(Definition.ItemId))
	{
		return Existing->StorageKind == Definition.StorageKind && Existing->MaxStack == Definition.MaxStack
			? EHSRInventoryOperationResult::NoOp
			: EHSRInventoryOperationResult::DuplicateDefinitionId;
	}

	return EHSRInventoryOperationResult::Success;
}

// AddStack：给堆叠物品加数量。校验链：定义存在、存储类型为 Stackable、数量为正、
// 不溢出 int32、不超过 MaxStack、且总占用槽位不超容量。全部通过才 Commit。
EHSRInventoryOperationResult UHSRInventorySubsystem::AddStack(FName ItemId, int32 Quantity)
{
	if (ItemId.IsNone())
	{
		return EHSRInventoryOperationResult::InvalidDefinitionId;
	}
	if (Quantity <= 0)
	{
		return EHSRInventoryOperationResult::InvalidQuantity;
	}
	const FDefinitionRule* Rule = Definitions.Find(ItemId);
	if (!Rule)
	{
		return EHSRInventoryOperationResult::UnknownDefinition;
	}
	if (Rule->StorageKind != EHSRItemStorageKind::Stackable)
	{
		return EHSRInventoryOperationResult::StorageKindMismatch;
	}

	const int32 Existing = Stacks.FindRef(ItemId);
	if (Quantity > MAX_int32 - Existing)
	{
		return EHSRInventoryOperationResult::QuantityOverflow;
	}
	const int32 NewQuantity = Existing + Quantity;
	if (NewQuantity > Rule->MaxStack)
	{
		return EHSRInventoryOperationResult::StackLimitExceeded;
	}

	// 在候选副本上运算，超容量不提交。
	TMap<FName, int32> CandidateStacks = Stacks;
	TMap<FGuid, FHSRItemInstance> CandidateUniqueItems = UniqueItems;
	CandidateStacks.Add(ItemId, NewQuantity);
	if (GetUsedSlots(CandidateStacks, CandidateUniqueItems) > Capacity)
	{
		return EHSRInventoryOperationResult::CapacityExceeded;
	}
	Commit(MoveTemp(CandidateStacks), MoveTemp(CandidateUniqueItems));
	return EHSRInventoryOperationResult::Success;
}

// RemoveStack：减少堆叠物品数量。数量不足则拒绝；减到 0 时移除该条目。
EHSRInventoryOperationResult UHSRInventorySubsystem::RemoveStack(FName ItemId, int32 Quantity)
{
	if (ItemId.IsNone())
	{
		return EHSRInventoryOperationResult::InvalidDefinitionId;
	}
	if (Quantity <= 0)
	{
		return EHSRInventoryOperationResult::InvalidQuantity;
	}
	const FDefinitionRule* Rule = Definitions.Find(ItemId);
	if (!Rule)
	{
		return EHSRInventoryOperationResult::UnknownDefinition;
	}
	if (Rule->StorageKind != EHSRItemStorageKind::Stackable)
	{
		return EHSRInventoryOperationResult::StorageKindMismatch;
	}
	const int32 Existing = Stacks.FindRef(ItemId);
	if (Existing < Quantity)
	{
		return EHSRInventoryOperationResult::InsufficientQuantity;
	}

	TMap<FName, int32> CandidateStacks = Stacks;
	TMap<FGuid, FHSRItemInstance> CandidateUniqueItems = UniqueItems;
	const int32 Remaining = Existing - Quantity;
	if (Remaining == 0)
	{
		CandidateStacks.Remove(ItemId);
	}
	else
	{
		CandidateStacks.Add(ItemId, Remaining);
	}
	Commit(MoveTemp(CandidateStacks), MoveTemp(CandidateUniqueItems));
	return EHSRInventoryOperationResult::Success;
}

// AddUnique：加入一个唯一物品实例。校验：实例 ID 合法、定义存在且为 Unique、实例未
// 重复；通过后检查容量并提交。
EHSRInventoryOperationResult UHSRInventorySubsystem::AddUnique(const FHSRItemInstance& Instance)
{
	if (!Instance.InstanceId.IsValid())
	{
		return EHSRInventoryOperationResult::InvalidInstanceId;
	}
	if (Instance.DefinitionId.IsNone())
	{
		return EHSRInventoryOperationResult::InvalidDefinitionId;
	}
	const FDefinitionRule* Rule = Definitions.Find(Instance.DefinitionId);
	if (!Rule)
	{
		return EHSRInventoryOperationResult::UnknownDefinition;
	}
	if (Rule->StorageKind != EHSRItemStorageKind::Unique)
	{
		return EHSRInventoryOperationResult::StorageKindMismatch;
	}
	if (const FHSRItemInstance* Existing = UniqueItems.Find(Instance.InstanceId))
	{
		return Existing->DefinitionId == Instance.DefinitionId
			? EHSRInventoryOperationResult::NoOp
			: EHSRInventoryOperationResult::DuplicateInstanceId;
	}

	TMap<FName, int32> CandidateStacks = Stacks;
	TMap<FGuid, FHSRItemInstance> CandidateUniqueItems = UniqueItems;
	CandidateUniqueItems.Add(Instance.InstanceId, Instance);
	if (GetUsedSlots(CandidateStacks, CandidateUniqueItems) > Capacity)
	{
		return EHSRInventoryOperationResult::CapacityExceeded;
	}
	Commit(MoveTemp(CandidateStacks), MoveTemp(CandidateUniqueItems));
	return EHSRInventoryOperationResult::Success;
}

// RemoveUnique：移除一个唯一物品实例。
EHSRInventoryOperationResult UHSRInventorySubsystem::RemoveUnique(const FGuid& InstanceId)
{
	if (!InstanceId.IsValid())
	{
		return EHSRInventoryOperationResult::InvalidInstanceId;
	}
	if (!UniqueItems.Contains(InstanceId))
	{
		return EHSRInventoryOperationResult::InstanceNotFound;
	}

	TMap<FName, int32> CandidateStacks = Stacks;
	TMap<FGuid, FHSRItemInstance> CandidateUniqueItems = UniqueItems;
	CandidateUniqueItems.Remove(InstanceId);
	Commit(MoveTemp(CandidateStacks), MoveTemp(CandidateUniqueItems));
	return EHSRInventoryOperationResult::Success;
}

// ApplyGrants：应用一批发放（Grants），并广播背包变更。
EHSRInventoryOperationResult UHSRInventorySubsystem::ApplyGrants(const TArray<FHSRInventoryGrant>& Grants)
{
	int64 CommittedRevision = Revision;
	return ApplyGrantsInternal(Grants, true, CommittedRevision);
}

// ApplyGrantsToCandidate：在候选副本上应用发放（不提交）。这是事务的基础：
// 先在这里干跑，全部合法后再由调用方决定是否提交。
EHSRInventoryOperationResult UHSRInventorySubsystem::ApplyGrantsToCandidate(const TArray<FHSRInventoryGrant>& Grants,
	TMap<FName, int32>& CandidateStacks, TMap<FGuid, FHSRItemInstance>& CandidateUniqueItems) const
{
	for (const FHSRInventoryGrant& Grant : Grants)
	{
		if (Grant.ItemId.IsNone())
		{
			return EHSRInventoryOperationResult::InvalidDefinitionId;
		}
		if (Grant.Quantity <= 0)
		{
			return EHSRInventoryOperationResult::InvalidQuantity;
		}
		const FDefinitionRule* Rule = Definitions.Find(Grant.ItemId);
		if (!Rule)
		{
			return EHSRInventoryOperationResult::UnknownDefinition;
		}

		if (Rule->StorageKind == EHSRItemStorageKind::Stackable)
		{
			// 堆叠物品：不得带实例 ID，数量累加且不超上限。
			if (!Grant.InstanceIds.IsEmpty())
			{
				return EHSRInventoryOperationResult::StorageKindMismatch;
			}
			const int32 Existing = CandidateStacks.FindRef(Grant.ItemId);
			if (Grant.Quantity > MAX_int32 - Existing)
			{
				return EHSRInventoryOperationResult::QuantityOverflow;
			}
			const int32 NewQuantity = Existing + Grant.Quantity;
			if (NewQuantity > Rule->MaxStack)
			{
				return EHSRInventoryOperationResult::StackLimitExceeded;
			}
			CandidateStacks.Add(Grant.ItemId, NewQuantity);
			continue;
		}

		// 唯一物品：每个实例必须有 ID，且数量必须等于实例数量。
		if (Grant.InstanceIds.Num() != Grant.Quantity)
		{
			return EHSRInventoryOperationResult::StorageKindMismatch;
		}
		for (const FGuid& InstanceId : Grant.InstanceIds)
		{
			if (!InstanceId.IsValid())
			{
				return EHSRInventoryOperationResult::InvalidInstanceId;
			}
			if (CandidateUniqueItems.Contains(InstanceId))
			{
				return EHSRInventoryOperationResult::DuplicateInstanceId;
			}
			CandidateUniqueItems.Add(InstanceId, {InstanceId, Grant.ItemId});
		}
	}
	return EHSRInventoryOperationResult::Success;
}

// ApplyGrantsInternal：发放的内部实现。bBroadcast 控制是否广播 InventoryChanged，
// 通过 OutRevision 回传新版本号。空发放直接 NoOp。
EHSRInventoryOperationResult UHSRInventorySubsystem::ApplyGrantsInternal(const TArray<FHSRInventoryGrant>& Grants, bool bBroadcast, int64& OutRevision)
{
	if (Grants.IsEmpty())
	{
		return EHSRInventoryOperationResult::NoOp;
	}

	// 候选副本上应用发放，再查容量。
	TMap<FName, int32> CandidateStacks = Stacks;
	TMap<FGuid, FHSRItemInstance> CandidateUniqueItems = UniqueItems;
	if (const EHSRInventoryOperationResult GrantResult =
			ApplyGrantsToCandidate(Grants, CandidateStacks, CandidateUniqueItems);
		GrantResult != EHSRInventoryOperationResult::Success)
	{
		return GrantResult;
	}

	if (GetUsedSlots(CandidateStacks, CandidateUniqueItems) > Capacity)
	{
		return EHSRInventoryOperationResult::CapacityExceeded;
	}

	// 提交并推进版本号。
	Stacks = MoveTemp(CandidateStacks);
	UniqueItems = MoveTemp(CandidateUniqueItems);
	OutRevision = ++Revision;
	if (bBroadcast)
	{
		BroadcastRevision(OutRevision);
	}
	return EHSRInventoryOperationResult::Success;
}

// GetDefinitionInfo：读取物品定义的存储类型与最大堆叠。
bool UHSRInventorySubsystem::GetDefinitionInfo(FName ItemId, EHSRItemStorageKind& OutStorageKind, int32& OutMaxStack) const
{
	const FDefinitionRule* Rule = Definitions.Find(ItemId);
	if (!Rule)
	{
		return false;
	}
	OutStorageKind = Rule->StorageKind;
	OutMaxStack = Rule->MaxStack;
	return true;
}

// GetSnapshot：导出当前背包快照（含容量、占用、版本号、排序后的物品列表）。
// 排序保证快照可复现——存档编码依赖这个稳定顺序。
void UHSRInventorySubsystem::GetSnapshot(FHSRInventorySnapshot& OutSnapshot) const
{
	OutSnapshot = FHSRInventorySnapshot();
	OutSnapshot.Capacity = Capacity;
	OutSnapshot.UsedSlots = GetUsedSlots(Stacks, UniqueItems);
	OutSnapshot.Revision = Revision;

	for (const TPair<FName, int32>& Entry : Stacks)
	{
		OutSnapshot.Stacks.Add({Entry.Key, Entry.Value});
	}
	OutSnapshot.Stacks.Sort([](const FHSRItemStackSnapshot& A, const FHSRItemStackSnapshot& B)
	{
		return A.ItemId.LexicalLess(B.ItemId);
	});

	UniqueItems.GenerateValueArray(OutSnapshot.UniqueItems);
	OutSnapshot.UniqueItems.Sort([](const FHSRItemInstance& A, const FHSRItemInstance& B)
	{
		if (A.DefinitionId != B.DefinitionId)
		{
			return A.DefinitionId.LexicalLess(B.DefinitionId);
		}
		return A.InstanceId < B.InstanceId;
	});
}

// ExportSaveData：导出存档用的背包数据（走一遍快照再拷贝）。
void UHSRInventorySubsystem::ExportSaveData(FHSRInventorySaveData& OutData) const
{
	FHSRInventorySnapshot Snapshot;
	GetSnapshot(Snapshot);
	OutData.Stacks = MoveTemp(Snapshot.Stacks);
	OutData.UniqueItems = MoveTemp(Snapshot.UniqueItems);
	OutData.Revision = Revision;
}

// PrepareRestore：读档恢复干跑。校验每个堆叠/唯一物品的定义存在且存储类型匹配、
// 数量在合法范围、无重复键、总占用不超容量。
bool UHSRInventorySubsystem::PrepareRestore(const FHSRInventorySaveData& Data, FHSRInventoryRestoreState& OutCandidate) const
{
	if (Data.Revision < 0)
	{
		return false;
	}

	FHSRInventoryRestoreState Candidate;
	Candidate.Revision = Data.Revision;
	for (const FHSRItemStackSnapshot& Stack : Data.Stacks)
	{
		const FDefinitionRule* Rule = Definitions.Find(Stack.ItemId);
		if (!Rule || Rule->StorageKind != EHSRItemStorageKind::Stackable
			|| Stack.Quantity <= 0 || Stack.Quantity > Rule->MaxStack
			|| Candidate.Stacks.Contains(Stack.ItemId))
		{
			return false;
		}
		Candidate.Stacks.Add(Stack.ItemId, Stack.Quantity);
	}
	for (const FHSRItemInstance& Instance : Data.UniqueItems)
	{
		const FDefinitionRule* Rule = Definitions.Find(Instance.DefinitionId);
		if (!Instance.InstanceId.IsValid() || !Rule || Rule->StorageKind != EHSRItemStorageKind::Unique
			|| Candidate.UniqueItems.Contains(Instance.InstanceId))
		{
			return false;
		}
		Candidate.UniqueItems.Add(Instance.InstanceId, Instance);
	}
	if (GetUsedSlots(Candidate.Stacks, Candidate.UniqueItems) > Capacity)
	{
		return false;
	}
	OutCandidate = MoveTemp(Candidate);
	return true;
}

// IsRestoreDifferent：判断候选恢复状态与当前背包是否不同（用于决定是否广播变更）。
bool UHSRInventorySubsystem::IsRestoreDifferent(const FHSRInventoryRestoreState& Candidate) const
{
	if (Revision != Candidate.Revision || Stacks.Num() != Candidate.Stacks.Num() || UniqueItems.Num() != Candidate.UniqueItems.Num())
	{
		return true;
	}
	for (const TPair<FName, int32>& Entry : Stacks)
	{
		if (Candidate.Stacks.FindRef(Entry.Key) != Entry.Value)
		{
			return true;
		}
	}
	for (const TPair<FGuid, FHSRItemInstance>& Entry : UniqueItems)
	{
		const FHSRItemInstance* Other = Candidate.UniqueItems.Find(Entry.Key);
		if (!Other || Other->DefinitionId != Entry.Value.DefinitionId)
		{
			return true;
		}
	}
	return false;
}

// CommitRestore：提交恢复后的背包状态。bNotify 控制是否广播。
void UHSRInventorySubsystem::CommitRestore(FHSRInventoryRestoreState&& Candidate, bool bNotify)
{
	Stacks = MoveTemp(Candidate.Stacks);
	UniqueItems = MoveTemp(Candidate.UniqueItems);
	Revision = Candidate.Revision;
	if (bNotify)
	{
		InventoryChanged.Broadcast(Revision);
	}
}

// PrepareSettlementCandidate：结算事务的背包预演。要求事务 ID 有效、期望版本号与
// 当前一致，在候选副本上应用发放并查容量，产出 NextRevision。
EHSRInventoryOperationResult UHSRInventorySubsystem::PrepareSettlementCandidate(const FGuid& TransactionId,
	const TArray<FHSRInventoryGrant>& Grants, int64 ExpectedRevision,
	FHSRInventorySettlementCandidate& OutCandidate) const
{
	if (ExpectedRevision != Revision)
	{
		return EHSRInventoryOperationResult::RevisionConflict;
	}
	if (Grants.IsEmpty())
	{
		return EHSRInventoryOperationResult::NoOp;
	}

	FHSRInventorySettlementCandidate Candidate;
	Candidate.TransactionId = TransactionId;
	Candidate.Stacks = Stacks;
	Candidate.UniqueItems = UniqueItems;
	if (const EHSRInventoryOperationResult GrantResult =
			ApplyGrantsToCandidate(Grants, Candidate.Stacks, Candidate.UniqueItems);
		GrantResult != EHSRInventoryOperationResult::Success)
	{
		return GrantResult;
	}
	if (GetUsedSlots(Candidate.Stacks, Candidate.UniqueItems) > Capacity)
	{
		return EHSRInventoryOperationResult::CapacityExceeded;
	}
	Candidate.NextRevision = Revision + 1;
	OutCandidate = MoveTemp(Candidate);
	return EHSRInventoryOperationResult::Success;
}

// InstallSettlementCandidateNoFail：结算提交阶段安装候选状态（预演已保证成功）。
void UHSRInventorySubsystem::InstallSettlementCandidateNoFail(FHSRInventorySettlementCandidate&& Candidate)
{
	Stacks = MoveTemp(Candidate.Stacks);
	UniqueItems = MoveTemp(Candidate.UniqueItems);
}

// FinalizeSettlementRevisionNoFail：结算提交阶段推进版本号。
void UHSRInventorySubsystem::FinalizeSettlementRevisionNoFail(int64 PreparedRevision)
{
	Revision = PreparedRevision;
}

// PublishSettlementCommit：结算提交阶段广播变更。
void UHSRInventorySubsystem::PublishSettlementCommit(int64 PreparedRevision)
{
	BroadcastRevision(Revision);
}

// PrepareEquipmentRemovalCandidate：装备「卸下」的背包预演——把该装备对应的唯一物品
// 从背包候选里移除（装备穿戴时该物品在背包里被标记为已装备占用，这里将其释放）。
EHSRInventoryOperationResult UHSRInventorySubsystem::PrepareEquipmentRemovalCandidate(const FGuid& InstanceId,
	const FName ExpectedItemId, const int64 ExpectedRevision, FHSRInventoryMovementCandidate& OutCandidate) const
{
	if (ExpectedRevision != Revision)
	{
		return EHSRInventoryOperationResult::RevisionConflict;
	}
	const FHSRItemInstance* Existing = UniqueItems.Find(InstanceId);
	if (!Existing)
	{
		return EHSRInventoryOperationResult::InstanceNotFound;
	}
	if (Existing->DefinitionId != ExpectedItemId)
	{
		return EHSRInventoryOperationResult::StorageKindMismatch;
	}

	FHSRInventoryMovementCandidate Candidate;
	Candidate.Stacks = Stacks;
	Candidate.UniqueItems = UniqueItems;
	Candidate.UniqueItems.Remove(InstanceId);
	Candidate.NextRevision = Revision + 1;
	OutCandidate = MoveTemp(Candidate);
	return EHSRInventoryOperationResult::Success;
}

// PrepareEquipmentAdditionCandidate：装备「穿戴」的背包预演——把装备对应的唯一物品
// 加回背包（占用一个容量槽位）。
EHSRInventoryOperationResult UHSRInventorySubsystem::PrepareEquipmentAdditionCandidate(const FGuid& InstanceId,
	const FName ItemId, const int64 ExpectedRevision, FHSRInventoryMovementCandidate& OutCandidate) const
{
	if (ExpectedRevision != Revision)
	{
		return EHSRInventoryOperationResult::RevisionConflict;
	}
	const FDefinitionRule* Rule = Definitions.Find(ItemId);
	if (!Rule)
	{
		return EHSRInventoryOperationResult::UnknownDefinition;
	}
	if (Rule->StorageKind != EHSRItemStorageKind::Unique)
	{
		return EHSRInventoryOperationResult::StorageKindMismatch;
	}
	if (UniqueItems.Contains(InstanceId))
	{
		return EHSRInventoryOperationResult::DuplicateInstanceId;
	}

	FHSRInventoryMovementCandidate Candidate;
	Candidate.Stacks = Stacks;
	Candidate.UniqueItems = UniqueItems;
	Candidate.UniqueItems.Add(InstanceId, {InstanceId, ItemId});
	if (GetUsedSlots(Candidate.Stacks, Candidate.UniqueItems) > Capacity)
	{
		return EHSRInventoryOperationResult::CapacityExceeded;
	}
	Candidate.NextRevision = Revision + 1;
	OutCandidate = MoveTemp(Candidate);
	return EHSRInventoryOperationResult::Success;
}

// PrepareEquipmentSwapCandidate：装备「替换」的背包预演——同时把新装备实例从背包移除
// 并把被换下的装备实例加回背包（两个唯一物品互换在背包中的占用状态）。
EHSRInventoryOperationResult UHSRInventorySubsystem::PrepareEquipmentSwapCandidate(const FGuid& IncomingInstanceId,
	const FName IncomingItemId, const FGuid& DisplacedInstanceId, const FName DisplacedItemId,
	const int64 ExpectedRevision, FHSRInventoryMovementCandidate& OutCandidate) const
{
	if (ExpectedRevision != Revision)
	{
		return EHSRInventoryOperationResult::RevisionConflict;
	}
	const FHSRItemInstance* Incoming = UniqueItems.Find(IncomingInstanceId);
	if (!Incoming)
	{
		return EHSRInventoryOperationResult::InstanceNotFound;
	}
	if (Incoming->DefinitionId != IncomingItemId)
	{
		return EHSRInventoryOperationResult::StorageKindMismatch;
	}
	const FDefinitionRule* DisplacedRule = Definitions.Find(DisplacedItemId);
	if (!DisplacedRule)
	{
		return EHSRInventoryOperationResult::UnknownDefinition;
	}
	if (DisplacedRule->StorageKind != EHSRItemStorageKind::Unique)
	{
		return EHSRInventoryOperationResult::StorageKindMismatch;
	}
	if (UniqueItems.Contains(DisplacedInstanceId))
	{
		return EHSRInventoryOperationResult::DuplicateInstanceId;
	}

	FHSRInventoryMovementCandidate Candidate;
	Candidate.Stacks = Stacks;
	Candidate.UniqueItems = UniqueItems;
	Candidate.UniqueItems.Remove(IncomingInstanceId);
	Candidate.UniqueItems.Add(DisplacedInstanceId, {DisplacedInstanceId, DisplacedItemId});
	if (GetUsedSlots(Candidate.Stacks, Candidate.UniqueItems) > Capacity)
	{
		return EHSRInventoryOperationResult::CapacityExceeded;
	}
	Candidate.NextRevision = Revision + 1;
	OutCandidate = MoveTemp(Candidate);
	return EHSRInventoryOperationResult::Success;
}

// PrepareEquipmentEnhancementCandidate：装备强化消耗材料的背包预演——从堆叠材料里
// 扣除强化成本（MaterialCost）。
EHSRInventoryOperationResult UHSRInventorySubsystem::PrepareEquipmentEnhancementCandidate(
	const FName MaterialItemId, const int32 MaterialCost, const int64 ExpectedRevision,
	FHSRInventoryEnhancementCandidate& OutCandidate) const
{
	if (ExpectedRevision != Revision)
	{
		return EHSRInventoryOperationResult::RevisionConflict;
	}
	if (MaterialItemId.IsNone())
	{
		return EHSRInventoryOperationResult::InvalidDefinitionId;
	}
	if (MaterialCost <= 0)
	{
		return EHSRInventoryOperationResult::InvalidQuantity;
	}
	const FDefinitionRule* Rule = Definitions.Find(MaterialItemId);
	if (!Rule)
	{
		return EHSRInventoryOperationResult::UnknownDefinition;
	}
	if (Rule->StorageKind != EHSRItemStorageKind::Stackable)
	{
		return EHSRInventoryOperationResult::StorageKindMismatch;
	}
	const int32 Existing = Stacks.FindRef(MaterialItemId);
	if (Existing < MaterialCost)
	{
		return EHSRInventoryOperationResult::InsufficientQuantity;
	}

	FHSRInventoryEnhancementCandidate Candidate;
	Candidate.Stacks = Stacks;
	Candidate.UniqueItems = UniqueItems;
	const int32 Remaining = Existing - MaterialCost;
	if (Remaining == 0)
	{
		Candidate.Stacks.Remove(MaterialItemId);
	}
	else
	{
		Candidate.Stacks.Add(MaterialItemId, Remaining);
	}
	Candidate.NextRevision = Revision + 1;
	OutCandidate = MoveTemp(Candidate);
	return EHSRInventoryOperationResult::Success;
}

// 下面三组 Install/Finalize/Publish 是装备移动（movement）与强化（enhancement）事务
// 的提交钩子：先安装候选状态、再定版版本号、最后广播。NoFail 语义表示预演已保证
// 必然成功，这里只需机械执行。

void UHSRInventorySubsystem::InstallEquipmentMovementCandidateNoFail(FHSRInventoryMovementCandidate&& Candidate)
{
	Stacks = MoveTemp(Candidate.Stacks);
	UniqueItems = MoveTemp(Candidate.UniqueItems);
}

void UHSRInventorySubsystem::FinalizeEquipmentMovementRevisionNoFail(const int64 PreparedRevision)
{
	Revision = PreparedRevision;
}

void UHSRInventorySubsystem::PublishEquipmentMovementCommit(const int64 PreparedRevision)
{
	check(Revision == PreparedRevision);
	BroadcastRevision(PreparedRevision);
}

void UHSRInventorySubsystem::InstallEquipmentEnhancementCandidateNoFail(
	FHSRInventoryEnhancementCandidate&& Candidate)
{
	Stacks = MoveTemp(Candidate.Stacks);
	UniqueItems = MoveTemp(Candidate.UniqueItems);
}

void UHSRInventorySubsystem::FinalizeEquipmentEnhancementRevisionNoFail(const int64 PreparedRevision)
{
	Revision = PreparedRevision;
}

void UHSRInventorySubsystem::PublishEquipmentEnhancementCommit(const int64 PreparedRevision)
{
	check(Revision == PreparedRevision);
	BroadcastRevision(PreparedRevision);
}

#if WITH_DEV_AUTOMATION_TESTS
// 自动化测试专用：调整容量（不能小于当前占用）。
bool UHSRInventorySubsystem::SetCapacityForAutomation(int32 NewCapacity)
{
	if (NewCapacity < 1 || GetUsedSlots(Stacks, UniqueItems) > NewCapacity)
	{
		return false;
	}
	Capacity = NewCapacity;
	return true;
}
#endif

// GetUsedSlots：占用槽位 = 堆叠条目数 + 唯一实例数。
int32 UHSRInventorySubsystem::GetUsedSlots(const TMap<FName, int32>& CandidateStacks, const TMap<FGuid, FHSRItemInstance>& CandidateUniqueItems) const
{
	return CandidateStacks.Num() + CandidateUniqueItems.Num();
}

// Commit：通用提交——写入两个表并广播版本号（自增）。
void UHSRInventorySubsystem::Commit(TMap<FName, int32>&& CandidateStacks, TMap<FGuid, FHSRItemInstance>&& CandidateUniqueItems)
{
	Stacks = MoveTemp(CandidateStacks);
	UniqueItems = MoveTemp(CandidateUniqueItems);
	InventoryChanged.Broadcast(++Revision);
}
