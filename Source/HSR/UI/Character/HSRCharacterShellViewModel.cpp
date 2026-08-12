#include "HSRCharacterShellViewModel.h"

#include "../HSRCharacterDetailViewModel.h"
#include "../HSREquipmentDetailViewModel.h"
#include "../../Progression/HSRCharacterProfileSubsystem.h"
#include "../../Progression/HSRCharacterProfileTypes.h"
#include "../../Equipment/HSREquipmentSubsystem.h"
#include "../../Equipment/HSREquipmentTypes.h"
#include "../../Party/HSRPartySubsystem.h"
#include "../../Party/HSRPartyTypes.h"
#include "../../Save/HSRSaveSubsystem.h"
#include "../../Data/Definitions/HSRCharacterDefinition.h"

// Initialize：初始化角色外壳 ViewModel。
//
// 数据来源：UHSRCharacterProfileSubsystem（角色档案全集）+
//           UHSRSaveSubsystem / UHSRPartySubsystem / UHSREquipmentSubsystem
//           （透传给内部两个子 ViewModel）。
// 转换方式：内部持有两个子 ViewModel——UHSRCharacterDetailViewModel（详情）
//           与 UHSREquipmentDetailViewModel（武器/遗器），由它们分别生成子快照，
//           本类再把它们与"角色列表、选中项、当前页签"合并成
//           FHSRCharacterShellSnapshot 对外输出。
// 广播时机：Broadcast() 统一广播 Changed 与 OnSnapshotChanged；
//           子 ViewModel 变化时通过 Handle*DetailChanged 回流到本类再广播。
void UHSRCharacterShellViewModel::Initialize(UHSRCharacterProfileSubsystem* InProfiles,
	UHSRSaveSubsystem* InSave, UHSRPartySubsystem* InParty, UHSREquipmentSubsystem* InEquipment)
{
	Uninitialize();
	Profiles = InProfiles;
	Save = InSave;
	Party = InParty;
	Equipment = InEquipment;
	CharacterDetailViewModel = NewObject<UHSRCharacterDetailViewModel>(this);
	EquipmentDetailViewModel = NewObject<UHSREquipmentDetailViewModel>(this);
	if (CharacterDetailViewModel)
	{
		CharacterDetailHandle = CharacterDetailViewModel->OnChanged().AddUObject(
			this, &UHSRCharacterShellViewModel::HandleCharacterDetailChanged);
	}
	if (EquipmentDetailViewModel)
	{
		EquipmentDetailHandle = EquipmentDetailViewModel->OnChanged().AddUObject(
			this, &UHSRCharacterShellViewModel::HandleEquipmentDetailChanged);
	}
	if (Profiles.IsValid())
	{
		ProfileHandle = Profiles->OnProfileChanged().AddUObject(
			this, &UHSRCharacterShellViewModel::HandleProfileChanged);
	}
	Refresh();
}

// Uninitialize：解绑全部订阅、反初始化两个子 ViewModel 并复位状态。
void UHSRCharacterShellViewModel::Uninitialize()
{
	if (Profiles.IsValid() && ProfileHandle.IsValid())
	{
		Profiles->OnProfileChanged().Remove(ProfileHandle);
	}
	if (CharacterDetailViewModel && CharacterDetailHandle.IsValid())
	{
		CharacterDetailViewModel->OnChanged().Remove(CharacterDetailHandle);
	}
	if (EquipmentDetailViewModel && EquipmentDetailHandle.IsValid())
	{
		EquipmentDetailViewModel->OnChanged().Remove(EquipmentDetailHandle);
	}
	ProfileHandle.Reset();
	CharacterDetailHandle.Reset();
	EquipmentDetailHandle.Reset();
	if (CharacterDetailViewModel)
	{
		CharacterDetailViewModel->Uninitialize();
	}
	if (EquipmentDetailViewModel)
	{
		EquipmentDetailViewModel->Shutdown();
	}
	CharacterDetailViewModel = nullptr;
	EquipmentDetailViewModel = nullptr;
	Profiles.Reset();
	Save.Reset();
	Party.Reset();
	Equipment.Reset();
	SelectedCharacterId = NAME_None;
	SelectedTab = EHSRCharacterShellTab::Detail;
	Snapshot = FHSRCharacterShellSnapshot();
	bHasSnapshot = false;
}

