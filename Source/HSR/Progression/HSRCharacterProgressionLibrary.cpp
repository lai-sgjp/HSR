#include "HSRCharacterProgressionLibrary.h"

#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "Curves/CurveFloat.h"

namespace HSRCharacterProgression
{
	// 判断经验曲线上的一个关键值是否为「合法的累计经验需求」：
	// 必须有限、非负、且是整数（RoundToFloat 后与自身几乎相等）。
	// 经验曲线存储的是「升到某级所需的累计经验」，小数会破坏 GetLevelForExperience
	// 的整数比较逻辑，因此这里强制要求整数值。
	static bool IsWholeNonNegative(float Value)
	{
		return FMath::IsFinite(Value) && Value >= 0.0f
			&& FMath::IsNearlyEqual(Value, FMath::RoundToFloat(Value));
	}
}

// ValidateDefinitionAndCurve：校验角色定义及其累计经验曲线的一致性。
// 这是所有升级/技能操作的公共前置校验：定义 ID 非空、最大等级 >= 1、技能上限合法、
// 曲线对象存在，并且曲线上「2..MaxLevel」每个整数等级键都存在、需求值合法且严格递增。
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
		if (Skill.Key.IsNone() || Skill.Value < 1)
		{
			return EHSRCharacterProgressionResult::InvalidSkillLevel;
		}
	}

	OutCurve = Definition->CumulativeExperienceCurve.Get();
	if (OutCurve == nullptr)
	{
		return EHSRCharacterProgressionResult::MissingExperienceCurve;
	}

	// 逐级校验：每个等级键必须存在，需求值必须是合法整数，且必须比上一级严格更大
	// （经验曲线不允许平台期或倒退）。上限 2^31-1 防止 int32 溢出。
	float PreviousRequirement = 0.0f;
	for (int32 Level = 2; Level <= Definition->MaxLevel; ++Level)
	{
		const FKeyHandle Key = OutCurve->FloatCurve.FindKey(static_cast<float>(Level), 0.0f);
		if (!Key.IsValid())
		{
			return EHSRCharacterProgressionResult::InvalidExperienceCurve;
		}
		const float Requirement = OutCurve->FloatCurve.GetKeyValue(Key);
		if (!HSRCharacterProgression::IsWholeNonNegative(Requirement)
			|| Requirement <= PreviousRequirement
			|| static_cast<double>(Requirement) >= 2147483648.0)
		{
			return EHSRCharacterProgressionResult::InvalidExperienceCurve;
		}
		PreviousRequirement = Requirement;
	}
	return EHSRCharacterProgressionResult::Success;
}

// GetLevelForExperience：由累计经验反推当前等级。
// 从 1 级开始逐级向上：只要经验还 >= 升到该级所需累计经验，就继续升；否则停在当前级。
// 曲线数据在 ValidateDefinitionAndCurve 已保证单调递增，因此这里不用做额外防御。
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

// ValidateRuntimeState：校验一份运行时状态是否与定义、经验曲线一致。
// 校验点：角色 ID 匹配、等级在 1..MaxLevel、经验非负、突破次数非负、每个技能等级
// 落在对应上限内，并且「经验反推的等级」与状态里声明的等级一致——这是防止状态被
// 手工篡改的关键约束（例如直接改等级而不改经验）。
EHSRCharacterProgressionResult UHSRCharacterProgressionLibrary::ValidateRuntimeState(const UHSRCharacterDefinition* Definition, const FHSRCharacterRuntimeState& State)
{
	const UCurveFloat* Curve = nullptr;
	const EHSRCharacterProgressionResult DefinitionResult = ValidateDefinitionAndCurve(Definition, Curve);
	if (DefinitionResult != EHSRCharacterProgressionResult::Success)
	{
		return DefinitionResult;
	}
	if (State.CharacterId != Definition->CharacterId
		|| State.Level < 1 || State.Level > Definition->MaxLevel
		|| State.Experience < 0 || State.Ascension < 0)
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
	return ExpectedLevel == State.Level
		? EHSRCharacterProgressionResult::Success
		: EHSRCharacterProgressionResult::InvalidRuntimeState;
}

// TryGrantExperience：给角色加经验并据此更新等级。
// 采用「先复制候选状态、成功后整体替换」的不可变式更新：先在 Candidate 上算出新
// 经验与等级，全部校验通过后才写回 InOutState。经验为 0 是合法的 no-op。
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

	// 用 int64 做加法以避免 int32 溢出后误判。
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

// TrySetSkillLevel：直接设定某技能等级（不校验经验曲线，只校验技能 ID 与上限）。
// 同样采用候选状态整体替换，且不校验技能当前等级——因为这是「设置」而非「升级」，
// 允许跳级与降级，只要落在定义的上限内。
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
