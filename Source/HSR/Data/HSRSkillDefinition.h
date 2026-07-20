#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/DataAsset.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "../GAS/Ability/HSRAbilityTypes.h"
#include "../GAS/Damage/HSRDamageRuleDefinition.h"
#include "HSRSkillDefinition.generated.h"

UCLASS(BlueprintType)
class HSR_API UHSRSkillDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
	FName SkillId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
	EHSRSkillCategory Category = EHSRSkillCategory::BasicAttack;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
	EHSRTargetType TargetType = EHSRTargetType::SingleEnemy;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
	TSubclassOf<UGameplayAbility> AbilityClass;

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

	/** Static damage multiplier. The default preserves existing P6 asset validity. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.000001", ClampMax = "100.0"))
	float AbilityMultiplier = 1.0f;

	/** Frozen Phase 7 rule used by formal Basic/Skill damage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	TSoftObjectPtr<UHSRDamageRuleDefinition> DamageRule;

	bool IsValidDefinition() const
	{
		return !SkillId.IsNone() && AbilityClass != nullptr;
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
	bool IsValidHealDefinition() const { return IsValidDefinition() && Category == EHSRSkillCategory::Heal && (TargetType == EHSRTargetType::SingleAlly || TargetType == EHSRTargetType::Self) && !EffectGameplayEffectClass.IsNull(); }
};
