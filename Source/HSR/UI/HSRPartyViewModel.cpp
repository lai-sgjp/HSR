#include "HSRPartyViewModel.h"

#include "../Party/HSRPartySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"

void UHSRPartyViewModel::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

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

bool UHSRPartyViewModel::GetSnapshot(FHSRPartyFrontendSnapshot& OutSnapshot) const
{
	OutSnapshot = Snapshot;
	return true;
}

void UHSRPartyViewModel::HandlePartyChanged(int64)
{
	Rebuild();
}

bool UHSRPartyViewModel::IsValidCandidateSlot(int32 SlotIndex) const
{
	return SlotIndex >= 0 && SlotIndex < Candidate.Slots.Num();
}

bool UHSRPartyViewModel::IsAvailableCharacter(FName CharacterId) const
{
	FHSRCharacterProfileSnapshot Ignored;
	return !CharacterId.IsNone() && Profiles.IsValid() && Profiles->GetProfileSnapshot(CharacterId, Ignored);
}

bool UHSRPartyViewModel::CandidateContains(FName CharacterId, int32 IgnoreSlot) const
{
	for (int32 Index = 0; Index < Candidate.Slots.Num(); ++Index)
	{
		if (Index != IgnoreSlot && Candidate.Slots[Index].CharacterId == CharacterId) return true;
	}
	return false;
}

EHSRPartyResult UHSRPartyViewModel::SetCandidateSlot(int32 SlotIndex, FName CharacterId)
{
	if (!IsValidCandidateSlot(SlotIndex)) return EHSRPartyResult::InvalidSlot;
	if (!IsAvailableCharacter(CharacterId)) return EHSRPartyResult::ProfileNotFound;
	if (CandidateContains(CharacterId, SlotIndex)) return EHSRPartyResult::DuplicateCharacter;
	Candidate.Slots[SlotIndex].CharacterId = CharacterId;
	bHasPendingChanges = true;
	PublishCandidate();
	return EHSRPartyResult::Success;
}

EHSRPartyResult UHSRPartyViewModel::ClearCandidateSlot(int32 SlotIndex)
{
	if (!IsValidCandidateSlot(SlotIndex)) return EHSRPartyResult::InvalidSlot;
	if (Candidate.Slots[SlotIndex].IsEmpty()) return EHSRPartyResult::EmptySlot;
	Candidate.Slots[SlotIndex] = FHSRPartySlot();
	bHasPendingChanges = true;
	PublishCandidate();
	return EHSRPartyResult::Success;
}

EHSRPartyResult UHSRPartyViewModel::SwapCandidateSlots(int32 FirstSlot, int32 SecondSlot)
{
	if (!IsValidCandidateSlot(FirstSlot) || !IsValidCandidateSlot(SecondSlot)) return EHSRPartyResult::InvalidSlot;
	if (FirstSlot == SecondSlot) return EHSRPartyResult::InvalidCandidate;
	Candidate.Slots.Swap(FirstSlot, SecondSlot);
	bHasPendingChanges = true;
	PublishCandidate();
	return EHSRPartyResult::Success;
}

EHSRPartyResult UHSRPartyViewModel::ConfirmCandidate()
{
	if (!Party.IsValid()) return EHSRPartyResult::InvalidCandidate;
	const EHSRPartyResult Result = Party->CommitCandidate(Candidate);
	if (Result == EHSRPartyResult::Success) ResetCandidateFromAuthority();
	return Result;
}

EHSRPartyResult UHSRPartyViewModel::CancelCandidate()
{
	if (!Party.IsValid()) return EHSRPartyResult::InvalidCandidate;
	ResetCandidateFromAuthority();
	return EHSRPartyResult::Success;
}

void UHSRPartyViewModel::ResetCandidateFromAuthority()
{
	bHasPendingChanges = false;
	Candidate = FHSRPartySnapshot();
	if (Party.IsValid()) Party->GetSnapshot(Candidate);
	Rebuild();
}

void UHSRPartyViewModel::PublishCandidate()
{
	Rebuild();
}

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
		if (!bHasPendingChanges) Candidate = AuthoritySnapshot;
		Next.Revision = Candidate.Revision;
		Next.bHasPendingChanges = bHasPendingChanges;
		TArray<FHSRCharacterProfileSnapshot> ProfileSnapshots;
		if (Profiles.IsValid() && Profiles->GetAllProfileSnapshots(ProfileSnapshots))
		{
			for (const FHSRCharacterProfileSnapshot& Profile : ProfileSnapshots) Next.AvailableCharacterIds.Add(Profile.RuntimeState.CharacterId);
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
