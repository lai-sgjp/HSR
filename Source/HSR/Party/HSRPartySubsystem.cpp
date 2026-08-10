#include "HSRPartySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#if WITH_EDITOR
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace
{
	void RunHSRPartyTest()
	{
		if (!GEngine) { UE_LOG(LogTemp, Error, TEXT("HSR.PartyTest FAILED NoEngine")); return; }
		UHSRPartySubsystem* Party = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (UWorld* World = Context.World(); World && World->IsPlayInEditor())
			{
				if (UGameInstance* GI = World->GetGameInstance()) { Party = GI->GetSubsystem<UHSRPartySubsystem>(); break; }
			}
		}
		if (!Party) { UE_LOG(LogTemp, Error, TEXT("HSR.PartyTest FAILED NoPIEPartySubsystem")); return; }
		int32 EventCount = 0; FDelegateHandle EventHandle = Party->OnPartyChanged().AddLambda([&EventCount](int64){ ++EventCount; });
		FHSRPartySnapshot Before; Party->GetSnapshot(Before);
		const EHSRPartyResult AddA = Party->AddCharacter(TEXT("Character.A"));
		const EHSRPartyResult AddB = Party->AddCharacter(TEXT("Character.B"));
		FHSRPartySnapshot Filled; Party->GetSnapshot(Filled);
		const int64 FailureRevision = Filled.Revision; const int32 FailureEvents = EventCount;
		const EHSRPartyResult Duplicate = Party->AddCharacter(TEXT("Character.A"));
		const EHSRPartyResult None = Party->AddCharacter(NAME_None);
		const EHSRPartyResult Unknown = Party->AddCharacter(TEXT("Character.Unknown"));
		const EHSRPartyResult BadSlot = Party->AddCharacter(TEXT("Character.A"), 99);
		FHSRPartySnapshot AfterFailures; Party->GetSnapshot(AfterFailures);
		const bool bFailuresUnchanged = AfterFailures.Revision == FailureRevision && EventCount == FailureEvents && AfterFailures.Slots.Num() == Filled.Slots.Num();
		const EHSRPartyResult Swap = Party->SwapSlots(0, 1);
		const EHSRPartyResult Remove = Party->RemoveCharacter(0);
		const EHSRPartyResult Replace = Party->ReplaceCharacter(0, TEXT("Character.A"));
		FHSRPartySnapshot Final; Party->GetSnapshot(Final);
		UE_LOG(LogTemp, Log, TEXT("HSR.PartyTest RESULT AddA=%d AddB=%d Duplicate=%d None=%d Unknown=%d BadSlot=%d Swap=%d Remove=%d Replace=%d Revision=%lld Events=%d FailureStateUnchanged=%d Slots=%d"),
			static_cast<int32>(AddA), static_cast<int32>(AddB), static_cast<int32>(Duplicate), static_cast<int32>(None), static_cast<int32>(Unknown), static_cast<int32>(BadSlot), static_cast<int32>(Swap), static_cast<int32>(Remove), static_cast<int32>(Replace), Final.Revision, EventCount, bFailuresUnchanged ? 1 : 0, Final.Slots.Num());
		Party->OnPartyChanged().Remove(EventHandle);
	}

	FAutoConsoleCommand HSRPartyTestCommand(
		TEXT("HSR.PartyTest"),
		TEXT("Runs the PartySubsystem PIE development harness."),
		FConsoleCommandDelegate::CreateStatic(&RunHSRPartyTest));
}
#endif

void UHSRPartySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Slots.SetNum(Capacity);
	Profiles = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
}

bool UHSRPartySubsystem::IsKnownProfile(FName CharacterId) const
{
	FHSRCharacterProfileSnapshot Snapshot;
	return !CharacterId.IsNone() && Profiles.IsValid() && Profiles->GetProfileSnapshot(CharacterId, Snapshot);
}

