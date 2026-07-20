#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/DataAsset.h"
#include "GameplayEffect.h"
#include "../GAS/Ability/HSRAbilityTypes.h"
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects", meta = (DisplayName = "Gameplay Effect Class", ToolTip = "Primary effect shared by Basic/Skill/Ultimate definitions. Bind the P6-003 Skill Damage Gameplay Effect here."))
	TSoftClassPtr<UGameplayEffect> EffectGameplayEffectClass;

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
			&& !EffectGameplayEffectClass.IsNull();
	}

	bool IsValidSkillDefinition() const
	{
		return IsValidDefinition() && Category == EHSRSkillCategory::Skill
			&& TargetType == EHSRTargetType::SingleEnemy && !EffectGameplayEffectClass.IsNull();
	}
	bool IsValidHealDefinition() const { return IsValidDefinition() && Category == EHSRSkillCategory::Heal && (TargetType == EHSRTargetType::SingleAlly || TargetType == EHSRTargetType::Self) && !EffectGameplayEffectClass.IsNull(); }
};
