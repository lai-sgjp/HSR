#include "HSRDamageExecutionCalculation.h"
#include "HSRDamageEffectContext.h"
#include "../Attribute/HSRCoreAttributeSet.h"
#include "GameplayEffectExtension.h"

namespace HSRDamageCapture
{
	struct FStatics
	{
		DECLARE_ATTRIBUTE_CAPTUREDEF(Attack);
		DECLARE_ATTRIBUTE_CAPTUREDEF(Defense);
		DECLARE_ATTRIBUTE_CAPTUREDEF(CritRate);
		DECLARE_ATTRIBUTE_CAPTUREDEF(CritDamage);

		FStatics()
		{
			DEFINE_ATTRIBUTE_CAPTUREDEF(UHSRCoreAttributeSet, Attack, Source, false);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UHSRCoreAttributeSet, CritRate, Source, false);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UHSRCoreAttributeSet, CritDamage, Source, false);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UHSRCoreAttributeSet, Defense, Target, false);
		}
	};

	const FStatics& Get()
	{
		static const FStatics Statics;
		return Statics;
	}
}

UHSRDamageExecutionCalculation::UHSRDamageExecutionCalculation()
{
	const HSRDamageCapture::FStatics& Statics = HSRDamageCapture::Get();
	RelevantAttributesToCapture.Add(Statics.AttackDef);
	RelevantAttributesToCapture.Add(Statics.CritRateDef);
	RelevantAttributesToCapture.Add(Statics.CritDamageDef);
	RelevantAttributesToCapture.Add(Statics.DefenseDef);
}

void UHSRDamageExecutionCalculation::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	const HSRDamageCapture::FStatics& Statics = HSRDamageCapture::Get();
	FHSRDamageEffectContext* Context = static_cast<FHSRDamageEffectContext*>(Spec.GetContext().Get());
	if (!Context || Context->GetScriptStruct() != FHSRDamageEffectContext::StaticStruct()) { return; }
	FAggregatorEvaluateParameters Params;
	float Attack = 0.0f, Defense = 0.0f, CritRate = 0.0f, CritDamage = 0.0f;
	const bool bCaptured = ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(Statics.AttackDef, Params, Attack)
		&& ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(Statics.DefenseDef, Params, Defense)
		&& ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(Statics.CritRateDef, Params, CritRate)
		&& ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(Statics.CritDamageDef, Params, CritDamage);
	FHSRDamageResult& Result = Context->DamageResult;
	Result.ActionId = Context->DamageContext.ActionId;
	Result.DamageType = Context->DamageContext.DamageType;
	if (!bCaptured) { Result.Result = EHSRDamageResultType::CaptureFailed; return; }
	if (!FMath::IsFinite(Attack) || !FMath::IsFinite(Defense) || !FMath::IsFinite(CritRate) || !FMath::IsFinite(CritDamage)) { Result.Result = EHSRDamageResultType::InvalidCapturedValue; return; }
	const float NormalAttack = FMath::Max(0.0f, Attack);
	const float NormalDefense = FMath::Max(0.0f, Defense);
	const bool bCritical = Context->DamageContext.CritRoll < FMath::Clamp(CritRate, 0.0f, 1.0f);
	const float RawDamage = FMath::Max(Context->MinDamage, NormalAttack * Context->DamageContext.AbilityMultiplier - NormalDefense * Context->DefenseCoefficient);
	const float CritMultiplier = bCritical ? 1.0f + FMath::Max(0.0f, CritDamage) : 1.0f;
	const float FinalDamage = FMath::Max(Context->MinDamage, static_cast<float>(FMath::RoundHalfFromZero(RawDamage * CritMultiplier)));
	Result.Breakdown.NormalizedAttack = NormalAttack;
	Result.Breakdown.NormalizedDefense = NormalDefense;
	Result.Breakdown.RawDamage = RawDamage;
	Result.Breakdown.bCritical = bCritical;
	Result.Breakdown.CritMultiplier = CritMultiplier;
	Result.Breakdown.FinalDamage = FinalDamage;
	Result.Result = EHSRDamageResultType::DamageResolved;
	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UHSRCoreAttributeSet::GetIncomingDamageAttribute(), EGameplayModOp::Additive, FinalDamage));
}
