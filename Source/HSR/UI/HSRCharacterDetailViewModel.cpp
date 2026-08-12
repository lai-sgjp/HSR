#include "HSRCharacterDetailViewModel.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Save/HSRSaveSubsystem.h"
#include "../Party/HSRPartySubsystem.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Equipment/HSREquipmentStatAggregator.h"
#include "../Equipment/HSREquipmentTypes.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "Curves/CurveFloat.h"

// Initialize：注入四个数据源并订阅它们的变化事件。
//
// 数据来源：UHSRCharacterProfileSubsystem（角色档案/等级/经验/天赋）+
//           UHSRSaveSubsystem（存档恢复事件）+
//           UHSRPartySubsystem（队伍，用于默认选中队长）+
//           UHSREquipmentSubsystem（装备/遗器，用于把装备加成并入派生属性）。
// 转换方式：见 BuildSnapshot —— 把档案、进度、装备聚合统一整理成
//           FHSRCharacterDetailSnapshot（纯值 DTO）供 UI 显示。
// 广播时机：SelectCharacter / RefreshSelected 等入口在快照更新后调用
//           BroadcastSnapshot()，统一广播 Changed 与 OnSnapshotChanged。
void UHSRCharacterDetailViewModel::Initialize(UHSRCharacterProfileSubsystem* P, UHSRSaveSubsystem* S,
	UHSRPartySubsystem* T, UHSREquipmentSubsystem* E)
{
	Uninitialize();
	Profiles = P;
	Save = S;
	Party = T;
	Equipment = E;
	if (P)
	{
		ProfileHandle = P->OnProfileChanged().AddUObject(this, &UHSRCharacterDetailViewModel::HandleProfile);
	}
	if (S)
	{
		RestoreHandle = S->OnRestoreCommitted().AddUObject(this, &UHSRCharacterDetailViewModel::HandleRestore);
	}
	if (T)
	{
		PartyHandle = T->OnPartyChanged().AddUObject(this, &UHSRCharacterDetailViewModel::HandleParty);
	}
	if (E)
	{
		EquipmentHandle = E->OnLoadoutChanged().AddUObject(this, &UHSRCharacterDetailViewModel::HandleEquipmentLoadout);
	}
}

// Uninitialize：解绑全部订阅并复位数据源引用。与 Initialize 对称，可重复安全调用。
void UHSRCharacterDetailViewModel::Uninitialize()
{
	if (Profiles.IsValid() && ProfileHandle.IsValid())
	{
		Profiles->OnProfileChanged().Remove(ProfileHandle);
	}
	if (Save.IsValid() && RestoreHandle.IsValid())
	{
		Save->OnRestoreCommitted().Remove(RestoreHandle);
	}
	if (Party.IsValid() && PartyHandle.IsValid())
	{
		Party->OnPartyChanged().Remove(PartyHandle);
	}
	if (Equipment.IsValid() && EquipmentHandle.IsValid())
	{
		Equipment->OnLoadoutChanged().Remove(EquipmentHandle);
	}
	ProfileHandle.Reset();
	RestoreHandle.Reset();
	PartyHandle.Reset();
	EquipmentHandle.Reset();
	Profiles.Reset();
	Save.Reset();
	Party.Reset();
	Equipment.Reset();
}

