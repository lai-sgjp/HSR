#include "HSRInventoryRewardViewModel.h"

#include "../Inventory/HSRInventorySubsystem.h"
#include "../Reward/HSRRewardSubsystem.h"

namespace
{
// 判断两笔背包发放（InventoryGrant）是否相等：物品 ID、数量、实例列表都相同才算相等。
// 用于快照对比——只有当数据真的变化时才值得广播。
bool AreGrantsEqual(const FHSRInventoryGrant& A, const FHSRInventoryGrant& B)
{
	return A.ItemId == B.ItemId && A.Quantity == B.Quantity && A.InstanceIds == B.InstanceIds;
}

// 判断两笔奖励凭证（RewardReceipt）是否相等：请求头、修订号与逐项发放都相等。
bool AreReceiptsEqual(const FHSRRewardReceipt& A, const FHSRRewardReceipt& B)
{
	// 请求的领取 ID/奖励定义 ID/随机种子、凭证修订号、发放数量任一项不同即不等。
	if (A.Request.ClaimId != B.Request.ClaimId || A.Request.RewardDefinitionId != B.Request.RewardDefinitionId
		|| A.Request.Seed != B.Request.Seed || A.Revision != B.Revision || A.Grants.Num() != B.Grants.Num())
	{
		return false;
	}
	// 逐项比较发放列表。
	for (int32 Index = 0; Index < A.Grants.Num(); ++Index)
	{
		if (!AreGrantsEqual(A.Grants[Index], B.Grants[Index]))
		{
			return false;
		}
	}
	return true;
}

// 判断两个“背包 + 奖励”组合快照是否相等：背包修订号/容量/占用槽数、
// 堆叠列表、唯一物品列表、凭证列表全部一致才算相等。
bool AreSnapshotsEqual(const FHSRInventoryRewardSnapshot& A, const FHSRInventoryRewardSnapshot& B)
{
	if (A.Inventory.Revision != B.Inventory.Revision || A.Inventory.Capacity != B.Inventory.Capacity
		|| A.Inventory.UsedSlots != B.Inventory.UsedSlots || A.Inventory.Stacks.Num() != B.Inventory.Stacks.Num()
		|| A.Inventory.UniqueItems.Num() != B.Inventory.UniqueItems.Num() || A.Receipts.Num() != B.Receipts.Num())
	{
		return false;
	}
	// 逐项比较堆叠（物品 ID 与数量）。
	for (int32 Index = 0; Index < A.Inventory.Stacks.Num(); ++Index)
	{
		const FHSRItemStackSnapshot& Left = A.Inventory.Stacks[Index];
		const FHSRItemStackSnapshot& Right = B.Inventory.Stacks[Index];
		if (Left.ItemId != Right.ItemId || Left.Quantity != Right.Quantity)
		{
			return false;
		}
	}
	// 逐项比较唯一物品（实例 ID 与定义 ID）。
	for (int32 Index = 0; Index < A.Inventory.UniqueItems.Num(); ++Index)
	{
		const FHSRItemInstance& Left = A.Inventory.UniqueItems[Index];
		const FHSRItemInstance& Right = B.Inventory.UniqueItems[Index];
		if (Left.InstanceId != Right.InstanceId || Left.DefinitionId != Right.DefinitionId)
		{
			return false;
		}
	}
	// 逐项比较奖励凭证。
	for (int32 Index = 0; Index < A.Receipts.Num(); ++Index)
	{
		if (!AreReceiptsEqual(A.Receipts[Index], B.Receipts[Index]))
		{
			return false;
		}
	}
	return true;
}
}

// 销毁前先关闭（解绑子系统事件），避免子系统在 VM 销毁后再回调它。
void UHSRInventoryRewardViewModel::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

// 初始化：本 VM 聚合两个数据源——背包子系统（物品/堆叠）与奖励子系统（奖励凭证）。
// 订阅两者的变化事件，任一数据变化都会触发 Rebuild 重建快照并广播。
void UHSRInventoryRewardViewModel::Initialize(UHSRInventorySubsystem* InInventory, UHSRRewardSubsystem* InReward)
{
	// 重新初始化前先清理旧的订阅。
	Shutdown();
	Inventory = InInventory;
	Reward = InReward;
	// 两个子系统缺一不可；缺任一都保持未就绪状态。
	if (!InInventory || !InReward)
	{
		return;
	}
	// 订阅背包变化、奖励发放、奖励恢复三类事件。
	InventoryHandle = InInventory->OnInventoryChanged().AddUObject(this, &ThisClass::HandleInventoryChanged);
	RewardCommittedHandle = InReward->OnRewardCommitted().AddUObject(this, &ThisClass::HandleRewardCommitted);
	RewardRestoredHandle = InReward->OnRewardRestored().AddUObject(this, &ThisClass::HandleRewardRestored);
	// 立即构建一次初始快照。
	Rebuild();
}

// 关闭：移除所有事件订阅、清空子系统引用与快照状态。
void UHSRInventoryRewardViewModel::Shutdown()
{
	if (Inventory.IsValid())
	{
		Inventory->OnInventoryChanged().Remove(InventoryHandle);
	}
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

// 取快照：未就绪时返回 false，就绪时把快照副本写入出参。
bool UHSRInventoryRewardViewModel::GetSnapshot(FHSRInventoryRewardSnapshot& OutSnapshot) const
{
	if (!bHasSnapshot)
	{
		return false;
	}
	OutSnapshot = Snapshot;
	return true;
}

// 三类子系统事件的统一处理：无论背包变化还是奖励发放/恢复，都只需重建快照并广播。
void UHSRInventoryRewardViewModel::HandleInventoryChanged(int64)
{
	Rebuild();
}

void UHSRInventoryRewardViewModel::HandleRewardCommitted(const FHSRRewardReceipt&)
{
	Rebuild();
}

void UHSRInventoryRewardViewModel::HandleRewardRestored(int64)
{
	Rebuild();
}

// 重建快照：从两个子系统拉取最新数据，与旧快照对比，只有确实变化才广播。
void UHSRInventoryRewardViewModel::Rebuild()
{
	if (!Inventory.IsValid() || !Reward.IsValid())
	{
		return;
	}
	FHSRInventoryRewardSnapshot Candidate;
	Inventory->GetSnapshot(Candidate.Inventory);
	Reward->GetReceipts(Candidate.Receipts);
	// 数据未变则跳过广播，避免 Widget 被无意义地反复刷新。
	if (bHasSnapshot && AreSnapshotsEqual(Snapshot, Candidate))
	{
		return;
	}
	// 有变化：移动新快照入位，并同时广播两个事件（OnChanged 供通用订阅，
	// OnSnapshotChanged 供特定订阅方，两者当前都指向同一套数据）。
	Snapshot = MoveTemp(Candidate);
	bHasSnapshot = true;
	Changed.Broadcast(Snapshot);
	OnSnapshotChanged.Broadcast(Snapshot);
}
