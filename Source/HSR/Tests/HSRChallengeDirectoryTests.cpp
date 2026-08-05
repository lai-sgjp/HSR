#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../Data/Definitions/HSREncounterDefinition.h"
#include "../UI/HSRChallengeDirectoryViewModel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRChallengeDirectoryProjectionTest,
	"HSR.UI.ChallengeDirectory.Projection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRChallengeDirectoryProjectionTest::RunTest(const FString&)
{
	UHSREncounterDefinition* Valid = NewObject<UHSREncounterDefinition>();
	Valid->EncounterId = TEXT("Encounter.A");
	Valid->EnemyDefinitionId = TEXT("Enemy.A");
	Valid->BattleMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/Map_Battle.Map_Battle")));

	UHSREncounterDefinition* Duplicate = NewObject<UHSREncounterDefinition>();
	Duplicate->EncounterId = Valid->EncounterId;
	Duplicate->EnemyDefinitionId = TEXT("Enemy.Duplicate");
	Duplicate->BattleMap = Valid->BattleMap;

	FHSRChallengeDirectorySource Available; Available.Definition = Valid; Available.bUnlocked = true;
	FHSRChallengeDirectorySource Locked; Locked.Definition = Duplicate; Locked.bUnlocked = false;
	UHSRChallengeDirectoryViewModel* ViewModel = NewObject<UHSRChallengeDirectoryViewModel>();
	TestEqual(TEXT("projection accepts source list"), ViewModel->Initialize({ Available, Locked }),
		EHSRChallengeDirectoryResult::Success);
	const FHSRChallengeDirectorySnapshot Snapshot = ViewModel->GetSnapshot();
	TestEqual(TEXT("duplicate id is deterministic single entry"), Snapshot.Entries.Num(), 1);
	TestTrue(TEXT("valid entry available"), Snapshot.Entries[0].bAvailable);
	UHSREncounterDefinition* Selected = nullptr;
	TestEqual(TEXT("available entry resolves"), ViewModel->ResolveSelection(TEXT("Encounter.A"), Selected),
		EHSRChallengeDirectoryResult::Success);
	TestEqual(TEXT("resolved definition preserved"), Selected, Valid);
	TestEqual(TEXT("unknown entry rejected"), ViewModel->ResolveSelection(TEXT("Encounter.Missing"), Selected),
		EHSRChallengeDirectoryResult::UnknownChallenge);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRChallengeDirectoryInvalidEntriesTest,
	"HSR.UI.ChallengeDirectory.InvalidEntries", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRChallengeDirectoryInvalidEntriesTest::RunTest(const FString&)
{
	UHSREncounterDefinition* LockedDefinition = NewObject<UHSREncounterDefinition>();
	LockedDefinition->EncounterId = TEXT("Encounter.Locked");
	LockedDefinition->EnemyDefinitionId = TEXT("Enemy.Locked");
	LockedDefinition->BattleMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/Map_Battle.Map_Battle")));
	FHSRChallengeDirectorySource Locked; Locked.Definition = LockedDefinition; Locked.bUnlocked = false;
	FHSRChallengeDirectorySource Missing;
	UHSRChallengeDirectoryViewModel* ViewModel = NewObject<UHSRChallengeDirectoryViewModel>();
	TestEqual(TEXT("mixed invalid source remains projectable"), ViewModel->Initialize({ Missing, Locked }),
		EHSRChallengeDirectoryResult::Success);
	TestEqual(TEXT("only stable id entry projected"), ViewModel->GetSnapshot().Entries.Num(), 1);
	TestFalse(TEXT("locked entry unavailable"), ViewModel->GetSnapshot().Entries[0].bAvailable);
	UHSREncounterDefinition* Selected = nullptr;
	TestEqual(TEXT("locked selection rejected"), ViewModel->ResolveSelection(TEXT("Encounter.Locked"), Selected),
		EHSRChallengeDirectoryResult::Locked);
	return true;
}

#endif
