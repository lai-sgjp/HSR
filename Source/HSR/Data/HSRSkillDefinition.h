#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/DataAsset.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "../GAS/Ability/HSRAbilityTypes.h"
#include "../GAS/Damage/HSRDamageRuleDefinition.h"
#include "HSRBreakTypes.h"
#include "HSRSkillDefinition.generated.h"

class UHSRStatusDefinition;

UCLASS(BlueprintType)
class HSR_API UHSRSkillDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Short authored label for command buttons. Empty values safely fall back to SkillId. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	FText DisplayName;

	/** Long authored presentation text for future battle-detail screens. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
	FName SkillId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
	EHSRSkillCategory Category = EHSRSkillCategory::BasicAttack;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
	EHSRTargetType TargetType = EHSRTargetType::SingleEnemy;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
	TSubclassOf<UGameplayAbility> AbilityClass;

	/** Energy granted to the acting participant after a successful Basic or Skill.
	 * The Coordinator applies it once as part of the authoritative action transaction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Resources", meta = (ClampMin = "0.0"))
	float EnergyGain = 20.0f;

	/** Used only by the Ultimate vertical slice. The ability owns the runtime
	 * commit; the DataAsset remains static configuration. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Costs", meta = (DisplayName = "Cost Gameplay Effect Class", ToolTip = "Ultimate Energy Cost only. GAS applies this Gameplay Effect; BattleCoordinator never writes Energy."))
	TSoftClassPtr<UGameplayEffect> CostGameplayEffectClass;

	/** Instant positive-Energy GAS compensation used only when an Ultimate
	 * cost committed but its prepared damage application failed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Costs")
	TSoftClassPtr<UGameplayEffect> EnergyRefundGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects", meta = (DisplayName = "Gameplay Effect Class", ToolTip = "Primary effect shared by Basic/Skill/Ultimate definitions. Bind the P6-003 Skill Damage Gameplay Effect here."))
	TSoftClassPtr<UGameplayEffect> EffectGameplayEffectClass;

	/** Static damage classification consumed by the Phase 7 damage execution. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	FGameplayTag DamageType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Element")
	FGameplayTag ElementTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Element", meta = (ClampMin = "0.000001", ClampMax = "1000.0"))
	float ToughnessDamage = 1.0f;

	/** P8-only Instant GE which modifies IncomingToughnessDamage and nothing else. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Element")
	TSoftClassPtr<UGameplayEffect> ToughnessDamageGameplayEffectClass;

	/** Static damage multiplier. The default preserves existing P6 asset validity. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.000001", ClampMax = "100.0"))
	float AbilityMultiplier = 1.0f;

	/** Frozen Phase 7 rule used by formal Basic/Skill damage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	TSoftObjectPtr<UHSRDamageRuleDefinition> DamageRule;

	/** Statuses applied to each resolved target after this skill lands.  Empty is the common
	 * case and costs nothing; authoring an entry is the whole story for a debuff skill, no
	 * C++ change required.  Entries that fail to load are skipped with a warning rather than
	 * failing the action. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status")
	TArray<TSoftObjectPtr<UHSRStatusDefinition>> AppliedStatuses;

	bool IsValidDefinition() const
	{
		return !SkillId.IsNone() && AbilityClass != nullptr;
	}

	EHSRElementToughnessContractResult GetElementToughnessContractResult() const
	{
		const EHSRElementToughnessContractResult ElementResult = FHSRToughnessConfiguration::ValidateElement(ElementTag);
		return ElementResult != EHSRElementToughnessContractResult::Valid ? ElementResult : FHSRToughnessConfiguration::ValidateToughnessDamage(ToughnessDamage);
	}

	bool IsValidUltimateDefinition() const
	{
		return IsValidDefinition()
			&& Category == EHSRSkillCategory::Ultimate
			&& TargetType == EHSRTargetType::SingleEnemy
			&& !CostGameplayEffectClass.IsNull()
			&& !EnergyRefundGameplayEffectClass.IsNull()
			&& !EffectGameplayEffectClass.IsNull();
	}

	bool IsValidSkillDefinition() const
	{
		return IsValidDefinition() && Category == EHSRSkillCategory::Skill
			&& TargetType == EHSRTargetType::SingleEnemy && !EffectGameplayEffectClass.IsNull();
	}
	bool IsValidHealDefinition() const
	{
		return IsValidDefinition()
			&& Category == EHSRSkillCategory::Heal
			&& (TargetType == EHSRTargetType::SingleAlly || TargetType == EHSRTargetType::Self)
			&& !EffectGameplayEffectClass.IsNull();
	}

	/** Category-dispatched validation.  One entry point so callers never have to know which
	 * per-category validator applies -- adding a category means extending this switch only. */
	bool IsValidForCategory() const
	{
		switch (Category)
		{
		case EHSRSkillCategory::BasicAttack:
			return IsValidDefinition() && TargetType == EHSRTargetType::SingleEnemy;
		case EHSRSkillCategory::Skill:
			return IsValidSkillDefinition();
		case EHSRSkillCategory::Ultimate:
			return IsValidUltimateDefinition();
		case EHSRSkillCategory::Heal:
			return IsValidHealDefinition();
		default:
			return false;
		}
	}

	/** Authored skill-point delta applied when this skill commits: negative spends, positive
	 * generates.  Authoring -1 on a Skill and +1 on a BasicAttack reproduces the legacy
	 * hardcoded behaviour, which is what GetSkillPointDelta() falls back to when left at 0. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cost")
	int32 SkillPointDelta = 0;

	/** Effective skill-point delta.  Falls back to the historical per-category rule when the
	 * DataAsset leaves SkillPointDelta at its default, so existing assets keep working. */
	int32 GetSkillPointDelta() const
	{
		if (SkillPointDelta != 0)
		{
			return SkillPointDelta;
		}
		switch (Category)
		{
		case EHSRSkillCategory::BasicAttack:
			return 1;
		case EHSRSkillCategory::Skill:
			return -1;
		default:
			return 0;
		}
	}

	/** Skill-point cost as the UI displays it: a positive number of points consumed. */
	int32 GetSkillPointCost() const
	{
		const int32 Delta = GetSkillPointDelta();
		return Delta < 0 ? -Delta : 0;
	}

	/**
	 * Behavioural predicates below replace open-coded Category comparisons at the call sites.
	 * Each answers one question a caller actually has; none of them expose Category itself, so a
	 * new category only has to answer these rather than have every caller learn its name.
	 *
	 * They read Category today because that is where the authored data lives. The point is not to
	 * hide the enum -- it is that the branch exists in one place per behaviour instead of once per
	 * caller, so a fifth category is a change here and nowhere else.
	 */

	/** True when this skill restores Health, so callers gate full-health rejection and heal-amount
	 *  sampling on capability rather than on being literally the Heal category. */
	bool RestoresHealth() const
	{
		return Category == EHSRSkillCategory::Heal;
	}

	/** True when this skill resolves through the formal prepared-damage seam. Heal deliberately
	 *  does not: it has no damage to prepare. */
	bool UsesPreparedDamage() const
	{
		return Category == EHSRSkillCategory::BasicAttack
			|| Category == EHSRSkillCategory::Skill
			|| Category == EHSRSkillCategory::Ultimate;
	}

	/** True when granting this skill must also configure the ability instance. BasicAttack carries
	 *  no per-action configuration, so granting it is complete on its own. */
	bool RequiresAbilityConfiguration() const
	{
		return Category != EHSRSkillCategory::BasicAttack;
	}

	/** True when a shortage of team skill points makes this skill unavailable. Derived from the
	 *  authored delta, so a skill authored to spend points is gated whatever its category. */
	bool RequiresSkillPointsToCommit() const
	{
		return GetSkillPointDelta() < 0;
	}
};
