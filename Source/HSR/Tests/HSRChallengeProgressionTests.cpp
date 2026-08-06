#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../Challenge/HSRChallengeProgressionSubsystem.h"
#include "../Data/Definitions/HSREncounterDefinition.h"
#include "../UI/HSRChallengeDirectoryViewModel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRChallengeProgressionIdempotenceTest,
	"HSR.Challenge.Progression.Idempotence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRChallengeProgressionIdempotenceTest::RunTest(const FString&)
{
	UHSRChallengeProgressionSubsystem* Progression = NewObject<UHSRChallengeProgressionSubsystem>();
	TestNotNull(TEXT("progression subsystem fixture exists"), Progression);
	if (!Progression)
	{
		return false;
	}

	int32 ChangeCount = 0;
	Progression->OnProgressionChanged().AddLambda([&](const FHSRChallengeProgressionSnapshot&)
	{
		++ChangeCount;
	});

	TestEqual(TEXT("first completion succeeds"), Progression->CompleteEncounter(TEXT("Encounter.Base")),
		EHSRChallengeProgressionResult::Success);
	TestTrue(TEXT("first completion is visible"), Progression->IsCompleted(TEXT("Encounter.Base")));
	TestEqual(TEXT("first completion advances revision"), Progression->GetSnapshot().Revision, int64(1));
	TestEqual(TEXT("first completion broadcasts once"), ChangeCount, 1);

	TestEqual(TEXT("duplicate completion is a no-op"), Progression->CompleteEncounter(TEXT("Encounter.Base")),
		EHSRChallengeProgressionResult::NoOp);
	TestEqual(TEXT("duplicate completion keeps revision"), Progression->GetSnapshot().Revision, int64(1));
	TestEqual(TEXT("duplicate completion does not broadcast"), ChangeCount, 1);
	TestEqual(TEXT("empty completion ID is rejected"), Progression->CompleteEncounter(NAME_None),
		EHSRChallengeProgressionResult::InvalidEncounterId);
	TestEqual(TEXT("invalid completion does not mutate revision"), Progression->GetSnapshot().Revision, int64(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRChallengeProgressionProjectionTest,
	"HSR.Challenge.Progression.DirectoryProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRChallengeProgressionProjectionTest::RunTest(const FString&)
{
	UHSRChallengeProgressionSubsystem* Progression = NewObject<UHSRChallengeProgressionSubsystem>();
	UHSREncounterDefinition* Base = NewObject<UHSREncounterDefinition>();
	Base->EncounterId = TEXT("Encounter.Base");
	Base->EnemyDefinitionId = TEXT("Enemy.Base");
	Base->BattleMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/Map_Battle.Map_Battle")));
	UHSREncounterDefinition* Locked = NewObject<UHSREncounterDefinition>();
	Locked->EncounterId = TEXT("Encounter.Locked");
	Locked->EnemyDefinitionId = TEXT("Enemy.Locked");
	Locked->BattleMap = Base->BattleMap;
	Locked->PrerequisiteEncounterIds.Add(Base->EncounterId);

	FHSRChallengeDirectorySource BaseSource;
	BaseSource.Definition = Base;
	BaseSource.bUnlocked = false;
	FHSRChallengeDirectorySource LockedSource;
	LockedSource.Definition = Locked;
	LockedSource.bUnlocked = true;
	UHSRChallengeDirectoryViewModel* ViewModel = NewObject<UHSRChallengeDirectoryViewModel>();

	TestEqual(TEXT("projection initializes"), ViewModel->Initialize({ BaseSource, LockedSource }, Progression),
		EHSRChallengeDirectoryResult::Success);
	FHSRChallengeDirectorySnapshot Snapshot = ViewModel->GetSnapshot();
	TestEqual(TEXT("two definitions project"), Snapshot.Entries.Num(), 2);
	if (Snapshot.Entries.Num() == 2)
	{
		TestEqual(TEXT("base starts available despite static lock"), Snapshot.Entries[0].Status,
			EHSRChallengeDirectoryStatus::Available);
		TestEqual(TEXT("dependent starts locked"), Snapshot.Entries[1].Status,
			EHSRChallengeDirectoryStatus::Locked);
	}

	TestEqual(TEXT("base completion succeeds"), Progression->CompleteEncounter(Base->EncounterId),
		EHSRChallengeProgressionResult::Success);
	ViewModel->Refresh();
	Snapshot = ViewModel->GetSnapshot();
	if (Snapshot.Entries.Num() == 2)
	{
		TestEqual(TEXT("base becomes completed"), Snapshot.Entries[0].Status,
			EHSRChallengeDirectoryStatus::Completed);
		TestEqual(TEXT("dependent becomes available"), Snapshot.Entries[1].Status,
			EHSRChallengeDirectoryStatus::Available);
	}
	return true;
}

#endif
