#include "HSRCharacterProgressionLibrary.h"

#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "Curves/CurveFloat.h"

namespace HSRCharacterProgression
{
	static bool IsWholeNonNegative(float Value)
	{
		return FMath::IsFinite(Value) && Value >= 0.0f && FMath::IsNearlyEqual(Value, FMath::RoundToFloat(Value));
	}
}

EHSRCharacterProgressionResult UHSRCharacterProgressionLibrary::ValidateDefinitionAndCurve(const UHSRCharacterDefinition* Definition, const UCurveFloat*& OutCurve)
{
	OutCurve = nullptr;
	if (Definition == nullptr)
	{
		return EHSRCharacterProgressionResult::MissingDefinition;
	}
	if (Definition->CharacterId.IsNone())
	{
		return EHSRCharacterProgressionResult::InvalidCharacterId;
	}
	if (Definition->MaxLevel < 1)
	{
		return EHSRCharacterProgressionResult::InvalidRuntimeState;
	}
	for (const TPair<FName, int32>& Skill : Definition->SkillMaxLevels)
	{
		if (Skill.Key.IsNone() || Skill.Value < 1) return EHSRCharacterProgressionResult::InvalidSkillLevel;
	}

	OutCurve = Definition->CumulativeExperienceCurve.Get();
	if (OutCurve == nullptr)
	{
		return EHSRCharacterProgressionResult::MissingExperienceCurve;
	}

	float PreviousRequirement = 0.0f;
	for (int32 Level = 2; Level <= Definition->MaxLevel; ++Level)
	{
		const FKeyHandle Key = OutCurve->FloatCurve.FindKey(static_cast<float>(Level), 0.0f);
		if (!Key.IsValid()) return EHSRCharacterProgressionResult::InvalidExperienceCurve;
		const float Requirement = OutCurve->FloatCurve.GetKeyValue(Key);
		if (!HSRCharacterProgression::IsWholeNonNegative(Requirement) || Requirement <= PreviousRequirement || static_cast<double>(Requirement) >= 2147483648.0)
		{
			return EHSRCharacterProgressionResult::InvalidExperienceCurve;
		}
		PreviousRequirement = Requirement;
	}
	return EHSRCharacterProgressionResult::Success;
}

EHSRCharacterProgressionResult UHSRCharacterProgressionLibrary::GetLevelForExperience(const UHSRCharacterDefinition& Definition, const UCurveFloat& Curve, int32 Experience, int32& OutLevel)
{
	OutLevel = 1;
	if (Experience < 0)
	{
		return EHSRCharacterProgressionResult::InvalidRuntimeState;
	}

	for (int32 DestinationLevel = 2; DestinationLevel <= Definition.MaxLevel; ++DestinationLevel)
	{
		if (Experience < FMath::RoundToInt(Curve.GetFloatValue(static_cast<float>(DestinationLevel))))
		{
			break;
		}
		OutLevel = DestinationLevel;
	}
	return EHSRCharacterProgressionResult::Success;
}

EHSRCharacterProgressionResult UHSRCharacterProgressionLibrary::ValidateRuntimeState(const UHSRCharacterDefinition* Definition, const FHSRCharacterRuntimeState& State)
{
	const UCurveFloat* Curve = nullptr;
	const EHSRCharacterProgressionResult DefinitionResult = ValidateDefinitionAndCurve(Definition, Curve);
	if (DefinitionResult != EHSRCharacterProgressionResult::Success)
	{
		return DefinitionResult;
	}
	if (State.CharacterId != Definition->CharacterId || State.Level < 1 || State.Level > Definition->MaxLevel || State.Experience < 0 || State.Ascension < 0)
	{
		return EHSRCharacterProgressionResult::InvalidRuntimeState;
	}
	for (const TPair<FName, int32>& Entry : State.SkillLevels)
	{
		const int32* MaxSkillLevel = Definition->SkillMaxLevels.Find(Entry.Key);
		if (!MaxSkillLevel || Entry.Value < 1 || Entry.Value > *MaxSkillLevel)
		{
			return EHSRCharacterProgressionResult::InvalidRuntimeState;
		}
	}

	int32 ExpectedLevel = 1;
	GetLevelForExperience(*Definition, *Curve, State.Experience, ExpectedLevel);
	return ExpectedLevel == State.Level ? EHSRCharacterProgressionResult::Success : EHSRCharacterProgressionResult::InvalidRuntimeState;
}

EHSRCharacterProgressionResult UHSRCharacterProgressionLibrary::TryGrantExperience(const UHSRCharacterDefinition* Definition, int32 ExperienceToGrant, FHSRCharacterRuntimeState& InOutState)
{
	if (ExperienceToGrant < 0)
	{
		return EHSRCharacterProgressionResult::NegativeExperience;
	}
	const EHSRCharacterProgressionResult ValidationResult = ValidateRuntimeState(Definition, InOutState);
	if (ValidationResult != EHSRCharacterProgressionResult::Success)
	{
		return ValidationResult;
	}
	if (ExperienceToGrant == 0)
	{
		return EHSRCharacterProgressionResult::Success;
	}

	const int64 NewExperience = static_cast<int64>(InOutState.Experience) + ExperienceToGrant;
	if (NewExperience > MAX_int32)
	{
		return EHSRCharacterProgressionResult::ExperienceOverflow;
	}

	const UCurveFloat* Curve = Definition->CumulativeExperienceCurve.Get();
	FHSRCharacterRuntimeState Candidate = InOutState;
	Candidate.Experience = static_cast<int32>(NewExperience);
	GetLevelForExperience(*Definition, *Curve, Candidate.Experience, Candidate.Level);
	InOutState = MoveTemp(Candidate);
	return EHSRCharacterProgressionResult::Success;
}

EHSRCharacterProgressionResult UHSRCharacterProgressionLibrary::TrySetSkillLevel(const UHSRCharacterDefinition* Definition, FName SkillId, int32 SkillLevel, FHSRCharacterRuntimeState& InOutState)
{
	if (Definition == nullptr || SkillId.IsNone() || !Definition->SkillMaxLevels.Contains(SkillId))
	{
		return EHSRCharacterProgressionResult::InvalidSkillId;
	}
	if (SkillLevel < 1 || SkillLevel > Definition->SkillMaxLevels.FindRef(SkillId))
	{
		return EHSRCharacterProgressionResult::InvalidSkillLevel;
	}
	FHSRCharacterRuntimeState Candidate = InOutState;
	Candidate.SkillLevels.Add(SkillId, SkillLevel);
	InOutState = MoveTemp(Candidate);
	return EHSRCharacterProgressionResult::Success;
}
