#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HSRCharacterDerivedStats.h"
#include "HSRCharacterProgressionTypes.h"
#include "HSRCharacterStatAggregator.generated.h"
class UHSRCharacterDefinition;
UCLASS()
class HSR_API UHSRCharacterStatAggregator : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	static bool BuildContext(const UHSRCharacterDefinition* Definition, const FHSRCharacterRuntimeState& Runtime, int64 Revision, FHSRCharacterProgressionContext& OutContext);
};
