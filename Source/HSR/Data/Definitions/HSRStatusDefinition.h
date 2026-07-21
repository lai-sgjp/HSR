#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "../../Status/HSRStatusTypes.h"
#include "HSRStatusDefinition.generated.h"

UCLASS(BlueprintType)
class HSR_API UHSRStatusDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display") FText DisplayName;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display", meta = (MultiLine = "true")) FText Description;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status") FName StatusId;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status", meta = (ClampMin = "1")) int32 DurationTurns = 2;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status", meta = (ClampMin = "1")) int32 MaxStacks = 1;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status") EHSRStatusTriggerTiming TriggerTiming = EHSRStatusTriggerTiming::TargetTurnEnded;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status") EHSRStatusRefreshPolicy RefreshPolicy = EHSRStatusRefreshPolicy::RefreshDuration;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status") FGameplayTag GrantedStatusTag;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status") TSoftClassPtr<UGameplayEffect> InfiniteGameplayEffectClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status") EHSRStatusEffectKind EffectKind = EHSRStatusEffectKind::TagOnly;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status") EHSRStatusClassification Classification = EHSRStatusClassification::Buff;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status", meta = (EditCondition = "Classification == EHSRStatusClassification::Debuff")) bool bDispellable = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status", meta = (EditCondition = "Classification == EHSRStatusClassification::Debuff")) FGameplayTag ImmunityTag;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status") EHSRSourceInvalidPolicy SourceInvalidPolicy = EHSRSourceInvalidPolicy::Keep;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (EditCondition = "EffectKind == EHSRStatusEffectKind::DamageOverTime")) FGameplayTag DamageType;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (EditCondition = "EffectKind == EHSRStatusEffectKind::DamageOverTime")) float DamageAbilityMultiplier = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (EditCondition = "EffectKind == EHSRStatusEffectKind::DamageOverTime")) TSoftObjectPtr<class UHSRDamageRuleDefinition> DamageRule;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (EditCondition = "EffectKind == EHSRStatusEffectKind::DamageOverTime")) TSoftClassPtr<UGameplayEffect> DamageGameplayEffectClass;

	EHSRStatusOperationResult Validate() const;
};
