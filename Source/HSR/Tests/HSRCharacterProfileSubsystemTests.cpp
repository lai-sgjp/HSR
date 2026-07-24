#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Curves/CurveFloat.h"
#include "Engine/GameInstance.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Data/Definitions/HSRCharacterCatalog.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Progression/HSRCharacterStatAggregator.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRCharacterProfileSubsystemTest, "HSR.Progression.Profile.Authority", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRCharacterProfileSubsystemTest::RunTest(const FString& Parameters)
{
	auto MakeDefinition = [](FName Id)
	{
		UHSRCharacterDefinition* Definition = NewObject<UHSRCharacterDefinition>(); Definition->CharacterId = Id; Definition->MaxLevel = 2; Definition->SkillMaxLevels.Add(TEXT("Skill.Basic"), 3);
		UCurveFloat* Curve = NewObject<UCurveFloat>(Definition); Curve->FloatCurve.AddKey(2.0f, 100.0f); Definition->CumulativeExperienceCurve = Curve; return Definition;
	};
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UHSRCharacterProfileSubsystem* Subsystem = NewObject<UHSRCharacterProfileSubsystem>(GameInstance);
	UHSRCharacterDefinition* A = MakeDefinition(TEXT("A")); UHSRCharacterDefinition* B = MakeDefinition(TEXT("B"));
	int32 Events = 0; int64 LastRevision = -1; Subsystem->OnProfileChanged().AddLambda([&Events, &LastRevision](FName, int64 Revision){ ++Events; LastRevision = Revision; });
	TestEqual(TEXT("Register A"), Subsystem->RegisterDefinition(A), EHSRCharacterProfileResult::Success);
	TestEqual(TEXT("Register B"), Subsystem->RegisterDefinition(B), EHSRCharacterProfileResult::Success);
	TestEqual(TEXT("Duplicate registration rejected"), Subsystem->RegisterDefinition(A), EHSRCharacterProfileResult::DefinitionAlreadyRegistered);
	UHSRCharacterDefinition* C = MakeDefinition(TEXT("C")); UHSRCharacterDefinition* Bad = MakeDefinition(NAME_None);
	TArray<const UHSRCharacterDefinition*> BadBatch{C, Bad}; TestEqual(TEXT("Bad batch rejected atomically"), Subsystem->RegisterDefinitions(BadBatch), EHSRCharacterProfileResult::ProgressionRejected);
	FHSRCharacterProfileSnapshot MissingSnapshot; TestFalse(TEXT("Bad batch did not commit C"), Subsystem->GetProfileSnapshot(TEXT("C"), MissingSnapshot));
	TestFalse(TEXT("Unknown snapshot returns false"), Subsystem->GetProfileSnapshot(TEXT("Unknown"), MissingSnapshot));
	TestEqual(TEXT("Unknown mutation rejected"), Subsystem->GrantExperience(TEXT("Unknown"), 1), EHSRCharacterProfileResult::ProfileNotFound);
	TestEqual(TEXT("Mutate A"), Subsystem->GrantExperience(TEXT("A"), 100), EHSRCharacterProfileResult::Success);
	TestEqual(TEXT("Zero is no-op"), Subsystem->GrantExperience(TEXT("A"), 0), EHSRCharacterProfileResult::Success);
	TestEqual(TEXT("Invalid skill rejected"), Subsystem->SetSkillLevel(TEXT("A"), NAME_None, 1), EHSRCharacterProfileResult::ProgressionRejected);
	TestEqual(TEXT("Valid skill mutation"), Subsystem->SetSkillLevel(TEXT("A"), TEXT("Skill.Basic"), 2), EHSRCharacterProfileResult::Success);
	TestEqual(TEXT("Same skill no-op"), Subsystem->SetSkillLevel(TEXT("A"), TEXT("Skill.Basic"), 2), EHSRCharacterProfileResult::Success);
	FHSRCharacterProfileSnapshot SA, SB; Subsystem->GetProfileSnapshot(TEXT("A"), SA); Subsystem->GetProfileSnapshot(TEXT("B"), SB);
	TestEqual(TEXT("Revision increments twice"), SA.RuntimeRevision, static_cast<int64>(2)); TestEqual(TEXT("Event twice"), Events, 2); TestEqual(TEXT("Event revision payload"), LastRevision, static_cast<int64>(2));
	TestEqual(TEXT("Profiles isolated"), SB.RuntimeState.Experience, 0); TestEqual(TEXT("B revision unchanged"), SB.RuntimeRevision, static_cast<int64>(0));
	SA.RuntimeState.Experience = 999; FHSRCharacterProfileSnapshot FreshA; Subsystem->GetProfileSnapshot(TEXT("A"), FreshA); TestEqual(TEXT("Snapshot copy is isolated"), FreshA.RuntimeState.Experience, 100);
	FHSRCharacterProgressionContext ContextA; TestTrue(TEXT("Profile builds character context"), Subsystem->GetProgressionContext(TEXT("A"), ContextA));
	TestEqual(TEXT("Context character"), ContextA.CharacterId, FName(TEXT("A"))); TestEqual(TEXT("Context revision"), ContextA.RuntimeRevision, static_cast<int64>(2));
	TestEqual(TEXT("Level growth bonus"), ContextA.ProgressionBonuses.MaxHealth, A->MaxHealthPerLevel);
	TestEqual(TEXT("Derived max health"), ContextA.DerivedStats.MaxHealth, A->BaseMaxHealth + A->MaxHealthPerLevel);
	FHSRCharacterProgressionContext UnchangedContext = ContextA; UHSRCharacterDefinition* InvalidStats = MakeDefinition(TEXT("InvalidStats")); InvalidStats->BaseSpeed = -1.0f;
	FHSRCharacterRuntimeState InvalidRuntime; InvalidRuntime.CharacterId = InvalidStats->CharacterId;
	TestFalse(TEXT("Invalid stats fail atomically"), UHSRCharacterStatAggregator::BuildContext(InvalidStats, InvalidRuntime, 0, UnchangedContext));
	TestEqual(TEXT("Failed aggregation leaves output"), UnchangedContext.CharacterId, ContextA.CharacterId);

	UHSRCharacterProfileSubsystem* CatalogSubsystem = NewObject<UHSRCharacterProfileSubsystem>(GameInstance);
	UHSRCharacterCatalog* Catalog = NewObject<UHSRCharacterCatalog>();
	TestEqual(TEXT("Null catalog class rejected"), CatalogSubsystem->RegisterLoadedCatalog(Catalog), EHSRCharacterProfileResult::Success);
	UHSRCharacterCatalog* NullEntryCatalog = NewObject<UHSRCharacterCatalog>(); NullEntryCatalog->Characters.Add(nullptr);
	AddExpectedError(TEXT("Reason=DefinitionCDO"), EAutomationExpectedErrorFlags::Contains, 1);
	TestEqual(TEXT("Null catalog entry rejected"), CatalogSubsystem->RegisterLoadedCatalog(NullEntryCatalog), EHSRCharacterProfileResult::AssetLoadFailed);
	UHSRCharacterCatalog* MissingCurveCatalog = NewObject<UHSRCharacterCatalog>(); MissingCurveCatalog->Characters.Add(UHSRCharacterDefinition::StaticClass());
	AddExpectedError(TEXT("Reason=ExperienceCurve"), EAutomationExpectedErrorFlags::Contains, 1);
	TestEqual(TEXT("Catalog CDO missing curve distinguished"), CatalogSubsystem->RegisterLoadedCatalog(MissingCurveCatalog), EHSRCharacterProfileResult::ExperienceCurveLoadFailed);
	return true;
}
#endif
