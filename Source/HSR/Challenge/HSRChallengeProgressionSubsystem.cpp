#include "HSRChallengeProgressionSubsystem.h"

namespace
{
	bool NameLess(const FName& A, const FName& B)
	{
		return A.ToString().ToLower().Compare(B.ToString().ToLower(), ESearchCase::CaseSensitive) < 0;
	}
}

void UHSRChallengeProgressionSubsystem::RebuildSnapshot()
{
	Snapshot.CompletedEncounterIds.Reset(CompletedEncounterIds.Num());
	for (const FName& EncounterId : CompletedEncounterIds)
	{
		Snapshot.CompletedEncounterIds.Add(EncounterId);
	}
	Snapshot.CompletedEncounterIds.Sort(NameLess);
}

EHSRChallengeProgressionResult UHSRChallengeProgressionSubsystem::CompleteEncounter(const FName EncounterId)
{
	if (!IsValidEncounterId(EncounterId))
	{
		return EHSRChallengeProgressionResult::InvalidEncounterId;
	}
	if (CompletedEncounterIds.Contains(EncounterId))
	{
		return EHSRChallengeProgressionResult::NoOp;
	}

	CompletedEncounterIds.Add(EncounterId);
	++Snapshot.Revision;
	RebuildSnapshot();
	ProgressionChanged.Broadcast(Snapshot);
	return EHSRChallengeProgressionResult::Success;
}

bool UHSRChallengeProgressionSubsystem::IsCompleted(const FName EncounterId) const
{
	return CompletedEncounterIds.Contains(EncounterId);
}

void UHSRChallengeProgressionSubsystem::ExportSaveData(FHSRChallengeProgressionSaveData& OutData) const
{
	OutData.CompletedEncounterIds = Snapshot.CompletedEncounterIds;
	OutData.Revision = Snapshot.Revision;
}

bool UHSRChallengeProgressionSubsystem::ValidateSaveData(const FHSRChallengeProgressionSaveData& Data)
{
	if (Data.Revision < 0)
	{
		return false;
	}

	TSet<FName> Seen;
	for (const FName EncounterId : Data.CompletedEncounterIds)
	{
		if (EncounterId.IsNone() || Seen.Contains(EncounterId))
		{
			return false;
		}
		Seen.Add(EncounterId);
	}
	return true;
}

bool UHSRChallengeProgressionSubsystem::PrepareRestore(
	const FHSRChallengeProgressionSaveData& Saved,
	FHSRChallengeProgressionSaveData& OutCandidate) const
{
	if (!ValidateSaveData(Saved))
	{
		return false;
	}
	OutCandidate = Saved;
	OutCandidate.CompletedEncounterIds.Sort(NameLess);
	return true;
}

bool UHSRChallengeProgressionSubsystem::IsRestoreDifferent(
	const FHSRChallengeProgressionSaveData& Candidate) const
{
	if (Snapshot.Revision != Candidate.Revision
		|| Snapshot.CompletedEncounterIds.Num() != Candidate.CompletedEncounterIds.Num())
	{
		return true;
	}

	TArray<FName> SortedCandidate = Candidate.CompletedEncounterIds;
	SortedCandidate.Sort(NameLess);
	return Snapshot.CompletedEncounterIds != SortedCandidate;
}

void UHSRChallengeProgressionSubsystem::CommitRestore(
	FHSRChallengeProgressionSaveData&& Candidate, const bool bNotify)
{
	CompletedEncounterIds.Reset();
	for (const FName EncounterId : Candidate.CompletedEncounterIds)
	{
		CompletedEncounterIds.Add(EncounterId);
	}
	Snapshot.Revision = Candidate.Revision;
	RebuildSnapshot();
	if (bNotify)
	{
		ProgressionChanged.Broadcast(Snapshot);
	}
}
