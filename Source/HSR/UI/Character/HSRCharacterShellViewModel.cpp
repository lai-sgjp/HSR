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

void UHSRCharacterShellViewModel::Uninitialize()
{
	if (Profiles.IsValid() && ProfileHandle.IsValid()) Profiles->OnProfileChanged().Remove(ProfileHandle);
	if (CharacterDetailViewModel && CharacterDetailHandle.IsValid())
		CharacterDetailViewModel->OnChanged().Remove(CharacterDetailHandle);
	if (EquipmentDetailViewModel && EquipmentDetailHandle.IsValid())
		EquipmentDetailViewModel->OnChanged().Remove(EquipmentDetailHandle);
	ProfileHandle.Reset();
	CharacterDetailHandle.Reset();
	EquipmentDetailHandle.Reset();
	if (CharacterDetailViewModel) CharacterDetailViewModel->Uninitialize();
	if (EquipmentDetailViewModel) EquipmentDetailViewModel->Shutdown();
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

bool UHSRCharacterShellViewModel::BuildEntries(TArray<FHSRCharacterShellEntrySnapshot>& OutEntries) const
{
	OutEntries.Reset();
	if (!Profiles.IsValid()) return false;
	TArray<FHSRCharacterProfileSnapshot> ProfileSnapshots;
	if (!Profiles->GetAllProfileSnapshots(ProfileSnapshots)) return false;
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
	OutEntries.Sort([](const FHSRCharacterShellEntrySnapshot& A, const FHSRCharacterShellEntrySnapshot& B)
	{
		return A.CharacterId.LexicalLess(B.CharacterId);
	});
	return true;
}

bool UHSRCharacterShellViewModel::ContainsCharacter(
	const TArray<FHSRCharacterShellEntrySnapshot>& Entries, FName CharacterId) const
{
	return Entries.ContainsByPredicate([CharacterId](const FHSRCharacterShellEntrySnapshot& Entry)
	{
		return Entry.CharacterId == CharacterId;
	});
}

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

EHSRCharacterShellResult UHSRCharacterShellViewModel::Refresh()
{
	if (!Profiles.IsValid()) return PublishFailure(EHSRCharacterShellResult::NotInitialized);
	TArray<FHSRCharacterShellEntrySnapshot> Entries;
	if (!BuildEntries(Entries)) return PublishFailure(EHSRCharacterShellResult::NotInitialized);
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

EHSRCharacterShellResult UHSRCharacterShellViewModel::SelectCharacter(FName CharacterId)
{
	if (!Profiles.IsValid()) return PublishFailure(EHSRCharacterShellResult::NotInitialized);
	TArray<FHSRCharacterShellEntrySnapshot> Entries;
	if (!BuildEntries(Entries)) return PublishFailure(EHSRCharacterShellResult::NotInitialized);
	if (Entries.IsEmpty()) return PublishFailure(EHSRCharacterShellResult::EmptyList);
	if (CharacterId.IsNone() || !ContainsCharacter(Entries, CharacterId))
		return EHSRCharacterShellResult::InvalidCharacterId;
	SelectedCharacterId = CharacterId;
	return RebuildSelected(Entries);
}

EHSRCharacterShellResult UHSRCharacterShellViewModel::SelectTab(EHSRCharacterShellTab Tab)
{
	if (!Profiles.IsValid()) return EHSRCharacterShellResult::NotInitialized;
	if (static_cast<uint8>(Tab) > static_cast<uint8>(EHSRCharacterShellTab::Outfit))
		return EHSRCharacterShellResult::InvalidTab;
	SelectedTab = Tab;
	if (!bHasSnapshot) return Refresh();
	Snapshot.SelectedTab = SelectedTab;
	UpdateSelectedTabState();
	Broadcast();
	return EHSRCharacterShellResult::Success;
}

EHSRCharacterShellResult UHSRCharacterShellViewModel::RebuildSelected(
	const TArray<FHSRCharacterShellEntrySnapshot>& Entries)
{
	bUpdating = true;
	Snapshot = FHSRCharacterShellSnapshot();
	Snapshot.CharacterEntries = Entries;
	Snapshot.SelectedCharacterId = SelectedCharacterId;
	Snapshot.SelectedTab = SelectedTab;
	for (FHSRCharacterShellEntrySnapshot& Entry : Snapshot.CharacterEntries)
		Entry.bIsSelected = Entry.CharacterId == SelectedCharacterId;

	EHSRCharacterDetailResult CharacterResult = EHSRCharacterDetailResult::NotInitialized;
	if (CharacterDetailViewModel)
	{
		CharacterDetailViewModel->Initialize(Profiles.Get(), Save.Get(), Party.Get(), Equipment.Get());
		CharacterResult = CharacterDetailViewModel->SelectCharacter(SelectedCharacterId);
		if (CharacterResult == EHSRCharacterDetailResult::Success)
			CharacterDetailViewModel->GetSnapshot(Snapshot.CharacterDetail);
	}
	if (CharacterResult != EHSRCharacterDetailResult::Success)
	{
		Snapshot.CharacterDetail.CharacterId = SelectedCharacterId;
		Snapshot.CharacterDetail.FailureReason = CharacterResult;
	}

	if (EquipmentDetailViewModel)
	{
		EquipmentDetailViewModel->Initialize(Equipment.Get(), HSRCharacterGuidFromProfileName(SelectedCharacterId));
		EquipmentDetailViewModel->GetSnapshot(Snapshot.EquipmentDetail);
	}

	bUpdating = false;
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

void UHSRCharacterShellViewModel::Broadcast()
{
	Changed.Broadcast(Snapshot);
	OnSnapshotChanged.Broadcast(Snapshot);
}

void UHSRCharacterShellViewModel::HandleProfileChanged(FName CharacterId, int64)
{
	if (CharacterId == SelectedCharacterId || SelectedCharacterId.IsNone()) Refresh();
}

void UHSRCharacterShellViewModel::HandleCharacterDetailChanged(const FHSRCharacterDetailSnapshot& InSnapshot)
{
	if (bUpdating) return;
	Snapshot.CharacterDetail = InSnapshot;
	Snapshot.bIsValid = InSnapshot.bIsValid;
	Snapshot.FailureReason = InSnapshot.bIsValid
		? EHSRCharacterShellResult::Success
		: MapCharacterResult(InSnapshot.FailureReason);
	bHasSnapshot = true;
	Broadcast();
}

void UHSRCharacterShellViewModel::HandleEquipmentDetailChanged(const FHSREquipmentDetailSnapshot& InSnapshot)
{
	if (bUpdating) return;
	Snapshot.EquipmentDetail = InSnapshot;
	UpdateSelectedTabState();
	bHasSnapshot = true;
	Broadcast();
}

void UHSRCharacterShellViewModel::UpdateSelectedTabState()
{
	Snapshot.bSelectedTabAvailable = false;
	Snapshot.SelectedTabFailureReason = EHSRCharacterShellResult::CharacterUnavailable;
	if (!Snapshot.bIsValid) return;

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

EHSRCharacterShellResult UHSRCharacterShellViewModel::MapCharacterResult(EHSRCharacterDetailResult Result)
{
	return Result == EHSRCharacterDetailResult::NotInitialized
		? EHSRCharacterShellResult::NotInitialized
		: EHSRCharacterShellResult::CharacterUnavailable;
}
