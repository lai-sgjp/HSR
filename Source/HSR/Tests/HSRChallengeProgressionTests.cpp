#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "../Challenge/HSRChallengeProgressionSubsystem.h"
#include "../Data/Definitions/HSREncounterDefinition.h"
#include "../Save/HSRSaveVersion.h"
#include "../UI/HSRChallengeDirectoryViewModel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRChallengeProgressionIdempotenceTest,
	"HSR.Challenge.Progression.Idempotence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRChallengeProgressionIdempotenceTest::RunTest(const FString&)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UHSRChallengeProgressionSubsystem* Progression = NewObject<UHSRChallengeProgressionSubsystem>(GameInstance);
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
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UHSRChallengeProgressionSubsystem* Progression = NewObject<UHSRChallengeProgressionSubsystem>(GameInstance);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRChallengeProgressionSaveProjectionTest,
	"HSR.Challenge.Progression.SaveProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRChallengeProgressionSaveProjectionTest::RunTest(const FString&)
{
	FHSRSaveData Data;
	Data.SchemaVersion = HSRSaveVersion::CurrentSchema;
	Data.PartySlots.SetNum(HSRSaveVersion::PartySlotCount);
	Data.ChallengeProgression.CompletedEncounterIds = { TEXT("Encounter.Next"), TEXT("Encounter.Base") };
	Data.ChallengeProgression.Revision = 2;

	TArray<uint8> Payload;
	TestTrue(TEXT("schema 8 challenge progression encodes"),
		HSRSaveVersion::EncodeCanonicalPayload(Data, Payload));
	FHSRSaveData Decoded;
	TestEqual(TEXT("schema 8 challenge progression decodes"),
		HSRSaveVersion::DecodeCanonicalPayload(Payload, Decoded),
		EHSRSaveDecodeResult::Success);
	TestEqual(TEXT("challenge progression revision round trips"),
		Decoded.ChallengeProgression.Revision, int64(2));
	TestEqual(TEXT("challenge progression rows round trip"),
		Decoded.ChallengeProgression.CompletedEncounterIds.Num(), 2);
	if (Decoded.ChallengeProgression.CompletedEncounterIds.Num() == 2)
	{
		TestEqual(TEXT("challenge rows use canonical order"),
			Decoded.ChallengeProgression.CompletedEncounterIds[0], FName(TEXT("encounter.base")));
		TestEqual(TEXT("challenge rows preserve second ID"),
			Decoded.ChallengeProgression.CompletedEncounterIds[1], FName(TEXT("encounter.next")));
	}

	FHSRSaveData Duplicate = Data;
	Duplicate.ChallengeProgression.CompletedEncounterIds.Add(TEXT("Encounter.Base"));
	TestFalse(TEXT("duplicate challenge completion rows reject"),
		HSRSaveVersion::EncodeCanonicalPayload(Duplicate, Payload));
	return true;
}

#endif
