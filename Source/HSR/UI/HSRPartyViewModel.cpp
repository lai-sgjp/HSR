#include "HSRPartyViewModel.h"

#include "../Party/HSRPartySubsystem.h"

void UHSRPartyViewModel::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

void UHSRPartyViewModel::Initialize(UHSRPartySubsystem* InParty)
{
	Shutdown();
	Party = InParty;
	if (InParty)
	{
		PartyChangedHandle = InParty->OnPartyChanged().AddUObject(this, &ThisClass::HandlePartyChanged);
	}
	Rebuild();
}

void UHSRPartyViewModel::Shutdown()
{
	if (Party.IsValid())
	{
		Party->OnPartyChanged().Remove(PartyChangedHandle);
	}
	Party.Reset();
	PartyChangedHandle.Reset();
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

void UHSRPartyViewModel::Rebuild()
{
	FHSRPartyFrontendSnapshot Next;
	FHSRPartySnapshot PartySnapshot;
	if (!Party.IsValid() || !Party->GetSnapshot(PartySnapshot))
	{
		Next.Status = EHSRPartyFrontendStatus::Unavailable;
	}
	else
	{
		bool bAnyOccupied = false;
		Next.Revision = PartySnapshot.Revision;
		for (int32 Index = 0; Index < PartySnapshot.Slots.Num(); ++Index)
		{
			FHSRPartySlotViewData& View = Next.Slots.AddDefaulted_GetRef();
			View.SlotIndex = Index;
			View.CharacterId = PartySnapshot.Slots[Index].CharacterId;
			View.bOccupied = !PartySnapshot.Slots[Index].IsEmpty();
			bAnyOccupied |= View.bOccupied;
		}
		Next.Status = bAnyOccupied ? EHSRPartyFrontendStatus::Ready : EHSRPartyFrontendStatus::Empty;
	}
	Snapshot = MoveTemp(Next);
	Changed.Broadcast(Snapshot);
	OnSnapshotChanged.Broadcast(Snapshot);
}
