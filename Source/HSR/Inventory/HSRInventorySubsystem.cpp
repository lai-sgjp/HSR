#include "HSRInventorySubsystem.h"

#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Reward/HSRSettlementTypes.h"

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

EHSRInventoryOperationResult UHSRInventorySubsystem::ApplyGrants(const TArray<FHSRInventoryGrant>& Grants)
{
	int64 CommittedRevision = Revision;
	return ApplyGrantsInternal(Grants, true, CommittedRevision);
}

EHSRInventoryOperationResult UHSRInventorySubsystem::ApplyGrantsInternal(const TArray<FHSRInventoryGrant>& Grants, bool bBroadcast, int64& OutRevision)
{
	if (Grants.IsEmpty())
	{
		return EHSRInventoryOperationResult::NoOp;
	}

	TMap<FName, int32> CandidateStacks = Stacks;
	TMap<FGuid, FHSRItemInstance> CandidateUniqueItems = UniqueItems;
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
		}
		else
		{
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
	}

	if (GetUsedSlots(CandidateStacks, CandidateUniqueItems) > Capacity)
	{
		return EHSRInventoryOperationResult::CapacityExceeded;
	}
	Stacks = MoveTemp(CandidateStacks);
	UniqueItems = MoveTemp(CandidateUniqueItems);
	OutRevision = ++Revision;
	if (bBroadcast)
	{
		BroadcastRevision(OutRevision);
	}
	return EHSRInventoryOperationResult::Success;
}

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

void UHSRInventorySubsystem::ExportSaveData(FHSRInventorySaveData& OutData) const
{
	FHSRInventorySnapshot Snapshot;
	GetSnapshot(Snapshot);
	OutData.Stacks = MoveTemp(Snapshot.Stacks);
	OutData.UniqueItems = MoveTemp(Snapshot.UniqueItems);
	OutData.Revision = Revision;
}

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
		if (!Rule || Rule->StorageKind != EHSRItemStorageKind::Stackable || Stack.Quantity <= 0 || Stack.Quantity > Rule->MaxStack || Candidate.Stacks.Contains(Stack.ItemId))
		{
			return false;
		}
		Candidate.Stacks.Add(Stack.ItemId, Stack.Quantity);
	}
	for (const FHSRItemInstance& Instance : Data.UniqueItems)
	{
		const FDefinitionRule* Rule = Definitions.Find(Instance.DefinitionId);
		if (!Instance.InstanceId.IsValid() || !Rule || Rule->StorageKind != EHSRItemStorageKind::Unique || Candidate.UniqueItems.Contains(Instance.InstanceId))
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

bool UHSRInventorySubsystem::IsRestoreDifferent(const FHSRInventoryRestoreState& Candidate) const
{
	if (Revision != Candidate.Revision || Stacks.Num() != Candidate.Stacks.Num() || UniqueItems.Num() != Candidate.UniqueItems.Num())
	{
		return true;
	}
	for (const TPair<FName, int32>& Entry : Stacks)
	{
		if (Candidate.Stacks.FindRef(Entry.Key) != Entry.Value) return true;
	}
	for (const TPair<FGuid, FHSRItemInstance>& Entry : UniqueItems)
	{
		const FHSRItemInstance* Other = Candidate.UniqueItems.Find(Entry.Key);
		if (!Other || Other->DefinitionId != Entry.Value.DefinitionId) return true;
	}
	return false;
}

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
	for (const FHSRInventoryGrant& Grant : Grants)
	{
		if (Grant.ItemId.IsNone() || Grant.Quantity <= 0)
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
			if (!Grant.InstanceIds.IsEmpty()) return EHSRInventoryOperationResult::StorageKindMismatch;
			const int32 Existing = Candidate.Stacks.FindRef(Grant.ItemId);
			if (Grant.Quantity > MAX_int32 - Existing) return EHSRInventoryOperationResult::QuantityOverflow;
			const int32 NewQuantity = Existing + Grant.Quantity;
			if (NewQuantity > Rule->MaxStack) return EHSRInventoryOperationResult::StackLimitExceeded;
			Candidate.Stacks.Add(Grant.ItemId, NewQuantity);
		}
		else
		{
			if (Grant.InstanceIds.Num() != Grant.Quantity) return EHSRInventoryOperationResult::StorageKindMismatch;
			for (const FGuid& InstanceId : Grant.InstanceIds)
			{
				if (!InstanceId.IsValid()) return EHSRInventoryOperationResult::InvalidInstanceId;
				if (Candidate.UniqueItems.Contains(InstanceId)) return EHSRInventoryOperationResult::DuplicateInstanceId;
				Candidate.UniqueItems.Add(InstanceId, {InstanceId, Grant.ItemId});
			}
		}
	}
	if (GetUsedSlots(Candidate.Stacks, Candidate.UniqueItems) > Capacity)
	{
		return EHSRInventoryOperationResult::CapacityExceeded;
	}
	Candidate.NextRevision = Revision + 1;
	OutCandidate = MoveTemp(Candidate);
	return EHSRInventoryOperationResult::Success;
}

