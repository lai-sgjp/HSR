#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "../../Equipment/HSREquipmentTypes.h"
#include "HSREquipmentDefinition.generated.h"

UCLASS(BlueprintType)
class HSR_API UHSREquipmentDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	FName DefinitionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	EHSREquipmentSlot Slot = EHSREquipmentSlot::Weapon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (ClampMin = "0"))
	int32 EnhancementCap = 0;
};