// BuildEntries：从档案子系统拉取全部角色档案，生成角色列表条目。
// 定义存在且可解析时显示定义里的 DisplayName 并标记可用；
// 否则退回用角色 ID 当名字并标记不可用（列表仍会显示该项）。
bool UHSRCharacterShellViewModel::BuildEntries(TArray<FHSRCharacterShellEntrySnapshot>& OutEntries) const
{
	OutEntries.Reset();
	if (!Profiles.IsValid())
	{
		return false;
	}
	TArray<FHSRCharacterProfileSnapshot> ProfileSnapshots;
	if (!Profiles->GetAllProfileSnapshots(ProfileSnapshots))
	{
		return false;
	}
	for (const FHSRCharacterProfileSnapshot& Profile : ProfileSnapshots)
	{
		FHSRCharacterShellEntrySnapshot Entry;
		Entry.CharacterId = Profile.RuntimeState.CharacterId;
		const UHSRCharacterDefinition* Definition = nullptr;
		if (Profiles->GetDefinition(Entry.CharacterId, Definition) && Definition)
		{
			Entry.DisplayName = Definition->DisplayName;
			Entry.bIsAvailable = true;
		}
		else
		{
			Entry.DisplayName = FText::FromName(Entry.CharacterId);
			Entry.bIsAvailable = false;
		}
		OutEntries.Add(MoveTemp(Entry));
	}
	// 按 CharacterId 字典序排序，保证角色列表顺序稳定。
	OutEntries.Sort([](const FHSRCharacterShellEntrySnapshot& A, const FHSRCharacterShellEntrySnapshot& B)
	{
		return A.CharacterId.LexicalLess(B.CharacterId);
	});
	return true;
}

// ContainsCharacter：判断某角色是否出现在角色列表中。
bool UHSRCharacterShellViewModel::ContainsCharacter(
	const TArray<FHSRCharacterShellEntrySnapshot>& Entries, FName CharacterId) const
{
	return Entries.ContainsByPredicate([CharacterId](const FHSRCharacterShellEntrySnapshot& Entry)
	{
		return Entry.CharacterId == CharacterId;
	});
}

// SelectInitialCharacter：确定首次进入外壳时默认选中的角色。
// 优先取队伍 0 号槽位的角色（需确实存在于角色列表中）；队伍不可用/为空时退回列表首项。
FName UHSRCharacterShellViewModel::SelectInitialCharacter(
	const TArray<FHSRCharacterShellEntrySnapshot>& Entries) const
{
	if (Party.IsValid())
	{
		FHSRPartySnapshot PartySnapshot;
		if (Party->GetSnapshot(PartySnapshot) && !PartySnapshot.Slots.IsEmpty()
			&& !PartySnapshot.Slots[0].IsEmpty()
			&& ContainsCharacter(Entries, PartySnapshot.Slots[0].CharacterId))
		{
			return PartySnapshot.Slots[0].CharacterId;
		}
	}
	return Entries.IsEmpty() ? NAME_None : Entries[0].CharacterId;
}

// Refresh：整体重建外壳快照。
// 列表为空时发布失败；当前选中项无效（为空或已不在列表）时重新选择默认角色。
EHSRCharacterShellResult UHSRCharacterShellViewModel::Refresh()
{
	if (!Profiles.IsValid())
	{
		return PublishFailure(EHSRCharacterShellResult::NotInitialized);
	}
	TArray<FHSRCharacterShellEntrySnapshot> Entries;
	if (!BuildEntries(Entries))
	{
		return PublishFailure(EHSRCharacterShellResult::NotInitialized);
	}
	if (Entries.IsEmpty())
	{
		SelectedCharacterId = NAME_None;
		return PublishFailure(EHSRCharacterShellResult::EmptyList);
	}
	if (SelectedCharacterId.IsNone() || !ContainsCharacter(Entries, SelectedCharacterId))
	{
		SelectedCharacterId = SelectInitialCharacter(Entries);
	}
	return RebuildSelected(Entries);
}

