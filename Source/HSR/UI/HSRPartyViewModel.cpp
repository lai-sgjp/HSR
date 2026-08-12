#include "HSRPartyViewModel.h"

#include "../Party/HSRPartySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"

// BeginDestroy：ViewModel 销毁前先解绑队伍子系统监听，避免回调悬挂。
void UHSRPartyViewModel::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

// Initialize：注入队伍子系统与档案子系统并订阅队伍变化事件。
//
// 数据来源：UHSRPartySubsystem（队伍权威快照，含各槽位角色与版本号）+
//           UHSRCharacterProfileSubsystem（角色档案全集，用于生成可选的可用角色列表）。
// 转换方式：把权威队伍快照投影为一份可编辑的候选草稿（Candidate），
//           并把"可用角色 ID"、"每个槽位的视图数据（是否被占用）"整理进
//           FHSRPartyFrontendSnapshot 供 UI 显示。
// 广播时机：Rebuild() 末尾统一广播 Changed 与 OnSnapshotChanged。
void UHSRPartyViewModel::Initialize(UHSRPartySubsystem* InParty, UHSRCharacterProfileSubsystem* InProfiles)
{
	Shutdown();
	Party = InParty;
	Profiles = InProfiles;
	if (InParty)
	{
		PartyChangedHandle = InParty->OnPartyChanged().AddUObject(this, &ThisClass::HandlePartyChanged);
	}
	ResetCandidateFromAuthority();
}

// Shutdown：解绑监听并复位全部状态。与 Initialize 成对，可重复安全调用。
void UHSRPartyViewModel::Shutdown()
{
	if (Party.IsValid())
	{
		Party->OnPartyChanged().Remove(PartyChangedHandle);
	}
	Party.Reset();
	Profiles.Reset();
	PartyChangedHandle.Reset();
	Candidate = FHSRPartySnapshot();
	bHasPendingChanges = false;
	Snapshot = FHSRPartyFrontendSnapshot();
}

// GetSnapshot：输出当前前端快照。本实现恒定返回 true（快照始终有效）。
bool UHSRPartyViewModel::GetSnapshot(FHSRPartyFrontendSnapshot& OutSnapshot) const
{
	OutSnapshot = Snapshot;
	return true;
}

// HandlePartyChanged：队伍权威数据变化时的回调——直接重建前端快照。
void UHSRPartyViewModel::HandlePartyChanged(int64)
{
	Rebuild();
}

// IsValidCandidateSlot：槽位下标是否落在候选列表范围内。
bool UHSRPartyViewModel::IsValidCandidateSlot(int32 SlotIndex) const
{
	return SlotIndex >= 0 && SlotIndex < Candidate.Slots.Num();
}

// IsAvailableCharacter：该角色是否真实存在且可被放入队伍（必须有档案）。
bool UHSRPartyViewModel::IsAvailableCharacter(FName CharacterId) const
{
	FHSRCharacterProfileSnapshot Ignored;
	return !CharacterId.IsNone() && Profiles.IsValid() && Profiles->GetProfileSnapshot(CharacterId, Ignored);
}

// CandidateContains：候选列表其它槽位是否已包含该角色（排除 IgnoreSlot）。
bool UHSRPartyViewModel::CandidateContains(FName CharacterId, int32 IgnoreSlot) const
{
	for (int32 Index = 0; Index < Candidate.Slots.Num(); ++Index)
	{
		if (Index != IgnoreSlot && Candidate.Slots[Index].CharacterId == CharacterId)
		{
			return true;
		}
	}
	return false;
}

// SetCandidateSlot：把某角色放入候选槽位（仅改草稿，不触碰队伍权威数据）。
EHSRPartyResult UHSRPartyViewModel::SetCandidateSlot(int32 SlotIndex, FName CharacterId)
{
	if (!IsValidCandidateSlot(SlotIndex))
	{
		return EHSRPartyResult::InvalidSlot;
	}
	if (!IsAvailableCharacter(CharacterId))
	{
		return EHSRPartyResult::ProfileNotFound;
	}
	if (CandidateContains(CharacterId, SlotIndex))
	{
		return EHSRPartyResult::DuplicateCharacter;
	}
	Candidate.Slots[SlotIndex].CharacterId = CharacterId;
	bHasPendingChanges = true;
	PublishCandidate();
	return EHSRPartyResult::Success;
}

// ClearCandidateSlot：清空候选槽位。空槽位清空是无操作（返回 EmptySlot）。
EHSRPartyResult UHSRPartyViewModel::ClearCandidateSlot(int32 SlotIndex)
{
	if (!IsValidCandidateSlot(SlotIndex))
	{
		return EHSRPartyResult::InvalidSlot;
	}
	if (Candidate.Slots[SlotIndex].IsEmpty())
	{
		return EHSRPartyResult::EmptySlot;
	}
	Candidate.Slots[SlotIndex] = FHSRPartySlot();
	bHasPendingChanges = true;
	PublishCandidate();
	return EHSRPartyResult::Success;
}