void UHSRInventorySubsystem::InstallSettlementCandidateNoFail(FHSRInventorySettlementCandidate&& Candidate)
{
	Stacks = MoveTemp(Candidate.Stacks);
	UniqueItems = MoveTemp(Candidate.UniqueItems);
}

void UHSRInventorySubsystem::FinalizeSettlementRevisionNoFail(int64 PreparedRevision)
{
	Revision = PreparedRevision;
}

void UHSRInventorySubsystem::PublishSettlementCommit(int64 PreparedRevision)
{
	BroadcastRevision(Revision);
}

EHSRInventoryOperationResult UHSRInventorySubsystem::PrepareEquipmentRemovalCandidate(const FGuid& InstanceId,
	const FName ExpectedItemId, const int64 ExpectedRevision, FHSRInventoryMovementCandidate& OutCandidate) const
{
	if (ExpectedRevision != Revision) return EHSRInventoryOperationResult::RevisionConflict;
	const FHSRItemInstance* Existing = UniqueItems.Find(InstanceId);
	if (!Existing) return EHSRInventoryOperationResult::InstanceNotFound;
	if (Existing->DefinitionId != ExpectedItemId) return EHSRInventoryOperationResult::StorageKindMismatch;
	FHSRInventoryMovementCandidate Candidate;
	Candidate.Stacks = Stacks;
	Candidate.UniqueItems = UniqueItems;
	Candidate.UniqueItems.Remove(InstanceId);
	Candidate.NextRevision = Revision + 1;
	OutCandidate = MoveTemp(Candidate);
	return EHSRInventoryOperationResult::Success;
}

EHSRInventoryOperationResult UHSRInventorySubsystem::PrepareEquipmentAdditionCandidate(const FGuid& InstanceId,
	const FName ItemId, const int64 ExpectedRevision, FHSRInventoryMovementCandidate& OutCandidate) const
{
	if (ExpectedRevision != Revision) return EHSRInventoryOperationResult::RevisionConflict;
	const FDefinitionRule* Rule = Definitions.Find(ItemId);
	if (!Rule) return EHSRInventoryOperationResult::UnknownDefinition;
	if (Rule->StorageKind != EHSRItemStorageKind::Unique) return EHSRInventoryOperationResult::StorageKindMismatch;
	if (UniqueItems.Contains(InstanceId)) return EHSRInventoryOperationResult::DuplicateInstanceId;
	FHSRInventoryMovementCandidate Candidate;
	Candidate.Stacks = Stacks;
	Candidate.UniqueItems = UniqueItems;
	Candidate.UniqueItems.Add(InstanceId, {InstanceId, ItemId});
	if (GetUsedSlots(Candidate.Stacks, Candidate.UniqueItems) > Capacity) return EHSRInventoryOperationResult::CapacityExceeded;
	Candidate.NextRevision = Revision + 1;
	OutCandidate = MoveTemp(Candidate);
	return EHSRInventoryOperationResult::Success;
}

