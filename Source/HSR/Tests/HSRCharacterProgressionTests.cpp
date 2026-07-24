#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Curves/CurveFloat.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Progression/HSRCharacterProgressionLibrary.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRCharacterProgressionTransactionTest, "HSR.Progression.Character.Transaction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRCharacterProgressionTransactionTest::RunTest(const FString& Parameters)
{
	UHSRCharacterDefinition* Definition = NewObject<UHSRCharacterDefinition>();
	Definition->CharacterId = TEXT("TestCharacter");
	Definition->MaxLevel = 3;
	Definition->SkillMaxLevels.Add(TEXT("Skill.Basic"), 5);
	UCurveFloat* Curve = NewObject<UCurveFloat>();
	Curve->FloatCurve.AddKey(2.0f, 100.0f);
	Curve->FloatCurve.AddKey(3.0f, 300.0f);
	Definition->CumulativeExperienceCurve = Curve;

	FHSRCharacterRuntimeState State;
	State.CharacterId = Definition->CharacterId;
	TestEqual(TEXT("Cross-level grant succeeds"), UHSRCharacterProgressionLibrary::TryGrantExperience(Definition, 300, State), EHSRCharacterProgressionResult::Success);
	TestEqual(TEXT("A single grant can cross multiple levels"), State.Level, 3);
	TestEqual(TEXT("Experience remains cumulative at cap"), State.Experience, 300);

	const FHSRCharacterRuntimeState BeforeFailure = State;
	TestEqual(TEXT("Negative experience is rejected"), UHSRCharacterProgressionLibrary::TryGrantExperience(Definition, -1, State), EHSRCharacterProgressionResult::NegativeExperience);
	TestEqual(TEXT("Rejected transaction has no side effect"), State.Experience, BeforeFailure.Experience);
	TestEqual(TEXT("Stable skill keys are accepted"), UHSRCharacterProgressionLibrary::TrySetSkillLevel(Definition, TEXT("Skill.Basic"), 2, State), EHSRCharacterProgressionResult::Success);
	TestEqual(TEXT("Skill level is keyed by FName"), State.SkillLevels.FindRef(TEXT("Skill.Basic")), 2);

	UHSRCharacterDefinition* MissingCurve = NewObject<UHSRCharacterDefinition>(); MissingCurve->CharacterId = TEXT("Missing"); MissingCurve->MaxLevel = 2;
	FHSRCharacterRuntimeState MissingState; MissingState.CharacterId = MissingCurve->CharacterId;
	TestEqual(TEXT("Missing curve rejected"), UHSRCharacterProgressionLibrary::TryGrantExperience(MissingCurve, 1, MissingState), EHSRCharacterProgressionResult::MissingExperienceCurve);
	TestEqual(TEXT("Missing curve unchanged"), MissingState.Experience, 0);

	const auto TestBadCurve = [this](const TCHAR* Label, float L2, float L3)
	{
		UHSRCharacterDefinition* Bad = NewObject<UHSRCharacterDefinition>(); Bad->CharacterId = TEXT("Bad"); Bad->MaxLevel = 3;
		UCurveFloat* BadCurve = NewObject<UCurveFloat>(Bad); BadCurve->FloatCurve.AddKey(2.0f, L2); BadCurve->FloatCurve.AddKey(3.0f, L3); Bad->CumulativeExperienceCurve = BadCurve;
		FHSRCharacterRuntimeState BadState; BadState.CharacterId = Bad->CharacterId;
		TestEqual(Label, UHSRCharacterProgressionLibrary::TryGrantExperience(Bad, 1, BadState), EHSRCharacterProgressionResult::InvalidExperienceCurve);
		TestEqual(FString(Label) + TEXT(" unchanged"), BadState.Experience, 0);
	};
	TestBadCurve(TEXT("Non-increasing curve rejected"), 100.0f, 100.0f);
	TestBadCurve(TEXT("Fractional curve rejected"), 100.5f, 300.0f);
	TestBadCurve(TEXT("Negative curve rejected"), -1.0f, 300.0f);
	TestBadCurve(TEXT("NaN curve rejected"), std::numeric_limits<float>::quiet_NaN(), 300.0f);
	TestBadCurve(TEXT("Infinite curve rejected"), std::numeric_limits<float>::infinity(), 300.0f);
	TestBadCurve(TEXT("2^31 boundary rejected"), 2147483648.0f, 2147483648.0f);
	UHSRCharacterDefinition* MissingKey = NewObject<UHSRCharacterDefinition>(); MissingKey->CharacterId = TEXT("MissingKey"); MissingKey->MaxLevel = 3;
	UCurveFloat* Interpolating = NewObject<UCurveFloat>(MissingKey); Interpolating->FloatCurve.AddKey(2.0f, 100.0f); Interpolating->FloatCurve.AddKey(4.0f, 500.0f); MissingKey->CumulativeExperienceCurve = Interpolating;
	FHSRCharacterRuntimeState MissingKeyState; MissingKeyState.CharacterId = MissingKey->CharacterId;
	TestEqual(TEXT("Missing exact level key rejected"), UHSRCharacterProgressionLibrary::ValidateRuntimeState(MissingKey, MissingKeyState), EHSRCharacterProgressionResult::InvalidExperienceCurve);
	UHSRCharacterDefinition* BadMax = NewObject<UHSRCharacterDefinition>(); BadMax->CharacterId = TEXT("BadMax"); BadMax->MaxLevel = 0; BadMax->CumulativeExperienceCurve = Curve;
	FHSRCharacterRuntimeState BadMaxState; BadMaxState.CharacterId = BadMax->CharacterId;
	TestEqual(TEXT("MaxLevel below one rejected"), UHSRCharacterProgressionLibrary::ValidateRuntimeState(BadMax, BadMaxState), EHSRCharacterProgressionResult::InvalidRuntimeState);

	FHSRCharacterRuntimeState Mismatch; Mismatch.CharacterId = TEXT("Other");
	TestEqual(TEXT("ID mismatch rejected"), UHSRCharacterProgressionLibrary::TryGrantExperience(Definition, 1, Mismatch), EHSRCharacterProgressionResult::InvalidRuntimeState);
	FHSRCharacterRuntimeState InvalidState; InvalidState.CharacterId = Definition->CharacterId; InvalidState.Level = 2;
	TestEqual(TEXT("State mismatch rejected"), UHSRCharacterProgressionLibrary::TryGrantExperience(Definition, 1, InvalidState), EHSRCharacterProgressionResult::InvalidRuntimeState);
	InvalidState.Level = 0; TestEqual(TEXT("Level zero rejected"), UHSRCharacterProgressionLibrary::ValidateRuntimeState(Definition, InvalidState), EHSRCharacterProgressionResult::InvalidRuntimeState);
	InvalidState.Level = 4; TestEqual(TEXT("Level above max rejected"), UHSRCharacterProgressionLibrary::ValidateRuntimeState(Definition, InvalidState), EHSRCharacterProgressionResult::InvalidRuntimeState);
	InvalidState.Level = 1; InvalidState.Ascension = -1; TestEqual(TEXT("Negative ascension rejected"), UHSRCharacterProgressionLibrary::ValidateRuntimeState(Definition, InvalidState), EHSRCharacterProgressionResult::InvalidRuntimeState);
	InvalidState.Ascension = 0; InvalidState.SkillLevels.Add(TEXT("Unknown.Stored"), 1); TestEqual(TEXT("Invalid stored skill rejected"), UHSRCharacterProgressionLibrary::ValidateRuntimeState(Definition, InvalidState), EHSRCharacterProgressionResult::InvalidRuntimeState);
	FHSRCharacterRuntimeState Overflow; Overflow.CharacterId = Definition->CharacterId; Overflow.Level = 3; Overflow.Experience = MAX_int32;
	TestEqual(TEXT("Overflow rejected"), UHSRCharacterProgressionLibrary::TryGrantExperience(Definition, 1, Overflow), EHSRCharacterProgressionResult::ExperienceOverflow);
	TestEqual(TEXT("Overflow unchanged"), Overflow.Experience, MAX_int32);
	TestEqual(TEXT("Zero at cap succeeds"), UHSRCharacterProgressionLibrary::TryGrantExperience(Definition, 0, Overflow), EHSRCharacterProgressionResult::Success);
	TestEqual(TEXT("None skill rejected"), UHSRCharacterProgressionLibrary::TrySetSkillLevel(Definition, NAME_None, 1, State), EHSRCharacterProgressionResult::InvalidSkillId);
	TestEqual(TEXT("Unknown skill rejected"), UHSRCharacterProgressionLibrary::TrySetSkillLevel(Definition, TEXT("Skill.Bad"), 1, State), EHSRCharacterProgressionResult::InvalidSkillId);
	TestEqual(TEXT("Zero skill level rejected"), UHSRCharacterProgressionLibrary::TrySetSkillLevel(Definition, TEXT("Skill.Basic"), 0, State), EHSRCharacterProgressionResult::InvalidSkillLevel);
	TestEqual(TEXT("Above max skill rejected"), UHSRCharacterProgressionLibrary::TrySetSkillLevel(Definition, TEXT("Skill.Basic"), 6, State), EHSRCharacterProgressionResult::InvalidSkillLevel);
	return true;
}

#endif
