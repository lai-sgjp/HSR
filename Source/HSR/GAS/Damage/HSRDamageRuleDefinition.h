#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HSRDamageRuleDefinition.generated.h"

/** Authored, immutable damage-rule values for the Phase 7 damage formula. */
UCLASS(BlueprintType)
class HSR_API UHSRDamageRuleDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Rule", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float DefenseCoefficient = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Rule", meta = (ClampMin = "0.0", ClampMax = "1000000.0"))
	float MinDamage = 1.0f;

	/** Checks the frozen P7-000 numeric contract before a rule can be used. */
	bool IsValidRuleDefinition() const
	{
		return FMath::IsFinite(DefenseCoefficient)
			&& FMath::IsFinite(MinDamage)
			&& DefenseCoefficient >= 0.0f && DefenseCoefficient <= 100.0f
			&& MinDamage >= 0.0f && MinDamage <= 1000000.0f;
	}
};