// SelectCharacter：显式选中某角色并重建快照。
EHSRCharacterShellResult UHSRCharacterShellViewModel::SelectCharacter(FName CharacterId)
{
	if (!Profiles.IsValid())
	{
		return PublishFailure(EHSRCharacterShellResult::NotInitialized);
	}
	TArray<FHSRCharacterShellEntrySnapshot> Entries;
	if (!BuildEntries(Entries))
	{
		return PublishFailure(EHSRCharacterShellResult::NotInitialized);
	}
	if (Entries.IsEmpty())
	{
		return PublishFailure(EHSRCharacterShellResult::EmptyList);
	}
	if (CharacterId.IsNone() || !ContainsCharacter(Entries, CharacterId))
	{
		return EHSRCharacterShellResult::InvalidCharacterId;
	}
	SelectedCharacterId = CharacterId;
	return RebuildSelected(Entries);
}

// SelectTab：切换页签（详情/轨迹/光锥/遗器/命座/外观）。
// 已有快照时只更新页签相关字段并广播；尚未有快照则走完整 Refresh。
EHSRCharacterShellResult UHSRCharacterShellViewModel::SelectTab(EHSRCharacterShellTab Tab)
{
	if (!Profiles.IsValid())
	{
		return EHSRCharacterShellResult::NotInitialized;
	}
	if (static_cast<uint8>(Tab) > static_cast<uint8>(EHSRCharacterShellTab::Outfit))
	{
		return EHSRCharacterShellResult::InvalidTab;
	}
	SelectedTab = Tab;
	if (!bHasSnapshot)
	{
		return Refresh();
	}
	Snapshot.SelectedTab = SelectedTab;
	UpdateSelectedTabState();
	Broadcast();
	return EHSRCharacterShellResult::Success;
}

// RebuildSelected：围绕当前选中的角色重建整个外壳快照。
// 流程：重置快照 -> 写入列表/选中项/页签 -> 标记选中 -> 分别驱动两个子 ViewModel
// 生成详情与装备快照 -> 汇总有效性 -> 更新页签可用状态 -> 广播。
EHSRCharacterShellResult UHSRCharacterShellViewModel::RebuildSelected(
	const TArray<FHSRCharacterShellEntrySnapshot>& Entries)
{
	// bUpdating 防止子 ViewModel 的广播回流导致重复重建（见 Handle*DetailChanged）。
	bUpdating = true;
	Snapshot = FHSRCharacterShellSnapshot();
	Snapshot.CharacterEntries = Entries;
	Snapshot.SelectedCharacterId = SelectedCharacterId;
	Snapshot.SelectedTab = SelectedTab;
	for (FHSRCharacterShellEntrySnapshot& Entry : Snapshot.CharacterEntries)
	{
		Entry.bIsSelected = Entry.CharacterId == SelectedCharacterId;
	}

	EHSRCharacterDetailResult CharacterResult = EHSRCharacterDetailResult::NotInitialized;
	if (CharacterDetailViewModel)
	{
		CharacterDetailViewModel->Initialize(Profiles.Get(), Save.Get(), Party.Get(), Equipment.Get());
		CharacterResult = CharacterDetailViewModel->SelectCharacter(SelectedCharacterId);
		if (CharacterResult == EHSRCharacterDetailResult::Success)
		{
			CharacterDetailViewModel->GetSnapshot(Snapshot.CharacterDetail);
		}
	}
	if (CharacterResult != EHSRCharacterDetailResult::Success)
	{
		// 详情构建失败：仍保留选中角色 ID 与失败原因，让 UI 显示降级信息。
		Snapshot.CharacterDetail.CharacterId = SelectedCharacterId;
		Snapshot.CharacterDetail.FailureReason = CharacterResult;
	}

	if (EquipmentDetailViewModel)
	{
		EquipmentDetailViewModel->Initialize(Equipment.Get(), HSRCharacterGuidFromProfileName(SelectedCharacterId));
		EquipmentDetailViewModel->GetSnapshot(Snapshot.EquipmentDetail);
	}

	bUpdating = false;
	// 整体有效性 = 详情子快照有效；失败原因由详情结果映射而来。
	Snapshot.bIsValid = CharacterResult == EHSRCharacterDetailResult::Success
		&& Snapshot.CharacterDetail.bIsValid;
	Snapshot.FailureReason = Snapshot.bIsValid
		? EHSRCharacterShellResult::Success
		: MapCharacterResult(CharacterResult);
	UpdateSelectedTabState();
	bHasSnapshot = true;
	Broadcast();
	return Snapshot.bIsValid ? EHSRCharacterShellResult::Success
		: EHSRCharacterShellResult::CharacterUnavailable;
}

