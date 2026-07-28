#pragma once

#include "CoreMinimal.h"
#include "HSRCharacterProgressionTypes.h"
#include "HSRCharacterProfileTypes.generated.h"

UENUM(BlueprintType)
enum class EHSRCharacterProfileResult : uint8
{
	Success,
	DefinitionAlreadyRegistered,
	ProfileNotFound,
	ProgressionRejected,
	CatalogNotLoaded,
	EmptyAssetPath,
	AssetLoadFailed,
	ExperienceCurveLoadFailed,
	RevisionConflict
};

USTRUCT(BlueprintType)
struct HSR_API FHSRCharacterProfileSnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FHSRCharacterRuntimeState RuntimeState;
	UPROPERTY(BlueprintReadOnly) int64 RuntimeRevision = 0;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FHSRCharacterProfileChanged, FName, int64);
