#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "../../Inventory/HSRItemTypes.h"
#include "HSRItemDefinition.generated.h"

UCLASS(BlueprintType)
class HSR_API UHSRItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	EHSRItemStorageKind StorageKind = EHSRItemStorageKind::Stackable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1"))
	int32 MaxStack = 99;
};
