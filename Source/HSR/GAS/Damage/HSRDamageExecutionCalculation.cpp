#include "HSRDamageExecutionCalculation.h"
#include <limits>
#include "HSRDamageEffectContext.h"
#include "../Attribute/HSRCoreAttributeSet.h"
#include "GameplayEffectExtension.h"

namespace HSRDamageCapture
{
	// 需要捕获的属性定义：攻击/暴击率/暴伤来自攻击方（Source），防御来自受击方（Target）。
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

// 构造函数：注册需要捕获的属性。
UHSRDamageExecutionCalculation::UHSRDamageExecutionCalculation()
{
	const HSRDamageCapture::FStatics& Statics = HSRDamageCapture::Get();
	RelevantAttributesToCapture.Add(Statics.AttackDef);
	RelevantAttributesToCapture.Add(Statics.CritRateDef);
	RelevantAttributesToCapture.Add(Statics.CritDamageDef);
	RelevantAttributesToCapture.Add(Statics.DefenseDef);
}

// 伤害公式执行：捕获攻防与暴击属性，套用公式算出最终伤害，
// 通过“传入伤害”元属性写回（由 AttributeSet 真正扣血），并把纯结果写入伤害上下文。
void UHSRDamageExecutionCalculation::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	const HSRDamageCapture::FStatics& Statics = HSRDamageCapture::Get();
	// 伤害上下文必须是我们自己的类型，否则直接返回（无法安全读取）。
	FHSRDamageEffectContext* Context = static_cast<FHSRDamageEffectContext*>(Spec.GetContext().Get());
	if (!Context || Context->GetScriptStruct() != FHSRDamageEffectContext::StaticStruct())
	{
		return;
	}
	FAggregatorEvaluateParameters Params;
	float Attack = 0.0f, Defense = 0.0f, CritRate = 0.0f, CritDamage = 0.0f;
	const bool bCaptured = ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(Statics.AttackDef, Params, Attack)
		&& ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(Statics.DefenseDef, Params, Defense)
		&& ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(Statics.CritRateDef, Params, CritRate)
		&& ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(Statics.CritDamageDef, Params, CritDamage);
	FHSRDamageResult& Result = Context->DamageResult;
	Result.ActionId = Context->DamageContext.ActionId;
	Result.DamageType = Context->DamageContext.DamageType;
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	// 开发测试注入：强制“捕获失败”或“捕获到非法值”。
	if (Context->TestInjection == EHSRDamageTestInjection::ForceCaptureFailed)
	{
		Result.Result = EHSRDamageResultType::CaptureFailed;
		return;
	}
	if (Context->TestInjection == EHSRDamageTestInjection::ForceInvalidCapturedValue)
	{
		Attack = std::numeric_limits<float>::quiet_NaN();
	}
#endif
	if (!bCaptured)
	{
		Result.Result = EHSRDamageResultType::CaptureFailed;
		return;
	}
	// 捕获值必须是有限值，否则判为非法捕获。
	if (!FMath::IsFinite(Attack) || !FMath::IsFinite(Defense) || !FMath::IsFinite(CritRate) || !FMath::IsFinite(CritDamage))
	{
		Result.Result = EHSRDamageResultType::InvalidCapturedValue;
		return;
	}
	// 伤害公式：
	// NormalAttack/NormalDefense 钳非负；暴击由预置的 CritRoll 与暴击率比较决定；
	// Raw = max(MinDamage, 攻×倍率 - 防×防御系数)；暴击乘区 = 1 + 暴伤；
	// Final = max(MinDamage, 四舍五入(Raw × 暴击乘区))。
	const float NormalAttack = FMath::Max(0.0f, Attack);
	const float NormalDefense = FMath::Max(0.0f, Defense);
	const bool bCritical = Context->DamageContext.CritRoll < FMath::Clamp(CritRate, 0.0f, 1.0f);
	const float RawDamage = FMath::Max(Context->MinDamage, NormalAttack * Context->DamageContext.AbilityMultiplier - NormalDefense * Context->DefenseCoefficient);
	const float CritMultiplier = bCritical ? 1.0f + FMath::Max(0.0f, CritDamage) : 1.0f;
	const float FinalDamage = FMath::Max(Context->MinDamage, static_cast<float>(FMath::RoundHalfFromZero(RawDamage * CritMultiplier)));
	// 把每一步结果写入纯结果 DTO（供公式追溯/测试）。
	Result.Breakdown.NormalizedAttack = NormalAttack;
	Result.Breakdown.NormalizedDefense = NormalDefense;
	Result.Breakdown.RawDamage = RawDamage;
	Result.Breakdown.bCritical = bCritical;
	Result.Breakdown.CritMultiplier = CritMultiplier;
	Result.Breakdown.FinalDamage = FinalDamage;
	Result.Result = EHSRDamageResultType::DamageResolved;
	// 输出“传入伤害”修饰符，让 AttributeSet 完成实际扣血。
	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UHSRCoreAttributeSet::GetIncomingDamageAttribute(), EGameplayModOp::Additive, FinalDamage));
}
