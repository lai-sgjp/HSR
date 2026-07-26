#include "HSRInventoryRewardViewModel.h"

#include "../Inventory/HSRInventorySubsystem.h"
#include "../Reward/HSRRewardSubsystem.h"

namespace
{
bool AreGrantsEqual(const FHSRInventoryGrant& A, const FHSRInventoryGrant& B)
{
	return A.ItemId == B.ItemId && A.Quantity == B.Quantity && A.InstanceIds == B.InstanceIds;
}

bool AreReceiptsEqual(const FHSRRewardReceipt& A, const FHSRRewardReceipt& B)
{
	if (A.Request.ClaimId != B.Request.ClaimId || A.Request.RewardDefinitionId != B.Request.RewardDefinitionId
		|| A.Request.Seed != B.Request.Seed || A.Revision != B.Revision || A.Grants.Num() != B.Grants.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < A.Grants.Num(); ++Index)
	{
		if (!AreGrantsEqual(A.Grants[Index], B.Grants[Index])) return false;
	}
	return true;
}

bool AreSnapshotsEqual(const FHSRInventoryRewardSnapshot& A, const FHSRInventoryRewardSnapshot& B)
{
	if (A.Inventory.Revision != B.Inventory.Revision || A.Inventory.Capacity != B.Inventory.Capacity
		|| A.Inventory.UsedSlots != B.Inventory.UsedSlots || A.Inventory.Stacks.Num() != B.Inventory.Stacks.Num()
		|| A.Inventory.UniqueItems.Num() != B.Inventory.UniqueItems.Num() || A.Receipts.Num() != B.Receipts.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < A.Inventory.Stacks.Num(); ++Index)
	{
		const FHSRItemStackSnapshot& Left = A.Inventory.Stacks[Index];
		const FHSRItemStackSnapshot& Right = B.Inventory.Stacks[Index];
		if (Left.ItemId != Right.ItemId || Left.Quantity != Right.Quantity) return false;
	}
	for (int32 Index = 0; Index < A.Inventory.UniqueItems.Num(); ++Index)
	{
		const FHSRItemInstance& Left = A.Inventory.UniqueItems[Index];
		const FHSRItemInstance& Right = B.Inventory.UniqueItems[Index];
		if (Left.InstanceId != Right.InstanceId || Left.DefinitionId != Right.DefinitionId) return false;
	}
	for (int32 Index = 0; Index < A.Receipts.Num(); ++Index)
	{
		if (!AreReceiptsEqual(A.Receipts[Index], B.Receipts[Index])) return false;
	}
	return true;
}
}

void UHSRInventoryRewardViewModel::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

void UHSRInventoryRewardViewModel::Initialize(UHSRInventorySubsystem* InInventory, UHSRRewardSubsystem* InReward)
{
	Shutdown();
	Inventory = InInventory;
	Reward = InReward;
	if (!InInventory || !InReward)
	{
		return;
	}
	InventoryHandle = InInventory->OnInventoryChanged().AddUObject(this, &ThisClass::HandleInventoryChanged);
	RewardCommittedHandle = InReward->OnRewardCommitted().AddUObject(this, &ThisClass::HandleRewardCommitted);
	RewardRestoredHandle = InReward->OnRewardRestored().AddUObject(this, &ThisClass::HandleRewardRestored);
	Rebuild();
}

void UHSRInventoryRewardViewModel::Shutdown()
{
	if (Inventory.IsValid()) Inventory->OnInventoryChanged().Remove(InventoryHandle);
	if (Reward.IsValid())
	{
		Reward->OnRewardCommitted().Remove(RewardCommittedHandle);
		Reward->OnRewardRestored().Remove(RewardRestoredHandle);
	}
	Inventory.Reset();
	Reward.Reset();
	bHasSnapshot = false;
	Snapshot = FHSRInventoryRewardSnapshot();
}

bool UHSRInventoryRewardViewModel::GetSnapshot(FHSRInventoryRewardSnapshot& OutSnapshot) const
{
	if (!bHasSnapshot) return false;
	OutSnapshot = Snapshot;
	return true;
}

void UHSRInventoryRewardViewModel::HandleInventoryChanged(int64) { Rebuild(); }
void UHSRInventoryRewardViewModel::HandleRewardCommitted(const FHSRRewardReceipt&) { Rebuild(); }
void UHSRInventoryRewardViewModel::HandleRewardRestored(int64) { Rebuild(); }

void UHSRInventoryRewardViewModel::Rebuild()
{
	if (!Inventory.IsValid() || !Reward.IsValid()) return;
	FHSRInventoryRewardSnapshot Candidate;
	Inventory->GetSnapshot(Candidate.Inventory);
	Reward->GetReceipts(Candidate.Receipts);
	if (bHasSnapshot && AreSnapshotsEqual(Snapshot, Candidate)) return;
	Snapshot = MoveTemp(Candidate);
	bHasSnapshot = true;
	Changed.Broadcast(Snapshot);
	OnSnapshotChanged.Broadcast(Snapshot);
}
