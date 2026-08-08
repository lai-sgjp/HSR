#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "../../Equipment/HSREquipmentTypes.h"
#include "HSREquipmentEnhancementCatalog.generated.h"

USTRUCT(BlueprintType)
struct HSR_API FHSREquipmentEnhancementRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment Enhancement")
	FName DefinitionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment Enhancement")
	EHSREquipmentKind Kind = EHSREquipmentKind::Equipment;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment Enhancement", meta = (ClampMin = "0"))
	int32 TargetLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment Enhancement")
	FName MaterialItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment Enhancement", meta = (ClampMin = "1"))
	int32 MaterialCost = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment Enhancement")
	TArray<FHSREquipmentModifier> TargetModifiers;
};

UCLASS(BlueprintType)
class HSR_API UHSREquipmentEnhancementCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment Enhancement")
	TArray<FHSREquipmentEnhancementRule> Rules;

	bool AddRule(const FHSREquipmentEnhancementRule& Rule);
	bool ResolveRule(FName DefinitionId, EHSREquipmentKind Kind, int32 TargetLevel,
		FHSREquipmentEnhancementRule& OutRule) const;
	void GetRulesFor(FName DefinitionId, EHSREquipmentKind Kind, int32 CurrentLevel,
		TArray<FHSREquipmentEnhancementRule>& OutRules) const;

private:
	static bool IsValidModifierSnapshot(const TArray<FHSREquipmentModifier>& Modifiers);
};
