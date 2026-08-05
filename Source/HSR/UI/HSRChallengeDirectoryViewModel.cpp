#include "HSRChallengeDirectoryViewModel.h"

#include "../Data/Definitions/HSREncounterDefinition.h"

EHSRChallengeDirectoryResult UHSRChallengeDirectoryViewModel::Initialize(
	const TArray<FHSRChallengeDirectorySource>& Sources)
{
	Snapshot = FHSRChallengeDirectorySnapshot();
	DefinitionsById.Reset();
	AvailableIds.Reset();

	for (const FHSRChallengeDirectorySource& Source : Sources)
	{
		UHSREncounterDefinition* Definition = Source.Definition;
		if (!Definition || Definition->EncounterId.IsNone() || DefinitionsById.Contains(Definition->EncounterId))
		{
			continue;
		}

		DefinitionsById.Add(Definition->EncounterId, Definition);
		FHSRChallengeDirectoryEntry& Entry = Snapshot.Entries.AddDefaulted_GetRef();
		Entry.EncounterId = Definition->EncounterId;
		Entry.EnemyDefinitionId = Definition->EnemyDefinitionId;
		Entry.BattleMapPath = Definition->BattleMap.IsNull()
			? NAME_None : FName(*Definition->BattleMap.GetLongPackageName());
		Entry.bUnlocked = Source.bUnlocked;
		const bool bDefinitionValid = !Entry.EnemyDefinitionId.IsNone() && !Entry.BattleMapPath.IsNone();
		Entry.bAvailable = Source.bUnlocked && bDefinitionValid;
		if (!Source.bUnlocked)
		{
			Entry.Diagnostic = FText::FromString(TEXT("Challenge is locked."));
		}
		else if (!bDefinitionValid)
		{
			Entry.Diagnostic = FText::FromString(TEXT("Challenge definition is incomplete."));
		}
		else
		{
			AvailableIds.Add(Entry.EncounterId);
		}
	}

	Snapshot.Entries.Sort([](const FHSRChallengeDirectoryEntry& A, const FHSRChallengeDirectoryEntry& B)
	{
		return A.EncounterId.LexicalLess(B.EncounterId);
	});
	return Snapshot.Entries.IsEmpty() ? EHSRChallengeDirectoryResult::EmptyDirectory
		: EHSRChallengeDirectoryResult::Success;
}

EHSRChallengeDirectoryResult UHSRChallengeDirectoryViewModel::ResolveSelection(
	const FName EncounterId, UHSREncounterDefinition*& OutDefinition) const
{
	OutDefinition = nullptr;
	const TObjectPtr<UHSREncounterDefinition>* Found = DefinitionsById.Find(EncounterId);
	if (!Found)
	{
		return EHSRChallengeDirectoryResult::UnknownChallenge;
	}
	if (!AvailableIds.Contains(EncounterId))
	{
		const FHSRChallengeDirectoryEntry* Entry = Snapshot.Entries.FindByPredicate(
			[EncounterId](const FHSRChallengeDirectoryEntry& Candidate)
			{
				return Candidate.EncounterId == EncounterId;
			});
		return Entry && !Entry->bUnlocked ? EHSRChallengeDirectoryResult::Locked
			: EHSRChallengeDirectoryResult::InvalidDefinition;
	}
	OutDefinition = Found->Get();
	return EHSRChallengeDirectoryResult::Success;
}