// SwapCandidateSlots：交换两个候选槽位的角色。
EHSRPartyResult UHSRPartyViewModel::SwapCandidateSlots(int32 FirstSlot, int32 SecondSlot)
{
	if (!IsValidCandidateSlot(FirstSlot) || !IsValidCandidateSlot(SecondSlot))
	{
		return EHSRPartyResult::InvalidSlot;
	}
	if (FirstSlot == SecondSlot)
	{
		return EHSRPartyResult::InvalidCandidate;
	}
	Candidate.Slots.Swap(FirstSlot, SecondSlot);
	bHasPendingChanges = true;
	PublishCandidate();
	return EHSRPartyResult::Success;
}

// ConfirmCandidate：把编辑好的候选草稿提交给队伍子系统（写回权威数据）。
// 提交成功后立即从权威数据重新拉取，清除"未提交改动"标记。
EHSRPartyResult UHSRPartyViewModel::ConfirmCandidate()
{
	if (!Party.IsValid())
	{
		return EHSRPartyResult::InvalidCandidate;
	}
	const EHSRPartyResult Result = Party->CommitCandidate(Candidate);
	if (Result == EHSRPartyResult::Success)
	{
		ResetCandidateFromAuthority();
	}
	return Result;
}

// CancelCandidate：放弃编辑，把草稿重置回队伍权威数据。
EHSRPartyResult UHSRPartyViewModel::CancelCandidate()
{
	if (!Party.IsValid())
	{
		return EHSRPartyResult::InvalidCandidate;
	}
	ResetCandidateFromAuthority();
	return EHSRPartyResult::Success;
}

// ResetCandidateFromAuthority：清除改动标记，用队伍子系统的最新快照覆盖候选草稿。
void UHSRPartyViewModel::ResetCandidateFromAuthority()
{
	bHasPendingChanges = false;
	Candidate = FHSRPartySnapshot();
	if (Party.IsValid())
	{
		Party->GetSnapshot(Candidate);
	}
	Rebuild();
}

// PublishCandidate：任何候选编辑后的统一出口——重建快照并广播。
void UHSRPartyViewModel::PublishCandidate()
{
	Rebuild();
}

// Rebuild：把草稿 + 档案数据整理成前端快照并广播。
// 关键逻辑：
//   - 队伍子系统不可用时快照标记为 Unavailable；
//   - 无未提交改动时草稿以权威快照为准（保证不落后）；
//   - 遍历档案生成"可用角色"列表供选择器使用；
//   - 每个槽位生成视图数据（是否被占用），并据此推导整体状态 Ready/Empty。
void UHSRPartyViewModel::Rebuild()
{
	FHSRPartyFrontendSnapshot Next;
	FHSRPartySnapshot AuthoritySnapshot;
	if (!Party.IsValid() || !Party->GetSnapshot(AuthoritySnapshot))
	{
		Next.Status = EHSRPartyFrontendStatus::Unavailable;
	}
	else
	{
		bool bAnyOccupied = false;
		if (!bHasPendingChanges)
		{
			Candidate = AuthoritySnapshot;
		}
		Next.Revision = Candidate.Revision;
		Next.ActiveSlot = Candidate.ActiveSlot;
		Next.bHasPendingChanges = bHasPendingChanges;
		TArray<FHSRCharacterProfileSnapshot> ProfileSnapshots;
		if (Profiles.IsValid() && Profiles->GetAllProfileSnapshots(ProfileSnapshots))
		{
			for (const FHSRCharacterProfileSnapshot& Profile : ProfileSnapshots)
			{
				Next.AvailableCharacterIds.Add(Profile.RuntimeState.CharacterId);
			}
		}
		for (int32 Index = 0; Index < Candidate.Slots.Num(); ++Index)
		{
			FHSRPartySlotViewData& View = Next.Slots.AddDefaulted_GetRef();
			View.SlotIndex = Index;
			View.CharacterId = Candidate.Slots[Index].CharacterId;
			View.bOccupied = !Candidate.Slots[Index].IsEmpty();
			bAnyOccupied |= View.bOccupied;
		}
		Next.Status = bAnyOccupied ? EHSRPartyFrontendStatus::Ready : EHSRPartyFrontendStatus::Empty;
	}
	Snapshot = MoveTemp(Next);
	Changed.Broadcast(Snapshot);
	OnSnapshotChanged.Broadcast(Snapshot);
}