bool UHSRPartySubsystem::IsDuplicate(const TArray<FHSRPartySlot>& Candidate, FName CharacterId, int32 IgnoreSlot) const
{
	for (int32 Index = 0; Index < Candidate.Num(); ++Index) if (Index != IgnoreSlot && Candidate[Index].CharacterId == CharacterId) return true;
	return false;
}

bool UHSRPartySubsystem::Commit(TArray<FHSRPartySlot>&& Candidate)
{
	if (Candidate.Num() != Capacity) return false;
	FString Members;
	for (int32 Index = 0; Index < Candidate.Num(); ++Index)
	{
		if (!Candidate[Index].IsEmpty())
		{
			if (!Members.IsEmpty()) Members += TEXT(",");
			Members += FString::Printf(TEXT("%d:%s"), Index, *Candidate[Index].CharacterId.ToString());
		}
	}
	Slots = MoveTemp(Candidate); ++Revision;
	UE_LOG(LogTemp, Log, TEXT("HSR.Party Commit Revision=%lld Members=%s"), Revision, Members.IsEmpty() ? TEXT("(empty)") : *Members);
	PartyChanged.Broadcast(Revision); return true;
}

EHSRPartyResult UHSRPartySubsystem::AddCharacter(FName CharacterId, int32 PreferredSlot)
{
	if (!IsKnownProfile(CharacterId)) return EHSRPartyResult::ProfileNotFound;
	if (IsDuplicate(Slots, CharacterId)) return EHSRPartyResult::DuplicateCharacter;
	int32 Slot = PreferredSlot;
	if (Slot == INDEX_NONE) { Slot = Slots.IndexOfByPredicate([](const FHSRPartySlot& Entry){ return Entry.IsEmpty(); }); }
	if (!IsValidSlot(Slot)) return PreferredSlot == INDEX_NONE ? EHSRPartyResult::Full : EHSRPartyResult::InvalidSlot;
	if (!Slots[Slot].IsEmpty()) return EHSRPartyResult::Full;
	TArray<FHSRPartySlot> Candidate = Slots; Candidate[Slot].CharacterId = CharacterId;
	return Commit(MoveTemp(Candidate)) ? EHSRPartyResult::Success : EHSRPartyResult::InvalidCandidate;
}

EHSRPartyResult UHSRPartySubsystem::RemoveCharacter(int32 Slot)
{
	if (!IsValidSlot(Slot)) return EHSRPartyResult::InvalidSlot;
	if (Slots[Slot].IsEmpty()) return EHSRPartyResult::EmptySlot;
	TArray<FHSRPartySlot> Candidate = Slots; Candidate[Slot] = FHSRPartySlot();
	return Commit(MoveTemp(Candidate)) ? EHSRPartyResult::Success : EHSRPartyResult::InvalidCandidate;
}

EHSRPartyResult UHSRPartySubsystem::ReplaceCharacter(int32 Slot, FName CharacterId)
{
	if (!IsValidSlot(Slot)) return EHSRPartyResult::InvalidSlot;
	if (!IsKnownProfile(CharacterId)) return EHSRPartyResult::ProfileNotFound;
	if (IsDuplicate(Slots, CharacterId, Slot)) return EHSRPartyResult::DuplicateCharacter;
	TArray<FHSRPartySlot> Candidate = Slots; Candidate[Slot].CharacterId = CharacterId;
	return Commit(MoveTemp(Candidate)) ? EHSRPartyResult::Success : EHSRPartyResult::InvalidCandidate;
}

EHSRPartyResult UHSRPartySubsystem::SwapSlots(int32 FirstSlot, int32 SecondSlot)
{
	if (!IsValidSlot(FirstSlot) || !IsValidSlot(SecondSlot)) return EHSRPartyResult::InvalidSlot;
	if (FirstSlot == SecondSlot || Slots[FirstSlot].IsEmpty() || Slots[SecondSlot].IsEmpty()) return EHSRPartyResult::EmptySlot;
	TArray<FHSRPartySlot> Candidate = Slots; Candidate.Swap(FirstSlot, SecondSlot);
	return Commit(MoveTemp(Candidate)) ? EHSRPartyResult::Success : EHSRPartyResult::InvalidCandidate;
}

