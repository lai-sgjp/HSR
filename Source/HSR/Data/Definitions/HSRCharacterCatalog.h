#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HSRCharacterCatalog.generated.h"
class UHSRCharacterDefinition;
UCLASS(BlueprintType)
class HSR_API UHSRCharacterCatalog : public UDataAsset
{
	GENERATED_BODY()
public:
	/** Data-only Blueprint definitions are selected as classes; their immutable authored data comes from the CDO. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TArray<TSubclassOf<UHSRCharacterDefinition>> Characters;
};