// BuildSnapshot：为指定角色构建一份完整详情快照。这是本 ViewModel 的核心"转换"逻辑。
// 依次完成：基础校验 -> 拉取档案/定义/进度 -> 基础字段 -> 叠加装备加成 ->
// 头像路径 -> 技能等级列表。任何一步缺失都返回对应的错误码。
EHSRCharacterDetailResult UHSRCharacterDetailViewModel::BuildSnapshot(FName Id, FHSRCharacterDetailSnapshot& Out) const
{
	if (!Profiles.IsValid())
	{
		return EHSRCharacterDetailResult::NotInitialized;
	}
	if (Id.IsNone())
	{
		return EHSRCharacterDetailResult::InvalidCharacterId;
	}
	FHSRCharacterProfileSnapshot P;
	if (!Profiles->GetProfileSnapshot(Id, P))
	{
		return EHSRCharacterDetailResult::ProfileNotFound;
	}
	const UHSRCharacterDefinition* D = nullptr;
	if (!Profiles->GetDefinition(Id, D) || !D)
	{
		return EHSRCharacterDetailResult::DefinitionNotFound;
	}
	FHSRCharacterProgressionContext C;
	if (!Profiles->GetProgressionContext(Id, C))
	{
		return EHSRCharacterDetailResult::InvalidSnapshot;
	}
	const UCurveFloat* Curve = D->CumulativeExperienceCurve.LoadSynchronous();
	if (!Curve)
	{
		return EHSRCharacterDetailResult::InvalidSnapshot;
	}

	FHSRCharacterDetailSnapshot N;
	N.CharacterId = Id;
	N.DisplayName = D->DisplayName;
	N.Level = P.RuntimeState.Level;
	N.MaxLevel = D->MaxLevel;
	N.Experience = P.RuntimeState.Experience;
	// 1 级没有"当前等级所需经验"；其余等级取累计经验曲线在当前等级的取值（向上取整）。
	N.ExperienceForCurrentLevel = N.Level <= 1 ? 0 : FMath::RoundToInt(Curve->GetFloatValue(N.Level));
	N.bAtMaxLevel = N.Level >= N.MaxLevel;
	// 满级时"下一级所需经验"与当前级一致（无实际意义，仅保持字段有效）。
	N.ExperienceForNextLevel = N.bAtMaxLevel ? N.ExperienceForCurrentLevel : FMath::RoundToInt(Curve->GetFloatValue(N.Level + 1));
	N.Ascension = P.RuntimeState.Ascension;
	N.RuntimeRevision = P.RuntimeRevision;
	N.BaseStats.MaxHealth = D->BaseMaxHealth;
	N.BaseStats.Attack = D->BaseAttack;
	N.BaseStats.Defense = D->BaseDefense;
	N.BaseStats.Speed = D->BaseSpeed;
	N.DerivedStats = C.DerivedStats;
	N.ProgressionBonuses = C.ProgressionBonuses;

	// Aggregate equipped weapon/relic stat bonuses on top of the derived (base + progression) values,
	// so the character detail screen reflects the loadout the same way battle does.
	// 在"基础 + 成长"派生值之上再叠加已装备武器/遗器的属性加成，
	// 使详情界面与战斗使用同一套"含装备"的属性口径。
	if (Equipment.IsValid())
	{
		FHSREquipmentLoadout Loadout;
		int32 EquipmentRevision = 0;
		if (Equipment->GetLoadout(HSRCharacterGuidFromProfileName(Id), Loadout, EquipmentRevision))
		{
			FHSREquipmentAggregate Aggregate;
			if (UHSREquipmentStatAggregator::Aggregate(Loadout, EquipmentRevision, Aggregate))
			{
				N.DerivedStats.MaxHealth = N.DerivedStats.MaxHealth + Aggregate.MaxHealth;
				N.DerivedStats.Attack = N.DerivedStats.Attack + Aggregate.Attack;
				N.DerivedStats.Defense = N.DerivedStats.Defense + Aggregate.Defense;
				N.DerivedStats.Speed = N.DerivedStats.Speed + Aggregate.Speed;
			}
		}
	}

	N.PortraitPath = D->Portrait.ToSoftObjectPath();
	N.bHasPortrait = !N.PortraitPath.IsNull();
	N.bIsValid = true;
	N.FailureReason = EHSRCharacterDetailResult::Success;
	// 把定义里的"技能最大等级表"投影成技能条目，当前等级取自角色档案的已升级等级。
	for (const auto& K : D->SkillMaxLevels)
	{
		FHSRCharacterDetailSkill Skill;
		Skill.SkillId = K.Key;
		Skill.Level = P.RuntimeState.SkillLevels.FindRef(K.Key);
		Skill.MaxLevel = K.Value;
		N.Skills.Add(Skill);
	}
	// 技能按 SkillId 字典序排序，保证详情面板显示顺序稳定。
	N.Skills.Sort([](const auto& A, const auto& B) { return A.SkillId.LexicalLess(B.SkillId); });
	Out = MoveTemp(N);
	return EHSRCharacterDetailResult::Success;
}

