#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../Data/Definitions/HSREncounterDefinition.h"
#include "../UI/HSRChallengeDirectoryViewModel.h"
#include "../UI/HSRChallengeDirectoryWidget.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRChallengeDirectorySelectionFailureMatrixTest,
	"HSR.UI.ChallengeDirectory.SelectionFailureMatrix", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRChallengeDirectorySelectionFailureMatrixTest::RunTest(const FString&)
{
	UHSREncounterDefinition* Valid = NewObject<UHSREncounterDefinition>();
	Valid->EncounterId = TEXT("Encounter.Valid");
	Valid->EnemyDefinitionId = TEXT("Enemy.Valid");
	Valid->BattleMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/Map_Battle.Map_Battle")));

	UHSREncounterDefinition* LockedDefinition = NewObject<UHSREncounterDefinition>();
	LockedDefinition->EncounterId = TEXT("Encounter.Locked");
	LockedDefinition->EnemyDefinitionId = TEXT("Enemy.Locked");
	LockedDefinition->BattleMap = Valid->BattleMap;

	UHSREncounterDefinition* SecondValid = NewObject<UHSREncounterDefinition>();
	SecondValid->EncounterId = TEXT("Encounter.SecondValid");
	SecondValid->EnemyDefinitionId = TEXT("Enemy.SecondValid");
	SecondValid->BattleMap = Valid->BattleMap;

	UHSREncounterDefinition* InvalidDefinition = NewObject<UHSREncounterDefinition>();
	InvalidDefinition->EncounterId = TEXT("Encounter.Invalid");
	InvalidDefinition->EnemyDefinitionId = NAME_None;
	InvalidDefinition->BattleMap = Valid->BattleMap;

	FHSRChallengeDirectorySource ValidSource;
	ValidSource.Definition = Valid;
	FHSRChallengeDirectorySource LockedSource;
	LockedSource.Definition = LockedDefinition;
	FHSRChallengeDirectorySource SecondValidSource;
	SecondValidSource.Definition = SecondValid;
	LockedSource.bUnlocked = false;
	FHSRChallengeDirectorySource InvalidSource;
	InvalidSource.Definition = InvalidDefinition;

	UHSRChallengeDirectoryWidget* Widget = NewObject<UHSRChallengeDirectoryWidget>();
	TestNotNull(TEXT("challenge widget is created"), Widget);
	if (!Widget)
	{
		return false;
	}

	TestEqual(TEXT("directory initializes"), Widget->InitializeDirectory(
		{ ValidSource, LockedSource, SecondValidSource, InvalidSource }), EHSRChallengeDirectoryResult::Success);
	TestEqual(TEXT("valid selection succeeds"), Widget->SelectChallenge(TEXT("Encounter.Valid")),
		EHSRChallengeDirectoryResult::Success);
	TestEqual(TEXT("valid selection is retained"), Widget->GetSelectedEncounterId(), FName(TEXT("Encounter.Valid")));
	FHSREncounterRequest UnavailableTemplate;
	TestEqual(TEXT("missing transition rejects valid Enter"), Widget->BuildChallengeTemplate(
		TEXT("Encounter.SecondValid"), EHSREncounterInitiative::Player, UnavailableTemplate).ResultType,
		EHSREncounterResultType::InvalidRequest);
	TestEqual(TEXT("missing transition preserves selection"), Widget->GetSelectedEncounterId(), FName(TEXT("Encounter.Valid")));

	TestEqual(TEXT("locked selection is rejected"), Widget->SelectChallenge(TEXT("Encounter.Locked")),
		EHSRChallengeDirectoryResult::Locked);
	TestEqual(TEXT("locked failure preserves selection"), Widget->GetSelectedEncounterId(), FName(TEXT("Encounter.Valid")));
	TestEqual(TEXT("invalid selection is rejected"), Widget->SelectChallenge(TEXT("Encounter.Invalid")),
		EHSRChallengeDirectoryResult::InvalidDefinition);
	TestEqual(TEXT("invalid failure preserves selection"), Widget->GetSelectedEncounterId(), FName(TEXT("Encounter.Valid")));
	TestEqual(TEXT("unknown selection is rejected"), Widget->SelectChallenge(TEXT("Encounter.Unknown")),
		EHSRChallengeDirectoryResult::UnknownChallenge);
	TestEqual(TEXT("unknown failure preserves selection"), Widget->GetSelectedEncounterId(), FName(TEXT("Encounter.Valid")));

	FHSREncounterRequest FailedTemplate;
	FailedTemplate.EncounterId = TEXT("Encounter.Prior");
	TestEqual(TEXT("failed Enter is rejected"), Widget->BuildChallengeTemplate(
		TEXT("Encounter.Locked"), EHSREncounterInitiative::Player, FailedTemplate).ResultType,
		EHSREncounterResultType::InvalidDefinition);
	TestEqual(TEXT("failed Enter preserves selection"), Widget->GetSelectedEncounterId(), FName(TEXT("Encounter.Valid")));

	UHSRChallengeDirectoryWidget* UninitializedWidget = NewObject<UHSRChallengeDirectoryWidget>();
	TestEqual(TEXT("uninitialized selection is controlled"), UninitializedWidget->SelectChallenge(TEXT("Encounter.Valid")),
		EHSRChallengeDirectoryResult::EmptyDirectory);
	TestEqual(TEXT("uninitialized widget has no selection"), UninitializedWidget->GetSelectedEncounterId(), NAME_None);
	return true;
}

#endif
