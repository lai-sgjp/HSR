#include "HSRPreBattleCandidateViewModel.h"

#include "../Party/HSRPartySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"

// Initialize：为一次"战前编队"初始化本 ViewModel。
//
// 数据来源：UHSRPartySubsystem（当前队伍权威数据）+
//           UHSRCharacterProfileSubsystem（角色档案，用于校验角色是否真实存在）+
//           来自遭遇定义的 FHSREncounterRequest 模板（遭遇/敌人/地图信息）。
// 转换方式：把队伍槽位内的角色 ID 投影为候选列表，后续编辑都在这份"草稿"上进行，
//           最终提交时才写回真实的遭遇请求。
// 广播时机：本类不主动广播；Widget 在每次操作后调用 GetSnapshot() 主动拉取。
void UHSRPreBattleCandidateViewModel::Initialize(UHSRPartySubsystem* InParty,
	UHSRCharacterProfileSubsystem* InProfiles, const FHSREncounterRequest& InTemplate)
{
	Party = InParty;
	Profiles = InProfiles;
	Template = InTemplate;
	CandidateCharacterIds.Reset();
	BuffIds.Reset();
	// 以当前队伍为初始候选：把每个槽位的角色 ID 平铺成候选列表。
	FHSRPartySnapshot PartySnapshot;
	if (Party.IsValid() && Party->GetSnapshot(PartySnapshot))
	{
		for (const FHSRPartySlot& Slot : PartySnapshot.Slots)
		{
			CandidateCharacterIds.Add(Slot.CharacterId);
		}
	}
	RebuildSnapshot();
}

// IsKnownProfile：判断角色 ID 是否对应一个真实存在的角色档案。
bool UHSRPreBattleCandidateViewModel::IsKnownProfile(FName CharacterId) const
{
	FHSRCharacterProfileSnapshot Profile;
	return Profiles.IsValid() && Profiles->GetProfileSnapshot(CharacterId, Profile);
}

// ContainsCandidate：判断某角色是否已出现在候选列表的其它槽位（忽略 IgnoreSlot）。
// 用于防止同一角色被放进多个槽位造成重复。
bool UHSRPreBattleCandidateViewModel::ContainsCandidate(FName CharacterId, int32 IgnoreSlot) const
{
	for (int32 Index = 0; Index < CandidateCharacterIds.Num(); ++Index)
	{
		if (Index != IgnoreSlot && CandidateCharacterIds[Index] == CharacterId && !CharacterId.IsNone())
		{
			return true;
		}
	}
	return false;
}

// SetCandidateSlot：把指定槽位设置为某个角色。校验通过后写入草稿并重建快照。
EHSRPreBattleCandidateResult UHSRPreBattleCandidateViewModel::SetCandidateSlot(int32 SlotIndex, FName CharacterId)
{
	if (!CandidateCharacterIds.IsValidIndex(SlotIndex))
	{
		return EHSRPreBattleCandidateResult::InvalidSlot;
	}
	if (!IsKnownProfile(CharacterId))
	{
		return EHSRPreBattleCandidateResult::ProfileNotFound;
	}
	if (ContainsCandidate(CharacterId, SlotIndex))
	{
		return EHSRPreBattleCandidateResult::DuplicateCharacter;
	}
	CandidateCharacterIds[SlotIndex] = CharacterId;
	RebuildSnapshot();
	return EHSRPreBattleCandidateResult::Success;
}

// SetBuff：为本次出战添加一个增益（Buff）。
// BuffId 不可为空；已在集合中的 Buff 是幂等的（不会重复添加）。
EHSRPreBattleCandidateResult UHSRPreBattleCandidateViewModel::SetBuff(FName BuffId)
{
	if (BuffId.IsNone())
	{
		return EHSRPreBattleCandidateResult::InvalidCandidate;
	}
	if (!BuffIds.Contains(BuffId))
	{
		BuffIds.Add(BuffId);
	}
	RebuildSnapshot();
	return EHSRPreBattleCandidateResult::Success;
}

