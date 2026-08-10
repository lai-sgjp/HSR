#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "../../UI/Inventory/HSRInventoryTypes.h"
#include "HSRInventoryCatalog.generated.h"

UCLASS(BlueprintType)
class HSR_API UHSRInventoryCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TArray<FHSRInventoryCatalogEntry> Entries;

	bool Validate(FString* OutError = nullptr) const;
	bool FindEntry(FName ItemId, FHSRInventoryCatalogEntry& OutEntry) const;
};