// BroadcastSnapshot：把当前快照同时广播给两个订阅通道（Changed + OnSnapshotChanged）。
void UHSRCharacterDetailViewModel::BroadcastSnapshot()
{
	Changed.Broadcast(Snapshot);
	OnSnapshotChanged.Broadcast(Snapshot);
}

// SelectCharacter：选中某角色并构建其详情快照。构建失败时保持原快照不变并返回错误码。
EHSRCharacterDetailResult UHSRCharacterDetailViewModel::SelectCharacter(FName Id)
{
	FHSRCharacterDetailSnapshot Candidate;
	const auto R = BuildSnapshot(Id, Candidate);
	if (R != EHSRCharacterDetailResult::Success)
	{
		return R;
	}
	SelectedId = Id;
	Snapshot = MoveTemp(Candidate);
	bHasSnapshot = true;
	BroadcastSnapshot();
	return R;
}

// SelectPartySlot0：默认选中队伍 0 号槽位的角色（通常为队长）。
EHSRCharacterDetailResult UHSRCharacterDetailViewModel::SelectPartySlot0()
{
	if (!Party.IsValid())
	{
		return EHSRCharacterDetailResult::NotInitialized;
	}
	FHSRPartySnapshot P;
	Party->GetSnapshot(P);
	if (P.Slots.IsEmpty() || P.Slots[0].IsEmpty())
	{
		return EHSRCharacterDetailResult::PartySlotEmpty;
	}
	return SelectCharacter(P.Slots[0].CharacterId);
}

// RefreshSelected：重新构建当前选中角色的快照。
// 若角色档案版本号（RuntimeRevision）未变化则跳过广播，避免无意义的 UI 抖动。
void UHSRCharacterDetailViewModel::RefreshSelected()
{
	if (SelectedId.IsNone())
	{
		return;
	}
	FHSRCharacterDetailSnapshot N;
	if (BuildSnapshot(SelectedId, N) != EHSRCharacterDetailResult::Success)
	{
		return;
	}
	if (bHasSnapshot && N.RuntimeRevision == Snapshot.RuntimeRevision)
	{
		return;
	}
	Snapshot = MoveTemp(N);
	bHasSnapshot = true;
	BroadcastSnapshot();
}

// HandleProfile：角色档案变化时，若影响的是当前选中角色则刷新。
void UHSRCharacterDetailViewModel::HandleProfile(FName Id, int64)
{
	if (Id == SelectedId)
	{
		RefreshSelected();
	}
}

// HandleRestore：存档恢复提交后，若恢复涉及当前选中角色则刷新。
void UHSRCharacterDetailViewModel::HandleRestore(const FHSRRestoreCommitInfo& Info)
{
	if (Info.ChangedCharacterIds.Contains(SelectedId))
	{
		RefreshSelected();
	}
}

// HandleParty：队伍变化时，若当前无选中角色则尝试默认选中队长。
void UHSRCharacterDetailViewModel::HandleParty(int64)
{
	if (SelectedId.IsNone())
	{
		SelectPartySlot0();
	}
}

// HandleEquipmentLoadout：装备变化时刷新当前选中角色的派生属性。
// A loadout change affects the selected character's displayed derived stats; refresh so the
// detail panel and shell both pick up the new equipment aggregate immediately.
// 装备变动会直接影响当前角色的派生属性显示，因此立即刷新，让详情面板与
// 角色外壳都能立刻拿到新的装备聚合结果。
void UHSRCharacterDetailViewModel::HandleEquipmentLoadout(const FGuid& CharacterId, int32)
{
	if (CharacterId == HSRCharacterGuidFromProfileName(SelectedId))
	{
		RefreshSelected();
	}
}
