#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "../../Equipment/HSREquipmentTypes.h"
#include "HSRRelicDefinition.generated.h"

UCLASS(BlueprintType)
class HSR_API UHSRRelicDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic")
	FName DefinitionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic")
	FName SetId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic")
	EHSRRelicSlot Slot = EHSRRelicSlot::Head;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic", meta = (ClampMin = "0"))
	int32 EnhancementCap = 0;

	/** Authored main-stat line granted when an instance of this relic is minted, so equipping a
	 *  dropped relic produces a visible derived-stat increase without a separate roll step. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic")
	TArray<FHSREquipmentModifier> DefaultModifiers;
};