EHSRInventoryOperationResult UHSRInventorySubsystem::PrepareEquipmentSwapCandidate(const FGuid& IncomingInstanceId,
	const FName IncomingItemId, const FGuid& DisplacedInstanceId, const FName DisplacedItemId,
	const int64 ExpectedRevision, FHSRInventoryMovementCandidate& OutCandidate) const
{
	if (ExpectedRevision != Revision) return EHSRInventoryOperationResult::RevisionConflict;
	const FHSRItemInstance* Incoming = UniqueItems.Find(IncomingInstanceId);
	if (!Incoming) return EHSRInventoryOperationResult::InstanceNotFound;
	if (Incoming->DefinitionId != IncomingItemId) return EHSRInventoryOperationResult::StorageKindMismatch;
	const FDefinitionRule* DisplacedRule = Definitions.Find(DisplacedItemId);
	if (!DisplacedRule) return EHSRInventoryOperationResult::UnknownDefinition;
	if (DisplacedRule->StorageKind != EHSRItemStorageKind::Unique) return EHSRInventoryOperationResult::StorageKindMismatch;
	if (UniqueItems.Contains(DisplacedInstanceId)) return EHSRInventoryOperationResult::DuplicateInstanceId;
	FHSRInventoryMovementCandidate Candidate;
	Candidate.Stacks = Stacks;
	Candidate.UniqueItems = UniqueItems;
	Candidate.UniqueItems.Remove(IncomingInstanceId);
	Candidate.UniqueItems.Add(DisplacedInstanceId, {DisplacedInstanceId, DisplacedItemId});
	if (GetUsedSlots(Candidate.Stacks, Candidate.UniqueItems) > Capacity) return EHSRInventoryOperationResult::CapacityExceeded;
	Candidate.NextRevision = Revision + 1;
	OutCandidate = MoveTemp(Candidate);
	return EHSRInventoryOperationResult::Success;
}

EHSRInventoryOperationResult UHSRInventorySubsystem::PrepareEquipmentEnhancementCandidate(
	const FName MaterialItemId, const int32 MaterialCost, const int64 ExpectedRevision,
	FHSRInventoryEnhancementCandidate& OutCandidate) const
{
	if (ExpectedRevision != Revision) return EHSRInventoryOperationResult::RevisionConflict;
	if (MaterialItemId.IsNone()) return EHSRInventoryOperationResult::InvalidDefinitionId;
	if (MaterialCost <= 0) return EHSRInventoryOperationResult::InvalidQuantity;
	const FDefinitionRule* Rule = Definitions.Find(MaterialItemId);
	if (!Rule) return EHSRInventoryOperationResult::UnknownDefinition;
	if (Rule->StorageKind != EHSRItemStorageKind::Stackable) return EHSRInventoryOperationResult::StorageKindMismatch;
	const int32 Existing = Stacks.FindRef(MaterialItemId);
	if (Existing < MaterialCost) return EHSRInventoryOperationResult::InsufficientQuantity;
	FHSRInventoryEnhancementCandidate Candidate;
	Candidate.Stacks = Stacks;
	Candidate.UniqueItems = UniqueItems;
	const int32 Remaining = Existing - MaterialCost;
	if (Remaining == 0) Candidate.Stacks.Remove(MaterialItemId);
	else Candidate.Stacks.Add(MaterialItemId, Remaining);
	Candidate.NextRevision = Revision + 1;
	OutCandidate = MoveTemp(Candidate);
	return EHSRInventoryOperationResult::Success;
}

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

int32 UHSRInventorySubsystem::GetUsedSlots(const TMap<FName, int32>& CandidateStacks, const TMap<FGuid, FHSRItemInstance>& CandidateUniqueItems) const
{
	return CandidateStacks.Num() + CandidateUniqueItems.Num();
}

void UHSRInventorySubsystem::Commit(TMap<FName, int32>&& CandidateStacks, TMap<FGuid, FHSRItemInstance>&& CandidateUniqueItems)
{
	Stacks = MoveTemp(CandidateStacks);
	UniqueItems = MoveTemp(CandidateUniqueItems);
	InventoryChanged.Broadcast(++Revision);
}