// PublishFailure：以失败结果发布一个"最简快照"（保留选中项与页签，其余字段清空）。
EHSRCharacterShellResult UHSRCharacterShellViewModel::PublishFailure(EHSRCharacterShellResult Result)
{
	Snapshot = FHSRCharacterShellSnapshot();
	Snapshot.SelectedCharacterId = SelectedCharacterId;
	Snapshot.SelectedTab = SelectedTab;
	Snapshot.FailureReason = Result;
	Snapshot.bIsValid = false;
	Snapshot.bSelectedTabAvailable = false;
	Snapshot.SelectedTabFailureReason = Result;
	bHasSnapshot = true;
	Broadcast();
	return Result;
}

// Broadcast：把当前快照同时广播给两个订阅通道。
void UHSRCharacterShellViewModel::Broadcast()
{
	Changed.Broadcast(Snapshot);
	OnSnapshotChanged.Broadcast(Snapshot);
}

// HandleProfileChanged：角色档案变化时，若影响的是当前选中角色或尚无选中角色则刷新。
void UHSRCharacterShellViewModel::HandleProfileChanged(FName CharacterId, int64)
{
	if (CharacterId == SelectedCharacterId || SelectedCharacterId.IsNone())
	{
		Refresh();
	}
}

// HandleCharacterDetailChanged：详情子 ViewModel 的广播回流。
// bUpdating 期间（正在被 RebuildSelected 驱动）忽略回流，避免重复广播。
void UHSRCharacterShellViewModel::HandleCharacterDetailChanged(const FHSRCharacterDetailSnapshot& InSnapshot)
{
	if (bUpdating)
	{
		return;
	}
	Snapshot.CharacterDetail = InSnapshot;
	Snapshot.bIsValid = InSnapshot.bIsValid;
	Snapshot.FailureReason = InSnapshot.bIsValid
		? EHSRCharacterShellResult::Success
		: MapCharacterResult(InSnapshot.FailureReason);
	bHasSnapshot = true;
	Broadcast();
}

// HandleEquipmentDetailChanged：装备子 ViewModel 的广播回流（仅影响装备相关页签状态）。
void UHSRCharacterShellViewModel::HandleEquipmentDetailChanged(const FHSREquipmentDetailSnapshot& InSnapshot)
{
	if (bUpdating)
	{
		return;
	}
	Snapshot.EquipmentDetail = InSnapshot;
	UpdateSelectedTabState();
	bHasSnapshot = true;
	Broadcast();
}

// UpdateSelectedTabState：根据当前页签与各子快照的有效性，更新页签可用状态。
// 详情/轨迹/信息页签依赖详情快照；光锥/遗器页签依赖装备快照；命座/外观当前不开放。
void UHSRCharacterShellViewModel::UpdateSelectedTabState()
{
	Snapshot.bSelectedTabAvailable = false;
	Snapshot.SelectedTabFailureReason = EHSRCharacterShellResult::CharacterUnavailable;
	if (!Snapshot.bIsValid)
	{
		return;
	}

	switch (SelectedTab)
	{
	case EHSRCharacterShellTab::Detail:
	case EHSRCharacterShellTab::Traces:
	case EHSRCharacterShellTab::Information:
		Snapshot.bSelectedTabAvailable = Snapshot.CharacterDetail.bIsValid;
		break;
	case EHSRCharacterShellTab::Weapon:
	case EHSRCharacterShellTab::Relics:
		Snapshot.bSelectedTabAvailable = Snapshot.EquipmentDetail.bIsValid;
		break;
	case EHSRCharacterShellTab::Eidolon:
	case EHSRCharacterShellTab::Outfit:
		Snapshot.bSelectedTabAvailable = false;
		break;
	}
	Snapshot.SelectedTabFailureReason = Snapshot.bSelectedTabAvailable
		? EHSRCharacterShellResult::Success
		: EHSRCharacterShellResult::CharacterUnavailable;
}

// MapCharacterResult：把详情子结果降维映射为外壳层的失败原因。
// 未初始化状态保留原样，其它失败一律归类为"角色不可用"。
EHSRCharacterShellResult UHSRCharacterShellViewModel::MapCharacterResult(EHSRCharacterDetailResult Result)
{
	return Result == EHSRCharacterDetailResult::NotInitialized
		? EHSRCharacterShellResult::NotInitialized
		: EHSRCharacterShellResult::CharacterUnavailable;
}
