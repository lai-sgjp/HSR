#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HSRCharacterProgressionTypes.h"
#include "HSRCharacterProgressionLibrary.generated.h"

class UHSRCharacterDefinition;

/** Transactional pure-value progression operations. All failure results leave InOutState unchanged. */
UCLASS()
class HSR_API UHSRCharacterProgressionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "HSR|Progression")
	static EHSRCharacterProgressionResult ValidateRuntimeState(const UHSRCharacterDefinition* Definition, const FHSRCharacterRuntimeState& State);

	UFUNCTION(BlueprintCallable, Category = "HSR|Progression")
	static EHSRCharacterProgressionResult TryGrantExperience(const UHSRCharacterDefinition* Definition, int32 ExperienceToGrant, UPARAM(ref) FHSRCharacterRuntimeState& InOutState);

	UFUNCTION(BlueprintCallable, Category = "HSR|Progression")
	static EHSRCharacterProgressionResult TrySetSkillLevel(const UHSRCharacterDefinition* Definition, FName SkillId, int32 SkillLevel, UPARAM(ref) FHSRCharacterRuntimeState& InOutState);

private:
	static EHSRCharacterProgressionResult ValidateDefinitionAndCurve(const UHSRCharacterDefinition* Definition, const class UCurveFloat*& OutCurve);
	static EHSRCharacterProgressionResult GetLevelForExperience(const UHSRCharacterDefinition& Definition, const class UCurveFloat& Curve, int32 Experience, int32& OutLevel);
};