// ConfirmCandidate：校验并提交候选阵容，输出可直接交给战斗切换子系统的遭遇请求。
// 校验内容：模板必须携带完整的遭遇/敌人/地图信息；队长（槽位 0）不得为空；
// 所有非空槽位必须对应已知角色且不得重复。
EHSRPreBattleCandidateResult UHSRPreBattleCandidateViewModel::ConfirmCandidate(FHSREncounterRequest& OutRequest)
{
	if (Template.EncounterId.IsNone() || Template.EnemyDefinitionId.IsNone() || Template.BattleMapPath.IsNone())
	{
		return EHSRPreBattleCandidateResult::InvalidEncounter;
	}
	if (CandidateCharacterIds.IsEmpty() || CandidateCharacterIds[0].IsNone())
	{
		return EHSRPreBattleCandidateResult::EmptyLeader;
	}
	// 二次校验：空槽位（IsNone）合法，但非空槽位必须通过档案校验且全局唯一。
	TSet<FName> Seen;
	for (const FName CharacterId : CandidateCharacterIds)
	{
		if (!CharacterId.IsNone() && !IsKnownProfile(CharacterId))
		{
			return EHSRPreBattleCandidateResult::ProfileNotFound;
		}
		if (!CharacterId.IsNone() && Seen.Contains(CharacterId))
		{
			return EHSRPreBattleCandidateResult::DuplicateCharacter;
		}
		if (!CharacterId.IsNone())
		{
			Seen.Add(CharacterId);
		}
	}
	OutRequest = Template;
	OutRequest.PlayerCharacterId = CandidateCharacterIds[0];
	// Densify: empty candidate slots are legal in the grid but meaningless as participants,
	// so the committed request carries only filled members, leader first.
	// 压缩：空槽位在网格里合法，但对参战者没有意义，
	// 因此最终提交的请求只携带"已填满"的成员，且队长排在最前。
	OutRequest.PlayerPartyIds.Reset();
	for (const FName CharacterId : CandidateCharacterIds)
	{
		if (!CharacterId.IsNone())
		{
			OutRequest.PlayerPartyIds.Add(CharacterId);
		}
	}
	OutRequest.BuffIds = BuffIds;
	return EHSRPreBattleCandidateResult::Success;
}

// CancelCandidate：放弃当前编辑，把候选阵容重置为队伍子系统的权威快照，并清空 Buff 选择。
EHSRPreBattleCandidateResult UHSRPreBattleCandidateViewModel::CancelCandidate()
{
	if (!Party.IsValid())
	{
		return EHSRPreBattleCandidateResult::InvalidCandidate;
	}
	FHSRPartySnapshot PartySnapshot;
	if (!Party->GetSnapshot(PartySnapshot))
	{
		return EHSRPreBattleCandidateResult::InvalidCandidate;
	}
	CandidateCharacterIds.Reset();
	for (const FHSRPartySlot& Slot : PartySnapshot.Slots)
	{
		CandidateCharacterIds.Add(Slot.CharacterId);
	}
	BuffIds.Reset();
	RebuildSnapshot();
	return EHSRPreBattleCandidateResult::Success;
}

// RebuildSnapshot：根据当前草稿重建对外快照。
// "是否有未提交改动"（bHasPendingChanges）的判定：
//   1) 只要选了任何 Buff 即视为有改动；
//   2) 若候选列表长度与队伍槽位一致，逐槽比较，任一槽位角色与权威队伍不同即视为有改动。
void UHSRPreBattleCandidateViewModel::RebuildSnapshot()
{
	FHSRPartySnapshot PartySnapshot;
	Snapshot = FHSRPreBattleCandidateSnapshot();
	Snapshot.CandidateCharacterIds = CandidateCharacterIds;
	Snapshot.BuffIds = BuffIds;
	Snapshot.EncounterId = Template.EncounterId;
	if (Party.IsValid() && Party->GetSnapshot(PartySnapshot))
	{
		Snapshot.PartyRevision = PartySnapshot.Revision;
	}
	Snapshot.bHasPendingChanges = !BuffIds.IsEmpty();
	if (PartySnapshot.Slots.Num() == CandidateCharacterIds.Num())
	{
		for (int32 Index = 0; Index < CandidateCharacterIds.Num(); ++Index)
		{
			Snapshot.bHasPendingChanges |= PartySnapshot.Slots[Index].CharacterId != CandidateCharacterIds[Index];
		}
	}
}
