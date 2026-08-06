#include "HSRChallengeDirectoryViewModel.h"

#include "../Challenge/HSRChallengeProgressionSubsystem.h"
#include "../Data/Definitions/HSREncounterDefinition.h"

EHSRChallengeDirectoryResult UHSRChallengeDirectoryViewModel::Initialize(
	const TArray<FHSRChallengeDirectorySource>& Sources,
	UHSRChallengeProgressionSubsystem* InProgression)
{
	ConfiguredSources = Sources;
	Progression = InProgression;
	DefinitionsById.Reset();

	for (const FHSRChallengeDirectorySource& Source : ConfiguredSources)
	{
		UHSREncounterDefinition* Definition = Source.Definition;
		if (!Definition || Definition->EncounterId.IsNone() || DefinitionsById.Contains(Definition->EncounterId))
		{
			continue;
		}

		DefinitionsById.Add(Definition->EncounterId, Definition);
	}

	return Refresh();
}

EHSRChallengeDirectoryResult UHSRChallengeDirectoryViewModel::Refresh()
{
	Snapshot = FHSRChallengeDirectorySnapshot();
	AvailableIds.Reset();
	TSet<FName> ProjectedIds;

	for (const FHSRChallengeDirectorySource& Source : ConfiguredSources)
	{
		UHSREncounterDefinition* Definition = Source.Definition;
		if (!Definition || Definition->EncounterId.IsNone() || ProjectedIds.Contains(Definition->EncounterId))
		{
			continue;
		}
		ProjectedIds.Add(Definition->EncounterId);

		FHSRChallengeDirectoryEntry& Entry = Snapshot.Entries.AddDefaulted_GetRef();
		Entry.EncounterId = Definition->EncounterId;
		Entry.EnemyDefinitionId = Definition->EnemyDefinitionId;
		Entry.BattleMapPath = Definition->BattleMap.IsNull()
			? NAME_None : FName(*Definition->BattleMap.GetLongPackageName());
		const bool bDefinitionValid = !Entry.EnemyDefinitionId.IsNone() && !Entry.BattleMapPath.IsNone();
		bool bPrerequisitesValid = true;
		bool bPrerequisitesComplete = true;
		if (bDefinitionValid)
		{
			for (const FName PrerequisiteId : Definition->PrerequisiteEncounterIds)
			{
				const TObjectPtr<UHSREncounterDefinition>* Prerequisite = DefinitionsById.Find(PrerequisiteId);
				if (PrerequisiteId.IsNone() || !Prerequisite || !Prerequisite->Get()
					|| (*Prerequisite)->EnemyDefinitionId.IsNone() || (*Prerequisite)->BattleMap.IsNull())
				{
					bPrerequisitesValid = false;
					break;
				}
				if (!Progression.IsValid() || !Progression->IsCompleted(PrerequisiteId))
				{
					bPrerequisitesComplete = false;
				}
			}
		}

		if (!bDefinitionValid || !bPrerequisitesValid)
		{
			Entry.Status = EHSRChallengeDirectoryStatus::Unavailable;
			Entry.Diagnostic = FText::FromString(TEXT("Challenge is unavailable."));
		}
		else if (Progression.IsValid() && Progression->IsCompleted(Entry.EncounterId))
		{
			Entry.Status = EHSRChallengeDirectoryStatus::Completed;
			Entry.Diagnostic = FText::FromString(TEXT("Challenge is completed."));
		}
		else if (!bPrerequisitesComplete)
		{
			Entry.Status = EHSRChallengeDirectoryStatus::Locked;
			Entry.Diagnostic = FText::FromString(TEXT("Challenge prerequisites are incomplete."));
		}
		else
		{
			Entry.Status = EHSRChallengeDirectoryStatus::Available;
			Entry.Diagnostic = FText::GetEmpty();
			AvailableIds.Add(Entry.EncounterId);
		}
		Entry.bCompleted = Entry.Status == EHSRChallengeDirectoryStatus::Completed;
		Entry.bUnlocked = Entry.Status == EHSRChallengeDirectoryStatus::Available;
		Entry.bAvailable = Entry.Status == EHSRChallengeDirectoryStatus::Available;
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
		if (!Entry)
		{
			return EHSRChallengeDirectoryResult::InvalidDefinition;
		}
		if (Entry->Status == EHSRChallengeDirectoryStatus::Locked)
		{
			return EHSRChallengeDirectoryResult::Locked;
		}
		if (Entry->Status == EHSRChallengeDirectoryStatus::Completed)
		{
			return EHSRChallengeDirectoryResult::Completed;
		}
		return EHSRChallengeDirectoryResult::InvalidDefinition;
	}
	OutDefinition = Found->Get();
	return EHSRChallengeDirectoryResult::Success;
}
