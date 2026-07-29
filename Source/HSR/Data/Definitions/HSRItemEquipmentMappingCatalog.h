#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "../../Inventory/HSRItemTypes.h"
#include "../../Equipment/HSREquipmentTypes.h"
#include "HSRItemEquipmentMappingCatalog.generated.h"

USTRUCT(BlueprintType)
struct HSR_API FHSRItemEquipmentMappingEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment Mapping")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment Mapping")
	FName EquipmentDefinitionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment Mapping")
	EHSREquipmentKind Kind = EHSREquipmentKind::Equipment;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment Mapping")
	int32 Slot = 0;
};

UCLASS(BlueprintType)
class HSR_API UHSRItemEquipmentMappingCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment Mapping")
	TArray<FHSRItemEquipmentMappingEntry> Mappings;

	bool AddMapping(const FHSRItemEquipmentMappingEntry& Entry);
	bool Resolve(FName ItemId, FHSRItemEquipmentMappingEntry& OutEntry) const;
	bool Validate(FName ItemId, EHSRItemStorageKind StorageKind, const TFunctionRef<bool(FName, EHSREquipmentKind, int32)>& DefinitionValidator, FHSRItemEquipmentMappingEntry& OutEntry) const;
};