EHSRPartyResult UHSRPartySubsystem::CommitCandidate(const FHSRPartySnapshot& Candidate)
{
	if (Candidate.Revision != Revision) return EHSRPartyResult::RevisionConflict;
	if (Candidate.Slots.Num() != Capacity) return EHSRPartyResult::InvalidCandidate;
	TSet<FName> Seen;
	for (const FHSRPartySlot& Slot : Candidate.Slots)
	{
		if (Slot.IsEmpty()) continue;
		if (!IsKnownProfile(Slot.CharacterId)) return EHSRPartyResult::ProfileNotFound;
		if (Seen.Contains(Slot.CharacterId)) return EHSRPartyResult::DuplicateCharacter;
		Seen.Add(Slot.CharacterId);
	}
	TArray<FHSRPartySlot> SlotsCandidate = Candidate.Slots;
	if (!Commit(MoveTemp(SlotsCandidate))) return EHSRPartyResult::InvalidCandidate;
	if (Candidate.ActiveSlot >= 0 && Candidate.ActiveSlot < Capacity && !Slots[Candidate.ActiveSlot].IsEmpty())
	{
		if (ActiveSlot != Candidate.ActiveSlot)
		{
			ActiveSlot = Candidate.ActiveSlot;
			++Revision;
			PartyChanged.Broadcast(Revision);
		}
	}
	return EHSRPartyResult::Success;
}

bool UHSRPartySubsystem::GetSnapshot(FHSRPartySnapshot& OutSnapshot) const
{
	OutSnapshot.Slots = Slots; OutSnapshot.ActiveSlot = ActiveSlot; OutSnapshot.Revision = Revision; return true;
}

EHSRPartyResult UHSRPartySubsystem::SetActiveSlot(int32 Slot)
{
	if (!IsValidSlot(Slot)) return EHSRPartyResult::InvalidSlot;
	if (Slots[Slot].IsEmpty()) return EHSRPartyResult::EmptySlot;
	if (ActiveSlot == Slot) return EHSRPartyResult::Success;
	ActiveSlot = Slot;
	++Revision;
	UE_LOG(LogTemp, Log, TEXT("HSR.Party SetActiveSlot Slot=%d CharacterId=%s Revision=%lld"),
		Slot, Slots[Slot].IsEmpty() ? TEXT("None") : *Slots[Slot].CharacterId.ToString(), Revision);
	PartyChanged.Broadcast(Revision);
	return EHSRPartyResult::Success;
}

bool UHSRPartySubsystem::PrepareRestore(const FHSRPartySnapshot& Saved,FHSRPartySnapshot& Out) const
{
	// A narrower roster is accepted and padded: legacy USaveGame blobs reach restore without
	// passing through MigrateToCurrent, so they still carry the pre-widening slot count.
	if(Saved.Slots.Num()>Capacity||Saved.Revision<0||Saved.ActiveSlot<0||Saved.ActiveSlot>=Capacity)return false; TSet<FName> Seen;
	for(const auto& Slot:Saved.Slots){ if(Slot.IsEmpty())continue; if(Seen.Contains(Slot.CharacterId)||!IsKnownProfile(Slot.CharacterId))return false; Seen.Add(Slot.CharacterId); }
	Out=Saved; Out.Slots.SetNum(Capacity);
	if (Out.Slots[Out.ActiveSlot].IsEmpty())
	{
		Out.ActiveSlot = Out.Slots.IndexOfByPredicate([](const FHSRPartySlot& Slot){ return !Slot.IsEmpty(); });
		if (Out.ActiveSlot == INDEX_NONE) Out.ActiveSlot = 0;
	}
	return true;
}
